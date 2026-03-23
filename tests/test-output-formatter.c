/*
 * test-output-formatter.c - Tests for GctlOutputFormatter
 *
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define GCTL_COMPILATION
#include <gitctl.h>

#include <string.h>

/* test_formatter_new: create, verify type */
static void
test_formatter_new(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_TABLE);
	g_assert_nonnull(fmt);
	g_assert_true(GCTL_IS_OUTPUT_FORMATTER(fmt));
	g_assert_cmpint(gctl_output_formatter_get_format(fmt),
	                ==, GCTL_OUTPUT_FORMAT_TABLE);
}

/* test_formatter_set_format: set JSON, verify get */
static void
test_formatter_set_format(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_TABLE);

	gctl_output_formatter_set_format(fmt, GCTL_OUTPUT_FORMAT_JSON);
	g_assert_cmpint(gctl_output_formatter_get_format(fmt),
	                ==, GCTL_OUTPUT_FORMAT_JSON);

	gctl_output_formatter_set_format(fmt, GCTL_OUTPUT_FORMAT_YAML);
	g_assert_cmpint(gctl_output_formatter_get_format(fmt),
	                ==, GCTL_OUTPUT_FORMAT_YAML);

	gctl_output_formatter_set_format(fmt, GCTL_OUTPUT_FORMAT_CSV);
	g_assert_cmpint(gctl_output_formatter_get_format(fmt),
	                ==, GCTL_OUTPUT_FORMAT_CSV);
}

/* test_formatter_format_empty: format empty array, verify "(no results)" */
static void
test_formatter_format_empty(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;
	g_autoptr(GPtrArray) resources = NULL;
	g_autofree gchar *output = NULL;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_TABLE);
	resources = g_ptr_array_new();

	output = gctl_output_formatter_format_resources(fmt, resources);
	g_assert_nonnull(output);
	g_assert_true(strstr(output, "(no results)") != NULL);
}

/*
 * test_formatter_format_pr_table: create PR resources, format as table,
 * verify column headers present
 */
static void
test_formatter_format_pr_table(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;
	g_autoptr(GPtrArray) resources = NULL;
	g_autofree gchar *output = NULL;
	GctlResource *pr;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_TABLE);
	resources = g_ptr_array_new_with_free_func(
		(GDestroyNotify)gctl_resource_free);

	pr = gctl_resource_new(GCTL_RESOURCE_KIND_PR);
	gctl_resource_set_number(pr, 42);
	gctl_resource_set_title(pr, "Add feature X");
	gctl_resource_set_state(pr, "open");
	gctl_resource_set_author(pr, "zach");
	g_ptr_array_add(resources, pr);

	output = gctl_output_formatter_format_resources(fmt, resources);
	g_assert_nonnull(output);

	/* Verify column headers are present */
	g_assert_true(strstr(output, "#") != NULL);
	g_assert_true(strstr(output, "TITLE") != NULL);
	g_assert_true(strstr(output, "STATE") != NULL);
	g_assert_true(strstr(output, "AUTHOR") != NULL);

	/* Verify data is present */
	g_assert_true(strstr(output, "#42") != NULL);
	g_assert_true(strstr(output, "Add feature X") != NULL);
	g_assert_true(strstr(output, "open") != NULL);
	g_assert_true(strstr(output, "zach") != NULL);
}

/*
 * test_formatter_format_pr_json: format PR resources as JSON, verify
 * valid JSON structure with correct fields
 */
static void
test_formatter_format_pr_json(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;
	g_autoptr(GPtrArray) resources = NULL;
	g_autofree gchar *output = NULL;
	GctlResource *pr;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_JSON);
	resources = g_ptr_array_new_with_free_func(
		(GDestroyNotify)gctl_resource_free);

	pr = gctl_resource_new(GCTL_RESOURCE_KIND_PR);
	gctl_resource_set_number(pr, 7);
	gctl_resource_set_title(pr, "Fix bug");
	gctl_resource_set_state(pr, "closed");
	gctl_resource_set_author(pr, "dev");
	g_ptr_array_add(resources, pr);

	output = gctl_output_formatter_format_resources(fmt, resources);
	g_assert_nonnull(output);

	/* Verify JSON contains expected fields */
	g_assert_true(strstr(output, "\"kind\"") != NULL);
	g_assert_true(strstr(output, "\"pr\"") != NULL);
	g_assert_true(strstr(output, "\"number\"") != NULL);
	g_assert_true(strstr(output, "\"title\"") != NULL);
	g_assert_true(strstr(output, "\"Fix bug\"") != NULL);
	g_assert_true(strstr(output, "\"state\"") != NULL);
	g_assert_true(strstr(output, "\"closed\"") != NULL);
	g_assert_true(strstr(output, "\"author\"") != NULL);
	g_assert_true(strstr(output, "\"dev\"") != NULL);

	/* Verify it starts with '[' (JSON array) */
	g_assert_true(output[0] == '[');
}

