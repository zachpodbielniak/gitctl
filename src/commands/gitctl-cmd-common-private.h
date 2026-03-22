/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
 * gitctl-cmd-common-private.h - Shared inline helper for command handlers
 *
 * This private header provides the execute_verb() function that all
 * command handlers use to dispatch an operation through the forge
 * interface.  It is included by each gitctl-cmd-*.c file and compiled
 * as a static inline to avoid link-time symbol collisions.
 *
 * This header is NOT part of the public API.
 */

#ifndef GCTL_CMD_COMMON_PRIVATE_H
#define GCTL_CMD_COMMON_PRIVATE_H

#include <glib.h>
#include <glib-object.h>

/* We need GCTL_COMPILATION so we can include internal headers directly */
#ifndef GCTL_COMPILATION
#define GCTL_COMPILATION
#endif

#include "gitctl-types.h"
#include "gitctl-enums.h"
#include "gitctl-error.h"
#include "core/gitctl-app.h"
#include "core/gitctl-executor.h"
#include "core/gitctl-context-resolver.h"
#include "core/gitctl-config.h"
#include "core/gitctl-output-formatter.h"
#include "boxed/gitctl-command-result.h"
#include "boxed/gitctl-forge-context.h"
#include "boxed/gitctl-resource.h"
#include "interfaces/gitctl-forge.h"
#include "module/gitctl-module-manager.h"

/**
 * GctlVerbEntry:
 * @name: the verb string (e.g. "list", "get")
 * @description: a short human-readable description for usage output
 * @verb: the #GctlVerb enum value
 *
 * A single entry in a command handler's verb dispatch table.
 */
typedef struct
{
	const gchar *name;
	const gchar *description;
	GctlVerb     verb;
} GctlVerbEntry;

/**
 * gctl_cmd_find_verb:
 * @table: (array length=n_entries): the verb dispatch table
 * @n_entries: number of entries in @table
 * @name: the verb string to search for
 *
 * Searches @table for a verb entry matching @name.
 *
 * Returns: (nullable): pointer to the matching #GctlVerbEntry, or %NULL
 */
static inline const GctlVerbEntry *
gctl_cmd_find_verb(
	const GctlVerbEntry *table,
	gsize                n_entries,
	const gchar         *name
){
	gsize i;

	if (name == NULL)
		return NULL;

	for (i = 0; i < n_entries; i++)
	{
		if (g_strcmp0(table[i].name, name) == 0)
			return &table[i];
	}

	return NULL;
}

/**
 * gctl_cmd_print_verb_table:
 * @noun: the command noun (e.g. "pr", "issue")
 * @table: (array length=n_entries): the verb dispatch table
 * @n_entries: number of entries in @table
 *
 * Prints a usage summary listing all available verbs for a command noun.
 */
static inline void
gctl_cmd_print_verb_table(
	const gchar         *noun,
	const GctlVerbEntry *table,
	gsize                n_entries
){
	gsize i;

	g_printerr("Usage: gitctl %s <verb> [options]\n\n", noun);
	g_printerr("Available verbs:\n");

	for (i = 0; i < n_entries; i++)
	{
		g_printerr("  %-14s %s\n", table[i].name, table[i].description);
	}

	g_printerr("\nRun 'gitctl %s <verb> --help' for verb-specific options.\n",
	           noun);
}

/*
 * gctl_str_replace:
 * @haystack: the input string
 * @needle: the substring to find
 * @replacement: the replacement string
 *
 * Replaces all occurrences of @needle in @haystack with @replacement.
 *
 * Returns: (transfer full): a newly allocated string
 */
static inline gchar *
gctl_str_replace(
	const gchar *haystack,
	const gchar *needle,
	const gchar *replacement
){
	g_auto(GStrv) parts = NULL;
	parts = g_strsplit(haystack, needle, -1);
	return g_strjoinv(replacement, parts);
}

typedef struct
{
	GctlResourceKind  kind;
	GctlVerb          verb;
	const gchar      *method;
	const gchar      *endpoint;
} GctlApiFallbackEntry;

