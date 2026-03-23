/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
 * gitctl-gitea-forge.c - Gitea forge backend module
 *
 * Implements the GctlForge interface using the `tea` CLI tool.
 *
 * Key tea differences:
 *   - Uses plural nouns: `tea pulls`, `tea issues`, `tea repos`
 *   - JSON output: `-o json` flag
 *   - Body parameter: `--description` for create
 *   - Comment: `tea comment <number> --body <body>`
 *   - Clone: uses git clone directly (no tea clone)
 *   - JSON format is Gitea API: "number" for number, "html_url"
 *     for URL, "user":{"login":"..."} for author
 */

#define GCTL_COMPILATION
#include <gitctl.h>
#include <gmodule.h>
#include <json-glib/json-glib.h>
#include <string.h>

/* ── Type declaration ─────────────────────────────────────────────── */

#define GCTL_TYPE_GITEA_FORGE (gctl_gitea_forge_get_type())

G_DECLARE_FINAL_TYPE(
	GctlGiteaForge,
	gctl_gitea_forge,
	GCTL,
	GITEA_FORGE,
	GctlModule
)

struct _GctlGiteaForge
{
	GctlModule parent_instance;
};

/* ── Forward declarations ─────────────────────────────────────────── */

static const gchar *gitea_forge_get_name(GctlForge *self);
static const gchar *gitea_forge_get_cli_tool(GctlForge *self);
static GctlForgeType gitea_forge_get_forge_type(GctlForge *self);
static gboolean gitea_forge_can_handle_url(GctlForge *self, const gchar *remote_url);
static gboolean gitea_forge_is_available(GctlForge *self);

static gchar **gitea_forge_build_argv(
	GctlForge *self, GctlResourceKind resource, GctlVerb verb,
	GctlForgeContext *context, GHashTable *params, GError **error);

static GPtrArray *gitea_forge_parse_list_output(
	GctlForge *self, GctlResourceKind resource,
	const gchar *raw_output, GError **error);

static GctlResource *gitea_forge_parse_get_output(
	GctlForge *self, GctlResourceKind resource,
	const gchar *raw_output, GError **error);

static gchar **gitea_forge_build_api_argv(
	GctlForge *self, const gchar *method, const gchar *endpoint,
	const gchar *body, GctlForgeContext *context, GError **error);

/* ── GctlModule overrides ─────────────────────────────────────────── */

static const gchar *
gitea_module_get_name(GctlModule *self)
{
	return "gitea";
}

static const gchar *
gitea_module_get_description(GctlModule *self)
{
	return "Gitea forge backend using the tea CLI";
}

static gboolean
gitea_module_activate(GctlModule *self)
{
	return TRUE;
}

static void
gitea_module_deactivate(GctlModule *self)
{
	/* nothing to tear down */
}

/* ── Interface init ───────────────────────────────────────────────── */

static void
gctl_gitea_forge_forge_init(GctlForgeInterface *iface)
{
	iface->get_name          = gitea_forge_get_name;
	iface->get_cli_tool      = gitea_forge_get_cli_tool;
	iface->get_forge_type    = gitea_forge_get_forge_type;
	iface->can_handle_url    = gitea_forge_can_handle_url;
	iface->is_available      = gitea_forge_is_available;
	iface->build_argv        = gitea_forge_build_argv;
	iface->parse_list_output = gitea_forge_parse_list_output;
	iface->parse_get_output  = gitea_forge_parse_get_output;
	iface->build_api_argv    = gitea_forge_build_api_argv;
}

/* ── Type registration ────────────────────────────────────────────── */

G_DEFINE_FINAL_TYPE_WITH_CODE(
	GctlGiteaForge,
	gctl_gitea_forge,
	GCTL_TYPE_MODULE,
	G_IMPLEMENT_INTERFACE(GCTL_TYPE_FORGE,
	                      gctl_gitea_forge_forge_init)
)

