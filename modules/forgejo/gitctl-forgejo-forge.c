/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
 * gitctl-forgejo-forge.c - Forgejo forge backend module
 *
 * Implements the GctlForge interface using the `fj` CLI tool.
 * Forgejo is a fork of Gitea, so the API format is largely Gitea-
 * compatible.
 *
 * Key fj differences:
 *   - Uses "search" instead of "list" for PRs and issues
 *   - Comment subcommand: `fj pr comment` / `fj issue comment`
 *   - Primary public instance: codeberg.org
 *   - JSON flag varies by subcommand; some use --json
 *   - API passthrough: `fj api <endpoint>` (method is positional)
 */

#define GCTL_COMPILATION
#include <gitctl.h>
#include <gmodule.h>
#include <json-glib/json-glib.h>
#include <string.h>

/* ── Type declaration ─────────────────────────────────────────────── */

#define GCTL_TYPE_FORGEJO_FORGE (gctl_forgejo_forge_get_type())

G_DECLARE_FINAL_TYPE(
	GctlForgejoForge,
	gctl_forgejo_forge,
	GCTL,
	FORGEJO_FORGE,
	GctlModule
)

struct _GctlForgejoForge
{
	GctlModule parent_instance;
};

/* ── Forward declarations ─────────────────────────────────────────── */

static const gchar *forgejo_forge_get_name(GctlForge *self);
static const gchar *forgejo_forge_get_cli_tool(GctlForge *self);
static GctlForgeType forgejo_forge_get_forge_type(GctlForge *self);
static gboolean forgejo_forge_can_handle_url(GctlForge *self, const gchar *remote_url);
static gboolean forgejo_forge_is_available(GctlForge *self);

static gchar **forgejo_forge_build_argv(
	GctlForge *self, GctlResourceKind resource, GctlVerb verb,
	GctlForgeContext *context, GHashTable *params, GError **error);

static GPtrArray *forgejo_forge_parse_list_output(
	GctlForge *self, GctlResourceKind resource,
	const gchar *raw_output, GError **error);

static GctlResource *forgejo_forge_parse_get_output(
	GctlForge *self, GctlResourceKind resource,
	const gchar *raw_output, GError **error);

static gchar **forgejo_forge_build_api_argv(
	GctlForge *self, const gchar *method, const gchar *endpoint,
	const gchar *body, GctlForgeContext *context, GError **error);

/* ── GctlModule overrides ─────────────────────────────────────────── */

static const gchar *
forgejo_module_get_name(GctlModule *self)
{
	return "forgejo";
}

static const gchar *
forgejo_module_get_description(GctlModule *self)
{
	return "Forgejo forge backend using the fj CLI";
}

static gboolean
forgejo_module_activate(GctlModule *self)
{
	return TRUE;
}

static void
forgejo_module_deactivate(GctlModule *self)
{
	/* nothing to tear down */
}

/* ── Interface init ───────────────────────────────────────────────── */

static void
gctl_forgejo_forge_forge_init(GctlForgeInterface *iface)
{
	iface->get_name          = forgejo_forge_get_name;
	iface->get_cli_tool      = forgejo_forge_get_cli_tool;
	iface->get_forge_type    = forgejo_forge_get_forge_type;
	iface->can_handle_url    = forgejo_forge_can_handle_url;
	iface->is_available      = forgejo_forge_is_available;
	iface->build_argv        = forgejo_forge_build_argv;
	iface->parse_list_output = forgejo_forge_parse_list_output;
	iface->parse_get_output  = forgejo_forge_parse_get_output;
	iface->build_api_argv    = forgejo_forge_build_api_argv;
}

/* ── Type registration ────────────────────────────────────────────── */

G_DEFINE_FINAL_TYPE_WITH_CODE(
	GctlForgejoForge,
	gctl_forgejo_forge,
	GCTL_TYPE_MODULE,
	G_IMPLEMENT_INTERFACE(GCTL_TYPE_FORGE,
	                      gctl_forgejo_forge_forge_init)
)

