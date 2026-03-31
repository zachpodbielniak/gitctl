/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-config.h - Configuration loader and accessor */

#ifndef GCTL_CONFIG_H
#define GCTL_CONFIG_H

#if !defined(GCTL_INSIDE) && !defined(GCTL_COMPILATION)
#error "Only <gitctl.h> can be included directly."
#endif

#include <glib-object.h>
#include "gitctl-enums.h"
#include "gitctl-types.h"

G_BEGIN_DECLS

#define GCTL_TYPE_CONFIG (gctl_config_get_type())

G_DECLARE_FINAL_TYPE(GctlConfig, gctl_config, GCTL, CONFIG, GObject)

/**
 * gctl_config_new:
 *
 * Creates a new #GctlConfig populated with built-in defaults.
 *
 * Default values:
 * - output format: %GCTL_OUTPUT_FORMAT_TABLE
 * - default remote: "origin"
 * - default forge: %GCTL_FORGE_TYPE_GITHUB
 * - forge hosts: github.com, gitlab.com, codeberg.org
 * - CLI paths: gh, glab, fj, tea
 *
 * Returns: (transfer full): a newly created #GctlConfig
 */
GctlConfig *
gctl_config_new(void);

/**
 * gctl_config_load:
 * @self: a #GctlConfig
 * @path: the path to a YAML configuration file
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Loads configuration from the file at @path.  Currently YAML parsing
 * is not yet implemented; this method logs a warning and returns %TRUE
 * with built-in defaults unchanged.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean
gctl_config_load(
	GctlConfig   *self,
	const gchar  *path,
	GError      **error
);

/**
 * gctl_config_load_default:
 * @self: a #GctlConfig
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Attempts to load the default configuration file from
 * `$XDG_CONFIG_HOME/gitctl/config.yaml` (falling back to
 * `~/.config/gitctl/config.yaml`).  If the file does not exist,
 * the built-in defaults are kept and %TRUE is returned.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean
gctl_config_load_default(
	GctlConfig  *self,
	GError     **error
);

/**
 * gctl_config_get_default_output_format:
 * @self: a #GctlConfig
 *
 * Retrieves the default output format from the configuration.
 *
 * Returns: the #GctlOutputFormat
 */
GctlOutputFormat
gctl_config_get_default_output_format(GctlConfig *self);

/**
 * gctl_config_get_default_remote:
 * @self: a #GctlConfig
 *
 * Retrieves the default git remote name from the configuration.
 *
 * Returns: (transfer none): the default remote name
 */
const gchar *
gctl_config_get_default_remote(GctlConfig *self);

/**
 * gctl_config_get_default_forge:
 * @self: a #GctlConfig
 *
 * Retrieves the default forge type from the configuration.
 *
 * Returns: the #GctlForgeType
 */
GctlForgeType
gctl_config_get_default_forge(GctlConfig *self);

/**
 * gctl_config_get_default_branch:
 * @self: a #GctlConfig
 *
 * Returns the default branch name for new repositories (e.g. "master").
 * If not configured, returns %NULL (the forge's own default is used).
 *
 * Returns: (transfer none) (nullable): the branch name, or %NULL
 */
const gchar *
gctl_config_get_default_branch(GctlConfig *self);

/**
 * gctl_config_get_forge_for_host:
 * @self: a #GctlConfig
 * @hostname: the hostname to look up
 *
 * Looks up the forge type associated with @hostname in the
 * configuration's forge-host mapping table.
 *
 * Returns: the #GctlForgeType, or %GCTL_FORGE_TYPE_UNKNOWN if
 *     @hostname is not registered
 */
GctlForgeType
gctl_config_get_forge_for_host(
	GctlConfig   *self,
	const gchar  *hostname
);

/**
 * gctl_config_get_default_host:
 * @self: a #GctlConfig
 * @forge_type: the #GctlForgeType to look up
 *
 * Returns the first (primary) hostname configured for @forge_type.
 * Useful for API calls when no git remote context is available.
 *
 * Returns: (transfer none) (nullable): the hostname, or %NULL
 */
const gchar *
gctl_config_get_default_host(
	GctlConfig   *self,
	GctlForgeType forge_type
);

/**
 * gctl_config_get_ssh_host:
 * @self: a #GctlConfig
 * @forge_type: the #GctlForgeType to look up
 *
 * Returns the SSH hostname for @forge_type, if different from the
 * default (HTTPS) host.  Falls back to the default host if no
 * explicit ssh_host is configured.
 *
 * Returns: (transfer none) (nullable): the SSH hostname
 */
const gchar *
gctl_config_get_ssh_host(
	GctlConfig   *self,
	GctlForgeType forge_type
);

/**
 * gctl_config_get_cli_path:
 * @self: a #GctlConfig
 * @forge_type: the #GctlForgeType to look up
 *
 * Returns the CLI tool path (or binary name) configured for
 * @forge_type.
 *
 * Returns: (transfer none) (nullable): the CLI tool path, or %NULL
 *     if no path is configured for @forge_type
 */
const gchar *
gctl_config_get_cli_path(
	GctlConfig   *self,
	GctlForgeType forge_type
);

/**
 * gctl_config_set_default_remote:
 * @self: a #GctlConfig
 * @remote: the new default remote name
 *
 * Overrides the default git remote name stored in the configuration.
 * This is typically called when the user passes --remote on the
 * command line or sets the GITCTL_REMOTE environment variable.
 */
void
gctl_config_set_default_remote(
	GctlConfig   *self,
	const gchar  *remote
);

/**
 * gctl_config_set_default_output_format:
 * @self: a #GctlConfig
 * @format: the new default output format
 *
 * Overrides the default output format stored in the configuration.
 */
void
gctl_config_set_default_output_format(
	GctlConfig       *self,
	GctlOutputFormat  format
);

/**
 * gctl_config_set_default_forge:
 * @self: a #GctlConfig
 * @forge_type: the new default forge type
 *
 * Overrides the default forge type stored in the configuration.
 */
void
gctl_config_set_default_forge(
	GctlConfig    *self,
	GctlForgeType  forge_type
);

/**
 * gctl_config_get_config_path:
 * @self: a #GctlConfig
 *
 * Returns the path to the configuration file that was loaded, or
 * %NULL if no file has been loaded yet.
 *
 * Returns: (transfer none) (nullable): the configuration file path
 */
const gchar *
gctl_config_get_config_path(GctlConfig *self);

/**
 * gctl_config_get_alias:
 * @self: a #GctlConfig
 * @alias: the alias name to look up
 *
 * Looks up a command alias in the configuration.  Aliases map a
 * short name to an expanded command string.
 *
 * Returns: (transfer none) (nullable): the expanded command string,
 *     or %NULL if @alias is not defined
 */
const gchar *
gctl_config_get_alias(
	GctlConfig   *self,
	const gchar  *alias
);

G_END_DECLS

#endif /* GCTL_CONFIG_H */