static void
gctl_gitea_forge_class_init(GctlGiteaForgeClass *klass)
{
	GctlModuleClass *module_class = GCTL_MODULE_CLASS(klass);

	module_class->get_name        = gitea_module_get_name;
	module_class->get_description = gitea_module_get_description;
	module_class->activate        = gitea_module_activate;
	module_class->deactivate      = gitea_module_deactivate;
}

static void
gctl_gitea_forge_init(GctlGiteaForge *self)
{
	/* no instance state */
}

/* ── Module entry point ───────────────────────────────────────────── */

/**
 * gctl_module_register:
 *
 * Entry point called by the module manager when loading this shared
 * library.  Returns the #GType of #GctlGiteaForge.
 *
 * Returns: the #GType for #GctlGiteaForge
 */
G_MODULE_EXPORT GType
gctl_module_register(void)
{
	return GCTL_TYPE_GITEA_FORGE;
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
		"Gitea does not support %s + %s",
		gctl_resource_kind_to_string(resource),
		gctl_verb_to_string(verb)
	);
}

/* ── Identity ─────────────────────────────────────────────────────── */

static const gchar *
gitea_forge_get_name(GctlForge *self)
{
	return "Gitea";
}

static const gchar *
gitea_forge_get_cli_tool(GctlForge *self)
{
	return "tea";
}

static GctlForgeType
gitea_forge_get_forge_type(GctlForge *self)
{
	return GCTL_FORGE_TYPE_GITEA;
}

/* ── Detection ────────────────────────────────────────────────────── */

/**
 * gitea_forge_can_handle_url:
 * @self: a #GctlForge
 * @remote_url: the git remote URL to test
 *
 * Gitea has no single canonical public instance, so this always
 * returns %FALSE.  The context resolver will match Gitea instances
 * via configuration.  This module is only selected when explicitly
 * configured or when the context resolver identifies a Gitea host.
 *
 * Returns: %FALSE (Gitea relies on explicit configuration)
 */
static gboolean
gitea_forge_can_handle_url(GctlForge *self, const gchar *remote_url)
{
	/*
	 * No default public instance for Gitea.  Detection relies
	 * on user configuration in .gitctl.yaml or the context resolver
	 * matching a known host.
	 */
	return FALSE;
}

static gboolean
gitea_forge_is_available(GctlForge *self)
{
	g_autofree gchar *path = g_find_program_in_path("tea");
	return (path != NULL);
}

/* ── build_argv: PR (pulls) operations ────────────────────────────── */

/**
 * build_pulls_argv:
 * @verb: the action to perform
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Builds tea CLI argv for pull request operations.  Gitea's tea CLI
 * uses `tea pulls` (plural) for PR operations.
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): argv
 */
