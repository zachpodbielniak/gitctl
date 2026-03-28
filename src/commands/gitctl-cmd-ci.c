/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
 * gitctl-cmd-ci.c - CI/pipeline command handler
 *
 * Implements the "ci" command with verb dispatch for: list, get,
 * log, and browse.
 *
 * Maps to: gh run list, glab ci list, fj actions list (API fallback),
 * tea (API fallback).
 */

#define GCTL_COMPILATION

#include <string.h>

#include "commands/gitctl-cmd-common-private.h"
#include "commands/gitctl-cmd-ci.h"

/* ── Verb dispatch table ─────────────────────────────────────────────── */

static const GctlVerbEntry ci_verbs[] = {
	{ "list",   "List recent workflow/pipeline runs",  GCTL_VERB_LIST   },
	{ "get",    "View a specific run",                 GCTL_VERB_GET    },
	{ "log",    "View run logs",                       GCTL_VERB_LOG    },
	{ "browse", "Open run in browser",                 GCTL_VERB_BROWSE },
};

static const gsize N_CI_VERBS = G_N_ELEMENTS(ci_verbs);

/* ── Usage printer ───────────────────────────────────────────────────── */

/**
 * print_usage:
 *
 * Prints the usage summary for the "ci" command, listing all
 * available verbs and their descriptions.
 */
static void
print_usage(void)
{
	gctl_cmd_print_verb_table("ci", ci_verbs, N_CI_VERBS);
}

/* ── ci list ─────────────────────────────────────────────────────────── */

/**
 * cmd_ci_list:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl ci list".  Parses --limit and --state options.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_ci_list(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	gint limit = 20;
	gchar *state = NULL;
	gboolean use_pager = FALSE;
	gint ret;

	GOptionEntry entries[] = {
		{ "limit", 'l', 0, G_OPTION_ARG_INT, &limit,
		  "Maximum number of results (default: 20)", "N" },
		{ "state", 's', 0, G_OPTION_ARG_STRING, &state,
		  "Filter by state (success/failure/running/all)", "STATE" },
		{ "pager", 0, 0, G_OPTION_ARG_NONE, &use_pager,
		  "Pipe output through $PAGER", NULL },
		{ NULL }
	};

	opt_context = g_option_context_new("- list CI runs");
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

	if (state != NULL)
		g_hash_table_insert(params, g_strdup("state"), g_strdup(state));

	if (use_pager)
		g_hash_table_insert(params, g_strdup("pager"), g_strdup("true"));

	ret = gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_CI,
	                            GCTL_VERB_LIST, NULL, params);

	g_free(state);

	return ret;
}

/* ── ci get ──────────────────────────────────────────────────────────── */

/**
 * cmd_ci_get:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl ci get <id>".  Views a specific workflow/pipeline run.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_ci_get(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *run_id;

	if (argc < 2)
	{
		g_printerr("error: run ID required\n");
		g_printerr("Usage: gitctl ci get <id>\n");
		return 1;
	}

	run_id = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_CI,
	                             GCTL_VERB_GET, run_id, params);
}

/* ── ci log ──────────────────────────────────────────────────────────── */

/**
 * cmd_ci_log:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl ci log <id>".  Views logs for a specific run.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_ci_log(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *run_id;

	if (argc < 2)
	{
		g_printerr("error: run ID required\n");
		g_printerr("Usage: gitctl ci log <id>\n");
		return 1;
	}

	run_id = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_CI,
	                             GCTL_VERB_LOG, run_id, params);
}

/* ── ci browse ───────────────────────────────────────────────────────── */

/**
 * cmd_ci_browse:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl ci browse [id]".  Opens a run in the browser.
 * The run ID is optional; if omitted, opens the CI overview page.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_ci_browse(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *run_id;

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	run_id = (argc >= 2) ? argv[1] : NULL;

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_CI,
	                             GCTL_VERB_BROWSE, run_id, params);
}

/* ── Main entry point ────────────────────────────────────────────────── */

/**
 * gctl_cmd_ci:
 * @app: the #GctlApp instance
 * @argc: argument count (verb + verb-specific args)
 * @argv: (array length=argc): argument vector, where argv[0] is the verb
 *
 * Main entry point for the "ci" command.  Extracts the verb from
 * argv[0], looks it up in the dispatch table, and delegates to the
 * appropriate verb handler.
 *
 * Returns: 0 on success, 1 on error
 */
gint
gctl_cmd_ci(
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
	entry = gctl_cmd_find_verb(ci_verbs, N_CI_VERBS, verb_name);

	if (entry == NULL)
	{
		g_printerr("error: unknown verb '%s' for ci command\n",
		           verb_name);
		print_usage();
		return 1;
	}

	switch (entry->verb)
	{
		case GCTL_VERB_LIST:
			return cmd_ci_list(app, argc, argv);
		case GCTL_VERB_GET:
			return cmd_ci_get(app, argc, argv);
		case GCTL_VERB_LOG:
			return cmd_ci_log(app, argc, argv);
		case GCTL_VERB_BROWSE:
			return cmd_ci_browse(app, argc, argv);
		default:
			g_printerr("error: verb '%s' not implemented for ci\n",
			           verb_name);
			return 1;
	}
}
