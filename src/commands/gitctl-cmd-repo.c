/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
 * gitctl-cmd-repo.c - Repository command handler
 *
 * Implements the "repo" command with verb dispatch for: list, get,
 * create, fork, clone, delete, and browse.
 *
 * All verbs parse their options and delegate to gctl_cmd_execute_verb().
 */

#define GCTL_COMPILATION

#include <string.h>
#include <json-glib/json-glib.h>

#include "commands/gitctl-cmd-common-private.h"
#include "commands/gitctl-cmd-repo.h"

/* ── Verb dispatch table ─────────────────────────────────────────────── */

static const GctlVerbEntry repo_verbs[] = {
	{ "list",   "List repositories",                GCTL_VERB_LIST   },
	{ "get",    "View a single repository",         GCTL_VERB_GET    },
	{ "create", "Create a new repository",          GCTL_VERB_CREATE },
	{ "fork",   "Fork a repository",                GCTL_VERB_FORK   },
	{ "clone",  "Clone a repository",               GCTL_VERB_CLONE  },
	{ "delete", "Delete a repository",              GCTL_VERB_DELETE },
	{ "browse", "Open the repository in browser",   GCTL_VERB_BROWSE },
	{ "star",   "Star a repository",               GCTL_VERB_STAR   },
	{ "unstar", "Remove star from repository",     GCTL_VERB_UNSTAR },
};

static const gsize N_REPO_VERBS = G_N_ELEMENTS(repo_verbs);

/* ── Usage printer ───────────────────────────────────────────────────── */

/**
 * print_usage:
 *
 * Prints the usage summary for the "repo" command, listing all
 * available verbs and their descriptions.
 */
static void
print_usage(void)
{
	gctl_cmd_print_verb_table("repo", repo_verbs, N_REPO_VERBS);
}

/* ── repo list ───────────────────────────────────────────────────────── */

/**
 * cmd_repo_list:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl repo list".  Parses --limit and --visibility options.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_repo_list(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	gint limit = 30;
	gchar *visibility = NULL;
	gint ret;

	GOptionEntry entries[] = {
		{ "limit", 'l', 0, G_OPTION_ARG_INT, &limit,
		  "Maximum number of results (default: 30)", "N" },
		{ "visibility", 'v', 0, G_OPTION_ARG_STRING, &visibility,
		  "Filter by visibility (public/private/all)", "VIS" },
		{ NULL }
	};

	opt_context = g_option_context_new("- list repositories");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	{
		gchar *limit_str;

		limit_str = g_strdup_printf("%d", limit);
		g_hash_table_insert(params, g_strdup("limit"), limit_str);
	}

	if (visibility != NULL)
		g_hash_table_insert(params, g_strdup("visibility"),
		                    g_strdup(visibility));

	ret = gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_REPO,
	                            GCTL_VERB_LIST, NULL, params);

	g_free(visibility);

	return ret;
}

/* ── repo get ────────────────────────────────────────────────────────── */

/**
 * cmd_repo_get:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl repo get <owner/repo>".
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_repo_get(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	const gchar *owner_repo;

	GOptionEntry entries[] = {
		{ NULL }
	};

	opt_context = g_option_context_new("<owner/repo> - view a repository");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	if (argc < 2)
	{
		g_printerr("error: repository identifier required (owner/repo)\n");
		g_printerr("Usage: gitctl repo get <owner/repo>\n");
		return 1;
	}

	owner_repo = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_REPO,
	                             GCTL_VERB_GET, owner_repo, params);
}

/* ── repo create ─────────────────────────────────────────────────────── */

/* Mirror-to option variables (used by cmd_repo_create) */
static gchar **opt_mirror_to = NULL;
static gchar *opt_token_github = NULL;
static gchar *opt_token_gitlab = NULL;
static gchar *opt_token_forgejo = NULL;
static gchar *opt_token_gitea = NULL;

/* ── URL parsing helper (shared with mirror command) ─────────────────── */

/**
 * repo_parse_mirror_url:
 * @url: a remote repository URL (HTTPS or SSH)
 * @host: (out) (transfer full): the hostname
 * @owner: (out) (transfer full): the repository owner
 * @repo: (out) (transfer full): the repository name (without .git suffix)
 *
 * Parses a mirror destination URL into its component parts.  Handles
 * HTTPS URLs (https://host/owner/repo.git) and SSH URLs
 * (git@host:owner/repo.git).
 *
 * Returns: %TRUE on success, %FALSE if the URL could not be parsed
 */
