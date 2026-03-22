/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
 * gitctl-gitlab-forge.c - GitLab forge backend module
 *
 * Implements the GctlForge interface using the `glab` CLI tool.
 * GitLab uses "merge request" (MR) instead of "pull request", so
 * GCTL_RESOURCE_KIND_PR maps to `glab mr` subcommands.
 *
 * Key glab differences from gh:
 *   - JSON output: `-F json` or `--output json`
 *   - Body parameter: `--description` instead of `--body`
 *   - Comment subcommand: `note` with `--message` flag
 *   - State filtering: boolean flags (--closed, --merged, --all)
 *     instead of `--state <value>`.  Open is the default.
 *   - Limit: `--per-page` instead of `--limit`
 *   - Edit: `update` instead of `edit`
 *   - JSON fields: "iid" not "number", "web_url" not "url",
 *     author is {"username": "..."}
 */

#define GCTL_COMPILATION
#include <gitctl.h>
#include <gmodule.h>
#include <json-glib/json-glib.h>
#include <string.h>

/* ── Type declaration ─────────────────────────────────────────────── */

#define GCTL_TYPE_GITLAB_FORGE (gctl_gitlab_forge_get_type())

G_DECLARE_FINAL_TYPE(
	GctlGitlabForge,
	gctl_gitlab_forge,
	GCTL,
	GITLAB_FORGE,
	GctlModule
)

struct _GctlGitlabForge
{
	GctlModule parent_instance;
};

/* ── Forward declarations ─────────────────────────────────────────── */

static const gchar *gitlab_forge_get_name(GctlForge *self);
static const gchar *gitlab_forge_get_cli_tool(GctlForge *self);
static GctlForgeType gitlab_forge_get_forge_type(GctlForge *self);
static gboolean gitlab_forge_can_handle_url(GctlForge *self, const gchar *remote_url);
static gboolean gitlab_forge_is_available(GctlForge *self);

static gchar **gitlab_forge_build_argv(
	GctlForge *self, GctlResourceKind resource, GctlVerb verb,
	GctlForgeContext *context, GHashTable *params, GError **error);

static GPtrArray *gitlab_forge_parse_list_output(
	GctlForge *self, GctlResourceKind resource,
	const gchar *raw_output, GError **error);

static GctlResource *gitlab_forge_parse_get_output(
	GctlForge *self, GctlResourceKind resource,
	const gchar *raw_output, GError **error);

static gchar **gitlab_forge_build_api_argv(
	GctlForge *self, const gchar *method, const gchar *endpoint,
	const gchar *body, GError **error);

/* ── GctlModule overrides ─────────────────────────────────────────── */

static const gchar *
gitlab_module_get_name(GctlModule *self)
{
	return "gitlab";
}

static const gchar *
gitlab_module_get_description(GctlModule *self)
{
	return "GitLab forge backend using the glab CLI";
}

static gboolean
gitlab_module_activate(GctlModule *self)
{
	return TRUE;
}

static void
gitlab_module_deactivate(GctlModule *self)
{
	/* nothing to tear down */
}

/* ── Interface init ───────────────────────────────────────────────── */

static void
gctl_gitlab_forge_forge_init(GctlForgeInterface *iface)
{
	iface->get_name          = gitlab_forge_get_name;
	iface->get_cli_tool      = gitlab_forge_get_cli_tool;
	iface->get_forge_type    = gitlab_forge_get_forge_type;
	iface->can_handle_url    = gitlab_forge_can_handle_url;
	iface->is_available      = gitlab_forge_is_available;
	iface->build_argv        = gitlab_forge_build_argv;
	iface->parse_list_output = gitlab_forge_parse_list_output;
	iface->parse_get_output  = gitlab_forge_parse_get_output;
	iface->build_api_argv    = gitlab_forge_build_api_argv;
}

/* ── Type registration ────────────────────────────────────────────── */

G_DEFINE_FINAL_TYPE_WITH_CODE(
	GctlGitlabForge,
	gctl_gitlab_forge,
	GCTL_TYPE_MODULE,
	G_IMPLEMENT_INTERFACE(GCTL_TYPE_FORGE,
	                      gctl_gitlab_forge_forge_init)
)

