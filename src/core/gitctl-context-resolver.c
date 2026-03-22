/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-context-resolver.c - Forge detection from git remote URLs */

#define GCTL_COMPILATION
#include "gitctl.h"

#include <gio/gio.h>
#include <string.h>

/* ── Private structure ────────────────────────────────────────────── */

struct _GctlContextResolver
{
	GObject parent_instance;

	GctlConfig    *config;       /* weak ref, not owned */
	GctlForgeType  forced_forge;
};

G_DEFINE_TYPE(GctlContextResolver, gctl_context_resolver, G_TYPE_OBJECT)

/* ── GObject vfuncs ───────────────────────────────────────────────── */

static void
gctl_context_resolver_finalize(GObject *object)
{
	GctlContextResolver *self;

	self = GCTL_CONTEXT_RESOLVER(object);

	/*
	 * config is a weak reference — we do not own it, so we just
	 * clear the pointer without unreffing.
	 */
	self->config = NULL;

	G_OBJECT_CLASS(gctl_context_resolver_parent_class)->finalize(object);
}

static void
gctl_context_resolver_class_init(GctlContextResolverClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);
	object_class->finalize = gctl_context_resolver_finalize;
}

static void
gctl_context_resolver_init(GctlContextResolver *self)
{
	self->config       = NULL;
	self->forced_forge = GCTL_FORGE_TYPE_UNKNOWN;
}

/* ── Internal helpers ─────────────────────────────────────────────── */

/*
 * run_git_remote_get_url:
 * @remote_name: the git remote name (e.g. "origin")
 * @error: return location for a #GError
 *
 * Spawns `git remote get-url <remote_name>` and returns the trimmed
 * stdout output.  On failure, sets @error and returns %NULL.
 */
static gchar *
run_git_remote_get_url(
	const gchar  *remote_name,
	GError      **error
){
	g_autoptr(GSubprocess) proc = NULL;
	g_autofree gchar *stdout_buf = NULL;
	g_autofree gchar *stderr_buf = NULL;

	proc = g_subprocess_new(
		G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
		error,
		"git", "remote", "get-url", remote_name, NULL);

	if (proc == NULL)
		return NULL;

	if (!g_subprocess_communicate_utf8(proc, NULL, NULL,
	                                   &stdout_buf, &stderr_buf, error))
	{
		return NULL;
	}

	if (!g_subprocess_get_successful(proc)) {
		g_set_error(error, GCTL_ERROR, GCTL_ERROR_NO_REMOTE,
		            "git remote get-url %s failed: %s",
		            remote_name,
		            stderr_buf ? stderr_buf : "(no output)");
		return NULL;
	}

	/* Trim trailing whitespace / newline */
	g_strstrip(stdout_buf);

	return g_steal_pointer(&stdout_buf);
}

/*
 * parse_https_url:
 * @url: an HTTPS git remote URL
 * @out_host: (out) (transfer full): the hostname
 * @out_owner: (out) (transfer full): the repository owner
 * @out_repo: (out) (transfer full): the repository name
 * @error: return location for a #GError
 *
 * Parses URLs of the form:
 *   https://github.com/owner/repo.git
 *   https://github.com/owner/repo
 *
 * Returns: %TRUE on success
 */
static gboolean
parse_https_url(
	const gchar  *url,
	gchar       **out_host,
	gchar       **out_owner,
	gchar       **out_repo,
	GError      **error
){
	g_autoptr(GUri) uri = NULL;
	const gchar *host;
	const gchar *path;
	g_auto(GStrv) segments = NULL;
	guint count;
	gchar *repo_name;

	uri = g_uri_parse(url, G_URI_FLAGS_NONE, error);
	if (uri == NULL)
		return FALSE;

	host = g_uri_get_host(uri);
	path = g_uri_get_path(uri);

	if (host == NULL || path == NULL || *path == '\0') {
		g_set_error(error, GCTL_ERROR, GCTL_ERROR_FORGE_DETECT,
		            "Could not extract host/path from URL: %s", url);
		return FALSE;
	}

	/*
	 * path looks like "/owner/repo" or "/owner/repo.git".
	 * Skip the leading '/' then split on '/'.
	 */
	if (*path == '/')
		path++;

	segments = g_strsplit(path, "/", 0);
	count = g_strv_length(segments);

	if (count < 2) {
		g_set_error(error, GCTL_ERROR, GCTL_ERROR_FORGE_DETECT,
		            "URL path has fewer than 2 segments: %s", url);
		return FALSE;
	}

	/* Strip .git suffix from repo name */
	repo_name = g_strdup(segments[1]);
	if (g_str_has_suffix(repo_name, ".git")) {
		repo_name[strlen(repo_name) - 4] = '\0';
	}

	*out_host  = g_strdup(host);
	*out_owner = g_strdup(segments[0]);
	*out_repo  = repo_name;

	return TRUE;
}