static const GctlApiFallbackEntry api_fallbacks[] = {
	/* PR operations */
	{ GCTL_RESOURCE_KIND_PR, GCTL_VERB_LIST,
	  "GET", "/repos/{owner}/{repo}/pulls" },
	{ GCTL_RESOURCE_KIND_PR, GCTL_VERB_GET,
	  "GET", "/repos/{owner}/{repo}/pulls/{number}" },
	{ GCTL_RESOURCE_KIND_PR, GCTL_VERB_CREATE,
	  "POST", "/repos/{owner}/{repo}/pulls" },
	{ GCTL_RESOURCE_KIND_PR, GCTL_VERB_CLOSE,
	  "PATCH", "/repos/{owner}/{repo}/pulls/{number}" },
	{ GCTL_RESOURCE_KIND_PR, GCTL_VERB_REOPEN,
	  "PATCH", "/repos/{owner}/{repo}/pulls/{number}" },
	{ GCTL_RESOURCE_KIND_PR, GCTL_VERB_MERGE,
	  "POST", "/repos/{owner}/{repo}/pulls/{number}/merge" },
	{ GCTL_RESOURCE_KIND_PR, GCTL_VERB_COMMENT,
	  "POST", "/repos/{owner}/{repo}/issues/{number}/comments" },
	{ GCTL_RESOURCE_KIND_PR, GCTL_VERB_REVIEW,
	  "POST", "/repos/{owner}/{repo}/pulls/{number}/reviews" },

	/* Issue operations */
	{ GCTL_RESOURCE_KIND_ISSUE, GCTL_VERB_LIST,
	  "GET", "/repos/{owner}/{repo}/issues" },
	{ GCTL_RESOURCE_KIND_ISSUE, GCTL_VERB_GET,
	  "GET", "/repos/{owner}/{repo}/issues/{number}" },
	{ GCTL_RESOURCE_KIND_ISSUE, GCTL_VERB_CREATE,
	  "POST", "/repos/{owner}/{repo}/issues" },
	{ GCTL_RESOURCE_KIND_ISSUE, GCTL_VERB_CLOSE,
	  "PATCH", "/repos/{owner}/{repo}/issues/{number}" },
	{ GCTL_RESOURCE_KIND_ISSUE, GCTL_VERB_REOPEN,
	  "PATCH", "/repos/{owner}/{repo}/issues/{number}" },
	{ GCTL_RESOURCE_KIND_ISSUE, GCTL_VERB_COMMENT,
	  "POST", "/repos/{owner}/{repo}/issues/{number}/comments" },

	/* Repo operations */
	{ GCTL_RESOURCE_KIND_REPO, GCTL_VERB_LIST,
	  "GET", "/user/repos" },
	{ GCTL_RESOURCE_KIND_REPO, GCTL_VERB_GET,
	  "GET", "/repos/{owner}/{repo}" },
	{ GCTL_RESOURCE_KIND_REPO, GCTL_VERB_CREATE,
	  "POST", "/user/repos" },
	{ GCTL_RESOURCE_KIND_REPO, GCTL_VERB_DELETE,
	  "DELETE", "/repos/{owner}/{repo}" },
	{ GCTL_RESOURCE_KIND_REPO, GCTL_VERB_FORK,
	  "POST", "/repos/{owner}/{repo}/forks" },

	/* Release operations */
	{ GCTL_RESOURCE_KIND_RELEASE, GCTL_VERB_LIST,
	  "GET", "/repos/{owner}/{repo}/releases" },
	{ GCTL_RESOURCE_KIND_RELEASE, GCTL_VERB_GET,
	  "GET", "/repos/{owner}/{repo}/releases/{number}" },
	{ GCTL_RESOURCE_KIND_RELEASE, GCTL_VERB_CREATE,
	  "POST", "/repos/{owner}/{repo}/releases" },
	{ GCTL_RESOURCE_KIND_RELEASE, GCTL_VERB_DELETE,
	  "DELETE", "/repos/{owner}/{repo}/releases/{number}" },
};

static inline const GctlApiFallbackEntry *
gctl_cmd_find_api_fallback(
	GctlResourceKind kind,
	GctlVerb         verb
){
	gsize i;
	for (i = 0; i < G_N_ELEMENTS(api_fallbacks); i++) {
		if (api_fallbacks[i].kind == kind &&
		    api_fallbacks[i].verb == verb)
			return &api_fallbacks[i];
	}
	return NULL;
}