static void
gctl_gitlab_forge_class_init(GctlGitlabForgeClass *klass)
{
	GctlModuleClass *module_class = GCTL_MODULE_CLASS(klass);

	module_class->get_name        = gitlab_module_get_name;
	module_class->get_description = gitlab_module_get_description;
	module_class->activate        = gitlab_module_activate;
	module_class->deactivate      = gitlab_module_deactivate;
}

static void
gctl_gitlab_forge_init(GctlGitlabForge *self)
{
	/* no instance state */
}

/* ── Module entry point ───────────────────────────────────────────── */

/**
 * gctl_module_register:
 *
 * Entry point called by the module manager when loading this shared
 * library.  Returns the #GType of #GctlGitlabForge.
 *
 * Returns: the #GType for #GctlGitlabForge
 */
G_MODULE_EXPORT GType
gctl_module_register(void)
{
	return GCTL_TYPE_GITLAB_FORGE;
}

/* ── Helpers ──────────────────────────────────────────────────────── */

static const gchar *
get_param(GHashTable *params, const gchar *key)
{
	if (params == NULL)
		return NULL;
	return (const gchar *)g_hash_table_lookup(params, key);
}

static void
set_unsupported(GError **error, GctlResourceKind resource, GctlVerb verb)
{
	g_set_error(
		error,
		GCTL_ERROR,
		GCTL_ERROR_FORGE_UNSUPPORTED,
		"GitLab does not support %s + %s",
		gctl_resource_kind_to_string(resource),
		gctl_verb_to_string(verb)
	);
}

/* ── Identity ─────────────────────────────────────────────────────── */

static const gchar *
gitlab_forge_get_name(GctlForge *self)
{
	return "GitLab";
}

static const gchar *
gitlab_forge_get_cli_tool(GctlForge *self)
{
	return "glab";
}

static GctlForgeType
gitlab_forge_get_forge_type(GctlForge *self)
{
	return GCTL_FORGE_TYPE_GITLAB;
}

/* ── Detection ────────────────────────────────────────────────────── */

/**
 * gitlab_forge_can_handle_url:
 * @self: a #GctlForge
 * @remote_url: the git remote URL to test
 *
 * Checks for "gitlab.com" in the URL.  Self-hosted GitLab instances
 * may also be detected by the context resolver, so this serves as
 * the primary heuristic for the default public instance.
 *
 * Returns: %TRUE if the URL likely points to a GitLab instance
 */
static gboolean
gitlab_forge_can_handle_url(GctlForge *self, const gchar *remote_url)
{
	if (remote_url == NULL)
		return FALSE;
	return (strstr(remote_url, "gitlab.com") != NULL ||
	        strstr(remote_url, "gitlab.") != NULL);
}

static gboolean
gitlab_forge_is_available(GctlForge *self)
{
	g_autofree gchar *path = g_find_program_in_path("glab");
	return (path != NULL);
}

/* ── build_argv: MR (PR) operations ──────────────────────────────── */

/**
 * build_mr_argv:
 * @verb: the action to perform
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Builds glab CLI argv for merge request operations.  GitLab uses
 * `glab mr` subcommands and `--description` where GitHub uses `--body`.
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): argv
 */