static void
gctl_forgejo_forge_class_init(GctlForgejoForgeClass *klass)
{
	GctlModuleClass *module_class = GCTL_MODULE_CLASS(klass);

	module_class->get_name        = forgejo_module_get_name;
	module_class->get_description = forgejo_module_get_description;
	module_class->activate        = forgejo_module_activate;
	module_class->deactivate      = forgejo_module_deactivate;
}

static void
gctl_forgejo_forge_init(GctlForgejoForge *self)
{
	/* no instance state */
}

/* ── Module entry point ───────────────────────────────────────────── */

/**
 * gctl_module_register:
 *
 * Entry point called by the module manager when loading this shared
 * library.  Returns the #GType of #GctlForgejoForge.
 *
 * Returns: the #GType for #GctlForgejoForge
 */
G_MODULE_EXPORT GType
gctl_module_register(void)
{
	return GCTL_TYPE_FORGEJO_FORGE;
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
		"Forgejo does not support %s + %s",
		gctl_resource_kind_to_string(resource),
		gctl_verb_to_string(verb)
	);
}

/* ── Identity ─────────────────────────────────────────────────────── */

static const gchar *
forgejo_forge_get_name(GctlForge *self)
{
	return "Forgejo";
}

static const gchar *
forgejo_forge_get_cli_tool(GctlForge *self)
{
	return "fj";
}

static GctlForgeType
forgejo_forge_get_forge_type(GctlForge *self)
{
	return GCTL_FORGE_TYPE_FORGEJO;
}

/* ── Detection ────────────────────────────────────────────────────── */

/**
 * forgejo_forge_can_handle_url:
 * @self: a #GctlForge
 * @remote_url: the git remote URL to test
 *
 * Checks for "codeberg.org" in the URL, the primary public Forgejo
 * instance.  Self-hosted instances may also be detected by the
 * context resolver.
 *
 * Returns: %TRUE if the URL likely points to a Forgejo instance
 */
static gboolean
forgejo_forge_can_handle_url(GctlForge *self, const gchar *remote_url)
{
	if (remote_url == NULL)
		return FALSE;
	return (strstr(remote_url, "codeberg.org") != NULL);
}

static gboolean
forgejo_forge_is_available(GctlForge *self)
{
	g_autofree gchar *path = g_find_program_in_path("fj");
	return (path != NULL);
}

/* ── build_argv: PR operations ────────────────────────────────────── */

