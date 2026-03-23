/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
 * gitctl-github-forge.c - GitHub forge backend module
 *
 * Implements the GctlForge interface using the `gh` CLI tool.
 * This is the reference implementation for forge backend modules.
 *
 * The module is loaded as a shared library (.so) by the module manager.
 * It exports the gctl_module_register() entry point which returns the
 * GType of the GctlGithubForge final type.
 */

#define GCTL_COMPILATION
#include <gitctl.h>
#include <gmodule.h>
#include <json-glib/json-glib.h>
#include <string.h>

/* ── Type declaration ─────────────────────────────────────────────── */

#define GCTL_TYPE_GITHUB_FORGE (gctl_github_forge_get_type())

G_DECLARE_FINAL_TYPE(
	GctlGithubForge,
	gctl_github_forge,
	GCTL,
	GITHUB_FORGE,
	GctlModule
)

struct _GctlGithubForge
{
	GctlModule parent_instance;
};

/* ── Forward declarations for interface methods ───────────────────── */

static const gchar *
github_forge_get_name(GctlForge *self);

static const gchar *
github_forge_get_cli_tool(GctlForge *self);

static GctlForgeType
github_forge_get_forge_type(GctlForge *self);

static gboolean
github_forge_can_handle_url(
	GctlForge    *self,
	const gchar  *remote_url
);

static gboolean
github_forge_is_available(GctlForge *self);

static gchar **
github_forge_build_argv(
	GctlForge          *self,
	GctlResourceKind    resource,
	GctlVerb            verb,
	GctlForgeContext   *context,
	GHashTable         *params,
	GError            **error
);

static GPtrArray *
github_forge_parse_list_output(
	GctlForge          *self,
	GctlResourceKind    resource,
	const gchar        *raw_output,
	GError            **error
);

static GctlResource *
github_forge_parse_get_output(
	GctlForge          *self,
	GctlResourceKind    resource,
	const gchar        *raw_output,
	GError            **error
);

static gchar **
github_forge_build_api_argv(
	GctlForge          *self,
	const gchar        *method,
	const gchar        *endpoint,
	const gchar        *body,
	GctlForgeContext   *context,
	GError            **error
);

/* ── GctlModule virtual method overrides ──────────────────────────── */

static const gchar *
github_module_get_name(GctlModule *self)
{
	return "github";
}

static const gchar *
github_module_get_description(GctlModule *self)
{
	return "GitHub forge backend using the gh CLI";
}

static gboolean
github_module_activate(GctlModule *self)
{
	return TRUE;
}

static void
github_module_deactivate(GctlModule *self)
{
	/* nothing to tear down */
}

/* ── Interface initialisation ─────────────────────────────────────── */

static void
gctl_github_forge_forge_init(GctlForgeInterface *iface)
{
	iface->get_name         = github_forge_get_name;
	iface->get_cli_tool     = github_forge_get_cli_tool;
	iface->get_forge_type   = github_forge_get_forge_type;
	iface->can_handle_url   = github_forge_can_handle_url;
	iface->is_available     = github_forge_is_available;
	iface->build_argv       = github_forge_build_argv;
	iface->parse_list_output = github_forge_parse_list_output;
	iface->parse_get_output = github_forge_parse_get_output;
	iface->build_api_argv   = github_forge_build_api_argv;
}

/* ── Type registration ────────────────────────────────────────────── */

G_DEFINE_FINAL_TYPE_WITH_CODE(
	GctlGithubForge,
	gctl_github_forge,
	GCTL_TYPE_MODULE,
	G_IMPLEMENT_INTERFACE(GCTL_TYPE_FORGE,
	                      gctl_github_forge_forge_init)
)

static void
gctl_github_forge_class_init(GctlGithubForgeClass *klass)
{
	GctlModuleClass *module_class = GCTL_MODULE_CLASS(klass);

	module_class->get_name        = github_module_get_name;
	module_class->get_description = github_module_get_description;
	module_class->activate        = github_module_activate;
	module_class->deactivate      = github_module_deactivate;
}

static void
gctl_github_forge_init(GctlGithubForge *self)
{
	/* no instance state needed */
}

/* ── Module entry point ───────────────────────────────────────────── */

/**
 * gctl_module_register:
 *
 * Entry point called by the module manager when loading this shared
 * library.  Returns the #GType of the #GctlGithubForge final type.
 *
 * Returns: the #GType for #GctlGithubForge
 */