static gchar **
build_mr_argv(
	GctlVerb            verb,
	GctlForgeContext   *context,
	GHashTable         *params,
	GError            **error
)
{
	g_autoptr(GPtrArray) argv = NULL;
	const gchar *val = NULL;

	argv = g_ptr_array_new_with_free_func(g_free);
	g_ptr_array_add(argv, g_strdup("glab"));
	g_ptr_array_add(argv, g_strdup("mr"));

	switch (verb) {
	case GCTL_VERB_LIST:
		g_ptr_array_add(argv, g_strdup("list"));

		/*
		 * glab does NOT use --state; instead it uses boolean flags:
		 *   --closed for closed MRs
		 *   --merged for merged MRs
		 *   --all    for all MRs
		 * Open is the default when no flag is given.
		 */
		val = get_param(params, "state");
		if (val != NULL) {
			if (g_strcmp0(val, "closed") == 0)
				g_ptr_array_add(argv, g_strdup("--closed"));
			else if (g_strcmp0(val, "merged") == 0)
				g_ptr_array_add(argv, g_strdup("--merged"));
			else if (g_strcmp0(val, "all") == 0)
				g_ptr_array_add(argv, g_strdup("--all"));
			/* "open" is the default — no flag needed */
		}

		val = get_param(params, "limit");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--per-page"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		g_ptr_array_add(argv, g_strdup("-F"));
		g_ptr_array_add(argv, g_strdup("json"));
		break;

	case GCTL_VERB_GET:
		g_ptr_array_add(argv, g_strdup("view"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		g_ptr_array_add(argv, g_strdup("-F"));
		g_ptr_array_add(argv, g_strdup("json"));
		break;

	case GCTL_VERB_CREATE:
		g_ptr_array_add(argv, g_strdup("create"));

		val = get_param(params, "title");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--title"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		val = get_param(params, "body");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--description"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		val = get_param(params, "base");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--target-branch"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		val = get_param(params, "head");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--source-branch"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		val = get_param(params, "draft");
		if (val != NULL && g_strcmp0(val, "true") == 0)
			g_ptr_array_add(argv, g_strdup("--draft"));
		break;

	case GCTL_VERB_CLOSE:
		g_ptr_array_add(argv, g_strdup("close"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));
		break;

	case GCTL_VERB_REOPEN:
		g_ptr_array_add(argv, g_strdup("reopen"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));
		break;

	case GCTL_VERB_MERGE:
		g_ptr_array_add(argv, g_strdup("merge"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		val = get_param(params, "strategy");
		if (val != NULL) {
			if (g_strcmp0(val, "rebase") == 0)
				g_ptr_array_add(argv, g_strdup("--rebase"));
			else if (g_strcmp0(val, "squash") == 0)
				g_ptr_array_add(argv, g_strdup("--squash"));
			/* default merge needs no flag on glab */
		}
		break;

	case GCTL_VERB_CHECKOUT:
		g_ptr_array_add(argv, g_strdup("checkout"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));
		break;

	case GCTL_VERB_COMMENT:
		/*
		 * glab uses "note" instead of "comment" for MRs,
		 * and --message instead of --body
		 */
		g_ptr_array_add(argv, g_strdup("note"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		val = get_param(params, "body");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--message"));
			g_ptr_array_add(argv, g_strdup(val));
		}
		break;

	case GCTL_VERB_REVIEW:
		g_ptr_array_add(argv, g_strdup("approve"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		/*
		 * glab only supports approve directly; for request-changes
		 * or comment reviews, fall through to a note
		 */
		val = get_param(params, "action");
		if (val != NULL && g_strcmp0(val, "approve") != 0) {
			/* reset argv for unsupported review types — use note */
			g_ptr_array_set_size(argv, 0);
			g_ptr_array_add(argv, g_strdup("glab"));
			g_ptr_array_add(argv, g_strdup("mr"));
			g_ptr_array_add(argv, g_strdup("note"));

			val = get_param(params, "number");
			if (val != NULL)
				g_ptr_array_add(argv, g_strdup(val));

			val = get_param(params, "body");
			if (val != NULL) {
				g_ptr_array_add(argv, g_strdup("--message"));
				g_ptr_array_add(argv, g_strdup(val));
			}
		}
		break;

	case GCTL_VERB_BROWSE:
		g_ptr_array_add(argv, g_strdup("view"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		g_ptr_array_add(argv, g_strdup("--web"));
		break;

	case GCTL_VERB_EDIT:
		g_ptr_array_add(argv, g_strdup("update"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		val = get_param(params, "title");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--title"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		val = get_param(params, "body");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--description"));
			g_ptr_array_add(argv, g_strdup(val));
		}
		break;

	default:
		set_unsupported(error, GCTL_RESOURCE_KIND_PR, verb);
		return NULL;
	}

	g_ptr_array_add(argv, NULL);
	return (gchar **)g_ptr_array_free(g_steal_pointer(&argv), FALSE);
}

/* ── build_argv: Issue operations ─────────────────────────────────── */

/**
 * build_issue_argv:
 * @verb: the action to perform
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Builds glab CLI argv for issue operations.
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): argv
 */
static gchar **
build_issue_argv(
	GctlVerb            verb,
	GctlForgeContext   *context,
	GHashTable         *params,
	GError            **error
)
{
	g_autoptr(GPtrArray) argv = NULL;
	const gchar *val = NULL;

	argv = g_ptr_array_new_with_free_func(g_free);
	g_ptr_array_add(argv, g_strdup("glab"));
	g_ptr_array_add(argv, g_strdup("issue"));

	switch (verb) {
	case GCTL_VERB_LIST:
		g_ptr_array_add(argv, g_strdup("list"));

		/*
		 * glab issue list also uses boolean flags instead of --state:
		 *   --closed for closed issues
		 *   --all    for all issues
		 * Open is the default when no flag is given.
		 */
		val = get_param(params, "state");
		if (val != NULL) {
			if (g_strcmp0(val, "closed") == 0)
				g_ptr_array_add(argv, g_strdup("--closed"));
			else if (g_strcmp0(val, "all") == 0)
				g_ptr_array_add(argv, g_strdup("--all"));
			/* "open" is the default — no flag needed */
		}

		val = get_param(params, "limit");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--per-page"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		g_ptr_array_add(argv, g_strdup("-F"));
		g_ptr_array_add(argv, g_strdup("json"));
		break;

	case GCTL_VERB_GET:
		g_ptr_array_add(argv, g_strdup("view"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		g_ptr_array_add(argv, g_strdup("-F"));
		g_ptr_array_add(argv, g_strdup("json"));
		break;

	case GCTL_VERB_CREATE:
		g_ptr_array_add(argv, g_strdup("create"));

		val = get_param(params, "title");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--title"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		val = get_param(params, "body");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--description"));
			g_ptr_array_add(argv, g_strdup(val));
		}
		break;

	case GCTL_VERB_CLOSE:
		g_ptr_array_add(argv, g_strdup("close"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));
		break;

	case GCTL_VERB_REOPEN:
		g_ptr_array_add(argv, g_strdup("reopen"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));
		break;

	case GCTL_VERB_COMMENT:
		g_ptr_array_add(argv, g_strdup("note"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		val = get_param(params, "body");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--message"));
			g_ptr_array_add(argv, g_strdup(val));
		}
		break;

	case GCTL_VERB_EDIT:
		g_ptr_array_add(argv, g_strdup("update"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		val = get_param(params, "title");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--title"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		val = get_param(params, "body");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--description"));
			g_ptr_array_add(argv, g_strdup(val));
		}
		break;

	case GCTL_VERB_BROWSE:
		g_ptr_array_add(argv, g_strdup("view"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		g_ptr_array_add(argv, g_strdup("--web"));
		break;

	default:
		set_unsupported(error, GCTL_RESOURCE_KIND_ISSUE, verb);
		return NULL;
	}

	g_ptr_array_add(argv, NULL);
	return (gchar **)g_ptr_array_free(g_steal_pointer(&argv), FALSE);
}

/* ── build_argv: Repo operations ──────────────────────────────────── */

/**
 * build_repo_argv:
 * @verb: the action to perform
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Builds glab CLI argv for repository operations.
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): argv
 */
static gchar **
build_repo_argv(
	GctlVerb            verb,
	GctlForgeContext   *context,
	GHashTable         *params,
	GError            **error
)
{
	g_autoptr(GPtrArray) argv = NULL;
	const gchar *val = NULL;

	argv = g_ptr_array_new_with_free_func(g_free);
	g_ptr_array_add(argv, g_strdup("glab"));
	g_ptr_array_add(argv, g_strdup("repo"));

	switch (verb) {
	case GCTL_VERB_LIST:
		g_ptr_array_add(argv, g_strdup("list"));

		val = get_param(params, "limit");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--per-page"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		g_ptr_array_add(argv, g_strdup("-F"));
		g_ptr_array_add(argv, g_strdup("json"));
		break;

	case GCTL_VERB_GET:
		g_ptr_array_add(argv, g_strdup("view"));

		val = get_param(params, "repo");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup(val));
		} else if (context != NULL && context->owner != NULL) {
			g_autofree gchar *slug = NULL;
			slug = gctl_forge_context_get_owner_repo(context);
			g_ptr_array_add(argv, g_strdup(slug));
		}

		g_ptr_array_add(argv, g_strdup("-F"));
		g_ptr_array_add(argv, g_strdup("json"));
		break;

	case GCTL_VERB_CREATE:
		g_ptr_array_add(argv, g_strdup("create"));

		val = get_param(params, "name");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--name"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		val = get_param(params, "private");
		if (val != NULL && g_strcmp0(val, "true") == 0)
			g_ptr_array_add(argv, g_strdup("--private"));

		val = get_param(params, "description");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--description"));
			g_ptr_array_add(argv, g_strdup(val));
		}
		break;

	case GCTL_VERB_FORK:
		g_ptr_array_add(argv, g_strdup("fork"));

		val = get_param(params, "repo");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup(val));
		} else if (context != NULL && context->owner != NULL) {
			g_autofree gchar *slug = NULL;
			slug = gctl_forge_context_get_owner_repo(context);
			g_ptr_array_add(argv, g_strdup(slug));
		}
		break;

	case GCTL_VERB_CLONE:
		g_ptr_array_add(argv, g_strdup("clone"));

		val = get_param(params, "repo");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup(val));
		} else if (context != NULL && context->owner != NULL) {
			g_autofree gchar *slug = NULL;
			slug = gctl_forge_context_get_owner_repo(context);
			g_ptr_array_add(argv, g_strdup(slug));
		}
		break;

	case GCTL_VERB_DELETE:
		g_ptr_array_add(argv, g_strdup("delete"));

		val = get_param(params, "repo");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup(val));
		} else if (context != NULL && context->owner != NULL) {
			g_autofree gchar *slug = NULL;
			slug = gctl_forge_context_get_owner_repo(context);
			g_ptr_array_add(argv, g_strdup(slug));
		}

		g_ptr_array_add(argv, g_strdup("--yes"));
		break;

	case GCTL_VERB_BROWSE:
		g_ptr_array_add(argv, g_strdup("view"));
		g_ptr_array_add(argv, g_strdup("--web"));
		break;

	default:
		set_unsupported(error, GCTL_RESOURCE_KIND_REPO, verb);
		return NULL;
	}

	g_ptr_array_add(argv, NULL);
	return (gchar **)g_ptr_array_free(g_steal_pointer(&argv), FALSE);
}

