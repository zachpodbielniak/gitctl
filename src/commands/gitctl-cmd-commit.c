/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
 * gitctl-cmd-commit.c - Commit viewing command handler
 *
 * Implements the "commit" command with verb dispatch for: list and get.
 *
 * This handler bypasses the forge entirely and uses local git directly,
 * since commits are always available locally.
 *
 * - list: runs "git log --oneline --format=..." via executor
 * - get:  runs "git show --stat <sha>" via executor
 */

#define GCTL_COMPILATION

#include <string.h>

#include "commands/gitctl-cmd-common-private.h"
#include "commands/gitctl-cmd-commit.h"

/* ── Verb dispatch table ─────────────────────────────────────────────── */

static const GctlVerbEntry commit_verbs[] = {
	{ "list",  "List recent commits",      GCTL_VERB_LIST },
	{ "get",   "View a specific commit",   GCTL_VERB_GET  },
};

static const gsize N_COMMIT_VERBS = G_N_ELEMENTS(commit_verbs);

/* ── Usage printer ───────────────────────────────────────────────────── */

/**
 * print_usage:
 *
 * Prints the usage summary for the "commit" command, listing all
 * available verbs and their descriptions.
 */
static void
print_usage(void)
{
	gctl_cmd_print_verb_table("commit", commit_verbs, N_COMMIT_VERBS);
}

/* ── commit list ─────────────────────────────────────────────────────── */

/**
 * cmd_commit_list:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl commit list".  Parses --limit and --branch options.
 * Runs "git log --oneline --format=..." and prints the output directly.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_commit_list(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	GctlExecutor *executor;
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GctlCommandResult) result = NULL;
	gint limit = 20;
	gchar *branch = NULL;
	g_autofree gchar *limit_str = NULL;
	const gchar *stdout_text;

	GOptionEntry entries[] = {
		{ "limit", 'l', 0, G_OPTION_ARG_INT, &limit,
		  "Maximum number of commits (default: 20)", "N" },
		{ "branch", 'b', 0, G_OPTION_ARG_STRING, &branch,
		  "Branch to list commits from", "BRANCH" },
		{ NULL }
	};

	opt_context = g_option_context_new("- list recent commits");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	executor = gctl_app_get_executor(app);
	if (executor == NULL)
	{
		g_printerr("error: application not properly initialized\n");
		return 1;
	}

	limit_str = g_strdup_printf("-n%d", limit);

	/*
	 * Build the git log command.  We use a pipe-delimited format
	 * for structured output: hash|subject|author|relative-date
	 */
	if (branch != NULL)
	{
		const gchar *git_argv[] = {
			"git", "log", "--oneline",
			"--format=%h|%s|%an|%ar",
			limit_str, branch, NULL
		};

		result = gctl_executor_run(executor, git_argv, &error);
	}
	else
	{
		const gchar *git_argv[] = {
			"git", "log", "--oneline",
			"--format=%h|%s|%an|%ar",
			limit_str, NULL
		};

		result = gctl_executor_run(executor, git_argv, &error);
	}

	g_free(branch);

	if (result == NULL)
	{
		g_printerr("error: failed to run git log: %s\n",
		           error ? error->message : "unknown error");
		return 1;
	}

	if (gctl_command_result_get_exit_code(result) != 0)
	{
		const gchar *stderr_text;

		stderr_text = gctl_command_result_get_stderr(result);
		if (stderr_text != NULL && stderr_text[0] != '\0')
			g_printerr("error: %s\n", stderr_text);
		else
			g_printerr("error: git log failed with status %d\n",
			           gctl_command_result_get_exit_code(result));
		return 1;
	}

	/* Print the formatted output */
	stdout_text = gctl_command_result_get_stdout(result);
	if (stdout_text != NULL && stdout_text[0] != '\0')
	{
		gchar **lines;
		gint i;

		lines = g_strsplit(stdout_text, "\n", -1);

		/* Print header */
		g_print("%-10s %-50s %-20s %s\n",
		        "HASH", "SUBJECT", "AUTHOR", "DATE");

		for (i = 0; lines[i] != NULL; i++)
		{
			gchar **fields;

			if (lines[i][0] == '\0')
				continue;

			fields = g_strsplit(lines[i], "|", 4);
			if (g_strv_length(fields) >= 4)
			{
				g_print("%-10s %-50.50s %-20.20s %s\n",
				        fields[0], fields[1],
				        fields[2], fields[3]);
			}
			else
			{
				/* Fallback: print raw line */
				g_print("%s\n", lines[i]);
			}
			g_strfreev(fields);
		}
		g_strfreev(lines);
	}
	else
	{
		g_print("No commits found.\n");
	}

	return 0;
}

/* ── commit get ──────────────────────────────────────────────────────── */

/**
 * cmd_commit_get:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl commit get <sha>".  Runs "git show --stat <sha>"
 * and prints the raw output.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_commit_get(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	GctlExecutor *executor;
	g_autoptr(GError) error = NULL;
	g_autoptr(GctlCommandResult) result = NULL;
	const gchar *sha;
	const gchar *stdout_text;

	if (argc < 2)
	{
		g_printerr("error: commit SHA required\n");
		g_printerr("Usage: gitctl commit get <sha>\n");
		return 1;
	}

	sha = argv[1];

	executor = gctl_app_get_executor(app);
	if (executor == NULL)
	{
		g_printerr("error: application not properly initialized\n");
		return 1;
	}

	{
		const gchar *git_argv[] = {
			"git", "show", "--stat", sha, NULL
		};

		result = gctl_executor_run(executor, git_argv, &error);
	}

	if (result == NULL)
	{
		g_printerr("error: failed to run git show: %s\n",
		           error ? error->message : "unknown error");
		return 1;
	}

	if (gctl_command_result_get_exit_code(result) != 0)
	{
		const gchar *stderr_text;

		stderr_text = gctl_command_result_get_stderr(result);
		if (stderr_text != NULL && stderr_text[0] != '\0')
			g_printerr("error: %s\n", stderr_text);
		else
			g_printerr("error: git show failed with status %d\n",
			           gctl_command_result_get_exit_code(result));
		return 1;
	}

	stdout_text = gctl_command_result_get_stdout(result);
	if (stdout_text != NULL && stdout_text[0] != '\0')
		g_print("%s", stdout_text);

	return 0;
}

/* ── Main entry point ────────────────────────────────────────────────── */

/**
 * gctl_cmd_commit:
 * @app: the #GctlApp instance
 * @argc: argument count (verb + verb-specific args)
 * @argv: (array length=argc): argument vector, where argv[0] is the verb
 *
 * Main entry point for the "commit" command.  Extracts the verb from
 * argv[0], looks it up in the dispatch table, and delegates to the
 * appropriate verb handler.
 *
 * Returns: 0 on success, 1 on error
 */
gint
gctl_cmd_commit(
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
	entry = gctl_cmd_find_verb(commit_verbs, N_COMMIT_VERBS, verb_name);

	if (entry == NULL)
	{
		g_printerr("error: unknown verb '%s' for commit command\n",
		           verb_name);
		print_usage();
		return 1;
	}

	switch (entry->verb)
	{
		case GCTL_VERB_LIST:
			return cmd_commit_list(app, argc, argv);
		case GCTL_VERB_GET:
			return cmd_commit_get(app, argc, argv);
		default:
			g_printerr("error: verb '%s' not implemented for commit\n",
			           verb_name);
			return 1;
	}
}