static gchar **
build_pulls_argv(
	GctlVerb            verb,
	GctlForgeContext   *context,
	GHashTable         *params,
	GError            **error
)
{
	g_autoptr(GPtrArray) argv = NULL;
	const gchar *val = NULL;

	argv = g_ptr_array_new_with_free_func(g_free);
	g_ptr_array_add(argv, g_strdup("tea"));

	switch (verb) {
	case GCTL_VERB_LIST:
		g_ptr_array_add(argv, g_strdup("pulls"));
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

		g_ptr_array_add(argv, g_strdup("-o"));
		g_ptr_array_add(argv, g_strdup("json"));
		break;

	case GCTL_VERB_GET:
		g_ptr_array_add(argv, g_strdup("pulls"));
		g_ptr_array_add(argv, g_strdup("view"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		g_ptr_array_add(argv, g_strdup("-o"));
		g_ptr_array_add(argv, g_strdup("json"));
		break;

	case GCTL_VERB_CREATE:
		g_ptr_array_add(argv, g_strdup("pulls"));
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
			g_ptr_array_add(argv, g_strdup("--base"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		val = get_param(params, "head");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--head"));
			g_ptr_array_add(argv, g_strdup(val));
		}
		break;

	case GCTL_VERB_CLOSE:
		g_ptr_array_add(argv, g_strdup("pulls"));
		g_ptr_array_add(argv, g_strdup("close"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));
		break;

	case GCTL_VERB_REOPEN:
		g_ptr_array_add(argv, g_strdup("pulls"));
		g_ptr_array_add(argv, g_strdup("reopen"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));
		break;

	case GCTL_VERB_MERGE:
		g_ptr_array_add(argv, g_strdup("pulls"));
		g_ptr_array_add(argv, g_strdup("merge"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		val = get_param(params, "strategy");
		if (val != NULL) {
			if (g_strcmp0(val, "rebase") == 0)
				g_ptr_array_add(argv, g_strdup("--style=rebase"));
			else if (g_strcmp0(val, "squash") == 0)
				g_ptr_array_add(argv, g_strdup("--style=squash"));
			else
				g_ptr_array_add(argv, g_strdup("--style=merge"));
		}
		break;

	case GCTL_VERB_CHECKOUT:
		g_ptr_array_add(argv, g_strdup("pulls"));
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

	case GCTL_VERB_BROWSE:
		g_ptr_array_add(argv, g_strdup("pulls"));
		g_ptr_array_add(argv, g_strdup("view"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		g_ptr_array_add(argv, g_strdup("--web"));
		break;

	case GCTL_VERB_EDIT:
		g_ptr_array_add(argv, g_strdup("pulls"));
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
			g_ptr_array_add(argv, g_strdup("--description"));
			g_ptr_array_add(argv, g_strdup(val));
		}
		break;

	case GCTL_VERB_DIFF:
		/*
		 * tea does not have a dedicated diff subcommand for pulls.
		 * Return unsupported so the API fallback is used.
		 */
		set_unsupported(error, GCTL_RESOURCE_KIND_PR, verb);
		return NULL;

	default:
		set_unsupported(error, GCTL_RESOURCE_KIND_PR, verb);
		return NULL;
	}

	g_ptr_array_add(argv, NULL);
	return (gchar **)g_ptr_array_free(g_steal_pointer(&argv), FALSE);
}

/* ── build_argv: Issue operations ─────────────────────────────────── */

/**
 * build_issues_argv:
 * @verb: the action to perform
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Builds tea CLI argv for issue operations.  Uses `tea issues`
 * (plural).
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): argv
 */
static gchar **
build_issues_argv(
	GctlVerb            verb,
	GctlForgeContext   *context,
	GHashTable         *params,
	GError            **error
)
{
	g_autoptr(GPtrArray) argv = NULL;
	const gchar *val = NULL;

	argv = g_ptr_array_new_with_free_func(g_free);
	g_ptr_array_add(argv, g_strdup("tea"));

	switch (verb) {
	case GCTL_VERB_LIST:
		g_ptr_array_add(argv, g_strdup("issues"));
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

		g_ptr_array_add(argv, g_strdup("-o"));
		g_ptr_array_add(argv, g_strdup("json"));
		break;

	case GCTL_VERB_GET:
		g_ptr_array_add(argv, g_strdup("issues"));
		g_ptr_array_add(argv, g_strdup("view"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		g_ptr_array_add(argv, g_strdup("-o"));
		g_ptr_array_add(argv, g_strdup("json"));
		break;

	case GCTL_VERB_CREATE:
		g_ptr_array_add(argv, g_strdup("issues"));
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
		g_ptr_array_add(argv, g_strdup("issues"));
		g_ptr_array_add(argv, g_strdup("close"));

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));
		break;

	case GCTL_VERB_REOPEN:
		g_ptr_array_add(argv, g_strdup("issues"));
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
		g_ptr_array_add(argv, g_strdup("issues"));
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
			g_ptr_array_add(argv, g_strdup("--description"));
			g_ptr_array_add(argv, g_strdup(val));
		}
		break;

	case GCTL_VERB_BROWSE:
		g_ptr_array_add(argv, g_strdup("issues"));
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
 * build_repos_argv:
 * @verb: the action to perform
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Builds tea CLI argv for repository operations.  Uses `tea repos`
 * (plural).  Clone uses `git clone` directly since tea does not
 * have a clone subcommand.
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): argv
 */
static gchar **
build_repos_argv(
	GctlVerb            verb,
	GctlForgeContext   *context,
	GHashTable         *params,
	GError            **error
)
{
	g_autoptr(GPtrArray) argv = NULL;
	const gchar *val = NULL;

	argv = g_ptr_array_new_with_free_func(g_free);

	switch (verb) {
	case GCTL_VERB_LIST:
		g_ptr_array_add(argv, g_strdup("tea"));
		g_ptr_array_add(argv, g_strdup("repos"));
		g_ptr_array_add(argv, g_strdup("list"));

		val = get_param(params, "limit");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--limit"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		g_ptr_array_add(argv, g_strdup("-o"));
		g_ptr_array_add(argv, g_strdup("json"));
		break;

	case GCTL_VERB_GET:
		g_ptr_array_add(argv, g_strdup("tea"));
		g_ptr_array_add(argv, g_strdup("repos"));
		g_ptr_array_add(argv, g_strdup("view"));

		val = get_param(params, "repo");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup(val));
		} else if (context != NULL && context->owner != NULL) {
			g_autofree gchar *slug = NULL;
			slug = gctl_forge_context_get_owner_repo(context);
			g_ptr_array_add(argv, g_strdup(slug));
		}

		g_ptr_array_add(argv, g_strdup("-o"));
		g_ptr_array_add(argv, g_strdup("json"));
		break;

	case GCTL_VERB_CREATE:
		g_ptr_array_add(argv, g_strdup("tea"));
		g_ptr_array_add(argv, g_strdup("repos"));
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
		g_ptr_array_add(argv, g_strdup("tea"));
		g_ptr_array_add(argv, g_strdup("repos"));
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
		/*
		 * tea does not have a clone subcommand, so we fall back
		 * to plain `git clone` using the remote URL.
		 */
		g_ptr_array_add(argv, g_strdup("git"));
		g_ptr_array_add(argv, g_strdup("clone"));

		val = get_param(params, "repo");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup(val));
		} else if (context != NULL && context->remote_url != NULL) {
			g_ptr_array_add(argv, g_strdup(context->remote_url));
		}
		break;

	case GCTL_VERB_DELETE:
		g_ptr_array_add(argv, g_strdup("tea"));
		g_ptr_array_add(argv, g_strdup("repos"));
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
		g_ptr_array_add(argv, g_strdup("tea"));
		g_ptr_array_add(argv, g_strdup("repos"));
		g_ptr_array_add(argv, g_strdup("view"));

		val = get_param(params, "repo");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup(val));
		} else if (context != NULL && context->owner != NULL) {
			g_autofree gchar *slug = NULL;
			slug = gctl_forge_context_get_owner_repo(context);
			g_ptr_array_add(argv, g_strdup(slug));
		}

		g_ptr_array_add(argv, g_strdup("--web"));
		break;

	case GCTL_VERB_MIGRATE:
		/*
		 * tea uses "repos migrate" for migration.  Reset the argv
		 * array since tea uses "repos" not "repo" as the noun.
		 */
		g_ptr_array_set_size(argv, 0);
		g_ptr_array_add(argv, g_strdup("tea"));
		g_ptr_array_add(argv, g_strdup("repos"));
		g_ptr_array_add(argv, g_strdup("migrate"));

		val = get_param(params, "source_url");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--clone-url"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		val = get_param(params, "name");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--name"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		/* tea uses individual flags instead of --include */
		val = get_param(params, "include");
		if (val != NULL) {
			if (strstr(val, "all") != NULL || strstr(val, "wiki") != NULL)
				g_ptr_array_add(argv, g_strdup("--wiki"));
			if (strstr(val, "all") != NULL || strstr(val, "issues") != NULL)
				g_ptr_array_add(argv, g_strdup("--issues"));
			if (strstr(val, "all") != NULL || strstr(val, "prs") != NULL)
				g_ptr_array_add(argv, g_strdup("--pull-requests"));
			if (strstr(val, "all") != NULL || strstr(val, "releases") != NULL)
				g_ptr_array_add(argv, g_strdup("--releases"));
			if (strstr(val, "all") != NULL || strstr(val, "milestones") != NULL)
				g_ptr_array_add(argv, g_strdup("--milestones"));
			if (strstr(val, "all") != NULL || strstr(val, "labels") != NULL)
				g_ptr_array_add(argv, g_strdup("--labels"));
			if (strstr(val, "all") != NULL || strstr(val, "lfs") != NULL)
				g_ptr_array_add(argv, g_strdup("--lfs"));
		}

		val = get_param(params, "mirror");
		if (val != NULL && g_strcmp0(val, "true") == 0)
			g_ptr_array_add(argv, g_strdup("--mirror"));

		val = get_param(params, "private");
		if (val != NULL && g_strcmp0(val, "true") == 0)
			g_ptr_array_add(argv, g_strdup("--private"));

		val = get_param(params, "service");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--service"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		val = get_param(params, "source_token");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--auth-token"));
			g_ptr_array_add(argv, g_strdup(val));
		}
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
 * build_releases_argv:
 * @verb: the action to perform
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Builds tea CLI argv for release operations.  Uses `tea releases`
 * (plural).
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): argv
 */