/* ── build_argv: Release operations ───────────────────────────────── */

/**
 * build_release_argv:
 * @verb: the action to perform
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Builds glab CLI argv for release operations.
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): argv
 */
static gchar **
build_release_argv(
	GctlVerb            verb,
	GctlForgeContext   *context,
	GHashTable         *params,
	GError            **error
)
{
	g_autoptr(GPtrArray) argv = NULL;
	const gchar *val = NULL;

	argv = g_ptr_array_new_with_free_func(g_free);
	g_ptr_array_add(argv, g_strdup("glab"));
	g_ptr_array_add(argv, g_strdup("release"));

	switch (verb) {
	case GCTL_VERB_LIST:
		g_ptr_array_add(argv, g_strdup("list"));

		val = get_param(params, "limit");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--per-page"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		g_ptr_array_add(argv, g_strdup("-F"));
		g_ptr_array_add(argv, g_strdup("json"));
		break;

	case GCTL_VERB_GET:
		g_ptr_array_add(argv, g_strdup("view"));

		val = get_param(params, "tag");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		g_ptr_array_add(argv, g_strdup("-F"));
		g_ptr_array_add(argv, g_strdup("json"));
		break;

	case GCTL_VERB_CREATE:
		g_ptr_array_add(argv, g_strdup("create"));

		val = get_param(params, "tag");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		val = get_param(params, "title");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--name"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		val = get_param(params, "notes");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--notes"));
			g_ptr_array_add(argv, g_strdup(val));
		}
		break;

	case GCTL_VERB_DELETE:
		g_ptr_array_add(argv, g_strdup("delete"));

		val = get_param(params, "tag");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		g_ptr_array_add(argv, g_strdup("--yes"));
		break;

	default:
		set_unsupported(error, GCTL_RESOURCE_KIND_RELEASE, verb);
		return NULL;
	}

	g_ptr_array_add(argv, NULL);
	return (gchar **)g_ptr_array_free(g_steal_pointer(&argv), FALSE);
}