static gboolean
repo_parse_mirror_url(
	const gchar  *url,
	gchar       **host,
	gchar       **owner,
	gchar       **repo
){
	/* HTTPS / HTTP URLs */
	if (g_str_has_prefix(url, "https://") ||
	    g_str_has_prefix(url, "http://"))
	{
		g_autoptr(GUri) uri = NULL;
		const gchar *uri_host;
		const gchar *path;
		g_auto(GStrv) segments = NULL;
		guint count;
		gchar *repo_name;

		uri = g_uri_parse(url, G_URI_FLAGS_NONE, NULL);
		if (uri == NULL)
			return FALSE;

		uri_host = g_uri_get_host(uri);
		path = g_uri_get_path(uri);

		if (uri_host == NULL || path == NULL || *path == '\0')
			return FALSE;

		if (*path == '/')
			path++;

		segments = g_strsplit(path, "/", 0);
		count = g_strv_length(segments);

		if (count < 2)
			return FALSE;

		repo_name = g_strdup(segments[1]);
		if (g_str_has_suffix(repo_name, ".git"))
			repo_name[strlen(repo_name) - 4] = '\0';

		*host  = g_strdup(uri_host);
		*owner = g_strdup(segments[0]);
		*repo  = repo_name;

		return TRUE;
	}

	/* SSH URLs: git@host:owner/repo.git */
	if (g_str_has_prefix(url, "git@") || strchr(url, ':') != NULL)
	{
		const gchar *at_sign;
		const gchar *colon;
		g_auto(GStrv) parts = NULL;
		guint count;
		gchar *repo_name;

		at_sign = strchr(url, '@');
		if (at_sign == NULL)
			return FALSE;

		colon = strchr(at_sign + 1, ':');
		if (colon == NULL)
			return FALSE;

		*host = g_strndup(at_sign + 1, (gsize)(colon - at_sign - 1));

		parts = g_strsplit(colon + 1, "/", 0);
		count = g_strv_length(parts);

		if (count < 2)
		{
			g_free(*host);
			*host = NULL;
			return FALSE;
		}

		repo_name = g_strdup(parts[1]);
		if (g_str_has_suffix(repo_name, ".git"))
			repo_name[strlen(repo_name) - 4] = '\0';

		*owner = g_strdup(parts[0]);
		*repo  = repo_name;

		return TRUE;
	}

	return FALSE;
}

/* ── Push mirror body builder ────────────────────────────────────────── */

/**
 * repo_build_push_mirror_body:
 * @forge_type: the source forge type
 * @url: the remote mirror URL
 * @token: (nullable): authentication token
 * @interval: the sync interval string (e.g. "8h0m0s")
 * @mask_token: if %TRUE, replaces the token with "***" (for dry-run)
 *
 * Builds a JSON request body for creating a push mirror on the source
 * forge, targeting the given URL.
 *
 * Returns: (transfer full) (nullable): a JSON string, or %NULL if the
 *     forge type does not support push mirrors
 */
static gchar *
repo_build_push_mirror_body(
	GctlForgeType  forge_type,
	const gchar   *url,
	const gchar   *token,
	const gchar   *interval,
	gboolean       mask_token
){
	g_autoptr(JsonBuilder) builder = NULL;
	g_autoptr(JsonGenerator) gen = NULL;
	g_autoptr(JsonNode) root = NULL;
	gchar *json_str;

	builder = json_builder_new();

	if (forge_type == GCTL_FORGE_TYPE_FORGEJO ||
	    forge_type == GCTL_FORGE_TYPE_GITEA)
	{
		json_builder_begin_object(builder);

		json_builder_set_member_name(builder, "remote_address");
		json_builder_add_string_value(builder, url);

		if (token != NULL)
		{
			json_builder_set_member_name(builder, "remote_password");
			json_builder_add_string_value(builder,
			                              mask_token ? "***" : token);
		}

		json_builder_set_member_name(builder, "interval");
		json_builder_add_string_value(builder, interval);

		json_builder_set_member_name(builder, "sync_on_commit");
		json_builder_add_boolean_value(builder, TRUE);

		json_builder_end_object(builder);
	}
	else if (forge_type == GCTL_FORGE_TYPE_GITLAB)
	{
		json_builder_begin_object(builder);

		json_builder_set_member_name(builder, "url");
		json_builder_add_string_value(builder, url);

		json_builder_set_member_name(builder, "enabled");
		json_builder_add_boolean_value(builder, TRUE);

		json_builder_end_object(builder);
	}
	else
	{
		/* GitHub does not support push mirrors via API */
		return NULL;
	}

	root = json_builder_get_root(builder);
	gen = json_generator_new();
	json_generator_set_root(gen, root);

	json_str = json_generator_to_data(gen, NULL);
	return json_str;
}

