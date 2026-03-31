/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-config.c - Configuration loader and accessor implementation */

#define GCTL_COMPILATION
#include "gitctl.h"

#include <gio/gio.h>
#include <yaml-glib.h>

/* ── Private structure ────────────────────────────────────────────── */

struct _GctlConfig
{
	GObject parent_instance;

	gchar            *config_path;
	GctlOutputFormat  default_output_format;
	gchar            *default_remote;
	GctlForgeType     default_forge;
	GHashTable       *forge_hosts;   /* gchar* hostname -> GINT_TO_POINTER(GctlForgeType) */
	GHashTable       *cli_paths;     /* gchar* forge key -> gchar* path */
	GHashTable       *default_hosts; /* gchar* forge key -> gchar* default hostname */
	GHashTable       *ssh_hosts;    /* gchar* forge key -> gchar* SSH hostname */
	GHashTable       *aliases;       /* gchar* alias -> gchar* expanded */
};

G_DEFINE_TYPE(GctlConfig, gctl_config, G_TYPE_OBJECT)

/* ── Helpers ──────────────────────────────────────────────────────── */

/*
 * forge_type_key:
 * @forge_type: a #GctlForgeType
 *
 * Returns a static string key for @forge_type suitable for use as a
 * hash-table key in the cli_paths table.
 */
static const gchar *
forge_type_key(GctlForgeType forge_type)
{
	switch (forge_type) {
	case GCTL_FORGE_TYPE_GITHUB:  return "github";
	case GCTL_FORGE_TYPE_GITLAB:  return "gitlab";
	case GCTL_FORGE_TYPE_FORGEJO: return "forgejo";
	case GCTL_FORGE_TYPE_GITEA:   return "gitea";
	default:                      return "unknown";
	}
}

/*
 * parse_output_format:
 * @str: the output format string from the config file
 *
 * Parses a string into a #GctlOutputFormat.  Returns
 * GCTL_OUTPUT_FORMAT_TABLE if the string is unrecognised.
 */
static GctlOutputFormat
parse_output_format(const gchar *str)
{
	if (str == NULL)
		return GCTL_OUTPUT_FORMAT_TABLE;

	if (g_ascii_strcasecmp(str, "table") == 0)
		return GCTL_OUTPUT_FORMAT_TABLE;
	if (g_ascii_strcasecmp(str, "json") == 0)
		return GCTL_OUTPUT_FORMAT_JSON;
	if (g_ascii_strcasecmp(str, "yaml") == 0)
		return GCTL_OUTPUT_FORMAT_YAML;
	if (g_ascii_strcasecmp(str, "csv") == 0)
		return GCTL_OUTPUT_FORMAT_CSV;

	g_warning("gitctl: unknown output format '%s', using 'table'", str);
	return GCTL_OUTPUT_FORMAT_TABLE;
}

/*
 * populate_defaults:
 * @self: a #GctlConfig
 *
 * Fills in built-in default values for all configuration fields.
 */
static void
populate_defaults(GctlConfig *self)
{
	/* Scalar defaults */
	self->default_output_format = GCTL_OUTPUT_FORMAT_TABLE;
	self->default_remote        = g_strdup("origin");
	self->default_forge         = GCTL_FORGE_TYPE_GITHUB;

	/* Forge-host mapping defaults */
	g_hash_table_insert(self->forge_hosts,
	                    g_strdup("github.com"),
	                    GINT_TO_POINTER(GCTL_FORGE_TYPE_GITHUB));
	g_hash_table_insert(self->forge_hosts,
	                    g_strdup("gitlab.com"),
	                    GINT_TO_POINTER(GCTL_FORGE_TYPE_GITLAB));
	g_hash_table_insert(self->forge_hosts,
	                    g_strdup("codeberg.org"),
	                    GINT_TO_POINTER(GCTL_FORGE_TYPE_FORGEJO));

	/* CLI tool path defaults */
	g_hash_table_insert(self->cli_paths,
	                    g_strdup(forge_type_key(GCTL_FORGE_TYPE_GITHUB)),
	                    g_strdup("gh"));
	g_hash_table_insert(self->cli_paths,
	                    g_strdup(forge_type_key(GCTL_FORGE_TYPE_GITLAB)),
	                    g_strdup("glab"));
	g_hash_table_insert(self->cli_paths,
	                    g_strdup(forge_type_key(GCTL_FORGE_TYPE_FORGEJO)),
	                    g_strdup("fj"));
	g_hash_table_insert(self->cli_paths,
	                    g_strdup(forge_type_key(GCTL_FORGE_TYPE_GITEA)),
	                    g_strdup("tea"));

	/* Default host per forge — matches the forge_hosts entries above */
	g_hash_table_insert(self->default_hosts,
	                    g_strdup(forge_type_key(GCTL_FORGE_TYPE_GITHUB)),
	                    g_strdup("github.com"));
	g_hash_table_insert(self->default_hosts,
	                    g_strdup(forge_type_key(GCTL_FORGE_TYPE_GITLAB)),
	                    g_strdup("gitlab.com"));
	g_hash_table_insert(self->default_hosts,
	                    g_strdup(forge_type_key(GCTL_FORGE_TYPE_FORGEJO)),
	                    g_strdup("codeberg.org"));
}