/* ── build_argv: dispatch ─────────────────────────────────────────── */

static gchar **
gitlab_forge_build_argv(
	GctlForge          *self,
	GctlResourceKind    resource,
	GctlVerb            verb,
	GctlForgeContext   *context,
	GHashTable         *params,
	GError            **error
)
{
	switch (resource) {
	case GCTL_RESOURCE_KIND_PR:
		return build_mr_argv(verb, context, params, error);

	case GCTL_RESOURCE_KIND_ISSUE:
		return build_issue_argv(verb, context, params, error);

	case GCTL_RESOURCE_KIND_REPO:
		return build_repo_argv(verb, context, params, error);

	case GCTL_RESOURCE_KIND_RELEASE:
		return build_release_argv(verb, context, params, error);

	default:
		g_set_error(
			error,
			GCTL_ERROR,
			GCTL_ERROR_FORGE_UNSUPPORTED,
			"GitLab: unknown resource kind %d",
			(gint)resource
		);
		return NULL;
	}
}

/* ── JSON parsing helpers ─────────────────────────────────────────── */

static const gchar *
json_object_get_string_safe(JsonObject *obj, const gchar *member)
{
	JsonNode *node;

	if (!json_object_has_member(obj, member))
		return NULL;

	node = json_object_get_member(obj, member);
	if (JSON_NODE_HOLDS_VALUE(node) &&
	    json_node_get_value_type(node) == G_TYPE_STRING)
		return json_node_get_string(node);

	return NULL;
}