/**
 * build_pr_argv:
 * @verb: the action to perform
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Builds fj CLI argv for pull request operations.  Forgejo uses
 * `fj pr search` instead of `fj pr list`.
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
	g_ptr_array_add(argv, g_strdup("fj"));
	g_ptr_array_add(argv, g_strdup("pr"));

	switch (verb) {
	case GCTL_VERB_LIST:
	case GCTL_VERB_GET:
		/* fj has no JSON output — fall through to API */
		set_unsupported(error, GCTL_RESOURCE_KIND_PR, verb);
		return NULL;

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
		/*
		 * fj does not have a dedicated diff subcommand for PRs.
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
 * build_issue_argv:
 * @verb: the action to perform
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Builds fj CLI argv for issue operations.  Forgejo uses
 * `fj issue search` instead of `fj issue list`.
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
	g_ptr_array_add(argv, g_strdup("fj"));
	g_ptr_array_add(argv, g_strdup("issue"));

	switch (verb) {
	case GCTL_VERB_LIST:
	case GCTL_VERB_GET:
		/* fj has no JSON output — fall through to API */
		set_unsupported(error, GCTL_RESOURCE_KIND_ISSUE, verb);
		return NULL;

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
 * @verb: the action to perform
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * Builds fj CLI argv for repository operations.
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
	g_ptr_array_add(argv, g_strdup("fj"));
	g_ptr_array_add(argv, g_strdup("repo"));

	switch (verb) {
	case GCTL_VERB_LIST:
		/* fj has no 'repo list' or 'repo search' — fall through to API */
		set_unsupported(error, GCTL_RESOURCE_KIND_REPO, verb);
		return NULL;

	case GCTL_VERB_GET:
		/* fj repo view has no --json flag — fall through to API */
		set_unsupported(error, GCTL_RESOURCE_KIND_REPO, verb);
		return NULL;

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
 * Builds fj CLI argv for release operations.
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
	g_ptr_array_add(argv, g_strdup("fj"));
	g_ptr_array_add(argv, g_strdup("release"));

	switch (verb) {
	case GCTL_VERB_LIST:
	case GCTL_VERB_GET:
		/* fj has no JSON output — fall through to API */
		set_unsupported(error, GCTL_RESOURCE_KIND_RELEASE, verb);
		return NULL;

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
 * Forgejo has no CLI support for mirror operations.  All verbs
 * return %GCTL_ERROR_FORGE_UNSUPPORTED so that the common layer
 * falls back to the API via forgejo_forge_build_api_argv().
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
	 * The fj CLI does not expose mirror management commands.
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
 * The fj CLI has no CI support.  All verbs return unsupported
 * to trigger the API fallback.
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
 * build_label_argv:
 * @verb: the action to perform
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * The fj CLI has no label management support.  All verbs return
 * unsupported to trigger the API fallback.
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
	set_unsupported(error, GCTL_RESOURCE_KIND_LABEL, verb);
	return NULL;
}

/* ── build_argv: Notification operations ──────────────────────────── */

/**
 * build_notification_argv:
 * @verb: the action to perform
 * @context: the forge context
 * @params: operation parameters
 * @error: return location for errors
 *
 * The fj CLI has no notification support.  All verbs return
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
 * The fj CLI has no SSH key management support.  All verbs return
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
 * The fj CLI has no webhook management support.  All verbs return
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
forgejo_forge_build_argv(
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
			"Forgejo: unknown resource kind %d",
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
 * extract_forgejo_author:
 * @obj: a #JsonObject from fj JSON output
 *
 * Forgejo/Gitea API encodes the author as {"login": "..."} under
 * either "user" or "poster" depending on the resource type.
 *
 * Returns: (transfer none) (nullable): the author login
 */
static const gchar *
extract_forgejo_author(JsonObject *obj)
{
	JsonObject *user_obj;
	JsonNode *node;
	const gchar *val;

	/* Try "user" first (Gitea/Forgejo API standard) */
	if (json_object_has_member(obj, "user")) {
		node = json_object_get_member(obj, "user");
		if (JSON_NODE_HOLDS_OBJECT(node)) {
			user_obj = json_node_get_object(node);
			val = json_object_get_string_safe(user_obj, "login");
			if (val != NULL)
				return val;
			return json_object_get_string_safe(user_obj, "username");
		}
	}

	/* Fall back to "poster" (used in some Forgejo responses) */
	if (json_object_has_member(obj, "poster")) {
		node = json_object_get_member(obj, "poster");
		if (JSON_NODE_HOLDS_OBJECT(node)) {
			user_obj = json_node_get_object(node);
			val = json_object_get_string_safe(user_obj, "login");
			if (val != NULL)
				return val;
			return json_object_get_string_safe(user_obj, "username");
		}
	}

	return NULL;
}

/**
 * parse_forgejo_pr_or_issue:
 * @obj: a #JsonObject from fj JSON output
 * @kind: the resource kind
 *
 * Parses a Forgejo/Gitea API JSON object into a #GctlResource.
 * Uses "number" for the number and "html_url" for the URL.
 *
 * Returns: (transfer full): a newly allocated #GctlResource
 */
static GctlResource *
parse_forgejo_pr_or_issue(JsonObject *obj, GctlResourceKind kind)
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

	val = extract_forgejo_author(obj);
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
 * parse_forgejo_repo:
 * @obj: a #JsonObject representing a Forgejo repository
 *
 * Returns: (transfer full): a newly allocated #GctlResource
 */
static GctlResource *
parse_forgejo_repo(JsonObject *obj)
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

	/* Forgejo uses "private" boolean; map to visibility string */
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
 * parse_forgejo_release:
 * @obj: a #JsonObject representing a Forgejo release
 *
 * Returns: (transfer full): a newly allocated #GctlResource
 */
static GctlResource *
parse_forgejo_release(JsonObject *obj)
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
		return parse_forgejo_pr_or_issue(obj, GCTL_RESOURCE_KIND_PR);

	case GCTL_RESOURCE_KIND_ISSUE:
		return parse_forgejo_pr_or_issue(obj, GCTL_RESOURCE_KIND_ISSUE);

	case GCTL_RESOURCE_KIND_REPO:
		return parse_forgejo_repo(obj);

	case GCTL_RESOURCE_KIND_RELEASE:
		return parse_forgejo_release(obj);

	default:
		return gctl_resource_new(resource);
	}
}