/*
 * test_formatter_format_repo: format repo resources, verify
 * NAME/DESCRIPTION/VISIBILITY columns
 */
static void
test_formatter_format_repo(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;
	g_autoptr(GPtrArray) resources = NULL;
	g_autofree gchar *output = NULL;
	GctlResource *repo;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_TABLE);
	resources = g_ptr_array_new_with_free_func(
		(GDestroyNotify)gctl_resource_free);

	repo = gctl_resource_new(GCTL_RESOURCE_KIND_REPO);
	gctl_resource_set_title(repo, "gitctl");
	gctl_resource_set_description(repo, "A CLI tool");
	gctl_resource_set_state(repo, "public");
	g_ptr_array_add(resources, repo);

	output = gctl_output_formatter_format_resources(fmt, resources);
	g_assert_nonnull(output);

	/* Verify repo-specific column headers */
	g_assert_true(strstr(output, "NAME") != NULL);
	g_assert_true(strstr(output, "DESCRIPTION") != NULL);
	g_assert_true(strstr(output, "VISIBILITY") != NULL);

	/* Verify data */
	g_assert_true(strstr(output, "gitctl") != NULL);
	g_assert_true(strstr(output, "A CLI tool") != NULL);
	g_assert_true(strstr(output, "public") != NULL);
}

/* test_formatter_format_single: format a single resource */
static void
test_formatter_format_single(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;
	g_autoptr(GctlResource) issue = NULL;
	g_autofree gchar *output = NULL;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_TABLE);

	issue = gctl_resource_new(GCTL_RESOURCE_KIND_ISSUE);
	gctl_resource_set_number(issue, 99);
	gctl_resource_set_title(issue, "Crash on exit");
	gctl_resource_set_state(issue, "open");
	gctl_resource_set_author(issue, "reporter");

	output = gctl_output_formatter_format_resource(fmt, issue);
	g_assert_nonnull(output);

	g_assert_true(strstr(output, "#99") != NULL);
	g_assert_true(strstr(output, "Crash on exit") != NULL);
}

/*
 * test_formatter_null_fields: format resource with NULL title/author,
 * verify no crash
 */
static void
test_formatter_null_fields(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;
	g_autoptr(GPtrArray) resources = NULL;
	g_autofree gchar *output = NULL;
	GctlResource *pr;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_TABLE);
	resources = g_ptr_array_new_with_free_func(
		(GDestroyNotify)gctl_resource_free);

	pr = gctl_resource_new(GCTL_RESOURCE_KIND_PR);
	/* Leave title, state, author as NULL */
	gctl_resource_set_number(pr, 1);
	g_ptr_array_add(resources, pr);

	/* Should not crash */
	output = gctl_output_formatter_format_resources(fmt, resources);
	g_assert_nonnull(output);
}

int
main(
	int     argc,
	char  **argv
){
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/output-formatter/new", test_formatter_new);
	g_test_add_func("/output-formatter/set-format", test_formatter_set_format);
	g_test_add_func("/output-formatter/format-empty", test_formatter_format_empty);
	g_test_add_func("/output-formatter/format-pr-table", test_formatter_format_pr_table);
	g_test_add_func("/output-formatter/format-pr-json", test_formatter_format_pr_json);
	g_test_add_func("/output-formatter/format-repo", test_formatter_format_repo);
	g_test_add_func("/output-formatter/format-single", test_formatter_format_single);
	g_test_add_func("/output-formatter/null-fields", test_formatter_null_fields);

	return g_test_run();
}