static gint64
json_object_get_int_safe(JsonObject *obj, const gchar *member)
{
	JsonNode *node;

	if (!json_object_has_member(obj, member))
		return 0;

	node = json_object_get_member(obj, member);
	if (JSON_NODE_HOLDS_VALUE(node))
		return json_node_get_int(node);

	return 0;
}

/**
 * extract_gitlab_author:
 * @obj: a #JsonObject from glab JSON output
 *
 * GitLab JSON encodes the author as {"username": "..."}.
 * This helper extracts the username string.
 *
 * Returns: (transfer none) (nullable): the author username
 */
static const gchar *
extract_gitlab_author(JsonObject *obj)
{
	JsonObject *author_obj;
	JsonNode *node;

	if (!json_object_has_member(obj, "author"))
		return NULL;

	node = json_object_get_member(obj, "author");
	if (!JSON_NODE_HOLDS_OBJECT(node))
		return NULL;

	author_obj = json_node_get_object(node);
	return json_object_get_string_safe(author_obj, "username");
}

/**
 * parse_gitlab_mr_or_issue:
 * @obj: a #JsonObject from glab JSON output
 * @kind: the resource kind
 *
 * Parses a single GitLab JSON object into a #GctlResource.
 * Uses "iid" for the number and "web_url" for the URL.
 *
 * Returns: (transfer full): a newly allocated #GctlResource
 */
static GctlResource *
parse_gitlab_mr_or_issue(JsonObject *obj, GctlResourceKind kind)
{
	GctlResource *res;
	const gchar *val;

	res = gctl_resource_new(kind);

	res->number = (gint)json_object_get_int_safe(obj, "iid");

	val = json_object_get_string_safe(obj, "title");
	if (val != NULL)
		gctl_resource_set_title(res, val);

	val = json_object_get_string_safe(obj, "state");
	if (val != NULL)
		gctl_resource_set_state(res, val);

	val = extract_gitlab_author(obj);
	if (val != NULL)
		gctl_resource_set_author(res, val);

	val = json_object_get_string_safe(obj, "web_url");
	if (val != NULL)
		gctl_resource_set_url(res, val);

	val = json_object_get_string_safe(obj, "created_at");
	if (val != NULL)
		gctl_resource_set_created_at(res, val);

	val = json_object_get_string_safe(obj, "updated_at");
	if (val != NULL)
		gctl_resource_set_updated_at(res, val);

	val = json_object_get_string_safe(obj, "description");
	if (val != NULL)
		gctl_resource_set_description(res, val);

	return res;
}

