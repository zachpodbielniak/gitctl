/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
 * gitctl-cmd-issue.c - Issue command handler
 *
 * Implements the "issue" command with verb dispatch for: list, get,
 * create, edit, close, reopen, comment, and browse.
 *
 * All verbs parse their options and delegate to gctl_cmd_execute_verb().
 */

#define GCTL_COMPILATION

#include "commands/gitctl-cmd-common-private.h"
#include "commands/gitctl-cmd-issue.h"

/* ── Verb dispatch table ─────────────────────────────────────────────── */

static const GctlVerbEntry issue_verbs[] = {
	{ "list",    "List issues",                      GCTL_VERB_LIST    },
	{ "get",     "View a single issue",              GCTL_VERB_GET     },
	{ "create",  "Create a new issue",               GCTL_VERB_CREATE  },
	{ "edit",    "Edit an existing issue",            GCTL_VERB_EDIT    },
	{ "close",   "Close an issue",                   GCTL_VERB_CLOSE   },
	{ "reopen",  "Reopen a closed issue",            GCTL_VERB_REOPEN  },
	{ "comment", "Comment on an issue",              GCTL_VERB_COMMENT },
	{ "browse",  "Open an issue in the browser",     GCTL_VERB_BROWSE  },
};

static const gsize N_ISSUE_VERBS = G_N_ELEMENTS(issue_verbs);

/* ── Usage printer ───────────────────────────────────────────────────── */

/**
 * print_usage:
 *
 * Prints the usage summary for the "issue" command, listing all
 * available verbs and their descriptions.
 */
static void
print_usage(void)
{
	gctl_cmd_print_verb_table("issue", issue_verbs, N_ISSUE_VERBS);
}

/* ── issue list ──────────────────────────────────────────────────────── */

/**
 * cmd_issue_list:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl issue list".  Parses --state, --limit, --author,
 * --label, and --assignee options.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_issue_list(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	gchar *state = NULL;
	gint limit = 30;
	gchar *author = NULL;
	gchar *label = NULL;
	gchar *assignee = NULL;
	gboolean use_pager = FALSE;
	gint ret;

	GOptionEntry entries[] = {
		{ "state", 's', 0, G_OPTION_ARG_STRING, &state,
		  "Filter by state (open/closed/all)", "STATE" },
		{ "limit", 'l', 0, G_OPTION_ARG_INT, &limit,
		  "Maximum number of results (default: 30)", "N" },
		{ "author", 'a', 0, G_OPTION_ARG_STRING, &author,
		  "Filter by author", "USER" },
		{ "label", 'L', 0, G_OPTION_ARG_STRING, &label,
		  "Filter by label", "LABEL" },
		{ "assignee", 'A', 0, G_OPTION_ARG_STRING, &assignee,
		  "Filter by assignee", "USER" },
		{ "pager", 0, 0, G_OPTION_ARG_NONE, &use_pager,
		  "Pipe output through $PAGER", NULL },
		{ NULL }
	};

	opt_context = g_option_context_new("- list issues");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	if (state != NULL)
		g_hash_table_insert(params, g_strdup("state"), g_strdup(state));
	else
		g_hash_table_insert(params, g_strdup("state"), g_strdup("open"));

	{
		gchar *limit_str;

		limit_str = g_strdup_printf("%d", limit);
		g_hash_table_insert(params, g_strdup("limit"), limit_str);
	}

	if (author != NULL)
		g_hash_table_insert(params, g_strdup("author"), g_strdup(author));

	if (label != NULL)
		g_hash_table_insert(params, g_strdup("label"), g_strdup(label));

	if (assignee != NULL)
		g_hash_table_insert(params, g_strdup("assignee"), g_strdup(assignee));

	if (use_pager)
		g_hash_table_insert(params, g_strdup("pager"), g_strdup("true"));

	ret = gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_ISSUE,
	                            GCTL_VERB_LIST, NULL, params);

	g_free(state);
	g_free(author);
	g_free(label);
	g_free(assignee);

	return ret;
}

/* ── issue get ───────────────────────────────────────────────────────── */