static inline gchar *
gctl_cmd_expand_endpoint(
	const gchar      *tmpl,
	GctlForgeContext *context,
	GHashTable       *params
){
	g_autofree gchar *s1 = NULL;
	g_autofree gchar *s2 = NULL;
	gchar *result;
	const gchar *number;

	s1 = gctl_str_replace(tmpl, "{owner}",
	                       gctl_forge_context_get_owner(context));
	s2 = gctl_str_replace(s1, "{repo}",
	                       gctl_forge_context_get_repo_name(context));

	number = (params != NULL)
		? (const gchar *)g_hash_table_lookup(params, "number")
		: NULL;

	if (number != NULL)
		result = gctl_str_replace(s2, "{number}", number);
	else
		result = g_strdup(s2);

	return result;
}

/**
 * gctl_cmd_execute_verb:
 * @app: the #GctlApp instance
 * @kind: the resource kind being operated on
 * @verb: the verb to execute
 * @id: (nullable): resource identifier (number as string, tag, owner/repo, etc.)
 * @params: (element-type utf8 utf8) (nullable): verb-specific parameters
 *
 * Shared execution flow for all command handlers.  This function:
 * 1. Gets executor, resolver, formatter, and module_manager from @app
 * 2. Resolves the forge context via gctl_context_resolver_resolve()
 * 3. Finds the appropriate forge module via the module manager
 * 4. Builds the argv array via gctl_forge_build_argv()
 * 5. Executes the command via gctl_executor_run()
 * 6. For LIST verbs: parses output and prints formatted resources
 * 7. For GET verbs: parses output and prints a single formatted resource
 * 8. For other verbs: prints the raw stdout from the command
 *
 * Returns: 0 on success, 1 on error
 */
