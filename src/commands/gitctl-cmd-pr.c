/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
 * gitctl-cmd-pr.c - Pull request command handler
 *
 * Implements the "pr" command with verb dispatch for: list, get, create,
 * edit, close, reopen, merge, checkout, comment, review, and browse.
 *
 * The list and get verbs have fully implemented execution flows.  All
 * other verbs parse their options and delegate to execute_verb().
 */

#define GCTL_COMPILATION

#include "commands/gitctl-cmd-common-private.h"
#include "commands/gitctl-cmd-pr.h"

/* ── Verb dispatch table ─────────────────────────────────────────────── */

static const GctlVerbEntry pr_verbs[] = {
	{ "list",     "List pull requests",                   GCTL_VERB_LIST     },
	{ "get",      "View a single pull request",           GCTL_VERB_GET      },
	{ "create",   "Create a new pull request",            GCTL_VERB_CREATE   },
	{ "edit",     "Edit an existing pull request",        GCTL_VERB_EDIT     },
	{ "close",    "Close a pull request",                 GCTL_VERB_CLOSE    },
	{ "reopen",   "Reopen a closed pull request",         GCTL_VERB_REOPEN   },
	{ "merge",    "Merge a pull request",                 GCTL_VERB_MERGE    },
	{ "checkout", "Checkout a pull request branch",       GCTL_VERB_CHECKOUT },
	{ "comment",  "Comment on a pull request",            GCTL_VERB_COMMENT  },
	{ "review",   "Review a pull request",                GCTL_VERB_REVIEW   },
	{ "browse",   "Open a pull request in the browser",   GCTL_VERB_BROWSE   },
	{ "diff",     "View pull request diff",              GCTL_VERB_DIFF     },
};

static const gsize N_PR_VERBS = G_N_ELEMENTS(pr_verbs);

/* ── Usage printer ───────────────────────────────────────────────────── */

/**
 * print_usage:
 *
 * Prints the usage summary for the "pr" command, listing all available
 * verbs and their descriptions.
 */
static void
print_usage(void)
{
	gctl_cmd_print_verb_table("pr", pr_verbs, N_PR_VERBS);
}

/* ── pr list ─────────────────────────────────────────────────────────── */

/**
 * cmd_pr_list:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl pr list".  Parses --state, --limit, --author,
 * --label, and --assignee options, then executes the list operation
 * through the full forge pipeline.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_pr_list(
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

	opt_context = g_option_context_new("- list pull requests");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	/* Build the params hash table for the forge backend */
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

	ret = gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_PR,
	                            GCTL_VERB_LIST, NULL, params);

	g_free(state);
	g_free(author);
	g_free(label);
	g_free(assignee);

	return ret;
}

/* ── pr get ──────────────────────────────────────────────────────────── */

/**
 * cmd_pr_get:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl pr get <number>".  Expects a positional PR number
 * argument, then executes the get operation through the full forge
 * pipeline.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_pr_get(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	const gchar *pr_number;

	GOptionEntry entries[] = {
		{ NULL }
	};

	opt_context = g_option_context_new("<number> - view a pull request");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	/* Expect exactly one positional argument: the PR number */
	if (argc < 2)
	{
		g_printerr("error: pull request number required\n");
		g_printerr("Usage: gitctl pr get <number>\n");
		return 1;
	}

	pr_number = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_PR,
	                             GCTL_VERB_GET, pr_number, params);
}

/* ── pr create ───────────────────────────────────────────────────────── */

/**
 * cmd_pr_create:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl pr create".  Parses --title, --body, --base, --head,
 * and --draft options.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_pr_create(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	gchar *title = NULL;
	gchar *body = NULL;
	gchar *base = NULL;
	gchar *head = NULL;
	gboolean draft = FALSE;
	gint ret;

	GOptionEntry entries[] = {
		{ "title", 't', 0, G_OPTION_ARG_STRING, &title,
		  "Pull request title", "TITLE" },
		{ "body", 'b', 0, G_OPTION_ARG_STRING, &body,
		  "Pull request body", "BODY" },
		{ "base", 'B', 0, G_OPTION_ARG_STRING, &base,
		  "Base branch", "BRANCH" },
		{ "head", 'H', 0, G_OPTION_ARG_STRING, &head,
		  "Head branch", "BRANCH" },
		{ "draft", 'd', 0, G_OPTION_ARG_NONE, &draft,
		  "Create as draft", NULL },
		{ NULL }
	};

	opt_context = g_option_context_new("- create a pull request");
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

	if (base != NULL)
		g_hash_table_insert(params, g_strdup("base"), g_strdup(base));

	if (head != NULL)
		g_hash_table_insert(params, g_strdup("head"), g_strdup(head));

	if (draft)
		g_hash_table_insert(params, g_strdup("draft"), g_strdup("true"));

	ret = gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_PR,
	                            GCTL_VERB_CREATE, NULL, params);

	g_free(title);
	g_free(body);
	g_free(base);
	g_free(head);

	return ret;
}

/* ── pr edit ─────────────────────────────────────────────────────────── */