static gchar **
build_releases_argv(
	GctlVerb            verb,
	GctlForgeContext   *context,
	GHashTable         *params,
	GError            **error
)
{
	g_autoptr(GPtrArray) argv = NULL;
	const gchar *val = NULL;

	argv = g_ptr_array_new_with_free_func(g_free);
	g_ptr_array_add(argv, g_strdup("tea"));
	g_ptr_array_add(argv, g_strdup("releases"));

	switch (verb) {
	case GCTL_VERB_LIST:
		g_ptr_array_add(argv, g_strdup("list"));

		val = get_param(params, "limit");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--limit"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		g_ptr_array_add(argv, g_strdup("-o"));
		g_ptr_array_add(argv, g_strdup("json"));
		break;

	case GCTL_VERB_GET:
		g_ptr_array_add(argv, g_strdup("view"));

		val = get_param(params, "tag");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));

		g_ptr_array_add(argv, g_strdup("-o"));
		g_ptr_array_add(argv, g_strdup("json"));
		break;

	case GCTL_VERB_CREATE:
		g_ptr_array_add(argv, g_strdup("create"));

		val = get_param(params, "tag");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--tag"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		val = get_param(params, "title");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--title"));
			g_ptr_array_add(argv, g_strdup(val));
		}

		val = get_param(params, "notes");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--note"));
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
 * Gitea has no CLI support for mirror operations.  All verbs
 * return %GCTL_ERROR_FORGE_UNSUPPORTED so that the common layer
 * falls back to the API via gitea_forge_build_api_argv(), which
 * constructs `tea api` calls against the Gitea REST endpoint.
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
	/*
	 * The tea CLI does not expose mirror management commands.
	 * Return UNSUPPORTED for every verb so the common dispatch
	 * layer triggers the API fallback path, which constructs
	 * the appropriate REST call via build_api_argv.
	 */
	set_unsupported(error, GCTL_RESOURCE_KIND_MIRROR, verb);
	return NULL;
}

