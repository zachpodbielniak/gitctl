/*
 * test-module-manager.c - Tests for GctlModuleManager
 *
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define GCTL_COMPILATION
#include <gitctl.h>

#include <glib/gstdio.h>
#include <stdio.h>

/* test_manager_new: create, verify non-null */
static void
test_manager_new(void)
{
	g_autoptr(GctlModuleManager) mgr = NULL;

	mgr = gctl_module_manager_new();
	g_assert_nonnull(mgr);
	g_assert_true(GCTL_IS_MODULE_MANAGER(mgr));
}

/* test_manager_get_modules_empty: verify empty modules array initially */
static void
test_manager_get_modules_empty(void)
{
	g_autoptr(GctlModuleManager) mgr = NULL;
	GPtrArray *modules;

	mgr = gctl_module_manager_new();

	modules = gctl_module_manager_get_modules(mgr);
	g_assert_nonnull(modules);
	g_assert_cmpuint(modules->len, ==, 0);
}

/*
 * test_manager_load_nonexistent_dir: load from non-existent directory,
 * should return FALSE and set error
 */
static void
test_manager_load_nonexistent_dir(void)
{
	g_autoptr(GctlModuleManager) mgr = NULL;
	g_autoptr(GError) error = NULL;
	gboolean ok;

	mgr = gctl_module_manager_new();

	ok = gctl_module_manager_load_from_directory(
		mgr,
		"/tmp/gitctl-test-nonexistent-module-dir-12345",
		&error
	);

	g_assert_false(ok);
	g_assert_nonnull(error);
}

/*
 * test_manager_load_empty_dir: load from empty temp directory,
 * should succeed with 0 modules
 */
static void
test_manager_load_empty_dir(void)
{
	g_autoptr(GctlModuleManager) mgr = NULL;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *tmpdir = NULL;
	gboolean ok;
	GPtrArray *modules;

	mgr = gctl_module_manager_new();

	/* Create a temporary empty directory */
	tmpdir = g_dir_make_tmp("gitctl-test-empty-XXXXXX", &error);
	g_assert_no_error(error);
	g_assert_nonnull(tmpdir);

	ok = gctl_module_manager_load_from_directory(mgr, tmpdir, &error);
	g_assert_true(ok);
	g_assert_no_error(error);

	modules = gctl_module_manager_get_modules(mgr);
	g_assert_nonnull(modules);
	g_assert_cmpuint(modules->len, ==, 0);

	/* Clean up */
	g_rmdir(tmpdir);
}

/*
 * test_manager_find_forge_none: find_forge when no modules loaded,
 * should return NULL
 */
static void
test_manager_find_forge_none(void)
{
	g_autoptr(GctlModuleManager) mgr = NULL;
	GctlForge *forge;

	mgr = gctl_module_manager_new();

	forge = gctl_module_manager_find_forge(mgr, GCTL_FORGE_TYPE_GITHUB);
	g_assert_null(forge);

	forge = gctl_module_manager_find_forge(mgr, GCTL_FORGE_TYPE_GITLAB);
	g_assert_null(forge);

	forge = gctl_module_manager_find_forge(mgr, GCTL_FORGE_TYPE_FORGEJO);
	g_assert_null(forge);
}

/* ── New tests ────────────────────────────────────────────────────── */

/*
 * test_manager_find_forge_for_url_none: find_forge_for_url when no
 * modules loaded, should return NULL
 */
static void
test_manager_find_forge_for_url_none(void)
{
	g_autoptr(GctlModuleManager) mgr = NULL;
	GctlForge *forge;

	mgr = gctl_module_manager_new();

	forge = gctl_module_manager_find_forge_for_url(
		mgr, "https://github.com/owner/repo.git");
	g_assert_null(forge);

	forge = gctl_module_manager_find_forge_for_url(
		mgr, "git@gitlab.com:owner/repo.git");
	g_assert_null(forge);
}

/*
 * test_manager_activate_deactivate_empty: calling activate_all and
 * deactivate_all on an empty module manager should not crash
 */
static void
test_manager_activate_deactivate_empty(void)
{
	g_autoptr(GctlModuleManager) mgr = NULL;

	mgr = gctl_module_manager_new();

	/* Should not crash with 0 modules */
	gctl_module_manager_activate_all(mgr);
	gctl_module_manager_deactivate_all(mgr);
}

/*
 * test_manager_load_real_modules: attempt to load modules from the
 * build output directory.  This test is skipped if the module dir
 * does not exist (e.g. when modules are not built).
 */
