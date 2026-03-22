/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
 * gitctl-cmd-api.c - Raw API passthrough command handler
 *
 * Implements the "api" command which does NOT use the verb dispatch
 * pattern.  Instead it passes a raw HTTP method and endpoint through
 * to the forge CLI's API interface.
 *
 * Usage: gitctl api <METHOD> <endpoint> [--body JSON]
 *
 * Supported methods: GET, POST, PUT, PATCH, DELETE
 */

#define GCTL_COMPILATION

#include "commands/gitctl-cmd-common-private.h"
#include "commands/gitctl-cmd-api.h"

/* ── Supported HTTP methods ──────────────────────────────────────────── */

/**
 * VALID_HTTP_METHODS:
 *
 * NULL-terminated list of HTTP methods accepted by the api command.
 */
static const gchar *VALID_HTTP_METHODS[] = {
	"GET", "POST", "PUT", "PATCH", "DELETE", NULL
};

/**
 * is_valid_method:
 * @method: the HTTP method string to validate
 *
 * Checks whether @method is a recognized HTTP method.  Comparison
 * is case-insensitive.
 *
 * Returns: %TRUE if @method is valid
 */
static gboolean
is_valid_method(const gchar *method)
{
	gsize i;

	for (i = 0; VALID_HTTP_METHODS[i] != NULL; i++)
	{
		if (g_ascii_strcasecmp(method, VALID_HTTP_METHODS[i]) == 0)
			return TRUE;
	}

	return FALSE;
}

/* ── Usage printer ───────────────────────────────────────────────────── */

/**
 * print_usage:
 *
 * Prints the usage summary for the "api" command.
 */
static void
print_usage(void)
{
	g_printerr("Usage: gitctl api <METHOD> <endpoint> [--body JSON]\n\n");
	g_printerr("Make a raw API request to the detected forge.\n\n");
	g_printerr("Methods: GET, POST, PUT, PATCH, DELETE\n\n");
	g_printerr("Examples:\n");
	g_printerr("  gitctl api GET /repos/{owner}/{repo}/pulls\n");
	g_printerr("  gitctl api POST /repos/{owner}/{repo}/issues "
	           "--body '{\"title\":\"Bug\"}'\n");
}

/* ── Main entry point ────────────────────────────────────────────────── */

/**
 * gctl_cmd_api:
 * @app: the #GctlApp instance
 * @argc: argument count
 * @argv: (array length=argc): argument vector, where argv[0] is the HTTP
 *     method and argv[1] is the API endpoint
 *
 * Main entry point for the "api" command.  Parses the HTTP method and
 * endpoint from positional arguments, optional --body from options,
 * then builds an API argv via gctl_forge_build_api_argv(), executes it,
 * and prints the raw output.
 *
 * Returns: 0 on success, 1 on error
 */
gint
gctl_cmd_api(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GctlForgeContext) context = NULL;
	g_autoptr(GctlCommandResult) result = NULL;
	GctlExecutor *executor;
	GctlContextResolver *resolver;
	GctlModuleManager *module_manager;
	GctlConfig *config;
	GctlForge *forge;
	gchar *body = NULL;
	const gchar *method;
	const gchar *endpoint;
	const gchar *default_remote;
	gchar **api_argv;

	GOptionEntry entries[] = {
		{ "body", 'b', 0, G_OPTION_ARG_STRING, &body,
		  "JSON request body", "JSON" },
		{ NULL }
	};

	/* Handle --help / -h before option parsing */
	if (argc >= 1 &&
	    (g_strcmp0(argv[0], "--help") == 0 ||
	     g_strcmp0(argv[0], "-h") == 0))
	{
		print_usage();
		return 0;
	}

	/*
	 * We need at least 2 positional args (method + endpoint).
	 * GOptionContext will consume recognized options, leaving
	 * positional args in argv.
	 */
	opt_context = g_option_context_new("<METHOD> <endpoint> - raw API call");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		g_free(body);
		return 1;
	}

	/* After option parsing, argv should have: program, method, endpoint */
	if (argc < 3)
	{
		g_printerr("error: HTTP method and endpoint required\n\n");
		print_usage();
		g_free(body);
		return 1;
	}

	method = argv[1];
	endpoint = argv[2];

	/* Validate the HTTP method */
	if (!is_valid_method(method))
	{
		g_printerr("error: invalid HTTP method '%s'\n", method);
		g_printerr("Valid methods: GET, POST, PUT, PATCH, DELETE\n");
		g_free(body);
		return 1;
	}

	/* Get subsystems from app */
	executor = gctl_app_get_executor(app);
	resolver = gctl_app_get_resolver(app);
	module_manager = gctl_app_get_module_manager(app);
	config = gctl_app_get_config(app);

	if (executor == NULL || resolver == NULL ||
	    module_manager == NULL || config == NULL)
	{
		g_printerr("error: application not properly initialized\n");
		g_free(body);
		return 1;
	}

	/* Resolve forge context */
	default_remote = gctl_config_get_default_remote(config);
	context = gctl_context_resolver_resolve(resolver, default_remote, &error);

	if (context == NULL)
	{
		g_printerr("error: failed to resolve forge context: %s\n",
		           error->message);
		g_free(body);
		return 1;
	}

	/* Find the forge module */
	forge = gctl_module_manager_find_forge(
	    module_manager,
	    gctl_forge_context_get_forge_type(context));

	if (forge == NULL)
	{
		g_printerr("error: no forge module available for %s\n",
		           gctl_forge_type_to_string(
		               gctl_forge_context_get_forge_type(context)));
		g_free(body);
		return 1;
	}

	/* Build the API argv */
	api_argv = gctl_forge_build_api_argv(forge, method, endpoint,
	                                     body, &error);
	g_free(body);

	if (api_argv == NULL)
	{
		g_printerr("error: failed to build API command: %s\n",
		           error->message);
		return 1;
	}

	/* Execute the command */
	result = gctl_executor_run(executor,
	                           (const gchar * const *)api_argv,
	                           &error);
	g_strfreev(api_argv);

	if (result == NULL)
	{
		g_printerr("error: failed to execute API command: %s\n",
		           error->message);
		return 1;
	}

	/* Print raw output */
	if (gctl_command_result_get_exit_code(result) != 0)
	{
		const gchar *stderr_text;

		stderr_text = gctl_command_result_get_stderr(result);
		if (stderr_text != NULL && stderr_text[0] != '\0')
			g_printerr("error: %s\n", stderr_text);
		else
			g_printerr("error: API call exited with status %d\n",
			           gctl_command_result_get_exit_code(result));
		return 1;
	}

	{
		const gchar *stdout_text;

		stdout_text = gctl_command_result_get_stdout(result);
		if (stdout_text != NULL && stdout_text[0] != '\0')
			g_print("%s", stdout_text);
	}

	return 0;
}