/**
 * parse_gitlab_repo:
 * @obj: a #JsonObject representing a GitLab project
 *
 * Returns: (transfer full): a newly allocated #GctlResource
 */
static GctlResource *
parse_gitlab_repo(JsonObject *obj)
{
	GctlResource *res;
	const gchar *val;

	res = gctl_resource_new(GCTL_RESOURCE_KIND_REPO);

	val = json_object_get_string_safe(obj, "name");
	if (val == NULL)
		val = json_object_get_string_safe(obj, "path");
	if (val != NULL)
		gctl_resource_set_title(res, val);

	val = json_object_get_string_safe(obj, "description");
	if (val != NULL)
		gctl_resource_set_description(res, val);

	val = json_object_get_string_safe(obj, "visibility");
	if (val != NULL)
		gctl_resource_set_state(res, val);

	val = json_object_get_string_safe(obj, "web_url");
	if (val != NULL)
		gctl_resource_set_url(res, val);

	return res;
}

/**
 * parse_gitlab_release:
 * @obj: a #JsonObject representing a GitLab release
 *
 * Returns: (transfer full): a newly allocated #GctlResource
 */
static GctlResource *
parse_gitlab_release(JsonObject *obj)
{
	GctlResource *res;
	const gchar *val;

	res = gctl_resource_new(GCTL_RESOURCE_KIND_RELEASE);

	val = json_object_get_string_safe(obj, "tag_name");
	if (val != NULL)
		gctl_resource_set_title(res, val);

	val = json_object_get_string_safe(obj, "name");
	if (val != NULL)
		gctl_resource_set_description(res, val);

	val = json_object_get_string_safe(obj, "released_at");
	if (val == NULL)
		val = json_object_get_string_safe(obj, "created_at");
	if (val != NULL)
		gctl_resource_set_created_at(res, val);

	val = json_object_get_string_safe(obj, "description");
	if (val != NULL)
		gctl_resource_set_extra(res, "body", val);

	return res;
}

/**
 * parse_single_object:
 * @obj: a #JsonObject
 * @resource: the resource kind
 *
 * Routes to the appropriate parser based on resource kind.
 *
 * Returns: (transfer full): a newly allocated #GctlResource
 */
static GctlResource *
parse_single_object(JsonObject *obj, GctlResourceKind resource)
{
	switch (resource) {
	case GCTL_RESOURCE_KIND_PR:
		return parse_gitlab_mr_or_issue(obj, GCTL_RESOURCE_KIND_PR);

	case GCTL_RESOURCE_KIND_ISSUE:
		return parse_gitlab_mr_or_issue(obj, GCTL_RESOURCE_KIND_ISSUE);

	case GCTL_RESOURCE_KIND_REPO:
		return parse_gitlab_repo(obj);

	case GCTL_RESOURCE_KIND_RELEASE:
		return parse_gitlab_release(obj);

	default:
		return gctl_resource_new(resource);
	}
}

/* ── parse_list_output ────────────────────────────────────────────── */