/* ── build_argv: CI operations ─────────────────────────────────────── */

/**
 * build_ci_argv:
 * @verb: the action to perform
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * The tea CLI has no CI / Actions support.  All verbs return
 * unsupported to trigger the API fallback.
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
	set_unsupported(error, GCTL_RESOURCE_KIND_CI, verb);
	return NULL;
}

/* ── build_argv: Commit operations ────────────────────────────────── */

/**
 * build_commit_argv:
 * @verb: the action to perform
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Commit operations use local git directly.  All verbs return
 * unsupported.
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
 * build_labels_argv:
 * @verb: the action to perform on the label
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Builds tea CLI argv for label operations.  Uses `tea labels`
 * (plural) subcommands.
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): argv
 */
static gchar **
build_labels_argv(
	GctlVerb            verb,
	GctlForgeContext   *context,
	GHashTable         *params,
	GError            **error
)
{
	g_autoptr(GPtrArray) argv = NULL;
	const gchar *val = NULL;

	argv = g_ptr_array_new_with_free_func(g_free);
	g_ptr_array_add(argv, g_strdup("tea"));
	g_ptr_array_add(argv, g_strdup("labels"));

	switch (verb) {
	case GCTL_VERB_LIST:
		g_ptr_array_add(argv, g_strdup("list"));

		g_ptr_array_add(argv, g_strdup("-o"));
		g_ptr_array_add(argv, g_strdup("json"));
		break;

	case GCTL_VERB_CREATE:
		g_ptr_array_add(argv, g_strdup("create"));

		val = get_param(params, "name");
		if (val != NULL) {
			g_ptr_array_add(argv, g_strdup("--name"));
			g_ptr_array_add(argv, g_strdup(val));
		}

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

		val = get_param(params, "number");
		if (val != NULL)
			g_ptr_array_add(argv, g_strdup(val));
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
 * The tea CLI has no notification support.  All verbs return
 * unsupported to trigger the API fallback.
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
 * @verb: the action to perform
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * The tea CLI has no SSH key management support.  All verbs return
 * unsupported to trigger the API fallback.
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
	set_unsupported(error, GCTL_RESOURCE_KIND_KEY, verb);
	return NULL;
}

/* ── build_argv: Webhook operations ───────────────────────────────── */

/**
 * build_webhook_argv:
 * @verb: the action to perform
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * The tea CLI has no webhook management support.  All verbs return
 * unsupported to trigger the API fallback.
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

static gchar **
gitea_forge_build_argv(
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
		return build_pulls_argv(verb, context, params, error);

	case GCTL_RESOURCE_KIND_ISSUE:
		return build_issues_argv(verb, context, params, error);

	case GCTL_RESOURCE_KIND_REPO:
		return build_repos_argv(verb, context, params, error);

	case GCTL_RESOURCE_KIND_RELEASE:
		return build_releases_argv(verb, context, params, error);

	case GCTL_RESOURCE_KIND_MIRROR:
		return build_mirror_argv(verb, context, params, error);

	case GCTL_RESOURCE_KIND_CI:
		return build_ci_argv(verb, context, params, error);

	case GCTL_RESOURCE_KIND_COMMIT:
		return build_commit_argv(verb, context, params, error);

	case GCTL_RESOURCE_KIND_LABEL:
		return build_labels_argv(verb, context, params, error);

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
			"Gitea: unknown resource kind %d",
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
 * extract_gitea_author:
 * @obj: a #JsonObject from tea JSON output
 *
 * Gitea API encodes the author as {"login": "..."} under the "user"
 * key for PRs and issues.
 *
 * Returns: (transfer none) (nullable): the author login
 */
static const gchar *
extract_gitea_author(JsonObject *obj)
{
	JsonObject *user_obj;
	JsonNode *node;

	if (!json_object_has_member(obj, "user"))
		return NULL;

	node = json_object_get_member(obj, "user");
	if (!JSON_NODE_HOLDS_OBJECT(node))
		return NULL;

	user_obj = json_node_get_object(node);
	return json_object_get_string_safe(user_obj, "login");
}

/**
 * parse_gitea_pr_or_issue:
 * @obj: a #JsonObject from tea JSON output
 * @kind: the resource kind
 *
 * Parses a Gitea API JSON object.  Uses "number" for the issue/PR
 * number and "html_url" for the URL.
 *
 * Returns: (transfer full): a newly allocated #GctlResource
 */
static GctlResource *
parse_gitea_pr_or_issue(JsonObject *obj, GctlResourceKind kind)
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

	val = extract_gitea_author(obj);
	if (val != NULL)
		gctl_resource_set_author(res, val);

	val = json_object_get_string_safe(obj, "html_url");
	if (val != NULL)
		gctl_resource_set_url(res, val);

	val = json_object_get_string_safe(obj, "created_at");
	if (val != NULL)
		gctl_resource_set_created_at(res, val);

	val = json_object_get_string_safe(obj, "updated_at");
	if (val != NULL)
		gctl_resource_set_updated_at(res, val);

	val = json_object_get_string_safe(obj, "body");
	if (val != NULL)
		gctl_resource_set_description(res, val);

	return res;
}