/* ── Mirror setup helper ─────────────────────────────────────────────── */

/**
 * get_token_for_forge:
 * @forge_type: the destination forge type
 *
 * Returns the appropriate token for a given forge type based on the
 * --token-* options passed on the command line.
 *
 * Returns: (transfer none) (nullable): the token string, or %NULL
 */
static const gchar *
get_token_for_forge(GctlForgeType forge_type)
{
	switch (forge_type)
	{
		case GCTL_FORGE_TYPE_GITHUB:  return opt_token_github;
		case GCTL_FORGE_TYPE_GITLAB:  return opt_token_gitlab;
		case GCTL_FORGE_TYPE_FORGEJO: return opt_token_forgejo;
		case GCTL_FORGE_TYPE_GITEA:   return opt_token_gitea;
		default:                      return NULL;
	}
}

/**
 * setup_mirror_to:
 * @app: the #GctlApp instance
 * @mirror_url: the destination mirror URL
 * @source_context: the source forge context (the repo we just created)
 *
 * Sets up a push mirror from the source repository to the destination
 * specified by @mirror_url.  This involves:
 *
 * 1. Parsing the mirror URL to extract host, owner, and repo
 * 2. Looking up the destination forge type from config
 * 3. Creating a blank repo on the destination (ignoring errors)
 * 4. Adding a push mirror on the source forge pointing to the dest
 *
 * On any failure, a warning is printed and execution continues.
 */
