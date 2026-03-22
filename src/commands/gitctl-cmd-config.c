/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
 * gitctl-cmd-config.c - Configuration management command handler
 *
 * Implements the "config" command with verb dispatch for: list, get,
 * and set.
 *
 * Unlike other command handlers, config operates on the local gitctl
 * configuration file and does NOT go through the forge interface.
 *
 * For the initial scaffold:
 * - "list" prints all current configuration values
 * - "get" prints a single configuration value by key
 * - "set" prints "not yet implemented" (requires YAML writing support)
 */

#define GCTL_COMPILATION

#include "commands/gitctl-cmd-common-private.h"
#include "commands/gitctl-cmd-config.h"

/* ── Verb dispatch table ─────────────────────────────────────────────── */

static const GctlVerbEntry config_verbs[] = {
	{ "list", "Show all configuration values",        GCTL_VERB_LIST },
	{ "get",  "Get a configuration value",            GCTL_VERB_GET  },
	{ "set",  "Set a configuration value",            GCTL_VERB_EDIT },
};

static const gsize N_CONFIG_VERBS = G_N_ELEMENTS(config_verbs);

/* ── Usage printer ───────────────────────────────────────────────────── */

/**
 * print_usage:
 *
 * Prints the usage summary for the "config" command, listing all
 * available verbs and their descriptions.
 */
static void
print_usage(void)
{
	gctl_cmd_print_verb_table("config", config_verbs, N_CONFIG_VERBS);
}

/* ── Known configuration keys ────────────────────────────────────────── */

/**
 * print_config_value:
 * @config: the #GctlConfig instance
 * @key: the configuration key to look up and print
 *
 * Looks up the configuration key and prints its value to stdout.
 * Recognized keys:
 * - "output.format" - default output format
 * - "default.remote" - default git remote name
 * - "default.forge" - default forge type
 *
 * Returns: %TRUE if the key was recognized, %FALSE otherwise
 */
static gboolean
print_config_value(
	GctlConfig   *config,
	const gchar  *key
){
	if (g_strcmp0(key, "output.format") == 0)
	{
		GctlOutputFormat fmt;
		const gchar *fmt_str;

		fmt = gctl_config_get_default_output_format(config);

		switch (fmt)
		{
			case GCTL_OUTPUT_FORMAT_TABLE:
				fmt_str = "table";
				break;
			case GCTL_OUTPUT_FORMAT_JSON:
				fmt_str = "json";
				break;
			case GCTL_OUTPUT_FORMAT_YAML:
				fmt_str = "yaml";
				break;
			case GCTL_OUTPUT_FORMAT_CSV:
				fmt_str = "csv";
				break;
			default:
				fmt_str = "unknown";
				break;
		}

		g_print("output.format = %s\n", fmt_str);
		return TRUE;
	}
	else if (g_strcmp0(key, "default.remote") == 0)
	{
		const gchar *remote;

		remote = gctl_config_get_default_remote(config);
		g_print("default.remote = %s\n",
		        (remote != NULL) ? remote : "(unset)");
		return TRUE;
	}
	else if (g_strcmp0(key, "default.forge") == 0)
	{
		GctlForgeType forge_type;

		forge_type = gctl_config_get_default_forge(config);
		g_print("default.forge = %s\n",
		        gctl_forge_type_to_string(forge_type));
		return TRUE;
	}

	return FALSE;
}

/* ── config list ─────────────────────────────────────────────────────── */

/**
 * cmd_config_list:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl config list".  Prints all current configuration
 * values to stdout.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_config_list(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	GctlConfig *config;

	(void)argc;
	(void)argv;

	config = gctl_app_get_config(app);

	if (config == NULL)
	{
		g_printerr("error: application not properly initialized\n");
		return 1;
	}

	print_config_value(config, "output.format");
	print_config_value(config, "default.remote");
	print_config_value(config, "default.forge");

	return 0;
}

/* ── config get ──────────────────────────────────────────────────────── */

/**
 * cmd_config_get:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl config get <key>".  Prints the value for a single
 * configuration key.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_config_get(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	GctlConfig *config;
	const gchar *key;

	if (argc < 2)
	{
		g_printerr("error: configuration key required\n");
		g_printerr("Usage: gitctl config get <key>\n\n");
		g_printerr("Available keys:\n");
		g_printerr("  output.format    Output format (table/json/yaml/csv)\n");
		g_printerr("  default.remote   Default git remote name\n");
		g_printerr("  default.forge    Default forge type\n");
		return 1;
	}

	config = gctl_app_get_config(app);

	if (config == NULL)
	{
		g_printerr("error: application not properly initialized\n");
		return 1;
	}

	key = argv[1];

	if (!print_config_value(config, key))
	{
		g_printerr("error: unknown configuration key '%s'\n", key);
		g_printerr("\nAvailable keys:\n");
		g_printerr("  output.format    Output format (table/json/yaml/csv)\n");
		g_printerr("  default.remote   Default git remote name\n");
		g_printerr("  default.forge    Default forge type\n");
		return 1;
	}

	return 0;
}

/* ── config set ──────────────────────────────────────────────────────── */

/**
 * cmd_config_set:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl config set <key> <value>".  Currently prints a
 * "not yet implemented" message since YAML writing support is needed.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_config_set(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	(void)app;

	if (argc < 3)
	{
		g_printerr("error: key and value required\n");
		g_printerr("Usage: gitctl config set <key> <value>\n");
		return 1;
	}

	g_printerr("error: 'config set' is not yet implemented\n");
	g_printerr("hint: edit %s/gitctl/config.yaml directly\n",
	           g_get_user_config_dir());

	return 1;
}

/* ── Main entry point ────────────────────────────────────────────────── */

/**
 * gctl_cmd_config:
 * @app: the #GctlApp instance
 * @argc: argument count (verb + verb-specific args)
 * @argv: (array length=argc): argument vector, where argv[0] is the verb
 *
 * Main entry point for the "config" command.  Extracts the verb from
 * argv[0], looks it up in the dispatch table, and delegates to the
 * appropriate verb handler.
 *
 * Returns: 0 on success, 1 on error
 */
gint
gctl_cmd_config(
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
	entry = gctl_cmd_find_verb(config_verbs, N_CONFIG_VERBS, verb_name);

	if (entry == NULL)
	{
		g_printerr("error: unknown verb '%s' for config command\n",
		           verb_name);
		print_usage();
		return 1;
	}

	/*
	 * Config uses GCTL_VERB_EDIT for "set" since there's no
	 * dedicated SET verb in the enum.  We dispatch based on the
	 * verb entry name rather than the enum value for clarity.
	 */
	if (g_strcmp0(entry->name, "list") == 0)
		return cmd_config_list(app, argc, argv);
	else if (g_strcmp0(entry->name, "get") == 0)
		return cmd_config_get(app, argc, argv);
	else if (g_strcmp0(entry->name, "set") == 0)
		return cmd_config_set(app, argc, argv);

	g_printerr("error: verb '%s' not implemented for config\n", verb_name);
	return 1;
}