static inline gint
gctl_cmd_execute_verb(
	GctlApp          *app,
	GctlResourceKind  kind,
	GctlVerb          verb,
	const gchar      *id,
	GHashTable       *params
){
	GctlExecutor *executor;
	GctlContextResolver *resolver;
	GctlOutputFormatter *formatter;
	GctlModuleManager *module_manager;
	GctlConfig *config;
	g_autoptr(GctlForgeContext) context = NULL;
	g_autoptr(GctlCommandResult) result = NULL;
	g_autoptr(GError) error = NULL;
	GctlForge *forge;
	gchar **argv;
	const gchar *default_remote;
	gboolean used_api_fallback = FALSE;

	/* Step 1: Retrieve subsystems from the app */
	executor = gctl_app_get_executor(app);
	resolver = gctl_app_get_resolver(app);
	formatter = gctl_app_get_formatter(app);
	module_manager = gctl_app_get_module_manager(app);
	config = gctl_app_get_config(app);

	if (executor == NULL || resolver == NULL ||
	    formatter == NULL || module_manager == NULL || config == NULL)
	{
		g_printerr("error: application not properly initialized\n");
		return 1;
	}

	/* Step 2: Resolve forge context from git remote */
	default_remote = gctl_config_get_default_remote(config);
	context = gctl_context_resolver_resolve(resolver, default_remote, &error);

	if (context == NULL)
	{
		g_printerr("error: failed to resolve forge context: %s\n",
		           error ? error->message : "unknown error");
		return 1;
	}

	/* Step 3: Find the forge module matching the detected forge type */
	forge = gctl_module_manager_find_forge(module_manager,
	                                       gctl_forge_context_get_forge_type(context));

	if (forge == NULL)
	{
		g_printerr("error: no forge module available for %s\n",
		           gctl_forge_type_to_string(
		               gctl_forge_context_get_forge_type(context)));
		return 1;
	}

	/* Step 3.5: Verify the forge CLI tool is available */
	if (!gctl_forge_is_available(forge))
	{
		const gchar *cli_name;
		const gchar *forge_name;
		GctlForgeType ft;

		cli_name = gctl_forge_get_cli_tool(forge);
		forge_name = gctl_forge_get_name(forge);
		ft = gctl_forge_context_get_forge_type(context);

		g_printerr("error: %s requires '%s' but it was not found in PATH.\n",
		           forge_name, cli_name);

		if (ft == GCTL_FORGE_TYPE_GITHUB)
			g_printerr("  Install: https://cli.github.com/\n");
		else if (ft == GCTL_FORGE_TYPE_GITLAB)
			g_printerr("  Install: https://gitlab.com/gitlab-org/cli\n");
		else if (ft == GCTL_FORGE_TYPE_FORGEJO)
			g_printerr("  Install: https://forgejo.org/docs/latest/admin/command-line/\n");
		else if (ft == GCTL_FORGE_TYPE_GITEA)
			g_printerr("  Install: https://gitea.com/gitea/tea\n");

		return 1;
	}

	/* Add the resource identifier to params if provided */
	if (id != NULL && params != NULL)
	{
		g_hash_table_insert(params,
		                    g_strdup("number"),
		                    g_strdup(id));
	}

	/* Step 4: Build the argv array for the forge CLI */
	argv = gctl_forge_build_argv(forge, kind, verb, context, params, &error);

	/* Step 4.5: If unsupported, try API fallback */
	if (argv == NULL &&
	    error != NULL &&
	    error->domain == GCTL_ERROR &&
	    error->code == GCTL_ERROR_FORGE_UNSUPPORTED)
	{
		const GctlApiFallbackEntry *fallback;

		fallback = gctl_cmd_find_api_fallback(kind, verb);
		if (fallback != NULL)
		{
			g_autofree gchar *endpoint = NULL;
			const gchar *body_str = NULL;

			g_clear_error(&error);

			endpoint = gctl_cmd_expand_endpoint(
			    fallback->endpoint, context, params);

			/* Check for a JSON body in params */
			if (params != NULL)
				body_str = (const gchar *)g_hash_table_lookup(params, "body");

			if (gctl_app_get_verbose(app) || gctl_executor_get_dry_run(executor))
				g_printerr("note: %s %s unsupported by CLI, falling back to API: %s %s\n",
				           gctl_resource_kind_to_string(kind),
				           gctl_verb_to_string(verb),
				           fallback->method, endpoint);

			argv = gctl_forge_build_api_argv(
			    forge, fallback->method, endpoint, body_str, &error);

			if (argv != NULL)
				used_api_fallback = TRUE;
		}
	}

	if (argv == NULL)
	{
		g_printerr("error: failed to build command: %s\n",
		           error ? error->message : "unknown error");
		return 1;
	}

	/* Step 5: Execute the command */
	result = gctl_executor_run(executor,
	                           (const gchar * const *)argv,
	                           &error);
	g_strfreev(argv);

	if (result == NULL)
	{
		g_printerr("error: failed to execute command: %s\n",
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
			g_printerr("error: command exited with status %d\n",
			           gctl_command_result_get_exit_code(result));
		return 1;
	}

	/*
	 * In dry-run mode the executor returns a synthetic result with
	 * empty stdout — skip parsing and return success.
	 */
	if (gctl_executor_get_dry_run(executor))
		return 0;

	/* API fallback returns raw JSON — print as-is, skip parsing */
	if (used_api_fallback)
	{
		const gchar *stdout_text;
		stdout_text = gctl_command_result_get_stdout(result);
		if (stdout_text != NULL && stdout_text[0] != '\0')
			g_print("%s", stdout_text);
		return 0;
	}

	/* Steps 6-8: Parse and format output based on verb type */
	if (verb == GCTL_VERB_LIST)
	{
		/* Step 6: Parse list output into resources and print them */
		g_autoptr(GPtrArray) resources = NULL;

		resources = gctl_forge_parse_list_output(
		    forge, kind,
		    gctl_command_result_get_stdout(result),
		    &error);

		if (resources == NULL)
		{
			g_printerr("error: failed to parse output: %s\n",
			           error ? error->message : "unknown error");
			return 1;
		}

		gctl_output_formatter_print_resources(formatter, resources);
	}
	else if (verb == GCTL_VERB_GET)
	{
		/* Step 7: Parse single resource and print it */
		g_autoptr(GctlResource) resource = NULL;

		resource = gctl_forge_parse_get_output(
		    forge, kind,
		    gctl_command_result_get_stdout(result),
		    &error);

		if (resource == NULL)
		{
			g_printerr("error: failed to parse output: %s\n",
			           error ? error->message : "unknown error");
			return 1;
		}

		gctl_output_formatter_print_resource(formatter, resource);
	}
	else
	{
		/* Step 8: For all other verbs, print raw stdout */
		const gchar *stdout_text;

		stdout_text = gctl_command_result_get_stdout(result);
		if (stdout_text != NULL && stdout_text[0] != '\0')
			g_print("%s", stdout_text);
	}

	return 0;
}

#endif /* GCTL_CMD_COMMON_PRIVATE_H */