static void
setup_mirror_to(
	GctlApp          *app,
	const gchar      *mirror_url,
	GctlForgeContext *source_context
){
	GctlConfig *config;
	GctlExecutor *executor;
	GctlModuleManager *mm;
	g_autofree gchar *dest_host = NULL;
	g_autofree gchar *dest_owner = NULL;
	g_autofree gchar *dest_repo = NULL;
	GctlForgeType dest_forge_type;
	GctlForge *dest_forge;
	GctlForgeType source_forge_type;
	GctlForge *source_forge;
	const gchar *token;
	gboolean verbose;
	gboolean is_dry_run;

	config = gctl_app_get_config(app);
	executor = gctl_app_get_executor(app);
	mm = gctl_app_get_module_manager(app);
	verbose = gctl_app_get_verbose(app);
	is_dry_run = gctl_executor_get_dry_run(executor);

	/* Step 1: Parse the mirror URL */
	if (!repo_parse_mirror_url(mirror_url, &dest_host, &dest_owner, &dest_repo))
	{
		g_printerr("warning: could not parse mirror URL '%s', skipping\n",
		           mirror_url);
		return;
	}

	if (verbose)
		g_printerr("note: setting up mirror to %s/%s on %s\n",
		           dest_owner, dest_repo, dest_host);

	/* Step 2: Look up dest forge type */
	dest_forge_type = gctl_config_get_forge_for_host(config, dest_host);
	if (dest_forge_type == GCTL_FORGE_TYPE_UNKNOWN)
	{
		g_printerr("warning: unknown forge for host '%s', skipping mirror\n",
		           dest_host);
		return;
	}

	/* Step 3: Find the dest forge module */
	dest_forge = gctl_module_manager_find_forge(mm, dest_forge_type);
	if (dest_forge == NULL)
	{
		g_printerr("warning: no module for %s, skipping mirror\n",
		           gctl_forge_type_to_string(dest_forge_type));
		return;
	}

	/* Step 4: Get the appropriate token */
	token = get_token_for_forge(dest_forge_type);

	/* Step 5: Create the dest repo (ignore errors -- may already exist) */
	{
		g_autoptr(GctlForgeContext) dest_context = NULL;
		g_autoptr(GHashTable) create_params = NULL;
		g_autoptr(GError) create_error = NULL;
		g_autoptr(GctlCommandResult) create_result = NULL;
		const gchar *dest_cli;
		gchar **create_argv;

		dest_cli = gctl_config_get_cli_path(config, dest_forge_type);

		dest_context = gctl_forge_context_new(
		    dest_forge_type, mirror_url,
		    dest_owner, dest_repo,
		    dest_host, dest_cli);

		create_params = g_hash_table_new_full(
		    g_str_hash, g_str_equal, g_free, g_free);
		g_hash_table_insert(create_params,
		                    g_strdup("name"), g_strdup(dest_repo));
		g_hash_table_insert(create_params,
		                    g_strdup("private"), g_strdup("true"));

		if (verbose)
			g_printerr("note: creating destination repo %s/%s on %s\n",
			           dest_owner, dest_repo,
			           gctl_forge_type_to_string(dest_forge_type));

		create_argv = gctl_forge_build_argv(
		    dest_forge, GCTL_RESOURCE_KIND_REPO,
		    GCTL_VERB_CREATE, dest_context,
		    create_params, &create_error);

		if (create_argv == NULL)
		{
			g_printerr("warning: could not build repo create command for "
			           "%s: %s\n",
			           gctl_forge_type_to_string(dest_forge_type),
			           create_error ? create_error->message : "unknown error");
		}
		else
		{
			create_result = gctl_executor_run(
			    executor,
			    (const gchar * const *)create_argv,
			    &create_error);
			g_strfreev(create_argv);

			if (create_result == NULL ||
			    gctl_command_result_get_exit_code(create_result) != 0)
			{
				g_printerr("warning: repo creation on %s failed "
				           "(may already exist), continuing\n",
				           gctl_forge_type_to_string(dest_forge_type));
			}
		}
	}

	/* Step 6: Add push mirror on the SOURCE forge */
	source_forge_type = gctl_forge_context_get_forge_type(source_context);
	source_forge = gctl_module_manager_find_forge(mm, source_forge_type);

	if (source_forge == NULL)
	{
		g_printerr("warning: no module for source forge %s, "
		           "cannot add push mirror\n",
		           gctl_forge_type_to_string(source_forge_type));
		return;
	}

	{
		g_autofree gchar *body = NULL;
		g_autofree gchar *endpoint = NULL;
		g_autoptr(GError) mirror_err = NULL;
		g_autoptr(GctlCommandResult) mirror_result = NULL;
		gchar **mirror_argv;

		body = repo_build_push_mirror_body(
		    source_forge_type, mirror_url, token,
		    "8h0m0s", is_dry_run);

		if (body == NULL)
		{
			g_printerr("warning: push mirrors not supported on %s, "
			           "skipping mirror to %s\n",
			           gctl_forge_type_to_string(source_forge_type),
			           mirror_url);
			return;
		}

		/* Build the push mirror API endpoint for the source repo */
		endpoint = g_strdup_printf("/repos/%s/%s/push_mirrors",
		                           gctl_forge_context_get_owner(source_context),
		                           gctl_forge_context_get_repo_name(source_context));

		if (verbose)
			g_printerr("note: adding push mirror: POST %s\n", endpoint);

		mirror_argv = gctl_forge_build_api_argv(
		    source_forge, "POST", endpoint, body,
		    source_context, &mirror_err);

		if (mirror_argv == NULL)
		{
			g_printerr("warning: could not build mirror API call: %s\n",
			           mirror_err ? mirror_err->message : "unknown error");
			return;
		}

		mirror_result = gctl_executor_run(
		    executor,
		    (const gchar * const *)mirror_argv,
		    &mirror_err);
		g_strfreev(mirror_argv);

		if (mirror_result == NULL ||
		    gctl_command_result_get_exit_code(mirror_result) != 0)
		{
			const gchar *stderr_text = NULL;
			if (mirror_result != NULL)
				stderr_text = gctl_command_result_get_stderr(mirror_result);
			g_printerr("warning: push mirror setup failed for %s",
			           mirror_url);
			if (stderr_text != NULL && stderr_text[0] != '\0')
				g_printerr(": %s", stderr_text);
			g_printerr("\n");
		}
		else if (verbose)
		{
			g_printerr("note: push mirror to %s configured successfully\n",
			           mirror_url);
		}
	}
}