G_MODULE_EXPORT GType
gctl_module_register(void)
{
	return GCTL_TYPE_GITHUB_FORGE;
}

/* ── Helper: look up a parameter from the hash table ──────────────── */

/**
 * get_param:
 * @params: (nullable): a hash table of string key-value pairs
 * @key: the key to look up
 *
 * Convenience wrapper that returns NULL when @params is NULL.
 *
 * Returns: (transfer none) (nullable): the value, or %NULL
 */
static const gchar *
get_param(
	GHashTable   *params,
	const gchar  *key
)
{
	if (params == NULL)
		return NULL;
	return (const gchar *)g_hash_table_lookup(params, key);
}

/* ── Helper: set unsupported error ────────────────────────────────── */

static void
set_unsupported(
	GError            **error,
	GctlResourceKind    resource,
	GctlVerb            verb
)
{
	g_set_error(
		error,
		GCTL_ERROR,
		GCTL_ERROR_FORGE_UNSUPPORTED,
		"GitHub does not support %s + %s",
		gctl_resource_kind_to_string(resource),
		gctl_verb_to_string(verb)
	);
}

/* ── Identity methods ─────────────────────────────────────────────── */

static const gchar *
github_forge_get_name(GctlForge *self)
{
	return "GitHub";
}

static const gchar *
github_forge_get_cli_tool(GctlForge *self)
{
	return "gh";
}

static GctlForgeType
github_forge_get_forge_type(GctlForge *self)
{
	return GCTL_FORGE_TYPE_GITHUB;
}

/* ── Detection ────────────────────────────────────────────────────── */

/**
 * github_forge_can_handle_url:
 * @self: a #GctlForge
 * @remote_url: the git remote URL to test
 *
 * Checks whether @remote_url points to a GitHub repository by
 * looking for "github.com" in the URL string.
 *
 * Returns: %TRUE if the URL contains "github.com"
 */
static gboolean
github_forge_can_handle_url(
	GctlForge    *self,
	const gchar  *remote_url
)
{
	if (remote_url == NULL)
		return FALSE;
	return (strstr(remote_url, "github.com") != NULL);
}

/**
 * github_forge_is_available:
 * @self: a #GctlForge
 *
 * Checks whether the `gh` CLI tool is available in $PATH.
 *
 * Returns: %TRUE if `gh` is found
 */
static gboolean
github_forge_is_available(GctlForge *self)
{
	g_autofree gchar *path = g_find_program_in_path("gh");
	return (path != NULL);
}

/* ── build_argv: PR operations ────────────────────────────────────── */

/**
 * build_pr_argv:
 * @verb: the action to perform on the PR
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Builds the gh CLI argument vector for pull request operations.
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): argv
 */