/* ── parse_list_output ────────────────────────────────────────────── */

static GPtrArray *
forgejo_forge_parse_list_output(
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
	 * fj sometimes returns a text message instead of JSON when
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
			"Forgejo: expected JSON array in list output"
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
forgejo_forge_parse_get_output(
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
			"Forgejo: empty output"
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
				"Forgejo: unexpected output: %.*s",
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
			"Forgejo: expected JSON object in get output"
		);
		return NULL;
	}

	obj = json_node_get_object(root);
	return parse_single_object(obj, resource);
}

/* ── build_api_argv ───────────────────────────────────────────────── */

/**
 * forgejo_forge_build_api_argv:
 * @self: a #GctlForge
 * @method: the HTTP method
 * @endpoint: the API endpoint path
 * @body: (nullable): optional JSON request body
 * @context: (transfer none) (nullable): the forge context (provides host URL)
 * @error: (nullable): return location for errors
 *
 * Builds argv for a curl-based API call to the Forgejo REST API.
 * The Forgejo CLI (`fj`) does not have an `api` subcommand, so we
 * use `curl` directly.  The full URL is built from the host in
 * @context as `https://<host>/api/v1<endpoint>`.
 *
 * Authentication is provided via the FORGEJO_TOKEN or GITEA_TOKEN
 * environment variable when available.
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): argv
 */
static gchar **
forgejo_forge_build_api_argv(
	GctlForge          *self,
	const gchar        *method,
	const gchar        *endpoint,
	const gchar        *body,
	GctlForgeContext   *context,
	GError            **error
)
{
	g_autoptr(GPtrArray) argv = NULL;
	const gchar *host;
	g_autofree gchar *full_url = NULL;

	argv = g_ptr_array_new_with_free_func(g_free);
	g_ptr_array_add(argv, g_strdup("curl"));
	g_ptr_array_add(argv, g_strdup("-s"));        /* silent */
	g_ptr_array_add(argv, g_strdup("-S"));        /* show errors */

	/* HTTP method */
	g_ptr_array_add(argv, g_strdup("-X"));
	g_ptr_array_add(argv, g_strdup(method));

	/* Accept JSON response */
	g_ptr_array_add(argv, g_strdup("-H"));
	g_ptr_array_add(argv, g_strdup("Accept: application/json"));

	/* Auth token from environment */
	{
		const gchar *token;

		token = g_getenv("FORGEJO_TOKEN");
		if (token == NULL)
			token = g_getenv("GITEA_TOKEN");  /* Forgejo is Gitea-compatible */
		if (token != NULL) {
			g_ptr_array_add(argv, g_strdup("-H"));
			g_ptr_array_add(argv, g_strdup_printf("Authorization: token %s", token));
		}
	}

	/* JSON body if provided */
	if (body != NULL && *body != '\0') {
		g_ptr_array_add(argv, g_strdup("-H"));
		g_ptr_array_add(argv, g_strdup("Content-Type: application/json"));
		g_ptr_array_add(argv, g_strdup("-d"));
		g_ptr_array_add(argv, g_strdup(body));
	}

	/* Build full URL: https://<host>/api/v1<endpoint> */
	host = (context != NULL) ? gctl_forge_context_get_host(context) : NULL;
	if (host != NULL) {
		full_url = g_strdup_printf("https://%s/api/v1%s", host, endpoint);
	} else {
		/* Fallback: assume endpoint is already a full URL */
		full_url = g_strdup(endpoint);
	}
	g_ptr_array_add(argv, g_strdup(full_url));

	g_ptr_array_add(argv, NULL);
	return (gchar **)g_ptr_array_free(g_steal_pointer(&argv), FALSE);
}