/**
 * cmd_issue_get:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl issue get <number>".
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_issue_get(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	const gchar *issue_number;

	GOptionEntry entries[] = {
		{ NULL }
	};

	opt_context = g_option_context_new("<number> - view an issue");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	if (argc < 2)
	{
		g_printerr("error: issue number required\n");
		g_printerr("Usage: gitctl issue get <number>\n");
		return 1;
	}

	issue_number = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_ISSUE,
	                             GCTL_VERB_GET, issue_number, params);
}

/* ── issue create ────────────────────────────────────────────────────── */

/**
 * cmd_issue_create:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl issue create".  Parses --title, --body, --assignee,
 * and --label options.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_issue_create(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	gchar *title = NULL;
	gchar *body = NULL;
	gchar *assignee = NULL;
	gchar *label = NULL;
	gint ret;

	GOptionEntry entries[] = {
		{ "title", 't', 0, G_OPTION_ARG_STRING, &title,
		  "Issue title", "TITLE" },
		{ "body", 'b', 0, G_OPTION_ARG_STRING, &body,
		  "Issue body", "BODY" },
		{ "assignee", 'a', 0, G_OPTION_ARG_STRING, &assignee,
		  "Assign to user", "USER" },
		{ "label", 'l', 0, G_OPTION_ARG_STRING, &label,
		  "Add label", "LABEL" },
		{ NULL }
	};

	opt_context = g_option_context_new("- create an issue");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	if (title != NULL)
		g_hash_table_insert(params, g_strdup("title"), g_strdup(title));

	if (body != NULL)
		g_hash_table_insert(params, g_strdup("body"), g_strdup(body));

	if (assignee != NULL)
		g_hash_table_insert(params, g_strdup("assignee"), g_strdup(assignee));

	if (label != NULL)
		g_hash_table_insert(params, g_strdup("label"), g_strdup(label));

	ret = gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_ISSUE,
	                            GCTL_VERB_CREATE, NULL, params);

	g_free(title);
	g_free(body);
	g_free(assignee);
	g_free(label);

	return ret;
}

/* ── issue edit ──────────────────────────────────────────────────────── */

/**
 * cmd_issue_edit:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl issue edit <number>".  Parses --title, --body,
 * --assignee, and --label options.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_issue_edit(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	gchar *title = NULL;
	gchar *body = NULL;
	gchar *assignee = NULL;
	gchar *label = NULL;
	const gchar *issue_number;
	gint ret;

	GOptionEntry entries[] = {
		{ "title", 't', 0, G_OPTION_ARG_STRING, &title,
		  "New title", "TITLE" },
		{ "body", 'b', 0, G_OPTION_ARG_STRING, &body,
		  "New body", "BODY" },
		{ "assignee", 'a', 0, G_OPTION_ARG_STRING, &assignee,
		  "Set assignee", "USER" },
		{ "label", 'l', 0, G_OPTION_ARG_STRING, &label,
		  "Set label", "LABEL" },
		{ NULL }
	};

	opt_context = g_option_context_new("<number> - edit an issue");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	if (argc < 2)
	{
		g_printerr("error: issue number required\n");
		g_printerr("Usage: gitctl issue edit <number> [options]\n");
		return 1;
	}

	issue_number = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	if (title != NULL)
		g_hash_table_insert(params, g_strdup("title"), g_strdup(title));

	if (body != NULL)
		g_hash_table_insert(params, g_strdup("body"), g_strdup(body));

	if (assignee != NULL)
		g_hash_table_insert(params, g_strdup("assignee"), g_strdup(assignee));

	if (label != NULL)
		g_hash_table_insert(params, g_strdup("label"), g_strdup(label));

	ret = gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_ISSUE,
	                            GCTL_VERB_EDIT, issue_number, params);

	g_free(title);
	g_free(body);
	g_free(assignee);
	g_free(label);

	return ret;
}

/* ── issue close ─────────────────────────────────────────────────────── */