static GPtrArray *
gitlab_forge_parse_list_output(
	GctlForge          *self,
	GctlResourceKind    resource,
	const gchar        *raw_output,
	GError            **error
)
{
	g_autoptr(JsonParser) parser = NULL;
	JsonNode *root;
	JsonArray *array;
	GPtrArray *results;
	guint i;
	guint len;

	/*
	 * glab sometimes returns a text message instead of JSON when
	 * there are no results (e.g. "No open issues match your search").
	 * Detect this and return an empty array gracefully.
	 */
	if (raw_output == NULL || *raw_output == '\0') {
		return g_ptr_array_new_with_free_func(
			(GDestroyNotify)gctl_resource_free);
	}

	/* If the output doesn't look like JSON, treat as empty */
	{
		const gchar *p = raw_output;
		while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
			p++;
		if (*p != '[' && *p != '{') {
			return g_ptr_array_new_with_free_func(
				(GDestroyNotify)gctl_resource_free);
		}
	}

	parser = json_parser_new();
	if (!json_parser_load_from_data(parser, raw_output, -1, error))
		return NULL;

	root = json_parser_get_root(parser);
	if (!JSON_NODE_HOLDS_ARRAY(root)) {
		g_set_error_literal(
			error, GCTL_ERROR, GCTL_ERROR_PARSE_OUTPUT,
			"GitLab: expected JSON array in list output"
		);
		return NULL;
	}

	array = json_node_get_array(root);
	len = json_array_get_length(array);
	results = g_ptr_array_new_with_free_func(
		(GDestroyNotify)gctl_resource_free
	);

	for (i = 0; i < len; i++) {
		JsonObject *obj = json_array_get_object_element(array, i);
		GctlResource *res = parse_single_object(obj, resource);
		g_ptr_array_add(results, res);
	}

	return results;
}

/* ── parse_get_output ─────────────────────────────────────────────── */

static GctlResource *
gitlab_forge_parse_get_output(
	GctlForge          *self,
	GctlResourceKind    resource,
	const gchar        *raw_output,
	GError            **error
)
{
	g_autoptr(JsonParser) parser = NULL;
	JsonNode *root;
	JsonObject *obj;

	if (raw_output == NULL || *raw_output == '\0') {
		g_set_error_literal(
			error, GCTL_ERROR, GCTL_ERROR_PARSE_OUTPUT,
			"GitLab: empty output"
		);
		return NULL;
	}

	/* Non-JSON text -> set error */
	{
		const gchar *p = raw_output;
		while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
			p++;
		if (*p != '[' && *p != '{') {
			g_set_error(
				error, GCTL_ERROR, GCTL_ERROR_PARSE_OUTPUT,
				"GitLab: unexpected output: %.*s",
				80, raw_output
			);
			return NULL;
		}
	}

	parser = json_parser_new();
	if (!json_parser_load_from_data(parser, raw_output, -1, error))
		return NULL;

	root = json_parser_get_root(parser);
	if (!JSON_NODE_HOLDS_OBJECT(root)) {
		g_set_error_literal(
			error, GCTL_ERROR, GCTL_ERROR_PARSE_OUTPUT,
			"GitLab: expected JSON object in get output"
		);
		return NULL;
	}

	obj = json_node_get_object(root);
	return parse_single_object(obj, resource);
}

/* ── build_api_argv ───────────────────────────────────────────────── */

/**
 * gitlab_forge_build_api_argv:
 * @self: a #GctlForge
 * @method: the HTTP method
 * @endpoint: the API endpoint path
 * @body: (nullable): optional JSON request body
 * @error: (nullable): return location for errors
 *
 * Builds argv for `glab api <endpoint> -X <method>`.
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): argv
 */
static gchar **
gitlab_forge_build_api_argv(
	GctlForge    *self,
	const gchar  *method,
	const gchar  *endpoint,
	const gchar  *body,
	GError      **error
)
{
	g_autoptr(GPtrArray) argv = NULL;

	argv = g_ptr_array_new_with_free_func(g_free);
	g_ptr_array_add(argv, g_strdup("glab"));
	g_ptr_array_add(argv, g_strdup("api"));
	g_ptr_array_add(argv, g_strdup(endpoint));

	g_ptr_array_add(argv, g_strdup("-X"));
	g_ptr_array_add(argv, g_strdup(method));

	if (body != NULL && *body != '\0') {
		g_ptr_array_add(argv, g_strdup("--input"));
		g_ptr_array_add(argv, g_strdup("-"));
	}

	g_ptr_array_add(argv, NULL);
	return (gchar **)g_ptr_array_free(g_steal_pointer(&argv), FALSE);
}