static void
test_manager_load_real_modules(void)
{
	g_autoptr(GctlModuleManager) mgr = NULL;
	g_autoptr(GError) error = NULL;
	GPtrArray *modules;
	gboolean ok;

	mgr = gctl_module_manager_new();

	/* Load from the build's module directory (set via -D at compile time) */
	ok = gctl_module_manager_load_from_directory(
		mgr, GCTL_DEV_MODULE_DIR, &error);

	if (!ok) {
		g_test_skip("Module directory not available or no modules built");
		return;
	}

	modules = gctl_module_manager_get_modules(mgr);
	g_assert_nonnull(modules);

	if (modules->len == 0) {
		g_test_skip("No modules found in build directory");
		return;
	}

	/* At least one module was loaded */
	g_assert_cmpuint(modules->len, >=, 1);

	/* Try to find specific forges */
	{
		GctlForge *github_forge;
		GctlForge *gitlab_forge;

		github_forge = gctl_module_manager_find_forge(
			mgr, GCTL_FORGE_TYPE_GITHUB);
		gitlab_forge = gctl_module_manager_find_forge(
			mgr, GCTL_FORGE_TYPE_GITLAB);

		/* At least one should be found if modules are built */
		g_assert_true(github_forge != NULL || gitlab_forge != NULL);

		if (github_forge != NULL) {
			g_assert_cmpstr(gctl_forge_get_name(github_forge),
			                ==, "GitHub");
			g_assert_cmpstr(gctl_forge_get_cli_tool(github_forge),
			                ==, "gh");
			g_assert_cmpint(gctl_forge_get_forge_type(github_forge),
			                ==, GCTL_FORGE_TYPE_GITHUB);
		}

		if (gitlab_forge != NULL) {
			g_assert_cmpstr(gctl_forge_get_name(gitlab_forge),
			                ==, "GitLab");
			g_assert_cmpstr(gctl_forge_get_cli_tool(gitlab_forge),
			                ==, "glab");
			g_assert_cmpint(gctl_forge_get_forge_type(gitlab_forge),
			                ==, GCTL_FORGE_TYPE_GITLAB);
		}
	}

	/* Activate and deactivate should not crash */
	gctl_module_manager_activate_all(mgr);
	gctl_module_manager_deactivate_all(mgr);
}

/*
 * test_manager_load_dir_with_non_so_files: create a temp directory
 * with non-.so files and verify they are ignored gracefully
 */
static void
test_manager_load_dir_with_non_so_files(void)
{
	g_autoptr(GctlModuleManager) mgr = NULL;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *tmpdir = NULL;
	g_autofree gchar *txtfile = NULL;
	g_autofree gchar *yamlfile = NULL;
	gboolean ok;
	GPtrArray *modules;
	FILE *fp;

	mgr = gctl_module_manager_new();

	tmpdir = g_dir_make_tmp("gitctl-test-nonmod-XXXXXX", &error);
	g_assert_no_error(error);

	/* Create some non-.so files in the directory */
	txtfile = g_build_filename(tmpdir, "readme.txt", NULL);
	fp = fopen(txtfile, "w");
	g_assert_nonnull(fp);
	fprintf(fp, "Not a module\n");
	fclose(fp);

	yamlfile = g_build_filename(tmpdir, "config.yaml", NULL);
	fp = fopen(yamlfile, "w");
	g_assert_nonnull(fp);
	fprintf(fp, "key: value\n");
	fclose(fp);

	ok = gctl_module_manager_load_from_directory(mgr, tmpdir, &error);
	g_assert_true(ok);
	g_assert_no_error(error);

	/* No modules should have been loaded (only .so files are modules) */
	modules = gctl_module_manager_get_modules(mgr);
	g_assert_nonnull(modules);
	g_assert_cmpuint(modules->len, ==, 0);

	/* Cleanup */
	g_unlink(txtfile);
	g_unlink(yamlfile);
	g_rmdir(tmpdir);
}

int
main(
	int     argc,
	char  **argv
){
	g_test_init(&argc, &argv, NULL);

	/* Original tests */
	g_test_add_func("/module-manager/new", test_manager_new);
	g_test_add_func("/module-manager/get-modules-empty", test_manager_get_modules_empty);
	g_test_add_func("/module-manager/load-nonexistent-dir", test_manager_load_nonexistent_dir);
	g_test_add_func("/module-manager/load-empty-dir", test_manager_load_empty_dir);
	g_test_add_func("/module-manager/find-forge-none", test_manager_find_forge_none);

	/* New tests */
	g_test_add_func("/module-manager/find-forge-for-url-none", test_manager_find_forge_for_url_none);
	g_test_add_func("/module-manager/activate-deactivate-empty", test_manager_activate_deactivate_empty);
	g_test_add_func("/module-manager/load-real-modules", test_manager_load_real_modules);
	g_test_add_func("/module-manager/load-dir-with-non-so-files", test_manager_load_dir_with_non_so_files);

	return g_test_run();
}