/*
 * apply_scalar_overrides:
 * @self: a #GctlConfig
 * @root_map: the root #YamlMapping from the config file
 *
 * Reads optional top-level scalar keys ("output", "remote",
 * "default_forge") from @root_map and overrides the corresponding
 * fields on @self.  Missing keys are silently ignored so the
 * previously-loaded defaults remain intact.
 */
static void
apply_scalar_overrides(
	GctlConfig  *self,
	YamlMapping *root_map
){
	const gchar *val;

	/* output format */
	val = yaml_mapping_get_string_member(root_map, "output");
	if (val != NULL)
		self->default_output_format = parse_output_format(val);

	/* default remote */
	val = yaml_mapping_get_string_member(root_map, "remote");
	if (val != NULL) {
		g_free(self->default_remote);
		self->default_remote = g_strdup(val);
	}

	/* default forge */
	val = yaml_mapping_get_string_member(root_map, "default_forge");
	if (val != NULL) {
		GctlForgeType ft;

		ft = gctl_forge_type_from_string(val);
		if (ft != GCTL_FORGE_TYPE_UNKNOWN)
			self->default_forge = ft;
		else
			g_warning("gitctl: unknown forge type '%s' in "
			          "default_forge, keeping default", val);
	}
}

/*
 * apply_forges_override:
 * @self: a #GctlConfig
 * @root_map: the root #YamlMapping from the config file
 *
 * Reads the optional "forges" mapping.  For each forge entry the
 * function:
 *   - Looks up "hosts" (a sequence of strings) and inserts each
 *     hostname into the forge_hosts hash table.
 *   - Looks up "cli" (a string) and inserts/replaces the entry in
 *     the cli_paths hash table.
 *
 * Unrecognised forge names are skipped with a warning.
 */
static void
apply_forges_override(
	GctlConfig  *self,
	YamlMapping *root_map
){
	YamlMapping *forges_map;
	guint        n_forges;
	guint        i;

	if (!yaml_mapping_has_member(root_map, "forges"))
		return;

	forges_map = yaml_mapping_get_mapping_member(root_map, "forges");
	if (forges_map == NULL)
		return;

	n_forges = yaml_mapping_get_size(forges_map);

	for (i = 0; i < n_forges; i++) {
		const gchar   *forge_name;
		YamlNode      *forge_node;
		YamlMapping   *forge_map;
		GctlForgeType  ft;
		const gchar   *cli_val;
		YamlSequence  *hosts_seq;
		guint          n_hosts;
		guint          j;

		forge_name = yaml_mapping_get_key(forges_map, i);
		if (forge_name == NULL)
			continue;

		ft = gctl_forge_type_from_string(forge_name);
		if (ft == GCTL_FORGE_TYPE_UNKNOWN) {
			g_warning("gitctl: unknown forge '%s' in config, "
			          "skipping", forge_name);
			continue;
		}

		forge_node = yaml_mapping_get_value(forges_map, i);
		if (forge_node == NULL)
			continue;

		forge_map = yaml_node_get_mapping(forge_node);
		if (forge_map == NULL)
			continue;

		/* cli override */
		cli_val = yaml_mapping_get_string_member(forge_map, "cli");
		if (cli_val != NULL) {
			g_hash_table_insert(self->cli_paths,
			                    g_strdup(forge_type_key(ft)),
			                    g_strdup(cli_val));
		}

		/* default_host explicit override */
		{
			const gchar *dh;

			dh = yaml_mapping_get_string_member(forge_map, "default_host");
			if (dh != NULL) {
				g_hash_table_insert(self->default_hosts,
				                    g_strdup(forge_type_key(ft)),
				                    g_strdup(dh));
			}
		}

		/* ssh_host override (for forges where SSH host differs) */
		{
			const gchar *sh;

			sh = yaml_mapping_get_string_member(forge_map, "ssh_host");
			if (sh != NULL) {
				g_hash_table_insert(self->ssh_hosts,
				                    g_strdup(forge_type_key(ft)),
				                    g_strdup(sh));
			}
		}

		/* hosts override */
		if (!yaml_mapping_has_member(forge_map, "hosts"))
			continue;

		hosts_seq = yaml_mapping_get_sequence_member(forge_map, "hosts");
		if (hosts_seq == NULL)
			continue;

		n_hosts = yaml_sequence_get_length(hosts_seq);
		for (j = 0; j < n_hosts; j++) {
			const gchar *host;

			host = yaml_sequence_get_string_element(hosts_seq, j);
			if (host != NULL) {
				g_autofree gchar *trimmed = g_strstrip(g_strdup(host));

				g_hash_table_insert(
					self->forge_hosts,
					g_strdup(trimmed),
					GINT_TO_POINTER(ft));

				/*
				 * If no explicit default_host was set,
				 * use the first host in the list.
				 */
				if (j == 0 && !yaml_mapping_has_member(
				                  forge_map, "default_host"))
				{
					g_hash_table_insert(
						self->default_hosts,
						g_strdup(forge_type_key(ft)),
						g_strdup(trimmed));
				}
			}
		}
	}
}