/**
 * cmd_repo_create:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl repo create <name>".  Parses --private,
 * --description, --clone, --mirror-to, and --token-* options.
 *
 * When --mirror-to is specified, after the primary repo creation
 * succeeds, a push mirror is set up to each destination URL.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_repo_create(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	gboolean is_private = FALSE;
	gchar *description = NULL;
	gboolean clone_after = FALSE;
	const gchar *repo_name;
	gint ret;

	/* Reset mirror-to statics for safety */
	opt_mirror_to = NULL;
	opt_token_github = NULL;
	opt_token_gitlab = NULL;
	opt_token_forgejo = NULL;
	opt_token_gitea = NULL;

	GOptionEntry entries[] = {
		{ "private", 'p', 0, G_OPTION_ARG_NONE, &is_private,
		  "Create as private repository", NULL },
		{ "description", 'd', 0, G_OPTION_ARG_STRING, &description,
		  "Repository description", "DESC" },
		{ "clone", 'c', 0, G_OPTION_ARG_NONE, &clone_after,
		  "Clone the repository after creating", NULL },
		{ "mirror-to", 0, 0, G_OPTION_ARG_STRING_ARRAY, &opt_mirror_to,
		  "Push mirror to this URL after creation (repeatable)", "URL" },
		{ "token-github", 0, 0, G_OPTION_ARG_STRING, &opt_token_github,
		  "GitHub token for mirror destination", "TOKEN" },
		{ "token-gitlab", 0, 0, G_OPTION_ARG_STRING, &opt_token_gitlab,
		  "GitLab token for mirror destination", "TOKEN" },
		{ "token-forgejo", 0, 0, G_OPTION_ARG_STRING, &opt_token_forgejo,
		  "Forgejo token for mirror destination", "TOKEN" },
		{ "token-gitea", 0, 0, G_OPTION_ARG_STRING, &opt_token_gitea,
		  "Gitea token for mirror destination", "TOKEN" },
		{ NULL }
	};

	opt_context = g_option_context_new("<name> - create a repository");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	if (argc < 2)
	{
		g_printerr("error: repository name required\n");
		g_printerr("Usage: gitctl repo create <name> [options]\n");
		return 1;
	}

	repo_name = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	if (is_private)
		g_hash_table_insert(params, g_strdup("private"), g_strdup("true"));

	if (description != NULL)
		g_hash_table_insert(params, g_strdup("description"),
		                    g_strdup(description));

	if (clone_after)
		g_hash_table_insert(params, g_strdup("clone"), g_strdup("true"));

	ret = gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_REPO,
	                            GCTL_VERB_CREATE, repo_name, params);

	/* Post-create: set up push mirrors if --mirror-to was specified */
	if (ret == 0 && opt_mirror_to != NULL)
	{
		GctlContextResolver *resolver;
		GctlConfig *config;
		g_autoptr(GctlForgeContext) source_context = NULL;
		g_autoptr(GError) resolve_err = NULL;
		const gchar *default_remote;
		gint m;

		config = gctl_app_get_config(app);
		resolver = gctl_app_get_resolver(app);
		default_remote = gctl_config_get_default_remote(config);

		source_context = gctl_context_resolver_resolve(
		    resolver, default_remote, &resolve_err);

		if (source_context == NULL)
		{
			g_printerr("warning: could not resolve source forge context "
			           "for mirror setup: %s\n",
			           resolve_err ? resolve_err->message : "unknown error");
		}
		else
		{
			for (m = 0; opt_mirror_to[m] != NULL; m++)
			{
				setup_mirror_to(app, opt_mirror_to[m], source_context);
			}
		}
	}

	g_free(description);
	g_strfreev(opt_mirror_to);
	g_free(opt_token_github);
	g_free(opt_token_gitlab);
	g_free(opt_token_forgejo);
	g_free(opt_token_gitea);

	/* Reset statics */
	opt_mirror_to = NULL;
	opt_token_github = NULL;
	opt_token_gitlab = NULL;
	opt_token_forgejo = NULL;
	opt_token_gitea = NULL;

	return ret;
}

/* ── repo fork ───────────────────────────────────────────────────────── */

/**
 * cmd_repo_fork:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl repo fork <owner/repo>".
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_repo_fork(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *owner_repo;

	if (argc < 2)
	{
		g_printerr("error: repository identifier required (owner/repo)\n");
		g_printerr("Usage: gitctl repo fork <owner/repo>\n");
		return 1;
	}

	owner_repo = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_REPO,
	                             GCTL_VERB_FORK, owner_repo, params);
}

/* ── repo clone ──────────────────────────────────────────────────────── */