/*
 * parse_ssh_url:
 * @url: an SSH git remote URL
 * @out_host: (out) (transfer full): the hostname
 * @out_owner: (out) (transfer full): the repository owner
 * @out_repo: (out) (transfer full): the repository name
 * @error: return location for a #GError
 *
 * Parses URLs of the form:
 *   git@github.com:owner/repo.git
 *   git@github.com:owner/repo
 *
 * Returns: %TRUE on success
 */
static gboolean
parse_ssh_url(
	const gchar  *url,
	gchar       **out_host,
	gchar       **out_owner,
	gchar       **out_repo,
	GError      **error
){
	const gchar *at_sign;
	const gchar *colon;
	g_auto(GStrv) parts = NULL;
	guint count;
	gchar *repo_name;

	/* Find the '@' and ':' that delimit host and path */
	at_sign = strchr(url, '@');
	if (at_sign == NULL) {
		g_set_error(error, GCTL_ERROR, GCTL_ERROR_FORGE_DETECT,
		            "SSH URL missing '@': %s", url);
		return FALSE;
	}

	colon = strchr(at_sign + 1, ':');
	if (colon == NULL) {
		g_set_error(error, GCTL_ERROR, GCTL_ERROR_FORGE_DETECT,
		            "SSH URL missing ':' after host: %s", url);
		return FALSE;
	}

	/* Extract host: between '@' and ':' */
	*out_host = g_strndup(at_sign + 1, (gsize)(colon - at_sign - 1));

	/* Extract path after ':' and split on '/' */
	parts = g_strsplit(colon + 1, "/", 0);
	count = g_strv_length(parts);

	if (count < 2) {
		g_set_error(error, GCTL_ERROR, GCTL_ERROR_FORGE_DETECT,
		            "SSH URL path has fewer than 2 segments: %s", url);
		g_free(*out_host);
		*out_host = NULL;
		return FALSE;
	}

	/* Strip .git suffix from repo name */
	repo_name = g_strdup(parts[1]);
	if (g_str_has_suffix(repo_name, ".git")) {
		repo_name[strlen(repo_name) - 4] = '\0';
	}

	*out_owner = g_strdup(parts[0]);
	*out_repo  = repo_name;

	return TRUE;
}

/*
 * parse_ssh_scheme_url:
 * @url: an SSH-scheme git remote URL
 * @out_host: (out) (transfer full): the hostname
 * @out_owner: (out) (transfer full): the repository owner
 * @out_repo: (out) (transfer full): the repository name
 * @error: return location for a #GError
 *
 * Parses URLs of the form:
 *   ssh://git@github.com/owner/repo.git
 *   ssh://git@host:2222/owner/repo.git
 *   ssh://host/owner/repo.git
 *
 * Returns: %TRUE on success
 */
static gboolean
parse_ssh_scheme_url(
	const gchar  *url,
	gchar       **out_host,
	gchar       **out_owner,
	gchar       **out_repo,
	GError      **error
){
	g_autoptr(GUri) uri = NULL;
	const gchar *host;
	const gchar *path;
	g_auto(GStrv) segments = NULL;
	guint count;
	gchar *repo_name;

	uri = g_uri_parse(url, G_URI_FLAGS_NONE, error);
	if (uri == NULL)
		return FALSE;

	host = g_uri_get_host(uri);
	path = g_uri_get_path(uri);

	if (host == NULL || path == NULL || *path == '\0') {
		g_set_error(error, GCTL_ERROR, GCTL_ERROR_FORGE_DETECT,
		            "Could not extract host/path from SSH URL: %s", url);
		return FALSE;
	}

	/*
	 * path looks like "/owner/repo" or "/owner/repo.git".
	 * Skip the leading '/' then split on '/'.
	 */
	if (*path == '/')
		path++;

	segments = g_strsplit(path, "/", 0);
	count = g_strv_length(segments);

	if (count < 2) {
		g_set_error(error, GCTL_ERROR, GCTL_ERROR_FORGE_DETECT,
		            "SSH URL path has fewer than 2 segments: %s", url);
		return FALSE;
	}

	/* Strip .git suffix from repo name */
	repo_name = g_strdup(segments[1]);
	if (g_str_has_suffix(repo_name, ".git"))
		repo_name[strlen(repo_name) - 4] = '\0';

	*out_host  = g_strdup(host);
	*out_owner = g_strdup(segments[0]);
	*out_repo  = repo_name;

	return TRUE;
}

