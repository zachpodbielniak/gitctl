/*
 * test-module-manager.c - Tests for GctlModuleManager
 *
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define GCTL_COMPILATION
#include <gitctl.h>

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

int
main(
	int     argc,
	char  **argv
){
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/module-manager/new", test_manager_new);
	g_test_add_func("/module-manager/get-modules-empty", test_manager_get_modules_empty);
	g_test_add_func("/module-manager/load-nonexistent-dir", test_manager_load_nonexistent_dir);
	g_test_add_func("/module-manager/load-empty-dir", test_manager_load_empty_dir);
	g_test_add_func("/module-manager/find-forge-none", test_manager_find_forge_none);

	return g_test_run();
}
