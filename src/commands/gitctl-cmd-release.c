/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
 * gitctl-cmd-release.c - Release command handler
 *
 * Implements the "release" command with verb dispatch for: list, get,
 * create, and delete.
 *
 * All verbs parse their options and delegate to gctl_cmd_execute_verb().
 */

#define GCTL_COMPILATION

#include "commands/gitctl-cmd-common-private.h"
#include "commands/gitctl-cmd-release.h"

/* ── Verb dispatch table ─────────────────────────────────────────────── */

static const GctlVerbEntry release_verbs[] = {
	{ "list",   "List releases",             GCTL_VERB_LIST   },
	{ "get",    "View a single release",     GCTL_VERB_GET    },
	{ "create", "Create a new release",      GCTL_VERB_CREATE },
	{ "delete", "Delete a release",          GCTL_VERB_DELETE },
};

static const gsize N_RELEASE_VERBS = G_N_ELEMENTS(release_verbs);

/* ── Usage printer ───────────────────────────────────────────────────── */

/**
 * print_usage:
 *
 * Prints the usage summary for the "release" command, listing all
 * available verbs and their descriptions.
 */
static void
print_usage(void)
{
	gctl_cmd_print_verb_table("release", release_verbs, N_RELEASE_VERBS);
}

/* ── release list ────────────────────────────────────────────────────── */

/**
 * cmd_release_list:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl release list".  Parses --limit option.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_release_list(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	gint limit = 30;

	GOptionEntry entries[] = {
		{ "limit", 'l', 0, G_OPTION_ARG_INT, &limit,
		  "Maximum number of results (default: 30)", "N" },
		{ NULL }
	};

	opt_context = g_option_context_new("- list releases");
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

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_RELEASE,
	                             GCTL_VERB_LIST, NULL, params);
}

/* ── release get ─────────────────────────────────────────────────────── */

/**
 * cmd_release_get:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl release get <tag>".
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_release_get(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	const gchar *tag;

	GOptionEntry entries[] = {
		{ NULL }
	};

	opt_context = g_option_context_new("<tag> - view a release");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	if (argc < 2)
	{
		g_printerr("error: release tag required\n");
		g_printerr("Usage: gitctl release get <tag>\n");
		return 1;
	}

	tag = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_RELEASE,
	                             GCTL_VERB_GET, tag, params);
}

/* ── release create ──────────────────────────────────────────────────── */

/**
 * cmd_release_create:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl release create".  Parses --tag, --title, --notes,
 * --draft, and --prerelease options.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_release_create(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	gchar *tag = NULL;
	gchar *title = NULL;
	gchar *notes = NULL;
	gboolean draft = FALSE;
	gboolean prerelease = FALSE;
	gint ret;

	GOptionEntry entries[] = {
		{ "tag", 'T', 0, G_OPTION_ARG_STRING, &tag,
		  "Release tag name", "TAG" },
		{ "title", 't', 0, G_OPTION_ARG_STRING, &title,
		  "Release title", "TITLE" },
		{ "notes", 'n', 0, G_OPTION_ARG_STRING, &notes,
		  "Release notes", "TEXT" },
		{ "draft", 'd', 0, G_OPTION_ARG_NONE, &draft,
		  "Create as draft release", NULL },
		{ "prerelease", 'p', 0, G_OPTION_ARG_NONE, &prerelease,
		  "Mark as prerelease", NULL },
		{ NULL }
	};

	opt_context = g_option_context_new("- create a release");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	if (tag != NULL)
		g_hash_table_insert(params, g_strdup("tag"), g_strdup(tag));

	if (title != NULL)
		g_hash_table_insert(params, g_strdup("title"), g_strdup(title));

	if (notes != NULL)
		g_hash_table_insert(params, g_strdup("notes"), g_strdup(notes));

	if (draft)
		g_hash_table_insert(params, g_strdup("draft"), g_strdup("true"));

	if (prerelease)
		g_hash_table_insert(params, g_strdup("prerelease"),
		                    g_strdup("true"));

	ret = gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_RELEASE,
	                            GCTL_VERB_CREATE, tag, params);

	g_free(tag);
	g_free(title);
	g_free(notes);

	return ret;
}

/* ── release delete ──────────────────────────────────────────────────── */

/**
 * cmd_release_delete:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl release delete <tag>".
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_release_delete(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *tag;

	if (argc < 2)
	{
		g_printerr("error: release tag required\n");
		g_printerr("Usage: gitctl release delete <tag>\n");
		return 1;
	}

	tag = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_RELEASE,
	                             GCTL_VERB_DELETE, tag, params);
}

/* ── Main entry point ────────────────────────────────────────────────── */

/**
 * gctl_cmd_release:
 * @app: the #GctlApp instance
 * @argc: argument count (verb + verb-specific args)
 * @argv: (array length=argc): argument vector, where argv[0] is the verb
 *
 * Main entry point for the "release" command.  Extracts the verb from
 * argv[0], looks it up in the dispatch table, and delegates to the
 * appropriate verb handler.
 *
 * Returns: 0 on success, 1 on error
 */
gint
gctl_cmd_release(
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
	entry = gctl_cmd_find_verb(release_verbs, N_RELEASE_VERBS, verb_name);

	if (entry == NULL)
	{
		g_printerr("error: unknown verb '%s' for release command\n",
		           verb_name);
		print_usage();
		return 1;
	}

	switch (entry->verb)
	{
		case GCTL_VERB_LIST:
			return cmd_release_list(app, argc, argv);
		case GCTL_VERB_GET:
			return cmd_release_get(app, argc, argv);
		case GCTL_VERB_CREATE:
			return cmd_release_create(app, argc, argv);
		case GCTL_VERB_DELETE:
			return cmd_release_delete(app, argc, argv);
		default:
			g_printerr("error: verb '%s' not implemented for release\n",
			           verb_name);
			return 1;
	}
}
