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

/**
 * cmd_repo_create:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl repo create <name>".  Parses --private,
 * --description, and --clone options.
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

	GOptionEntry entries[] = {
		{ "private", 'p', 0, G_OPTION_ARG_NONE, &is_private,
		  "Create as private repository", NULL },
		{ "description", 'd', 0, G_OPTION_ARG_STRING, &description,
		  "Repository description", "DESC" },
		{ "clone", 'c', 0, G_OPTION_ARG_NONE, &clone_after,
		  "Clone the repository after creating", NULL },
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

	g_free(description);

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
		default:
			g_printerr("error: verb '%s' not implemented for repo\n",
			           verb_name);
			return 1;
	}
}
