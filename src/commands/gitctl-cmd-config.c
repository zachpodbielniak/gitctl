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
 * Supported operations:
 * - "list" prints all current configuration values
 * - "get" prints a single configuration value by key
 * - "set" validates, applies, and persists a configuration value
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

/*
 * config_format_name:
 * @fmt: a #GctlOutputFormat
 *
 * Returns the canonical string name for an output format.
 *
 * Returns: (transfer none): the format name
 */
static const gchar *
config_format_name(GctlOutputFormat fmt)
{
	switch (fmt)
	{
		case GCTL_OUTPUT_FORMAT_TABLE: return "table";
		case GCTL_OUTPUT_FORMAT_JSON:  return "json";
		case GCTL_OUTPUT_FORMAT_YAML:  return "yaml";
		case GCTL_OUTPUT_FORMAT_CSV:   return "csv";
		default:                       return "table";
	}
}

/*
 * config_parse_output_format:
 * @value: the string value to parse
 *
 * Parses an output format string. Returns -1 if invalid.
 *
 * Returns: the format enum value, or -1
 */
static gint
config_parse_output_format(const gchar *value)
{
	if (g_ascii_strcasecmp(value, "table") == 0)
		return (gint)GCTL_OUTPUT_FORMAT_TABLE;
	if (g_ascii_strcasecmp(value, "json") == 0)
		return (gint)GCTL_OUTPUT_FORMAT_JSON;
	if (g_ascii_strcasecmp(value, "yaml") == 0)
		return (gint)GCTL_OUTPUT_FORMAT_YAML;
	if (g_ascii_strcasecmp(value, "csv") == 0)
		return (gint)GCTL_OUTPUT_FORMAT_CSV;
	return -1;
}

/*
 * config_write_yaml_line:
 * @contents: the full file contents (or empty string)
 * @key: the YAML key to set (top-level scalar)
 * @value: the new value string
 *
 * Finds the line starting with "@key:" in @contents and replaces the
 * value portion.  If the key is not found, appends a new line.
 *
 * Returns: (transfer full): the updated file contents
 */
static gchar *
config_write_yaml_line(
	const gchar *contents,
	const gchar *key,
	const gchar *value
){
	g_autofree gchar *prefix = NULL;
	g_autofree gchar *new_line = NULL;
	gchar **lines;
	gboolean found;
	GString *result;
	gint i;

	prefix   = g_strdup_printf("%s:", key);
	new_line = g_strdup_printf("%s: %s", key, value);
	found    = FALSE;

	lines  = g_strsplit(contents, "\n", -1);
	result = g_string_new(NULL);

	for (i = 0; lines[i] != NULL; i++)
	{
		g_autofree gchar *stripped = g_strstrip(g_strdup(lines[i]));

		if (g_str_has_prefix(stripped, prefix))
		{
			g_string_append(result, new_line);
			found = TRUE;
		}
		else
		{
			g_string_append(result, lines[i]);
		}

		/*
		 * Append a newline for every line except the very last
		 * empty element that g_strsplit produces from a trailing
		 * newline.
		 */
		if (lines[i + 1] != NULL)
			g_string_append_c(result, '\n');
	}

	/* If the key was not found, append it */
	if (!found)
	{
		/* Ensure we have a trailing newline before appending */
		if (result->len > 0 && result->str[result->len - 1] != '\n')
			g_string_append_c(result, '\n');
		g_string_append(result, new_line);
		g_string_append_c(result, '\n');
	}

	g_strfreev(lines);

	return g_string_free(result, FALSE);
}