/*
 * apply_aliases_override:
 * @self: a #GctlConfig
 * @root_map: the root #YamlMapping from the config file
 *
 * Reads the optional "aliases" mapping.  Each key-value pair is
 * inserted into the aliases hash table, overriding any existing
 * entry with the same key.
 */
static void
apply_aliases_override(
	GctlConfig  *self,
	YamlMapping *root_map
){
	YamlMapping *aliases_map;
	guint        n_aliases;
	guint        i;

	if (!yaml_mapping_has_member(root_map, "aliases"))
		return;

	aliases_map = yaml_mapping_get_mapping_member(root_map, "aliases");
	if (aliases_map == NULL)
		return;

	n_aliases = yaml_mapping_get_size(aliases_map);

	for (i = 0; i < n_aliases; i++) {
		const gchar *key;
		const gchar *val;

		key = yaml_mapping_get_key(aliases_map, i);
		if (key == NULL)
			continue;

		val = yaml_mapping_get_string_member(aliases_map, key);
		if (val != NULL) {
			g_hash_table_insert(self->aliases,
			                    g_strdup(key),
			                    g_strdup(val));
		}
	}
}

/* ── GObject vfuncs ───────────────────────────────────────────────── */

static void
gctl_config_finalize(GObject *object)
{
	GctlConfig *self;

	self = GCTL_CONFIG(object);

	g_free(self->config_path);
	g_free(self->default_remote);
	g_clear_pointer(&self->forge_hosts, g_hash_table_unref);
	g_clear_pointer(&self->cli_paths, g_hash_table_unref);
	g_clear_pointer(&self->default_hosts, g_hash_table_unref);
	g_clear_pointer(&self->ssh_hosts, g_hash_table_unref);
	g_clear_pointer(&self->aliases, g_hash_table_unref);

	G_OBJECT_CLASS(gctl_config_parent_class)->finalize(object);
}

static void
gctl_config_class_init(GctlConfigClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);
	object_class->finalize = gctl_config_finalize;
}

static void
gctl_config_init(GctlConfig *self)
{
	self->config_path = NULL;

	self->forge_hosts   = g_hash_table_new_full(g_str_hash, g_str_equal,
	                                            g_free, NULL);
	self->cli_paths     = g_hash_table_new_full(g_str_hash, g_str_equal,
	                                            g_free, g_free);
	self->default_hosts = g_hash_table_new_full(g_str_hash, g_str_equal,
	                                            g_free, g_free);
	self->ssh_hosts     = g_hash_table_new_full(g_str_hash, g_str_equal,
	                                            g_free, g_free);
	self->aliases       = g_hash_table_new_full(g_str_hash, g_str_equal,
	                                            g_free, g_free);

	populate_defaults(self);
}

/* ── Public API ───────────────────────────────────────────────────── */

GctlConfig *
gctl_config_new(void)
{
	return (GctlConfig *)g_object_new(GCTL_TYPE_CONFIG, NULL);
}

