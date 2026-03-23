/*
 * test-config.c - Tests for GctlConfig
 *
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define GCTL_COMPILATION
#include <gitctl.h>

#include <glib/gstdio.h>
#include <stdio.h>

/* ── Original tests ───────────────────────────────────────────────── */

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

/* ── New tests: YAML config file loading ──────────────────────────── */

/*
 * test_config_load_yaml: create a real YAML config file with all
 * supported keys and verify every override is applied correctly
 */
static void
test_config_load_yaml(void)
{
	g_autoptr(GctlConfig) config = NULL;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *tmpdir = NULL;
	g_autofree gchar *config_path = NULL;
	gboolean ok;
	FILE *fp;

	tmpdir = g_dir_make_tmp("gitctl-config-XXXXXX", &error);
	g_assert_no_error(error);
	g_assert_nonnull(tmpdir);

	config_path = g_build_filename(tmpdir, "config.yaml", NULL);

	fp = fopen(config_path, "w");
	g_assert_nonnull(fp);
	fprintf(fp,
		"output: json\n"
		"remote: upstream\n"
		"default_forge: gitlab\n"
		"\n"
		"forges:\n"
		"  forgejo:\n"
		"    default_host: git.example.com\n"
		"    hosts:\n"
		"      - git.example.com\n"
		"      - codeberg.org\n"
		"    cli: fj\n"
		"\n"
		"aliases:\n"
		"  prl: \"pr list\"\n"
		"  ic: \"issue create\"\n"
	);
	fclose(fp);

	config = gctl_config_new();
	ok = gctl_config_load(config, config_path, &error);

	g_assert_true(ok);
	g_assert_no_error(error);

	/* Verify scalar overrides */
	g_assert_cmpint(gctl_config_get_default_output_format(config),
	                ==, GCTL_OUTPUT_FORMAT_JSON);
	g_assert_cmpstr(gctl_config_get_default_remote(config),
	                ==, "upstream");
	g_assert_cmpint(gctl_config_get_default_forge(config),
	                ==, GCTL_FORGE_TYPE_GITLAB);

	/* Verify forge host mapping */
	g_assert_cmpint(gctl_config_get_forge_for_host(config, "git.example.com"),
	                ==, GCTL_FORGE_TYPE_FORGEJO);
	g_assert_cmpint(gctl_config_get_forge_for_host(config, "codeberg.org"),
	                ==, GCTL_FORGE_TYPE_FORGEJO);

	/* Verify explicit default_host */
	g_assert_cmpstr(gctl_config_get_default_host(config, GCTL_FORGE_TYPE_FORGEJO),
	                ==, "git.example.com");

	/* Verify aliases */
	g_assert_cmpstr(gctl_config_get_alias(config, "prl"), ==, "pr list");
	g_assert_cmpstr(gctl_config_get_alias(config, "ic"), ==, "issue create");
	g_assert_null(gctl_config_get_alias(config, "nonexistent"));

	/* Verify config path was recorded */
	g_assert_cmpstr(gctl_config_get_config_path(config), ==, config_path);

	/* Cleanup */
	g_unlink(config_path);
	g_rmdir(tmpdir);
}

/*
 * test_config_load_empty_yaml: load a file that exists but contains
 * only whitespace or an empty document.  Defaults should remain.
 *
 * Note: depending on yaml-glib behaviour, an empty file may fail to
 * parse or return an empty root node.  We verify the function does
 * not crash and returns a consistent result.
 */
static void
test_config_load_empty_yaml(void)
{
	g_autoptr(GctlConfig) config = NULL;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *tmpdir = NULL;
	g_autofree gchar *config_path = NULL;
	gboolean ok;
	FILE *fp;

	tmpdir = g_dir_make_tmp("gitctl-config-XXXXXX", &error);
	g_assert_no_error(error);

	config_path = g_build_filename(tmpdir, "empty.yaml", NULL);

	fp = fopen(config_path, "w");
	g_assert_nonnull(fp);
	/* Write an empty YAML document */
	fprintf(fp, "---\n");
	fclose(fp);

	config = gctl_config_new();
	ok = gctl_config_load(config, config_path, &error);

	/*
	 * An empty YAML doc may or may not be valid depending on
	 * yaml-glib.  Either outcome is acceptable as long as defaults
	 * are preserved when ok is TRUE.
	 */
	if (ok) {
		g_assert_no_error(error);
		g_assert_cmpint(gctl_config_get_default_output_format(config),
		                ==, GCTL_OUTPUT_FORMAT_TABLE);
		g_assert_cmpstr(gctl_config_get_default_remote(config),
		                ==, "origin");
	} else {
		g_assert_nonnull(error);
	}

	/* Cleanup */
	g_unlink(config_path);
	g_rmdir(tmpdir);
}

/*
 * test_config_load_partial_yaml: load a config that sets only some
 * keys.  Unset keys should retain their defaults.
 */