/**
 * cmd_pr_edit:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl pr edit <number>".  Parses --title, --body,
 * --assignee, and --label options.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_pr_edit(
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
	const gchar *pr_number;
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

	opt_context = g_option_context_new("<number> - edit a pull request");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	if (argc < 2)
	{
		g_printerr("error: pull request number required\n");
		g_printerr("Usage: gitctl pr edit <number> [options]\n");
		return 1;
	}

	pr_number = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	if (title != NULL)
		g_hash_table_insert(params, g_strdup("title"), g_strdup(title));

	if (body != NULL)
		g_hash_table_insert(params, g_strdup("body"), g_strdup(body));

	if (assignee != NULL)
		g_hash_table_insert(params, g_strdup("assignee"), g_strdup(assignee));

	if (label != NULL)
		g_hash_table_insert(params, g_strdup("label"), g_strdup(label));

	ret = gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_PR,
	                            GCTL_VERB_EDIT, pr_number, params);

	g_free(title);
	g_free(body);
	g_free(assignee);
	g_free(label);

	return ret;
}

/* ── pr close ────────────────────────────────────────────────────────── */

/**
 * cmd_pr_close:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl pr close <number>".
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_pr_close(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *pr_number;

	if (argc < 2)
	{
		g_printerr("error: pull request number required\n");
		g_printerr("Usage: gitctl pr close <number>\n");
		return 1;
	}

	pr_number = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_PR,
	                             GCTL_VERB_CLOSE, pr_number, params);
}

/* ── pr reopen ───────────────────────────────────────────────────────── */

/**
 * cmd_pr_reopen:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl pr reopen <number>".
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_pr_reopen(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *pr_number;

	if (argc < 2)
	{
		g_printerr("error: pull request number required\n");
		g_printerr("Usage: gitctl pr reopen <number>\n");
		return 1;
	}

	pr_number = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_PR,
	                             GCTL_VERB_REOPEN, pr_number, params);
}

/* ── pr merge ────────────────────────────────────────────────────────── */

/**
 * cmd_pr_merge:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl pr merge <number>".  Parses --method option
 * (merge/rebase/squash).
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_pr_merge(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	gchar *method = NULL;
	const gchar *pr_number;
	gint ret;

	GOptionEntry entries[] = {
		{ "method", 'm', 0, G_OPTION_ARG_STRING, &method,
		  "Merge method (merge/rebase/squash)", "METHOD" },
		{ NULL }
	};

	opt_context = g_option_context_new("<number> - merge a pull request");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	if (argc < 2)
	{
		g_printerr("error: pull request number required\n");
		g_printerr("Usage: gitctl pr merge <number> [--method METHOD]\n");
		return 1;
	}

	pr_number = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	if (method != NULL)
		g_hash_table_insert(params, g_strdup("strategy"), g_strdup(method));

	ret = gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_PR,
	                            GCTL_VERB_MERGE, pr_number, params);

	g_free(method);

	return ret;
}

/* ── pr checkout ─────────────────────────────────────────────────────── */

/**
 * cmd_pr_checkout:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl pr checkout <number>".
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_pr_checkout(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *pr_number;

	if (argc < 2)
	{
		g_printerr("error: pull request number required\n");
		g_printerr("Usage: gitctl pr checkout <number>\n");
		return 1;
	}

	pr_number = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_PR,
	                             GCTL_VERB_CHECKOUT, pr_number, params);
}

/* ── pr comment ──────────────────────────────────────────────────────── */

/**
 * cmd_pr_comment:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl pr comment <number>".  Parses --body option.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_pr_comment(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	gchar *body = NULL;
	const gchar *pr_number;
	gint ret;

	GOptionEntry entries[] = {
		{ "body", 'b', 0, G_OPTION_ARG_STRING, &body,
		  "Comment body", "TEXT" },
		{ NULL }
	};

	opt_context = g_option_context_new("<number> - comment on a pull request");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	if (argc < 2)
	{
		g_printerr("error: pull request number required\n");
		g_printerr("Usage: gitctl pr comment <number> --body TEXT\n");
		return 1;
	}

	pr_number = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	if (body != NULL)
		g_hash_table_insert(params, g_strdup("body"), g_strdup(body));

	ret = gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_PR,
	                            GCTL_VERB_COMMENT, pr_number, params);

	g_free(body);

	return ret;
}

/* ── pr review ───────────────────────────────────────────────────────── */