/**
 * gctl_config_load:
 * @self: a #GctlConfig
 * @path: path to a YAML configuration file
 * @error: (nullable): return location for a #GError
 *
 * Parses the YAML file at @path using yaml-glib and overrides the
 * built-in defaults with any values found in the file.  Missing
 * keys are silently ignored so partial configuration files are
 * perfectly valid.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean
gctl_config_load(
	GctlConfig   *self,
	const gchar  *path,
	GError      **error
){
	g_autoptr(YamlParser) parser = NULL;
	YamlNode    *root;
	YamlMapping *root_map;

	g_return_val_if_fail(GCTL_IS_CONFIG(self), FALSE);
	g_return_val_if_fail(path != NULL, FALSE);

	if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
		g_set_error(error, GCTL_ERROR, GCTL_ERROR_CONFIG_PARSE,
		            "Configuration file does not exist: %s", path);
		return FALSE;
	}

	/* Parse the YAML file */
	parser = yaml_parser_new_immutable();
	if (!yaml_parser_load_from_file(parser, path, error))
		return FALSE;

	/* Retrieve the root node; must be a mapping */
	root = yaml_parser_get_root(parser);
	if (root == NULL) {
		g_set_error(error, GCTL_ERROR, GCTL_ERROR_CONFIG_PARSE,
		            "Configuration file is empty: %s", path);
		return FALSE;
	}

	root_map = yaml_node_get_mapping(root);
	if (root_map == NULL) {
		g_set_error(error, GCTL_ERROR, GCTL_ERROR_CONFIG_PARSE,
		            "Configuration file root is not a mapping: %s",
		            path);
		return FALSE;
	}

	/* Apply overrides from the parsed YAML -- each helper tolerates
	 * missing keys so partial configs work fine. */
	apply_scalar_overrides(self, root_map);
	apply_forges_override(self, root_map);
	apply_aliases_override(self, root_map);

	/* Record which file we loaded */
	g_free(self->config_path);
	self->config_path = g_strdup(path);

	g_debug("gitctl: loaded configuration from %s", path);

	return TRUE;
}

/**
 * gctl_config_load_default:
 * @self: a #GctlConfig
 * @error: (nullable): return location for a #GError
 *
 * Attempts to load the configuration from the default path
 * ($XDG_CONFIG_HOME/gitctl/config.yaml).  If the file does not
 * exist the built-in defaults are kept and %TRUE is returned.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean
gctl_config_load_default(
	GctlConfig  *self,
	GError     **error
){
	g_autofree gchar *path = NULL;
	const gchar *xdg_config;

	g_return_val_if_fail(GCTL_IS_CONFIG(self), FALSE);

	/*
	 * Build the default config path from XDG_CONFIG_HOME, falling
	 * back to ~/.config if unset.
	 */
	xdg_config = g_get_user_config_dir();
	path = g_build_filename(xdg_config, "gitctl", "config.yaml", NULL);

	if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
		/* No config file -- keep defaults, this is not an error */
		g_debug("gitctl: no config file at %s, using defaults", path);
		return TRUE;
	}

	return gctl_config_load(self, path, error);
}

GctlOutputFormat
gctl_config_get_default_output_format(GctlConfig *self)
{
	g_return_val_if_fail(GCTL_IS_CONFIG(self), GCTL_OUTPUT_FORMAT_TABLE);

	return self->default_output_format;
}

const gchar *
gctl_config_get_default_remote(GctlConfig *self)
{
	g_return_val_if_fail(GCTL_IS_CONFIG(self), "origin");

	return self->default_remote;
}

GctlForgeType
gctl_config_get_default_forge(GctlConfig *self)
{
	g_return_val_if_fail(GCTL_IS_CONFIG(self), GCTL_FORGE_TYPE_UNKNOWN);

	return self->default_forge;
}

GctlForgeType
gctl_config_get_forge_for_host(
	GctlConfig   *self,
	const gchar  *hostname
){
	gpointer value;

	g_return_val_if_fail(GCTL_IS_CONFIG(self), GCTL_FORGE_TYPE_UNKNOWN);
	g_return_val_if_fail(hostname != NULL, GCTL_FORGE_TYPE_UNKNOWN);

	if (g_hash_table_lookup_extended(self->forge_hosts, hostname,
	                                 NULL, &value))
	{
		return (GctlForgeType)GPOINTER_TO_INT(value);
	}

	return GCTL_FORGE_TYPE_UNKNOWN;
}