/**
 * parse_gitea_repo:
 * @obj: a #JsonObject representing a Gitea repository
 *
 * Returns: (transfer full): a newly allocated #GctlResource
 */
static GctlResource *
parse_gitea_repo(JsonObject *obj)
{
	GctlResource *res;
	const gchar *val;

	res = gctl_resource_new(GCTL_RESOURCE_KIND_REPO);

	val = json_object_get_string_safe(obj, "name");
	if (val == NULL)
		val = json_object_get_string_safe(obj, "full_name");
	if (val != NULL)
		gctl_resource_set_title(res, val);

	val = json_object_get_string_safe(obj, "description");
	if (val != NULL)
		gctl_resource_set_description(res, val);

	/* Gitea uses "private" boolean; map to visibility string */
	if (json_object_has_member(obj, "private")) {
		gboolean is_private = json_object_get_boolean_member(
			obj, "private"
		);
		gctl_resource_set_state(res, is_private ? "private" : "public");
	}

	val = json_object_get_string_safe(obj, "html_url");
	if (val != NULL)
		gctl_resource_set_url(res, val);

	return res;
}

/**
 * parse_gitea_release:
 * @obj: a #JsonObject representing a Gitea release
 *
 * Returns: (transfer full): a newly allocated #GctlResource
 */