static void
test_config_load_partial_yaml(void)
{
	g_autoptr(GctlConfig) config = NULL;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *tmpdir = NULL;
	g_autofree gchar *config_path = NULL;
	gboolean ok;
	FILE *fp;

	tmpdir = g_dir_make_tmp("gitctl-config-XXXXXX", &error);
	g_assert_no_error(error);

	config_path = g_build_filename(tmpdir, "partial.yaml", NULL);

	fp = fopen(config_path, "w");
	g_assert_nonnull(fp);
	/* Only override output format; leave remote and forge as defaults */
	fprintf(fp, "output: csv\n");
	fclose(fp);

	config = gctl_config_new();
	ok = gctl_config_load(config, config_path, &error);

	g_assert_true(ok);
	g_assert_no_error(error);

	/* output should be overridden to CSV */
	g_assert_cmpint(gctl_config_get_default_output_format(config),
	                ==, GCTL_OUTPUT_FORMAT_CSV);

	/* remote and forge should retain defaults */
	g_assert_cmpstr(gctl_config_get_default_remote(config),
	                ==, "origin");
	g_assert_cmpint(gctl_config_get_default_forge(config),
	                ==, GCTL_FORGE_TYPE_GITHUB);

	/* Default hosts should still be intact */
	g_assert_cmpstr(gctl_config_get_default_host(config, GCTL_FORGE_TYPE_GITHUB),
	                ==, "github.com");

	/* Cleanup */
	g_unlink(config_path);
	g_rmdir(tmpdir);
}

/*
 * test_config_default_host_env_override: set GITCTL_HOST_FORGEJO
 * env var and verify it overrides the config file value
 */
static void
test_config_default_host_env_override(void)
{
	g_autoptr(GctlConfig) config = NULL;
	const gchar *host;

	config = gctl_config_new();

	/* Set environment override */
	g_setenv("GITCTL_HOST_FORGEJO", "custom-forgejo.example.com", TRUE);

	host = gctl_config_get_default_host(config, GCTL_FORGE_TYPE_FORGEJO);
	g_assert_cmpstr(host, ==, "custom-forgejo.example.com");

	/* Clear the env var and verify it falls back to config default */
	g_unsetenv("GITCTL_HOST_FORGEJO");

	host = gctl_config_get_default_host(config, GCTL_FORGE_TYPE_FORGEJO);
	g_assert_cmpstr(host, ==, "codeberg.org");
}

/*
 * test_config_env_override_github: set GITCTL_HOST_GITHUB
 * env var and verify it overrides the default
 */
static void
test_config_env_override_github(void)
{
	g_autoptr(GctlConfig) config = NULL;
	const gchar *host;

	config = gctl_config_new();

	g_setenv("GITCTL_HOST_GITHUB", "github.example.corp", TRUE);

	host = gctl_config_get_default_host(config, GCTL_FORGE_TYPE_GITHUB);
	g_assert_cmpstr(host, ==, "github.example.corp");

	g_unsetenv("GITCTL_HOST_GITHUB");

	host = gctl_config_get_default_host(config, GCTL_FORGE_TYPE_GITHUB);
	g_assert_cmpstr(host, ==, "github.com");
}

/*
 * test_config_load_yaml_cli_override: verify that a forge's "cli"
 * key in the YAML config overrides the default CLI path
 */
static void
test_config_load_yaml_cli_override(void)
{
	g_autoptr(GctlConfig) config = NULL;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *tmpdir = NULL;
	g_autofree gchar *config_path = NULL;
	gboolean ok;
	FILE *fp;

	tmpdir = g_dir_make_tmp("gitctl-config-XXXXXX", &error);
	g_assert_no_error(error);

	config_path = g_build_filename(tmpdir, "cli-override.yaml", NULL);

	fp = fopen(config_path, "w");
	g_assert_nonnull(fp);
	fprintf(fp,
		"forges:\n"
		"  github:\n"
		"    cli: /usr/local/bin/gh-custom\n"
		"  gitlab:\n"
		"    cli: /opt/glab\n"
	);
	fclose(fp);

	config = gctl_config_new();
	ok = gctl_config_load(config, config_path, &error);

	g_assert_true(ok);
	g_assert_no_error(error);

	g_assert_cmpstr(gctl_config_get_cli_path(config, GCTL_FORGE_TYPE_GITHUB),
	                ==, "/usr/local/bin/gh-custom");
	g_assert_cmpstr(gctl_config_get_cli_path(config, GCTL_FORGE_TYPE_GITLAB),
	                ==, "/opt/glab");

	/* Forgejo should remain as default since it was not overridden */
	g_assert_cmpstr(gctl_config_get_cli_path(config, GCTL_FORGE_TYPE_FORGEJO),
	                ==, "fj");

	/* Cleanup */
	g_unlink(config_path);
	g_rmdir(tmpdir);
}

int
main(
	int     argc,
	char  **argv
){
	g_test_init(&argc, &argv, NULL);

	/* Original tests */
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

	/* New YAML loading tests */
	g_test_add_func("/config/load-yaml", test_config_load_yaml);
	g_test_add_func("/config/load-empty-yaml", test_config_load_empty_yaml);
	g_test_add_func("/config/load-partial-yaml", test_config_load_partial_yaml);
	g_test_add_func("/config/default-host-env-override", test_config_default_host_env_override);
	g_test_add_func("/config/env-override-github", test_config_env_override_github);
	g_test_add_func("/config/load-yaml-cli-override", test_config_load_yaml_cli_override);

	return g_test_run();
}