const gchar *
gctl_config_get_default_host(
	GctlConfig   *self,
	GctlForgeType forge_type
){
	const gchar *env_val;

	g_return_val_if_fail(GCTL_IS_CONFIG(self), NULL);

	/*
	 * Environment variables take highest priority:
	 *   GITCTL_HOST_GITHUB, GITCTL_HOST_GITLAB,
	 *   GITCTL_HOST_FORGEJO, GITCTL_HOST_GITEA
	 */
	switch (forge_type) {
	case GCTL_FORGE_TYPE_GITHUB:
		env_val = g_getenv("GITCTL_HOST_GITHUB");
		break;
	case GCTL_FORGE_TYPE_GITLAB:
		env_val = g_getenv("GITCTL_HOST_GITLAB");
		break;
	case GCTL_FORGE_TYPE_FORGEJO:
		env_val = g_getenv("GITCTL_HOST_FORGEJO");
		break;
	case GCTL_FORGE_TYPE_GITEA:
		env_val = g_getenv("GITCTL_HOST_GITEA");
		break;
	default:
		env_val = NULL;
	}

	if (env_val != NULL && *env_val != '\0')
		return env_val;

	/*
	 * Fall back to the default_hosts table, populated from:
	 *   1. Explicit "default_host" key in the YAML forge config
	 *   2. First entry in the "hosts" array for that forge
	 *   3. Built-in defaults (github.com, gitlab.com, codeberg.org)
	 */
	return (const gchar *)g_hash_table_lookup(
		self->default_hosts, forge_type_key(forge_type));
}

const gchar *
gctl_config_get_ssh_host(
	GctlConfig   *self,
	GctlForgeType forge_type
){
	const gchar *ssh_host;

	g_return_val_if_fail(GCTL_IS_CONFIG(self), NULL);

	/* Check for explicit ssh_host first */
	ssh_host = (const gchar *)g_hash_table_lookup(
		self->ssh_hosts, forge_type_key(forge_type));
	if (ssh_host != NULL)
		return ssh_host;

	/* Fall back to default host (same host for HTTPS and SSH) */
	return gctl_config_get_default_host(self, forge_type);
}

const gchar *
gctl_config_get_cli_path(
	GctlConfig   *self,
	GctlForgeType forge_type
){
	g_return_val_if_fail(GCTL_IS_CONFIG(self), NULL);

	return (const gchar *)g_hash_table_lookup(
		self->cli_paths, forge_type_key(forge_type));
}

/**
 * gctl_config_set_default_remote:
 * @self: a #GctlConfig
 * @remote: the new default remote name
 *
 * Overrides the default git remote name.  The previous value is freed
 * and replaced with a copy of @remote.
 */
void
gctl_config_set_default_remote(
	GctlConfig  *self,
	const gchar *remote
){
	g_return_if_fail(GCTL_IS_CONFIG(self));
	g_return_if_fail(remote != NULL);

	g_free(self->default_remote);
	self->default_remote = g_strdup(remote);
}

/**
 * gctl_config_set_default_output_format:
 * @self: a #GctlConfig
 * @format: the new default output format
 *
 * Overrides the default output format.
 */
void
gctl_config_set_default_output_format(
	GctlConfig       *self,
	GctlOutputFormat  format
){
	g_return_if_fail(GCTL_IS_CONFIG(self));

	self->default_output_format = format;
}

/**
 * gctl_config_set_default_forge:
 * @self: a #GctlConfig
 * @forge_type: the new default forge type
 *
 * Overrides the default forge type.
 */
void
gctl_config_set_default_forge(
	GctlConfig    *self,
	GctlForgeType  forge_type
){
	g_return_if_fail(GCTL_IS_CONFIG(self));

	self->default_forge = forge_type;
}

/**
 * gctl_config_get_config_path:
 * @self: a #GctlConfig
 *
 * Returns the path to the configuration file that was loaded, or
 * %NULL if no file has been loaded yet.
 *
 * Returns: (transfer none) (nullable): the config file path
 */
const gchar *
gctl_config_get_config_path(GctlConfig *self)
{
	g_return_val_if_fail(GCTL_IS_CONFIG(self), NULL);

	return self->config_path;
}

const gchar *
gctl_config_get_alias(
	GctlConfig   *self,
	const gchar  *alias
){
	g_return_val_if_fail(GCTL_IS_CONFIG(self), NULL);
	g_return_val_if_fail(alias != NULL, NULL);

	return (const gchar *)g_hash_table_lookup(self->aliases, alias);
}