/**
 * cmd_repo_clone:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl repo clone <owner/repo>".
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_repo_clone(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *owner_repo;

	if (argc < 2)
	{
		g_printerr("error: repository identifier required (owner/repo)\n");
		g_printerr("Usage: gitctl repo clone <owner/repo>\n");
		return 1;
	}

	owner_repo = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_REPO,
	                             GCTL_VERB_CLONE, owner_repo, params);
}

/* ── repo delete ─────────────────────────────────────────────────────── */

/**
 * cmd_repo_delete:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl repo delete <owner/repo>".
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_repo_delete(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *owner_repo;

	if (argc < 2)
	{
		g_printerr("error: repository identifier required (owner/repo)\n");
		g_printerr("Usage: gitctl repo delete <owner/repo>\n");
		return 1;
	}

	owner_repo = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_REPO,
	                             GCTL_VERB_DELETE, owner_repo, params);
}

/* ── repo browse ─────────────────────────────────────────────────────── */

/**
 * cmd_repo_browse:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl repo browse".  Opens the repository in the browser.
 * No arguments required; uses the current repo context.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_repo_browse(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;

	(void)argc;
	(void)argv;

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_REPO,
	                             GCTL_VERB_BROWSE, NULL, params);
}

/* ── repo star ───────────────────────────────────────────────────────── */

/**
 * cmd_repo_star:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl repo star [owner/repo]".  Stars the specified
 * repository, or the current repository if no argument is given.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_repo_star(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *owner_repo;

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	owner_repo = (argc >= 2) ? argv[1] : NULL;

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_REPO,
	                             GCTL_VERB_STAR, owner_repo, params);
}

/* ── repo unstar ─────────────────────────────────────────────────────── */

/**
 * cmd_repo_unstar:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl repo unstar [owner/repo]".  Removes the star from
 * the specified repository, or the current repository if no argument
 * is given.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_repo_unstar(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *owner_repo;

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	owner_repo = (argc >= 2) ? argv[1] : NULL;

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_REPO,
	                             GCTL_VERB_UNSTAR, owner_repo, params);
}

/* ── Main entry point ────────────────────────────────────────────────── */

/**
 * gctl_cmd_repo:
 * @app: the #GctlApp instance
 * @argc: argument count (verb + verb-specific args)
 * @argv: (array length=argc): argument vector, where argv[0] is the verb
 *
 * Main entry point for the "repo" command.  Extracts the verb from
 * argv[0], looks it up in the dispatch table, and delegates to the
 * appropriate verb handler.
 *
 * Returns: 0 on success, 1 on error
 */
gint
gctl_cmd_repo(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	const GctlVerbEntry *entry;
	const gchar *verb_name;

	if (argc < 1)
	{
		print_usage();
		return 0;
	}

	/* Handle --help / -h before verb lookup */
	if (g_strcmp0(argv[0], "--help") == 0 ||
	    g_strcmp0(argv[0], "-h") == 0)
	{
		print_usage();
		return 0;
	}

	verb_name = argv[0];
	entry = gctl_cmd_find_verb(repo_verbs, N_REPO_VERBS, verb_name);

	if (entry == NULL)
	{
		g_printerr("error: unknown verb '%s' for repo command\n",
		           verb_name);
		print_usage();
		return 1;
	}

	switch (entry->verb)
	{
		case GCTL_VERB_LIST:
			return cmd_repo_list(app, argc, argv);
		case GCTL_VERB_GET:
			return cmd_repo_get(app, argc, argv);
		case GCTL_VERB_CREATE:
			return cmd_repo_create(app, argc, argv);
		case GCTL_VERB_FORK:
			return cmd_repo_fork(app, argc, argv);
		case GCTL_VERB_CLONE:
			return cmd_repo_clone(app, argc, argv);
		case GCTL_VERB_DELETE:
			return cmd_repo_delete(app, argc, argv);
		case GCTL_VERB_BROWSE:
			return cmd_repo_browse(app, argc, argv);
		case GCTL_VERB_STAR:
			return cmd_repo_star(app, argc, argv);
		case GCTL_VERB_UNSTAR:
			return cmd_repo_unstar(app, argc, argv);
		default:
			g_printerr("error: verb '%s' not implemented for repo\n",
			           verb_name);
			return 1;
	}
}
