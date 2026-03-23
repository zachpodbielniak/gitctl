/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
 * gitctl-cmd-status.c - Repository status command handler
 *
 * Implements the "status" command which shows an overview of the current
 * repository: detected forge type and host, owner/repo, open PRs count,
 * open issues count, and recent activity summary.
 *
 * This does NOT follow the noun-verb pattern.  It resolves the forge
 * context and runs sub-queries (pr list, issue list) to gather counts.
 */

#define GCTL_COMPILATION

#include <string.h>

#include "commands/gitctl-cmd-common-private.h"
#include "commands/gitctl-cmd-status.h"

/* ── Usage printer ───────────────────────────────────────────────────── */

/**
 * print_usage:
 *
 * Prints the usage summary for the "status" command.
 */
static void
print_usage(void)
{
	g_printerr("Usage: gitctl status\n\n");
	g_printerr("Show an overview of the current repository:\n");
	g_printerr("  - Detected forge type and host\n");
	g_printerr("  - Repository owner/name\n");
	g_printerr("  - Open pull request count\n");
	g_printerr("  - Open issue count\n");
	g_printerr("  - Recent activity summary\n");
}

/* ── Count helper ────────────────────────────────────────────────────── */

/**
 * count_open_resources:
 * @app: the #GctlApp instance
 * @kind: the resource kind to count (PR or ISSUE)
 * @forge: the #GctlForge module
 * @context: the resolved #GctlForgeContext
 *
 * Runs a list command for @kind with state=open and limit=100,
 * then counts the lines in the output as a rough resource count.
 *
 * Returns: the number of open resources, or -1 on error
 */
static gint
count_open_resources(
	GctlApp          *app,
	GctlResourceKind  kind,
	GctlForge        *forge,
	GctlForgeContext *context
){
	GctlExecutor *executor;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GctlCommandResult) result = NULL;
	gchar **argv;
	const gchar *stdout_text;
	gchar **lines;
	gint count;

	executor = gctl_app_get_executor(app);

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_insert(params, g_strdup("state"), g_strdup("open"));
	g_hash_table_insert(params, g_strdup("limit"), g_strdup("100"));

	argv = gctl_forge_build_argv(forge, kind, GCTL_VERB_LIST,
	                              context, params, &error);
	if (argv == NULL)
		return -1;

	result = gctl_executor_run(executor,
	                           (const gchar * const *)argv,
	                           &error);
	g_strfreev(argv);

	if (result == NULL ||
	    gctl_command_result_get_exit_code(result) != 0)
		return -1;

	stdout_text = gctl_command_result_get_stdout(result);
	if (stdout_text == NULL || stdout_text[0] == '\0')
		return 0;

	/*
	 * Parse the output through the forge's list parser to get an
	 * accurate count of resources returned.
	 */
	{
		g_autoptr(GPtrArray) resources = NULL;

		resources = gctl_forge_parse_list_output(
		    forge, kind, stdout_text, &error);

		if (resources != NULL)
			return (gint)resources->len;
	}

	/* Fallback: count non-empty lines */
	lines = g_strsplit(stdout_text, "\n", -1);
	count = 0;
	if (lines != NULL)
	{
		gint i;
		for (i = 0; lines[i] != NULL; i++)
		{
			if (lines[i][0] != '\0')
				count++;
		}
		g_strfreev(lines);
	}

	return count;
}

/* ── Recent activity helper ──────────────────────────────────────────── */

/**
 * print_recent_activity:
 * @app: the #GctlApp instance
 *
 * Runs "git log --oneline -5" to show recent commit activity.
 */
static void
print_recent_activity(GctlApp *app)
{
	GctlExecutor *executor;
	g_autoptr(GError) error = NULL;
	g_autoptr(GctlCommandResult) result = NULL;
	const gchar *argv[] = {
		"git", "log", "--oneline", "-5", "--format=%h %s (%ar)", NULL
	};
	const gchar *stdout_text;

	executor = gctl_app_get_executor(app);

	result = gctl_executor_run(executor, argv, &error);
	if (result == NULL ||
	    gctl_command_result_get_exit_code(result) != 0)
	{
		g_print("  (unable to retrieve recent commits)\n");
		return;
	}

	stdout_text = gctl_command_result_get_stdout(result);
	if (stdout_text != NULL && stdout_text[0] != '\0')
		g_print("%s", stdout_text);
	else
		g_print("  (no commits)\n");
}

/* ── Main entry point ────────────────────────────────────────────────── */

/**
 * gctl_cmd_status:
 * @app: the #GctlApp instance
 * @argc: argument count
 * @argv: (array length=argc): argument vector
 *
 * Main entry point for the "status" command.  Resolves the forge
 * context, gathers PR/issue counts, and prints a summary.
 *
 * Returns: 0 on success, 1 on error
 */
gint
gctl_cmd_status(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	GctlContextResolver *resolver;
	GctlConfig *config;
	GctlModuleManager *module_manager;
	g_autoptr(GctlForgeContext) context = NULL;
	g_autoptr(GError) error = NULL;
	GctlForge *forge;
	const gchar *default_remote;
	g_autofree gchar *owner_repo = NULL;
	gint pr_count;
	gint issue_count;

	/* Handle --help / -h */
	if (argc >= 1 &&
	    (g_strcmp0(argv[0], "--help") == 0 ||
	     g_strcmp0(argv[0], "-h") == 0))
	{
		print_usage();
		return 0;
	}

	/* Retrieve subsystems */
	resolver = gctl_app_get_resolver(app);
	config = gctl_app_get_config(app);
	module_manager = gctl_app_get_module_manager(app);

	if (resolver == NULL || config == NULL || module_manager == NULL)
	{
		g_printerr("error: application not properly initialized\n");
		return 1;
	}

	/* Resolve forge context */
	default_remote = gctl_config_get_default_remote(config);
	context = gctl_context_resolver_resolve(resolver, default_remote, &error);

	if (context == NULL)
	{
		g_printerr("error: failed to resolve forge context: %s\n",
		           error ? error->message : "unknown error");
		return 1;
	}

	/* Find the forge module */
	forge = gctl_module_manager_find_forge(module_manager,
	                                       gctl_forge_context_get_forge_type(context));

	/* Print header */
	owner_repo = gctl_forge_context_get_owner_repo(context);

	g_print("Repository Status\n");
	g_print("─────────────────────────────────────\n");
	g_print("  Forge:      %s\n",
	        gctl_forge_type_to_string(
	            gctl_forge_context_get_forge_type(context)));
	g_print("  Host:       %s\n",
	        gctl_forge_context_get_host(context));
	g_print("  Repository: %s\n", owner_repo);

	/* Gather counts if forge module is available */
	if (forge != NULL && gctl_forge_is_available(forge))
	{
		pr_count = count_open_resources(
		    app, GCTL_RESOURCE_KIND_PR, forge, context);
		issue_count = count_open_resources(
		    app, GCTL_RESOURCE_KIND_ISSUE, forge, context);

		if (pr_count >= 0)
			g_print("  Open PRs:   %d\n", pr_count);
		else
			g_print("  Open PRs:   (unavailable)\n");

		if (issue_count >= 0)
			g_print("  Open Issues: %d\n", issue_count);
		else
			g_print("  Open Issues: (unavailable)\n");
	}
	else
	{
		g_print("  Open PRs:   (forge CLI not available)\n");
		g_print("  Open Issues: (forge CLI not available)\n");
	}

	g_print("\nRecent Activity\n");
	g_print("─────────────────────────────────────\n");
	print_recent_activity(app);

	return 0;
}