static gchar **
build_pr_argv(
	GctlVerb            verb,
	GctlForgeContext   *context,
	GHashTable         *params,
	GError            **error
)
{
	g_autoptr(GPtrArray) argv = NULL;
	const gchar *val = NULL;

	argv = g_ptr_array_new_with_free_func(g_free);
	g_ptr_array_add(argv, g_strdup("gh"));
	g_ptr_array_add(argv, g_strdup("pr"));

	switch (verb) {
	case GCTL_VERB_LIST:
		g_ptr_array_add(argv, g_strdup("list"));

		val = get_param(params, "state");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--state"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		val = get_param(params, "limit");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--limit"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		g_ptr_array_add(argv, g_strdup("--json"));
		g_ptr_array_add(argv, g_strdup(
			"number,title,state,author,url,createdAt,updatedAt"
		));
		break;

	case GCTL_VERB_GET:
		g_ptr_array_add(argv, g_strdup("view"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		g_ptr_array_add(argv, g_strdup("--json"));
		g_ptr_array_add(argv, g_strdup(
			"number,title,state,body,author,url,createdAt,updatedAt"
		));
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
			g_ptr_array_add(argv, g_strdup("--body"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		val = get_param(params, "base");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--base"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		val = get_param(params, "head");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--head"));
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

		/* merge strategy: --merge, --rebase, or --squash */
		val = get_param(params, "strategy");
		if (val != NULL) {
			if (g_strcmp0(val, "rebase") == 0)
				g_ptr_array_add(argv, g_strdup("--rebase"));
			else if (g_strcmp0(val, "squash") == 0)
				g_ptr_array_add(argv, g_strdup("--squash"));
			else
				g_ptr_array_add(argv, g_strdup("--merge"));
		}
		break;

	case GCTL_VERB_CHECKOUT:
		g_ptr_array_add(argv, g_strdup("checkout"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));
		break;

	case GCTL_VERB_COMMENT:
		g_ptr_array_add(argv, g_strdup("comment"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		val = get_param(params, "body");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--body"));
			g_ptr_array_add(argv, g_strdup(val));
		}
		break;

	case GCTL_VERB_REVIEW:
		g_ptr_array_add(argv, g_strdup("review"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		/* review action: approve, request-changes, or comment */
		val = get_param(params, "action");
		if (val != NULL) {
			if (g_strcmp0(val, "approve") == 0)
				g_ptr_array_add(argv, g_strdup("--approve"));
			else if (g_strcmp0(val, "request-changes") == 0)
				g_ptr_array_add(argv, g_strdup("--request-changes"));
			else
				g_ptr_array_add(argv, g_strdup("--comment"));
		}

		val = get_param(params, "body");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--body"));
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

	case GCTL_VERB_EDIT:
		g_ptr_array_add(argv, g_strdup("edit"));

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
			g_ptr_array_add(argv, g_strdup("--body"));
			g_ptr_array_add(argv, g_strdup(val));
		}
		break;

	case GCTL_VERB_DIFF:
		g_ptr_array_add(argv, g_strdup("diff"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));
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
 * @verb: the action to perform on the issue
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Builds the gh CLI argument vector for issue operations.
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
	g_ptr_array_add(argv, g_strdup("gh"));
	g_ptr_array_add(argv, g_strdup("issue"));

	switch (verb) {
	case GCTL_VERB_LIST:
		g_ptr_array_add(argv, g_strdup("list"));

		val = get_param(params, "state");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--state"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		val = get_param(params, "limit");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--limit"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		g_ptr_array_add(argv, g_strdup("--json"));
		g_ptr_array_add(argv, g_strdup(
			"number,title,state,author,url,createdAt,updatedAt"
		));
		break;

	case GCTL_VERB_GET:
		g_ptr_array_add(argv, g_strdup("view"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		g_ptr_array_add(argv, g_strdup("--json"));
		g_ptr_array_add(argv, g_strdup(
			"number,title,state,body,author,url,createdAt,updatedAt"
		));
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
			g_ptr_array_add(argv, g_strdup("--body"));
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
		g_ptr_array_add(argv, g_strdup("comment"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		val = get_param(params, "body");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--body"));
			g_ptr_array_add(argv, g_strdup(val));
		}
		break;

	case GCTL_VERB_EDIT:
		g_ptr_array_add(argv, g_strdup("edit"));

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
			g_ptr_array_add(argv, g_strdup("--body"));
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
 * @verb: the action to perform on the repo
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Builds the gh CLI argument vector for repository operations.
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
	g_ptr_array_add(argv, g_strdup("gh"));
	g_ptr_array_add(argv, g_strdup("repo"));

	switch (verb) {
	case GCTL_VERB_LIST:
		g_ptr_array_add(argv, g_strdup("list"));

		g_ptr_array_add(argv, g_strdup("--json"));
		g_ptr_array_add(argv, g_strdup(
			"name,description,visibility,url"
		));

		val = get_param(params, "limit");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--limit"));
			g_ptr_array_add(argv, g_strdup(val));
		}
		break;

	case GCTL_VERB_GET:
		g_ptr_array_add(argv, g_strdup("view"));

		/* owner/repo from params or context */
		val = get_param(params, "repo");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup(val));
		} else if (context != NULL && context->owner != NULL) {
			g_autofree gchar *slug = NULL;
			slug = gctl_forge_context_get_owner_repo(context);
			g_ptr_array_add(argv, g_strdup(slug));
		}

		g_ptr_array_add(argv, g_strdup("--json"));
		g_ptr_array_add(argv, g_strdup(
			"name,description,visibility,url"
		));
		break;

	case GCTL_VERB_CREATE:
		g_ptr_array_add(argv, g_strdup("create"));

		val = get_param(params, "name");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		val = get_param(params, "private");
		if (val != NULL && g_strcmp0(val, "true") == 0)
			g_ptr_array_add(argv, g_strdup("--private"));

		val = get_param(params, "description");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--description"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		val = get_param(params, "clone");
		if (val != NULL && g_strcmp0(val, "true") == 0)
			g_ptr_array_add(argv, g_strdup("--clone"));
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

	case GCTL_VERB_STAR:
		/* gh repo star is not a supported subcommand — use API */
		set_unsupported(error, GCTL_RESOURCE_KIND_REPO, verb);
		return NULL;

	case GCTL_VERB_UNSTAR:
		/* gh repo unstar is not a supported subcommand — use API */
		set_unsupported(error, GCTL_RESOURCE_KIND_REPO, verb);
		return NULL;

	case GCTL_VERB_MIGRATE:
		/* gh has no native migrate/import command — unsupported */
		set_unsupported(error, GCTL_RESOURCE_KIND_REPO, verb);
		return NULL;

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
 * @verb: the action to perform on the release
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Builds the gh CLI argument vector for release operations.
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
	g_ptr_array_add(argv, g_strdup("gh"));
	g_ptr_array_add(argv, g_strdup("release"));

	switch (verb) {
	case GCTL_VERB_LIST:
		g_ptr_array_add(argv, g_strdup("list"));

		g_ptr_array_add(argv, g_strdup("--json"));
		g_ptr_array_add(argv, g_strdup(
			"tagName,name,publishedAt,isDraft,isPrerelease"
		));

		val = get_param(params, "limit");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--limit"));
			g_ptr_array_add(argv, g_strdup(val));
		}
		break;

	case GCTL_VERB_GET:
		g_ptr_array_add(argv, g_strdup("view"));

		val = get_param(params, "tag");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		g_ptr_array_add(argv, g_strdup("--json"));
		g_ptr_array_add(argv, g_strdup(
			"tagName,name,body,publishedAt,isDraft"
		));
		break;

	case GCTL_VERB_CREATE:
		g_ptr_array_add(argv, g_strdup("create"));

		val = get_param(params, "tag");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		val = get_param(params, "title");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--title"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		val = get_param(params, "notes");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--notes"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		val = get_param(params, "draft");
		if (val != NULL && g_strcmp0(val, "true") == 0)
			g_ptr_array_add(argv, g_strdup("--draft"));

		val = get_param(params, "prerelease");
		if (val != NULL && g_strcmp0(val, "true") == 0)
			g_ptr_array_add(argv, g_strdup("--prerelease"));
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

/* ── build_argv: Mirror operations ────────────────────────────────── */

/**
 * build_mirror_argv:
 * @verb: the action to perform on the mirror
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Builds the gh CLI argument vector for mirror operations.
 * GitHub has no native push mirror support.  Only fork-based
 * sync is available via `gh repo sync`.
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): argv
 */
static gchar **
build_mirror_argv(
	GctlVerb            verb,
	GctlForgeContext   *context,
	GHashTable         *params,
	GError            **error
)
{
	g_autoptr(GPtrArray) argv = NULL;
	const gchar *val = NULL;

	switch (verb) {
	case GCTL_VERB_SYNC:
		/*
		 * `gh repo sync` synchronises a fork with its upstream.
		 * Optionally accepts --source <owner/repo> to specify
		 * the upstream repository to sync from.
		 */
		argv = g_ptr_array_new_with_free_func(g_free);
		g_ptr_array_add(argv, g_strdup("gh"));
		g_ptr_array_add(argv, g_strdup("repo"));
		g_ptr_array_add(argv, g_strdup("sync"));

		val = get_param(params, "source");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--source"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		g_ptr_array_add(argv, NULL);
		return (gchar **)g_ptr_array_free(
			g_steal_pointer(&argv), FALSE
		);

	default:
		g_set_error(
			error,
			GCTL_ERROR,
			GCTL_ERROR_FORGE_UNSUPPORTED,
			"GitHub does not support push mirrors natively. "
			"Consider using GitHub Actions for mirror-push workflows. "
			"For fork sync, use: gitctl mirror sync"
		);
		return NULL;
	}
}

/* ── build_argv: CI operations ─────────────────────────────────────── */

/**
 * build_ci_argv:
 * @verb: the action to perform on the CI run
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Builds the gh CLI argument vector for CI / Actions run operations.
 * Uses `gh run` subcommands.
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): argv
 */
static gchar **
build_ci_argv(
	GctlVerb            verb,
	GctlForgeContext   *context,
	GHashTable         *params,
	GError            **error
)
{
	g_autoptr(GPtrArray) argv = NULL;
	const gchar *val = NULL;

	argv = g_ptr_array_new_with_free_func(g_free);
	g_ptr_array_add(argv, g_strdup("gh"));
	g_ptr_array_add(argv, g_strdup("run"));

	switch (verb) {
	case GCTL_VERB_LIST:
		g_ptr_array_add(argv, g_strdup("list"));

		val = get_param(params, "limit");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--limit"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		g_ptr_array_add(argv, g_strdup("--json"));
		g_ptr_array_add(argv, g_strdup(
			"databaseId,displayTitle,status,conclusion,headBranch,createdAt,updatedAt"
		));
		break;

	case GCTL_VERB_GET:
		g_ptr_array_add(argv, g_strdup("view"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		g_ptr_array_add(argv, g_strdup("--json"));
		g_ptr_array_add(argv, g_strdup(
			"databaseId,displayTitle,status,conclusion,headBranch,createdAt,updatedAt"
		));
		break;

	case GCTL_VERB_LOG:
		g_ptr_array_add(argv, g_strdup("view"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		g_ptr_array_add(argv, g_strdup("--log"));
		break;

	case GCTL_VERB_BROWSE:
		g_ptr_array_add(argv, g_strdup("view"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		g_ptr_array_add(argv, g_strdup("--web"));
		break;

	default:
		set_unsupported(error, GCTL_RESOURCE_KIND_CI, verb);
		return NULL;
	}

	g_ptr_array_add(argv, NULL);
	return (gchar **)g_ptr_array_free(g_steal_pointer(&argv), FALSE);
}

/* ── build_argv: Commit operations ────────────────────────────────── */

/**
 * build_commit_argv:
 * @verb: the action to perform
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Commit operations use local git directly, so all verbs return
 * unsupported to signal the caller to use git commands instead.
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): argv
 */
static gchar **
build_commit_argv(
	GctlVerb            verb,
	GctlForgeContext   *context,
	GHashTable         *params,
	GError            **error
)
{
	set_unsupported(error, GCTL_RESOURCE_KIND_COMMIT, verb);
	return NULL;
}

/* ── build_argv: Label operations ─────────────────────────────────── */

/**
 * build_label_argv:
 * @verb: the action to perform on the label
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Builds the gh CLI argument vector for label operations.
 * Uses `gh label` subcommands.
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): argv
 */
static gchar **
build_label_argv(
	GctlVerb            verb,
	GctlForgeContext   *context,
	GHashTable         *params,
	GError            **error
)
{
	g_autoptr(GPtrArray) argv = NULL;
	const gchar *val = NULL;

	argv = g_ptr_array_new_with_free_func(g_free);
	g_ptr_array_add(argv, g_strdup("gh"));
	g_ptr_array_add(argv, g_strdup("label"));

	switch (verb) {
	case GCTL_VERB_LIST:
		g_ptr_array_add(argv, g_strdup("list"));

		g_ptr_array_add(argv, g_strdup("--json"));
		g_ptr_array_add(argv, g_strdup("name,color,description"));

		val = get_param(params, "limit");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--limit"));
			g_ptr_array_add(argv, g_strdup(val));
		}
		break;

	case GCTL_VERB_CREATE:
		g_ptr_array_add(argv, g_strdup("create"));

		val = get_param(params, "name");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		val = get_param(params, "color");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--color"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		val = get_param(params, "description");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--description"));
			g_ptr_array_add(argv, g_strdup(val));
		}
		break;

	case GCTL_VERB_DELETE:
		g_ptr_array_add(argv, g_strdup("delete"));

		val = get_param(params, "name");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		g_ptr_array_add(argv, g_strdup("--yes"));
		break;

	default:
		set_unsupported(error, GCTL_RESOURCE_KIND_LABEL, verb);
		return NULL;
	}

	g_ptr_array_add(argv, NULL);
	return (gchar **)g_ptr_array_free(g_steal_pointer(&argv), FALSE);
}

/* ── build_argv: Notification operations ──────────────────────────── */

/**
 * build_notification_argv:
 * @verb: the action to perform
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Notification operations are not supported by the gh CLI directly.
 * All verbs return unsupported to trigger the API fallback.
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): argv
 */
static gchar **
build_notification_argv(
	GctlVerb            verb,
	GctlForgeContext   *context,
	GHashTable         *params,
	GError            **error
)
{
	set_unsupported(error, GCTL_RESOURCE_KIND_NOTIFICATION, verb);
	return NULL;
}

/* ── build_argv: Key operations ───────────────────────────────────── */

/**
 * build_key_argv:
 * @verb: the action to perform on the SSH key
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Builds the gh CLI argument vector for SSH key operations.
 * Uses `gh ssh-key` subcommands.
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): argv
 */
static gchar **
build_key_argv(
	GctlVerb            verb,
	GctlForgeContext   *context,
	GHashTable         *params,
	GError            **error
)
{
	g_autoptr(GPtrArray) argv = NULL;
	const gchar *val = NULL;

	argv = g_ptr_array_new_with_free_func(g_free);
	g_ptr_array_add(argv, g_strdup("gh"));
	g_ptr_array_add(argv, g_strdup("ssh-key"));

	switch (verb) {
	case GCTL_VERB_LIST:
		g_ptr_array_add(argv, g_strdup("list"));
		break;

	case GCTL_VERB_CREATE:
		g_ptr_array_add(argv, g_strdup("add"));

		val = get_param(params, "key_file");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		val = get_param(params, "title");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--title"));
			g_ptr_array_add(argv, g_strdup(val));
		}
		break;

	case GCTL_VERB_DELETE:
		g_ptr_array_add(argv, g_strdup("delete"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		g_ptr_array_add(argv, g_strdup("--yes"));
		break;

	default:
		set_unsupported(error, GCTL_RESOURCE_KIND_KEY, verb);
		return NULL;
	}

	g_ptr_array_add(argv, NULL);
	return (gchar **)g_ptr_array_free(g_steal_pointer(&argv), FALSE);
}

/* ── build_argv: Webhook operations ───────────────────────────────── */

/**
 * build_webhook_argv:
 * @verb: the action to perform
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Webhook operations are not supported by the gh CLI directly.
 * All verbs return unsupported to trigger the API fallback.
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): argv
 */
static gchar **
build_webhook_argv(
	GctlVerb            verb,
	GctlForgeContext   *context,
	GHashTable         *params,
	GError            **error
)
{
	set_unsupported(error, GCTL_RESOURCE_KIND_WEBHOOK, verb);
	return NULL;
}

/* ── build_argv: dispatch ─────────────────────────────────────────── */

/**
 * github_forge_build_argv:
 * @self: a #GctlForge
 * @resource: the resource kind to operate on
 * @verb: the action to perform
 * @context: (transfer none): the forge context
 * @params: (element-type utf8 utf8) (nullable): parameters
 * @error: (nullable): return location for a #GError
 *
 * Dispatches to the appropriate per-resource builder and returns a
 * NULL-terminated argument vector for the `gh` CLI.
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): argv
 */
static gchar **
github_forge_build_argv(
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
		return build_pr_argv(verb, context, params, error);

	case GCTL_RESOURCE_KIND_ISSUE:
		return build_issue_argv(verb, context, params, error);

	case GCTL_RESOURCE_KIND_REPO:
		return build_repo_argv(verb, context, params, error);

	case GCTL_RESOURCE_KIND_RELEASE:
		return build_release_argv(verb, context, params, error);

	case GCTL_RESOURCE_KIND_MIRROR:
		return build_mirror_argv(verb, context, params, error);

	case GCTL_RESOURCE_KIND_CI:
		return build_ci_argv(verb, context, params, error);

	case GCTL_RESOURCE_KIND_COMMIT:
		return build_commit_argv(verb, context, params, error);

	case GCTL_RESOURCE_KIND_LABEL:
		return build_label_argv(verb, context, params, error);

	case GCTL_RESOURCE_KIND_NOTIFICATION:
		return build_notification_argv(verb, context, params, error);

	case GCTL_RESOURCE_KIND_KEY:
		return build_key_argv(verb, context, params, error);

	case GCTL_RESOURCE_KIND_WEBHOOK:
		return build_webhook_argv(verb, context, params, error);

	default:
		g_set_error(
			error,
			GCTL_ERROR,
			GCTL_ERROR_FORGE_UNSUPPORTED,
			"GitHub: unknown resource kind %d",
			(gint)resource
		);
		return NULL;
	}
}

/* ── JSON parsing helpers ─────────────────────────────────────────── */

/**
 * json_object_get_string_safe:
 * @obj: a #JsonObject
 * @member: the member name to look up
 *
 * Returns the string value of @member from @obj, or NULL if the
 * member is missing or not a string.
 *
 * Returns: (transfer none) (nullable): the string value
 */
static const gchar *
json_object_get_string_safe(
	JsonObject   *obj,
	const gchar  *member
)
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

/**
 * json_object_get_int_safe:
 * @obj: a #JsonObject
 * @member: the member name to look up
 *
 * Returns the integer value of @member from @obj, or 0 if the
 * member is missing or not numeric.
 *
 * Returns: the integer value
 */
static gint64
json_object_get_int_safe(
	JsonObject   *obj,
	const gchar  *member
)
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
 * extract_gh_author:
 * @obj: a #JsonObject from gh --json output
 *
 * GitHub's --json output encodes the author as an object with a
 * "login" field.  This helper extracts that login string.
 *
 * Returns: (transfer none) (nullable): the author login
 */
static const gchar *
extract_gh_author(JsonObject *obj)
{
	JsonObject *author_obj;
	JsonNode *node;

	if (!json_object_has_member(obj, "author"))
		return NULL;

	node = json_object_get_member(obj, "author");
	if (!JSON_NODE_HOLDS_OBJECT(node))
		return NULL;

	author_obj = json_node_get_object(node);
	return json_object_get_string_safe(author_obj, "login");
}

/**
 * parse_gh_pr_or_issue:
 * @obj: a #JsonObject representing a PR or issue
 * @kind: the resource kind
 *
 * Parses a single GitHub JSON object (from --json output) into a
 * #GctlResource for PR or issue resources.
 *
 * Returns: (transfer full): a newly allocated #GctlResource
 */
static GctlResource *
parse_gh_pr_or_issue(
	JsonObject        *obj,
	GctlResourceKind   kind
)
{
	GctlResource *res;
	const gchar *val;

	res = gctl_resource_new(kind);

	res->number = (gint)json_object_get_int_safe(obj, "number");

	val = json_object_get_string_safe(obj, "title");
	if (val != NULL)
		gctl_resource_set_title(res, val);

	val = json_object_get_string_safe(obj, "state");
	if (val != NULL)
		gctl_resource_set_state(res, val);

	val = extract_gh_author(obj);
	if (val != NULL)
		gctl_resource_set_author(res, val);

	val = json_object_get_string_safe(obj, "url");
	if (val != NULL)
		gctl_resource_set_url(res, val);

	val = json_object_get_string_safe(obj, "createdAt");
	if (val != NULL)
		gctl_resource_set_created_at(res, val);

	val = json_object_get_string_safe(obj, "updatedAt");
	if (val != NULL)
		gctl_resource_set_updated_at(res, val);

	/* body is only present in get (view) output */
	val = json_object_get_string_safe(obj, "body");
	if (val != NULL)
		gctl_resource_set_description(res, val);

	return res;
}

/**
 * parse_gh_repo:
 * @obj: a #JsonObject representing a repository
 *
 * Parses a single GitHub JSON repo object into a #GctlResource.
 *
 * Returns: (transfer full): a newly allocated #GctlResource
 */
static GctlResource *
parse_gh_repo(JsonObject *obj)
{
	GctlResource *res;
	const gchar *val;

	res = gctl_resource_new(GCTL_RESOURCE_KIND_REPO);

	val = json_object_get_string_safe(obj, "name");
	if (val != NULL)
		gctl_resource_set_title(res, val);

	val = json_object_get_string_safe(obj, "description");
	if (val != NULL)
		gctl_resource_set_description(res, val);

	val = json_object_get_string_safe(obj, "visibility");
	if (val != NULL)
		gctl_resource_set_state(res, val);

	val = json_object_get_string_safe(obj, "url");
	if (val != NULL)
		gctl_resource_set_url(res, val);

	return res;
}

/**
 * parse_gh_release:
 * @obj: a #JsonObject representing a release
 *
 * Parses a single GitHub JSON release object into a #GctlResource.
 *
 * Returns: (transfer full): a newly allocated #GctlResource
 */
static GctlResource *
parse_gh_release(JsonObject *obj)
{
	GctlResource *res;
	const gchar *val;

	res = gctl_resource_new(GCTL_RESOURCE_KIND_RELEASE);

	val = json_object_get_string_safe(obj, "tagName");
	if (val != NULL)
		gctl_resource_set_title(res, val);

	val = json_object_get_string_safe(obj, "name");
	if (val != NULL)
		gctl_resource_set_description(res, val);

	val = json_object_get_string_safe(obj, "publishedAt");
	if (val != NULL)
		gctl_resource_set_created_at(res, val);

	val = json_object_get_string_safe(obj, "body");
	if (val != NULL)
		gctl_resource_set_extra(res, "body", val);

	/* Store boolean fields as string extras */
	if (json_object_has_member(obj, "isDraft")) {
		gctl_resource_set_extra(
			res, "isDraft",
			json_object_get_boolean_member(obj, "isDraft")
				? "true" : "false"
		);
	}

	if (json_object_has_member(obj, "isPrerelease")) {
		gctl_resource_set_extra(
			res, "isPrerelease",
			json_object_get_boolean_member(obj, "isPrerelease")
				? "true" : "false"
		);
	}

	return res;
}

/**
 * parse_single_object:
 * @obj: a #JsonObject from the CLI output
 * @resource: the resource kind
 *
 * Routes a single JSON object to the appropriate parser based on
 * @resource kind.
 *
 * Returns: (transfer full): a newly allocated #GctlResource
 */
static GctlResource *
parse_single_object(
	JsonObject        *obj,
	GctlResourceKind   resource
)
{
	switch (resource) {
	case GCTL_RESOURCE_KIND_PR:
		return parse_gh_pr_or_issue(obj, GCTL_RESOURCE_KIND_PR);

	case GCTL_RESOURCE_KIND_ISSUE:
		return parse_gh_pr_or_issue(obj, GCTL_RESOURCE_KIND_ISSUE);

	case GCTL_RESOURCE_KIND_REPO:
		return parse_gh_repo(obj);

	case GCTL_RESOURCE_KIND_RELEASE:
		return parse_gh_release(obj);

	default:
		return gctl_resource_new(resource);
	}
}

/* ── parse_list_output ────────────────────────────────────────────── */

/**
 * github_forge_parse_list_output:
 * @self: a #GctlForge
 * @resource: the kind of resource that was listed
 * @raw_output: the raw JSON output from `gh ... --json ...`
 * @error: (nullable): return location for a #GError
 *
 * Parses JSON array output from `gh` into a #GPtrArray of
 * #GctlResource.  GitHub's `--json` flag always produces a JSON
 * array even for a single result.
 *
 * Returns: (transfer full) (element-type GctlResource) (nullable): array
 */
static GPtrArray *
github_forge_parse_list_output(
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
	 * gh sometimes returns a text message instead of JSON when
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
			error,
			GCTL_ERROR,
			GCTL_ERROR_PARSE_OUTPUT,
			"GitHub: expected JSON array in list output"
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

/**
 * github_forge_parse_get_output:
 * @self: a #GctlForge
 * @resource: the kind of resource that was fetched
 * @raw_output: the raw JSON output from `gh ... --json ...`
 * @error: (nullable): return location for a #GError
 *
 * Parses JSON object output from `gh` view into a single
 * #GctlResource.
 *
 * Returns: (transfer full) (nullable): the resource, or %NULL on error
 */
static GctlResource *
github_forge_parse_get_output(
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
			error,
			GCTL_ERROR,
			GCTL_ERROR_PARSE_OUTPUT,
			"GitHub: empty output"
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
				error,
				GCTL_ERROR,
				GCTL_ERROR_PARSE_OUTPUT,
				"GitHub: unexpected output: %.*s",
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
			error,
			GCTL_ERROR,
			GCTL_ERROR_PARSE_OUTPUT,
			"GitHub: expected JSON object in get output"
		);
		return NULL;
	}

	obj = json_node_get_object(root);
	return parse_single_object(obj, resource);
}

/* ── build_api_argv ───────────────────────────────────────────────── */

/**
 * github_forge_build_api_argv:
 * @self: a #GctlForge
 * @method: the HTTP method (e.g. "GET", "POST")
 * @endpoint: the API endpoint path
 * @body: (nullable): optional JSON request body
 * @context: (transfer none) (nullable): the forge context
 * @error: (nullable): return location for a #GError
 *
 * Builds argv for `gh api <endpoint> -X <method>`.  If @body is
 * provided, it is passed via `--input -` (the caller must pipe
 * the body to stdin).
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): argv
 */
static gchar **
github_forge_build_api_argv(
	GctlForge          *self,
	const gchar        *method,
	const gchar        *endpoint,
	const gchar        *body,
	GctlForgeContext   *context,
	GError            **error
)
{
	g_autoptr(GPtrArray) argv = NULL;

	argv = g_ptr_array_new_with_free_func(g_free);
	g_ptr_array_add(argv, g_strdup("gh"));
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
