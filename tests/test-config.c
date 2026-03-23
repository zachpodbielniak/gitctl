/*
 * test-config.c - Tests for GctlConfig
 *
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define GCTL_COMPILATION
#include <gitctl.h>

/* test_config_new: create, verify non-null and correct type */
static void
test_config_new(void)
{
	g_autoptr(GctlConfig) config = NULL;

	config = gctl_config_new();
	g_assert_nonnull(config);
	g_assert_true(GCTL_IS_CONFIG(config));
}

/* test_config_defaults: verify default output format, remote, and forge */
static void
test_config_defaults(void)
{
	g_autoptr(GctlConfig) config = NULL;

	config = gctl_config_new();

	g_assert_cmpint(gctl_config_get_default_output_format(config),
	                ==, GCTL_OUTPUT_FORMAT_TABLE);
	g_assert_cmpstr(gctl_config_get_default_remote(config),
	                ==, "origin");
	g_assert_cmpint(gctl_config_get_default_forge(config),
	                ==, GCTL_FORGE_TYPE_GITHUB);
}

/* test_config_default_hosts: verify github.com, gitlab.com, codeberg.org */
static void
test_config_default_hosts(void)
{
	g_autoptr(GctlConfig) config = NULL;
	const gchar *gh_host;
	const gchar *gl_host;
	const gchar *fj_host;

	config = gctl_config_new();

	gh_host = gctl_config_get_default_host(config, GCTL_FORGE_TYPE_GITHUB);
	gl_host = gctl_config_get_default_host(config, GCTL_FORGE_TYPE_GITLAB);
	fj_host = gctl_config_get_default_host(config, GCTL_FORGE_TYPE_FORGEJO);

	g_assert_cmpstr(gh_host, ==, "github.com");
	g_assert_cmpstr(gl_host, ==, "gitlab.com");
	g_assert_cmpstr(fj_host, ==, "codeberg.org");
}

/* test_config_cli_paths: verify gh, glab, fj, tea defaults */
static void
test_config_cli_paths(void)
{
	g_autoptr(GctlConfig) config = NULL;

	config = gctl_config_new();

	g_assert_cmpstr(gctl_config_get_cli_path(config, GCTL_FORGE_TYPE_GITHUB),
	                ==, "gh");
	g_assert_cmpstr(gctl_config_get_cli_path(config, GCTL_FORGE_TYPE_GITLAB),
	                ==, "glab");
	g_assert_cmpstr(gctl_config_get_cli_path(config, GCTL_FORGE_TYPE_FORGEJO),
	                ==, "fj");
	g_assert_cmpstr(gctl_config_get_cli_path(config, GCTL_FORGE_TYPE_GITEA),
	                ==, "tea");
}

/*
 * test_config_forge_for_host: verify hostname-to-forge mapping
 * github.com->GITHUB, gitlab.com->GITLAB, codeberg.org->FORGEJO,
 * unknown.com->UNKNOWN
 */
static void
test_config_forge_for_host(void)
{
	g_autoptr(GctlConfig) config = NULL;

	config = gctl_config_new();

	g_assert_cmpint(gctl_config_get_forge_for_host(config, "github.com"),
	                ==, GCTL_FORGE_TYPE_GITHUB);
	g_assert_cmpint(gctl_config_get_forge_for_host(config, "gitlab.com"),
	                ==, GCTL_FORGE_TYPE_GITLAB);
	g_assert_cmpint(gctl_config_get_forge_for_host(config, "codeberg.org"),
	                ==, GCTL_FORGE_TYPE_FORGEJO);
	g_assert_cmpint(gctl_config_get_forge_for_host(config, "unknown.com"),
	                ==, GCTL_FORGE_TYPE_UNKNOWN);
}

/* test_config_set_default_remote: set and verify */
static void
test_config_set_default_remote(void)
{
	g_autoptr(GctlConfig) config = NULL;

	config = gctl_config_new();

	gctl_config_set_default_remote(config, "upstream");
	g_assert_cmpstr(gctl_config_get_default_remote(config),
	                ==, "upstream");

	/* Set again to verify overwrite works */
	gctl_config_set_default_remote(config, "fork");
	g_assert_cmpstr(gctl_config_get_default_remote(config),
	                ==, "fork");
}