/**
 * cmd_config_set:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl config set <key> <value>".  Supports the following
 * top-level configuration keys:
 *
 * - output.format   : table, json, yaml, csv
 * - default.remote  : any string (git remote name)
 * - default.forge   : github, gitlab, forgejo, gitea
 *
 * The value is validated, applied to the in-memory config, and then
 * persisted by updating the YAML config file on disk.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_config_set(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	GctlConfig *config;
	const gchar *key;
	const gchar *value;
	const gchar *config_path;
	g_autofree gchar *default_path = NULL;
	g_autofree gchar *contents = NULL;
	g_autofree gchar *new_contents = NULL;
	g_autofree gchar *config_dir = NULL;
	g_autoptr(GError) error = NULL;
	const gchar *yaml_key;
	const gchar *yaml_value;

	if (argc < 3)
	{
		g_printerr("error: key and value required\n");
		g_printerr("Usage: gitctl config set <key> <value>\n\n");
		g_printerr("Available keys:\n");
		g_printerr("  output.format    Output format (table/json/yaml/csv)\n");
		g_printerr("  default.remote   Default git remote name\n");
		g_printerr("  default.forge    Default forge type (github/gitlab/forgejo/gitea)\n");
		return 1;
	}

	config = gctl_app_get_config(app);
	if (config == NULL)
	{
		g_printerr("error: application not properly initialized\n");
		return 1;
	}

	key   = argv[1];
	value = argv[2];

	/*
	 * Validate the key and value, apply to in-memory config, and
	 * determine the YAML key name and serialised value for the file.
	 */
	if (g_strcmp0(key, "output.format") == 0)
	{
		gint fmt;

		fmt = config_parse_output_format(value);
		if (fmt < 0)
		{
			g_printerr("error: invalid output format '%s'\n", value);
			g_printerr("Valid values: table, json, yaml, csv\n");
			return 1;
		}

		gctl_config_set_default_output_format(
			config, (GctlOutputFormat)fmt);
		yaml_key   = "output";
		yaml_value = config_format_name((GctlOutputFormat)fmt);
	}
	else if (g_strcmp0(key, "default.remote") == 0)
	{
		gctl_config_set_default_remote(config, value);
		yaml_key   = "remote";
		yaml_value = value;
	}
	else if (g_strcmp0(key, "default.forge") == 0)
	{
		GctlForgeType ft;

		ft = gctl_forge_type_from_string(value);
		if (ft == GCTL_FORGE_TYPE_UNKNOWN)
		{
			g_printerr("error: unknown forge type '%s'\n", value);
			g_printerr("Valid values: github, gitlab, forgejo, gitea\n");
			return 1;
		}

		gctl_config_set_default_forge(config, ft);
		yaml_key   = "default_forge";
		yaml_value = gctl_forge_type_to_string(ft);
	}
	else
	{
		g_printerr("error: unknown configuration key '%s'\n", key);
		g_printerr("\nAvailable keys:\n");
		g_printerr("  output.format    Output format (table/json/yaml/csv)\n");
		g_printerr("  default.remote   Default git remote name\n");
		g_printerr("  default.forge    Default forge type (github/gitlab/forgejo/gitea)\n");
		return 1;
	}

	/*
	 * Determine the config file path.  Use the path that was loaded
	 * during initialisation; fall back to the XDG default location
	 * if no file was loaded (e.g. fresh install).
	 */
	config_path = gctl_config_get_config_path(config);
	if (config_path == NULL)
	{
		default_path = g_build_filename(
			g_get_user_config_dir(), "gitctl", "config.yaml", NULL);
		config_path = default_path;
	}

	/* Ensure the config directory exists */
	config_dir = g_path_get_dirname(config_path);
	if (g_mkdir_with_parents(config_dir, 0755) != 0)
	{
		g_printerr("error: could not create config directory '%s'\n",
		           config_dir);
		return 1;
	}

	/* Read existing file contents (empty string if file doesn't exist) */
	if (g_file_test(config_path, G_FILE_TEST_EXISTS))
	{
		if (!g_file_get_contents(config_path, &contents, NULL, &error))
		{
			g_printerr("error: could not read '%s': %s\n",
			           config_path, error->message);
			return 1;
		}
	}
	else
	{
		contents = g_strdup("# gitctl configuration\n");
	}

	/* Update the YAML line and write back */
	new_contents = config_write_yaml_line(contents, yaml_key, yaml_value);

	if (!g_file_set_contents(config_path, new_contents, -1, &error))
	{
		g_printerr("error: could not write '%s': %s\n",
		           config_path, error->message);
		return 1;
	}

	g_print("%s = %s\n", key, yaml_value);
	return 0;
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