/**
 * cmd_pr_review:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl pr review <number>".  Parses --approve,
 * --request-changes, --comment, and --body options.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_pr_review(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	gboolean approve = FALSE;
	gboolean request_changes = FALSE;
	gboolean comment_only = FALSE;
	gchar *body = NULL;
	const gchar *pr_number;
	gint ret;

	GOptionEntry entries[] = {
		{ "approve", 0, 0, G_OPTION_ARG_NONE, &approve,
		  "Approve the pull request", NULL },
		{ "request-changes", 0, 0, G_OPTION_ARG_NONE, &request_changes,
		  "Request changes", NULL },
		{ "comment", 0, 0, G_OPTION_ARG_NONE, &comment_only,
		  "Leave a review comment without approval", NULL },
		{ "body", 'b', 0, G_OPTION_ARG_STRING, &body,
		  "Review body", "TEXT" },
		{ NULL }
	};

	opt_context = g_option_context_new("<number> - review a pull request");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	if (argc < 2)
	{
		g_printerr("error: pull request number required\n");
		g_printerr("Usage: gitctl pr review <number> "
		           "[--approve|--request-changes|--comment] [--body TEXT]\n");
		return 1;
	}

	pr_number = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	if (approve)
		g_hash_table_insert(params, g_strdup("action"),
		                    g_strdup("approve"));
	else if (request_changes)
		g_hash_table_insert(params, g_strdup("action"),
		                    g_strdup("request-changes"));
	else if (comment_only)
		g_hash_table_insert(params, g_strdup("action"),
		                    g_strdup("comment"));

	if (body != NULL)
		g_hash_table_insert(params, g_strdup("body"), g_strdup(body));

	ret = gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_PR,
	                            GCTL_VERB_REVIEW, pr_number, params);

	g_free(body);

	return ret;
}

/* ── pr browse ───────────────────────────────────────────────────────── */

/**
 * cmd_pr_browse:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl pr browse [number]".  The PR number is optional;
 * if omitted, opens the PR list in the browser.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_pr_browse(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *pr_number;

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	pr_number = (argc >= 2) ? argv[1] : NULL;

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_PR,
	                             GCTL_VERB_BROWSE, pr_number, params);
}

/* ── pr diff ─────────────────────────────────────────────────────────── */

/**
 * cmd_pr_diff:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl pr diff <number>".  Views the diff of a pull request.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_pr_diff(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *pr_number;

	if (argc < 2)
	{
		g_printerr("error: pull request number required\n");
		g_printerr("Usage: gitctl pr diff <number>\n");
		return 1;
	}

	pr_number = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_PR,
	                             GCTL_VERB_DIFF, pr_number, params);
}

/* ── Main entry point ────────────────────────────────────────────────── */

/**
 * gctl_cmd_pr:
 * @app: the #GctlApp instance
 * @argc: argument count (verb + verb-specific args)
 * @argv: (array length=argc): argument vector, where argv[0] is the verb
 *
 * Main entry point for the "pr" command.  Extracts the verb from
 * argv[0], looks it up in the dispatch table, and delegates to the
 * appropriate verb handler.
 *
 * Returns: 0 on success, 1 on error
 */
gint
gctl_cmd_pr(
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
	entry = gctl_cmd_find_verb(pr_verbs, N_PR_VERBS, verb_name);

	if (entry == NULL)
	{
		g_printerr("error: unknown verb '%s' for pr command\n", verb_name);
		print_usage();
		return 1;
	}

	switch (entry->verb)
	{
		case GCTL_VERB_LIST:
			return cmd_pr_list(app, argc, argv);
		case GCTL_VERB_GET:
			return cmd_pr_get(app, argc, argv);
		case GCTL_VERB_CREATE:
			return cmd_pr_create(app, argc, argv);
		case GCTL_VERB_EDIT:
			return cmd_pr_edit(app, argc, argv);
		case GCTL_VERB_CLOSE:
			return cmd_pr_close(app, argc, argv);
		case GCTL_VERB_REOPEN:
			return cmd_pr_reopen(app, argc, argv);
		case GCTL_VERB_MERGE:
			return cmd_pr_merge(app, argc, argv);
		case GCTL_VERB_CHECKOUT:
			return cmd_pr_checkout(app, argc, argv);
		case GCTL_VERB_COMMENT:
			return cmd_pr_comment(app, argc, argv);
		case GCTL_VERB_REVIEW:
			return cmd_pr_review(app, argc, argv);
		case GCTL_VERB_BROWSE:
			return cmd_pr_browse(app, argc, argv);
		case GCTL_VERB_DIFF:
			return cmd_pr_diff(app, argc, argv);
		default:
			g_printerr("error: verb '%s' not implemented for pr\n",
			           verb_name);
			return 1;
	}
}