/* test_config_set_default_output_format: set and verify */
static void
test_config_set_default_output_format(void)
{
	g_autoptr(GctlConfig) config = NULL;

	config = gctl_config_new();

	gctl_config_set_default_output_format(config, GCTL_OUTPUT_FORMAT_JSON);
	g_assert_cmpint(gctl_config_get_default_output_format(config),
	                ==, GCTL_OUTPUT_FORMAT_JSON);

	gctl_config_set_default_output_format(config, GCTL_OUTPUT_FORMAT_YAML);
	g_assert_cmpint(gctl_config_get_default_output_format(config),
	                ==, GCTL_OUTPUT_FORMAT_YAML);

	gctl_config_set_default_output_format(config, GCTL_OUTPUT_FORMAT_CSV);
	g_assert_cmpint(gctl_config_get_default_output_format(config),
	                ==, GCTL_OUTPUT_FORMAT_CSV);

	gctl_config_set_default_output_format(config, GCTL_OUTPUT_FORMAT_TABLE);
	g_assert_cmpint(gctl_config_get_default_output_format(config),
	                ==, GCTL_OUTPUT_FORMAT_TABLE);
}

/* test_config_set_default_forge: set and verify */
static void
test_config_set_default_forge(void)
{
	g_autoptr(GctlConfig) config = NULL;

	config = gctl_config_new();

	gctl_config_set_default_forge(config, GCTL_FORGE_TYPE_GITLAB);
	g_assert_cmpint(gctl_config_get_default_forge(config),
	                ==, GCTL_FORGE_TYPE_GITLAB);

	gctl_config_set_default_forge(config, GCTL_FORGE_TYPE_FORGEJO);
	g_assert_cmpint(gctl_config_get_default_forge(config),
	                ==, GCTL_FORGE_TYPE_FORGEJO);
}

/* test_config_get_alias_null: verify NULL for non-existent alias */
static void
test_config_get_alias_null(void)
{
	g_autoptr(GctlConfig) config = NULL;

	config = gctl_config_new();

	g_assert_null(gctl_config_get_alias(config, "nonexistent"));
	g_assert_null(gctl_config_get_alias(config, "pr-list"));
}

/*
 * test_config_load_nonexistent: load from a non-existent path,
 * should return FALSE and set error
 */
static void
test_config_load_nonexistent(void)
{
	g_autoptr(GctlConfig) config = NULL;
	g_autoptr(GError) error = NULL;
	gboolean ok;

	config = gctl_config_new();

	ok = gctl_config_load(config,
	                      "/tmp/gitctl-test-nonexistent-config-12345.yaml",
	                      &error);

	g_assert_false(ok);
	g_assert_nonnull(error);
	g_assert_cmpint(error->domain, ==, GCTL_ERROR);
	g_assert_cmpint(error->code, ==, GCTL_ERROR_CONFIG_PARSE);
}

/*
 * test_config_load_default_no_file: load_default when no config file
 * exists, should succeed with defaults intact
 */
static void
test_config_load_default_no_file(void)
{
	g_autoptr(GctlConfig) config = NULL;
	g_autoptr(GError) error = NULL;
	gboolean ok;

	config = gctl_config_new();

	/*
	 * Unless the user has a gitctl config.yaml in their XDG dir,
	 * this should return TRUE with defaults intact.  We verify
	 * defaults are still valid after the call.
	 */
	ok = gctl_config_load_default(config, &error);

	g_assert_true(ok);
	g_assert_no_error(error);

	/* Defaults should still be intact */
	g_assert_cmpint(gctl_config_get_default_output_format(config),
	                ==, GCTL_OUTPUT_FORMAT_TABLE);
	g_assert_cmpstr(gctl_config_get_default_remote(config),
	                ==, "origin");
}

int
main(
	int     argc,
	char  **argv
){
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/config/new", test_config_new);
	g_test_add_func("/config/defaults", test_config_defaults);
	g_test_add_func("/config/default-hosts", test_config_default_hosts);
	g_test_add_func("/config/cli-paths", test_config_cli_paths);
	g_test_add_func("/config/forge-for-host", test_config_forge_for_host);
	g_test_add_func("/config/set-default-remote", test_config_set_default_remote);
	g_test_add_func("/config/set-default-output-format", test_config_set_default_output_format);
	g_test_add_func("/config/set-default-forge", test_config_set_default_forge);
	g_test_add_func("/config/get-alias-null", test_config_get_alias_null);
	g_test_add_func("/config/load-nonexistent", test_config_load_nonexistent);
	g_test_add_func("/config/load-default-no-file", test_config_load_default_no_file);

	return g_test_run();
}