/**
 * cmd_issue_close:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl issue close <number>".
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_issue_close(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *issue_number;

	if (argc < 2)
	{
		g_printerr("error: issue number required\n");
		g_printerr("Usage: gitctl issue close <number>\n");
		return 1;
	}

	issue_number = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_ISSUE,
	                             GCTL_VERB_CLOSE, issue_number, params);
}

/* ── issue reopen ────────────────────────────────────────────────────── */

/**
 * cmd_issue_reopen:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl issue reopen <number>".
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_issue_reopen(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *issue_number;

	if (argc < 2)
	{
		g_printerr("error: issue number required\n");
		g_printerr("Usage: gitctl issue reopen <number>\n");
		return 1;
	}

	issue_number = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_ISSUE,
	                             GCTL_VERB_REOPEN, issue_number, params);
}

/* ── issue comment ───────────────────────────────────────────────────── */

/**
 * cmd_issue_comment:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl issue comment <number>".  Parses --body option.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_issue_comment(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	gchar *body = NULL;
	const gchar *issue_number;
	gint ret;

	GOptionEntry entries[] = {
		{ "body", 'b', 0, G_OPTION_ARG_STRING, &body,
		  "Comment body", "TEXT" },
		{ NULL }
	};

	opt_context = g_option_context_new("<number> - comment on an issue");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	if (argc < 2)
	{
		g_printerr("error: issue number required\n");
		g_printerr("Usage: gitctl issue comment <number> --body TEXT\n");
		return 1;
	}

	issue_number = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	if (body != NULL)
		g_hash_table_insert(params, g_strdup("body"), g_strdup(body));

	ret = gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_ISSUE,
	                            GCTL_VERB_COMMENT, issue_number, params);

	g_free(body);

	return ret;
}

/* ── issue browse ────────────────────────────────────────────────────── */

/**
 * cmd_issue_browse:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl issue browse [number]".  The issue number is optional;
 * if omitted, opens the issue list in the browser.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_issue_browse(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *issue_number;

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	issue_number = (argc >= 2) ? argv[1] : NULL;

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_ISSUE,
	                             GCTL_VERB_BROWSE, issue_number, params);
}

/* ── Main entry point ────────────────────────────────────────────────── */

/**
 * gctl_cmd_issue:
 * @app: the #GctlApp instance
 * @argc: argument count (verb + verb-specific args)
 * @argv: (array length=argc): argument vector, where argv[0] is the verb
 *
 * Main entry point for the "issue" command.  Extracts the verb from
 * argv[0], looks it up in the dispatch table, and delegates to the
 * appropriate verb handler.
 *
 * Returns: 0 on success, 1 on error
 */
gint
gctl_cmd_issue(
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
	entry = gctl_cmd_find_verb(issue_verbs, N_ISSUE_VERBS, verb_name);

	if (entry == NULL)
	{
		g_printerr("error: unknown verb '%s' for issue command\n",
		           verb_name);
		print_usage();
		return 1;
	}

	switch (entry->verb)
	{
		case GCTL_VERB_LIST:
			return cmd_issue_list(app, argc, argv);
		case GCTL_VERB_GET:
			return cmd_issue_get(app, argc, argv);
		case GCTL_VERB_CREATE:
			return cmd_issue_create(app, argc, argv);
		case GCTL_VERB_EDIT:
			return cmd_issue_edit(app, argc, argv);
		case GCTL_VERB_CLOSE:
			return cmd_issue_close(app, argc, argv);
		case GCTL_VERB_REOPEN:
			return cmd_issue_reopen(app, argc, argv);
		case GCTL_VERB_COMMENT:
			return cmd_issue_comment(app, argc, argv);
		case GCTL_VERB_BROWSE:
			return cmd_issue_browse(app, argc, argv);
		default:
			g_printerr("error: verb '%s' not implemented for issue\n",
			           verb_name);
			return 1;
	}
}
