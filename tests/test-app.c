/*
 * test-app.c - Tests for GctlApp
 *
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define GCTL_COMPILATION
#include <gitctl.h>

/* test_app_new: create app, verify non-null and type check */
static void
test_app_new(void)
{
	g_autoptr(GctlApp) app = NULL;

	app = gctl_app_new();
	g_assert_nonnull(app);
	g_assert_true(GCTL_IS_APP(app));
}

/* test_app_is_derivable: verify the GctlApp type is derivable */
static void
test_app_is_derivable(void)
{
	GType app_type;

	app_type = GCTL_TYPE_APP;
	g_assert_true(G_TYPE_IS_DERIVABLE(app_type));
}

/* test_app_properties: verify dry-run defaults to FALSE, output-format defaults to TABLE */
static void
test_app_properties(void)
{
	g_autoptr(GctlApp) app = NULL;

	app = gctl_app_new();

	g_assert_false(gctl_app_get_dry_run(app));
	g_assert_cmpint(gctl_app_get_output_format(app), ==, GCTL_OUTPUT_FORMAT_TABLE);
}

/* test_app_dry_run_property: set dry-run, verify it reads back TRUE */
static void
test_app_dry_run_property(void)
{
	g_autoptr(GctlApp) app = NULL;

	app = gctl_app_new();

	g_assert_false(gctl_app_get_dry_run(app));

	gctl_app_set_dry_run(app, TRUE);
	g_assert_true(gctl_app_get_dry_run(app));

	gctl_app_set_dry_run(app, FALSE);
	g_assert_false(gctl_app_get_dry_run(app));
}

/* test_app_output_format_property: set to JSON, verify reads back */
static void
test_app_output_format_property(void)
{
	g_autoptr(GctlApp) app = NULL;

	app = gctl_app_new();

	gctl_app_set_output_format(app, GCTL_OUTPUT_FORMAT_JSON);
	g_assert_cmpint(gctl_app_get_output_format(app), ==, GCTL_OUTPUT_FORMAT_JSON);

	gctl_app_set_output_format(app, GCTL_OUTPUT_FORMAT_YAML);
	g_assert_cmpint(gctl_app_get_output_format(app), ==, GCTL_OUTPUT_FORMAT_YAML);

	gctl_app_set_output_format(app, GCTL_OUTPUT_FORMAT_CSV);
	g_assert_cmpint(gctl_app_get_output_format(app), ==, GCTL_OUTPUT_FORMAT_CSV);

	gctl_app_set_output_format(app, GCTL_OUTPUT_FORMAT_TABLE);
	g_assert_cmpint(gctl_app_get_output_format(app), ==, GCTL_OUTPUT_FORMAT_TABLE);
}

/* test_app_verbose_property: set verbose, verify */
static void
test_app_verbose_property(void)
{
	g_autoptr(GctlApp) app = NULL;

	app = gctl_app_new();

	g_assert_false(gctl_app_get_verbose(app));

	gctl_app_set_verbose(app, TRUE);
	g_assert_true(gctl_app_get_verbose(app));

	gctl_app_set_verbose(app, FALSE);
	g_assert_false(gctl_app_get_verbose(app));
}

/* test_version: check gctl_get_version and gctl_get_version_string */
static void
test_version(void)
{
	guint major, minor, micro;
	const gchar *version_str;

	gctl_get_version(&major, &minor, &micro);

	g_assert_cmpuint(major, ==, GCTL_VERSION_MAJOR);
	g_assert_cmpuint(minor, ==, GCTL_VERSION_MINOR);
	g_assert_cmpuint(micro, ==, GCTL_VERSION_MICRO);

	version_str = gctl_get_version_string();
	g_assert_nonnull(version_str);
	g_assert_cmpstr(version_str, ==, GCTL_VERSION_STRING);

	/* Verify we can pass NULL for any output parameter */
	gctl_get_version(NULL, NULL, NULL);
	gctl_get_version(&major, NULL, NULL);
	gctl_get_version(NULL, &minor, NULL);
	gctl_get_version(NULL, NULL, &micro);
}

int
main(
	int     argc,
	char  **argv
){
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/app/new", test_app_new);
	g_test_add_func("/app/is-derivable", test_app_is_derivable);
	g_test_add_func("/app/properties", test_app_properties);
	g_test_add_func("/app/dry-run-property", test_app_dry_run_property);
	g_test_add_func("/app/output-format-property", test_app_output_format_property);
	g_test_add_func("/app/verbose-property", test_app_verbose_property);
	g_test_add_func("/app/version", test_version);

	return g_test_run();
}