static GctlResource *
parse_gitea_release(JsonObject *obj)
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

	val = json_object_get_string_safe(obj, "published_at");
	if (val == NULL)
		val = json_object_get_string_safe(obj, "created_at");
	if (val != NULL)
		gctl_resource_set_created_at(res, val);

	val = json_object_get_string_safe(obj, "body");
	if (val != NULL)
		gctl_resource_set_extra(res, "body", val);

	if (json_object_has_member(obj, "draft")) {
		gctl_resource_set_extra(
			res, "isDraft",
			json_object_get_boolean_member(obj, "draft")
				? "true" : "false"
		);
	}

	if (json_object_has_member(obj, "prerelease")) {
		gctl_resource_set_extra(
			res, "isPrerelease",
			json_object_get_boolean_member(obj, "prerelease")
				? "true" : "false"
		);
	}

	return res;
}

static GctlResource *
parse_single_object(JsonObject *obj, GctlResourceKind resource)
{
	switch (resource) {
	case GCTL_RESOURCE_KIND_PR:
		return parse_gitea_pr_or_issue(obj, GCTL_RESOURCE_KIND_PR);

	case GCTL_RESOURCE_KIND_ISSUE:
		return parse_gitea_pr_or_issue(obj, GCTL_RESOURCE_KIND_ISSUE);

	case GCTL_RESOURCE_KIND_REPO:
		return parse_gitea_repo(obj);

	case GCTL_RESOURCE_KIND_RELEASE:
		return parse_gitea_release(obj);

	default:
		return gctl_resource_new(resource);
	}
}

/* ── parse_list_output ────────────────────────────────────────────── */

static GPtrArray *
gitea_forge_parse_list_output(
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
	 * tea sometimes returns a text message instead of JSON when
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
			"Gitea: expected JSON array in list output"
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
gitea_forge_parse_get_output(
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
			"Gitea: empty output"
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
				"Gitea: unexpected output: %.*s",
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
			"Gitea: expected JSON object in get output"
		);
		return NULL;
	}

	obj = json_node_get_object(root);
	return parse_single_object(obj, resource);
}

/* ── build_api_argv ───────────────────────────────────────────────── */

/**
 * gitea_forge_build_api_argv:
 * @self: a #GctlForge
 * @method: the HTTP method
 * @endpoint: the API endpoint path
 * @body: (nullable): optional JSON request body
 * @context: (transfer none) (nullable): the forge context
 * @error: (nullable): return location for errors
 *
 * Builds argv for `tea api <endpoint> -X <method>`.
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): argv
 */
static gchar **
gitea_forge_build_api_argv(
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
	g_ptr_array_add(argv, g_strdup("tea"));
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