/*
 * parse_remote_url:
 * @url: the raw remote URL string
 * @out_host: (out) (transfer full): the hostname
 * @out_owner: (out) (transfer full): the repository owner
 * @out_repo: (out) (transfer full): the repository name
 * @error: return location for a #GError
 *
 * Dispatches to the appropriate URL parser based on the URL scheme.
 *
 * Returns: %TRUE on success
 */
static gboolean
parse_remote_url(
	const gchar  *url,
	gchar       **out_host,
	gchar       **out_owner,
	gchar       **out_repo,
	GError      **error
){
	if (g_str_has_prefix(url, "https://") ||
	    g_str_has_prefix(url, "http://"))
	{
		return parse_https_url(url, out_host, out_owner, out_repo, error);
	}

	/* ssh:// scheme URLs (may include port) */
	if (g_str_has_prefix(url, "ssh://"))
	{
		return parse_ssh_scheme_url(url, out_host, out_owner, out_repo, error);
	}

	if (g_str_has_prefix(url, "git@") || strchr(url, ':') != NULL) {
		return parse_ssh_url(url, out_host, out_owner, out_repo, error);
	}

	g_set_error(error, GCTL_ERROR, GCTL_ERROR_FORGE_DETECT,
	            "Unrecognized remote URL format: %s", url);
	return FALSE;
}

/* ── Public API ───────────────────────────────────────────────────── */

GctlContextResolver *
gctl_context_resolver_new(GctlConfig *config)
{
	GctlContextResolver *self;

	g_return_val_if_fail(GCTL_IS_CONFIG(config), NULL);

	self = (GctlContextResolver *)g_object_new(
		GCTL_TYPE_CONTEXT_RESOLVER, NULL);
	self->config = config;

	return self;
}

void
gctl_context_resolver_set_forced_forge(
	GctlContextResolver  *self,
	GctlForgeType         forge_type
){
	g_return_if_fail(GCTL_IS_CONTEXT_RESOLVER(self));

	self->forced_forge = forge_type;
}

GctlForgeContext *
gctl_context_resolver_resolve(
	GctlContextResolver  *self,
	const gchar          *remote_name,
	GError              **error
){
	g_autofree gchar *url = NULL;
	g_autofree gchar *host = NULL;
	g_autofree gchar *owner = NULL;
	g_autofree gchar *repo = NULL;
	GctlForgeType forge_type;
	const gchar *cli_path;
	GctlForgeContext *ctx;

	g_return_val_if_fail(GCTL_IS_CONTEXT_RESOLVER(self), NULL);
	g_return_val_if_fail(remote_name != NULL, NULL);

	/* Step 1: Get the remote URL */
	url = run_git_remote_get_url(remote_name, error);

	if (url != NULL) {
		/* Step 2: Parse host, owner, repo from the URL */
		if (!parse_remote_url(url, &host, &owner, &repo, error))
			return NULL;
	}

	/* Step 3: Determine forge type */
	if (self->forced_forge != GCTL_FORGE_TYPE_UNKNOWN) {
		forge_type = self->forced_forge;

		/*
		 * When the forge is forced and the remote URL couldn't be
		 * resolved, clear the error — we can still proceed for
		 * operations that don't need owner/repo context (e.g.
		 * repo list, repo create).
		 */
		if (url == NULL)
			g_clear_error(error);
	} else {
		if (url == NULL)
			return NULL;

		forge_type = gctl_config_get_forge_for_host(self->config, host);
		if (forge_type == GCTL_FORGE_TYPE_UNKNOWN) {
			g_set_error(error, GCTL_ERROR, GCTL_ERROR_FORGE_DETECT,
			            "Could not detect forge type for host '%s'. "
			            "Use --forge to specify explicitly.", host);
			return NULL;
		}
	}

	/* Step 4: Get CLI tool path */
	cli_path = gctl_config_get_cli_path(self->config, forge_type);

	/* Step 5: Build the forge context boxed type */
	ctx = gctl_forge_context_new(
		forge_type,
		url,
		owner,
		repo,
		host,
		cli_path);

	return ctx;
}
