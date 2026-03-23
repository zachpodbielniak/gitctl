/*
 * test-output-formatter.c - Tests for GctlOutputFormatter
 *
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define GCTL_COMPILATION
#include <gitctl.h>

#include <string.h>

/* ── Original tests ───────────────────────────────────────────────── */

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

/* ── New tests: CSV format ────────────────────────────────────────── */

/*
 * test_formatter_format_csv: format PR resources as CSV, verify
 * header row and data row are present
 */
static void
test_formatter_format_csv(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;
	g_autoptr(GPtrArray) resources = NULL;
	g_autofree gchar *output = NULL;
	GctlResource *pr;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_CSV);
	resources = g_ptr_array_new_with_free_func(
		(GDestroyNotify)gctl_resource_free);

	pr = gctl_resource_new(GCTL_RESOURCE_KIND_PR);
	gctl_resource_set_number(pr, 42);
	gctl_resource_set_title(pr, "Fix bug");
	gctl_resource_set_state(pr, "open");
	gctl_resource_set_author(pr, "testuser");
	g_ptr_array_add(resources, pr);

	output = gctl_output_formatter_format_resources(fmt, resources);
	g_assert_nonnull(output);

	/* CSV should have header row with column names */
	g_assert_true(strstr(output, "#") != NULL);
	g_assert_true(strstr(output, "TITLE") != NULL);
	g_assert_true(strstr(output, "STATE") != NULL);
	g_assert_true(strstr(output, "AUTHOR") != NULL);

	/* Verify data is present */
	g_assert_true(strstr(output, "Fix bug") != NULL);
	g_assert_true(strstr(output, "#42") != NULL);
	g_assert_true(strstr(output, "testuser") != NULL);
	g_assert_true(strstr(output, "open") != NULL);
}

/*
 * test_formatter_csv_escaping: verify CSV fields containing commas,
 * double quotes, and newlines are properly quoted
 */
static void
test_formatter_csv_escaping(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;
	g_autoptr(GPtrArray) resources = NULL;
	g_autofree gchar *output = NULL;
	GctlResource *pr;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_CSV);
	resources = g_ptr_array_new_with_free_func(
		(GDestroyNotify)gctl_resource_free);

	pr = gctl_resource_new(GCTL_RESOURCE_KIND_PR);
	gctl_resource_set_number(pr, 10);
	/* Title contains a comma -- should be quoted in CSV */
	gctl_resource_set_title(pr, "Fix bug, improve perf");
	gctl_resource_set_state(pr, "open");
	gctl_resource_set_author(pr, "dev");
	g_ptr_array_add(resources, pr);

	output = gctl_output_formatter_format_resources(fmt, resources);
	g_assert_nonnull(output);

	/* The title should be double-quoted because it contains a comma */
	g_assert_true(strstr(output, "\"Fix bug, improve perf\"") != NULL);
}

/*
 * test_formatter_csv_quotes_in_field: verify double quotes within
 * a field are escaped by doubling them
 */
static void
test_formatter_csv_quotes_in_field(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;
	g_autoptr(GPtrArray) resources = NULL;
	g_autofree gchar *output = NULL;
	GctlResource *pr;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_CSV);
	resources = g_ptr_array_new_with_free_func(
		(GDestroyNotify)gctl_resource_free);

	pr = gctl_resource_new(GCTL_RESOURCE_KIND_PR);
	gctl_resource_set_number(pr, 11);
	/* Title contains double quotes -- should be escaped */
	gctl_resource_set_title(pr, "Fix \"critical\" bug");
	gctl_resource_set_state(pr, "closed");
	gctl_resource_set_author(pr, "dev");
	g_ptr_array_add(resources, pr);

	output = gctl_output_formatter_format_resources(fmt, resources);
	g_assert_nonnull(output);

	/* Quotes within field should be doubled: ""critical"" inside outer quotes */
	g_assert_true(strstr(output, "\"\"critical\"\"") != NULL);
}

/* ── New tests: YAML format ───────────────────────────────────────── */

/*
 * test_formatter_format_yaml: format PR resources as YAML, verify
 * key: value pairs are present
 */
static void
test_formatter_format_yaml(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;
	g_autoptr(GPtrArray) resources = NULL;
	g_autofree gchar *output = NULL;
	GctlResource *pr;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_YAML);
	resources = g_ptr_array_new_with_free_func(
		(GDestroyNotify)gctl_resource_free);

	pr = gctl_resource_new(GCTL_RESOURCE_KIND_PR);
	gctl_resource_set_number(pr, 5);
	gctl_resource_set_title(pr, "Refactor parsing");
	gctl_resource_set_state(pr, "merged");
	gctl_resource_set_author(pr, "contributor");
	g_ptr_array_add(resources, pr);

	output = gctl_output_formatter_format_resources(fmt, resources);
	g_assert_nonnull(output);

	/* Verify YAML key: value pairs */
	g_assert_true(strstr(output, "kind: pr") != NULL);
	g_assert_true(strstr(output, "number: 5") != NULL);
	g_assert_true(strstr(output, "title: Refactor parsing") != NULL);
	g_assert_true(strstr(output, "state: merged") != NULL);
	g_assert_true(strstr(output, "author: contributor") != NULL);
}

/*
 * test_formatter_yaml_multiple: format multiple resources as YAML,
 * verify "---" separator is present between entries
 */
static void
test_formatter_yaml_multiple(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;
	g_autoptr(GPtrArray) resources = NULL;
	g_autofree gchar *output = NULL;
	GctlResource *pr1;
	GctlResource *pr2;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_YAML);
	resources = g_ptr_array_new_with_free_func(
		(GDestroyNotify)gctl_resource_free);

	pr1 = gctl_resource_new(GCTL_RESOURCE_KIND_PR);
	gctl_resource_set_number(pr1, 1);
	gctl_resource_set_title(pr1, "First PR");
	g_ptr_array_add(resources, pr1);

	pr2 = gctl_resource_new(GCTL_RESOURCE_KIND_PR);
	gctl_resource_set_number(pr2, 2);
	gctl_resource_set_title(pr2, "Second PR");
	g_ptr_array_add(resources, pr2);

	output = gctl_output_formatter_format_resources(fmt, resources);
	g_assert_nonnull(output);

	/* Verify the YAML document separator appears between entries */
	g_assert_true(strstr(output, "---") != NULL);
	g_assert_true(strstr(output, "First PR") != NULL);
	g_assert_true(strstr(output, "Second PR") != NULL);
}

/* ── New tests: All resource kinds in table format ────────────────── */

/*
 * test_formatter_ci_table: verify CI resource table columns
 * (ID, TITLE, STATUS, BRANCH)
 */
static void
test_formatter_ci_table(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;
	g_autoptr(GPtrArray) resources = NULL;
	g_autofree gchar *output = NULL;
	GctlResource *ci;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_TABLE);
	resources = g_ptr_array_new_with_free_func(
		(GDestroyNotify)gctl_resource_free);

	ci = gctl_resource_new(GCTL_RESOURCE_KIND_CI);
	gctl_resource_set_title(ci, "Build Pipeline");
	gctl_resource_set_extra(ci, "run_id", "12345");
	gctl_resource_set_extra(ci, "status", "success");
	gctl_resource_set_extra(ci, "branch", "main");
	g_ptr_array_add(resources, ci);

	output = gctl_output_formatter_format_resources(fmt, resources);
	g_assert_nonnull(output);

	g_assert_true(strstr(output, "ID") != NULL);
	g_assert_true(strstr(output, "TITLE") != NULL);
	g_assert_true(strstr(output, "STATUS") != NULL);
	g_assert_true(strstr(output, "BRANCH") != NULL);
	g_assert_true(strstr(output, "12345") != NULL);
	g_assert_true(strstr(output, "Build Pipeline") != NULL);
	g_assert_true(strstr(output, "success") != NULL);
	g_assert_true(strstr(output, "main") != NULL);
}

/*
 * test_formatter_commit_table: verify COMMIT resource table columns
 * (SHA, MESSAGE, AUTHOR, DATE)
 */
static void
test_formatter_commit_table(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;
	g_autoptr(GPtrArray) resources = NULL;
	g_autofree gchar *output = NULL;
	GctlResource *commit;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_TABLE);
	resources = g_ptr_array_new_with_free_func(
		(GDestroyNotify)gctl_resource_free);

	commit = gctl_resource_new(GCTL_RESOURCE_KIND_COMMIT);
	gctl_resource_set_title(commit, "Initial commit");
	gctl_resource_set_author(commit, "zach");
	gctl_resource_set_created_at(commit, "2026-03-23");
	gctl_resource_set_extra(commit, "sha", "abc123f");
	g_ptr_array_add(resources, commit);

	output = gctl_output_formatter_format_resources(fmt, resources);
	g_assert_nonnull(output);

	g_assert_true(strstr(output, "SHA") != NULL);
	g_assert_true(strstr(output, "MESSAGE") != NULL);
	g_assert_true(strstr(output, "AUTHOR") != NULL);
	g_assert_true(strstr(output, "DATE") != NULL);
	g_assert_true(strstr(output, "abc123f") != NULL);
	g_assert_true(strstr(output, "Initial commit") != NULL);
	g_assert_true(strstr(output, "zach") != NULL);
	g_assert_true(strstr(output, "2026-03-23") != NULL);
}

/*
 * test_formatter_label_table: verify LABEL resource table columns
 * (NAME, COLOR, DESCRIPTION)
 */
static void
test_formatter_label_table(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;
	g_autoptr(GPtrArray) resources = NULL;
	g_autofree gchar *output = NULL;
	GctlResource *label;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_TABLE);
	resources = g_ptr_array_new_with_free_func(
		(GDestroyNotify)gctl_resource_free);

	label = gctl_resource_new(GCTL_RESOURCE_KIND_LABEL);
	gctl_resource_set_title(label, "bug");
	gctl_resource_set_description(label, "Something is broken");
	gctl_resource_set_extra(label, "color", "#d73a4a");
	g_ptr_array_add(resources, label);

	output = gctl_output_formatter_format_resources(fmt, resources);
	g_assert_nonnull(output);

	g_assert_true(strstr(output, "NAME") != NULL);
	g_assert_true(strstr(output, "COLOR") != NULL);
	g_assert_true(strstr(output, "DESCRIPTION") != NULL);
	g_assert_true(strstr(output, "bug") != NULL);
	g_assert_true(strstr(output, "#d73a4a") != NULL);
	g_assert_true(strstr(output, "Something is broken") != NULL);
}

/*
 * test_formatter_notification_table: verify NOTIFICATION resource
 * table columns (ID, TYPE, TITLE, REPO)
 */
static void
test_formatter_notification_table(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;
	g_autoptr(GPtrArray) resources = NULL;
	g_autofree gchar *output = NULL;
	GctlResource *notif;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_TABLE);
	resources = g_ptr_array_new_with_free_func(
		(GDestroyNotify)gctl_resource_free);

	notif = gctl_resource_new(GCTL_RESOURCE_KIND_NOTIFICATION);
	gctl_resource_set_title(notif, "New comment on PR #42");
	gctl_resource_set_extra(notif, "notification_id", "99001");
	gctl_resource_set_extra(notif, "subject_type", "PullRequest");
	gctl_resource_set_extra(notif, "repo", "owner/repo");
	g_ptr_array_add(resources, notif);

	output = gctl_output_formatter_format_resources(fmt, resources);
	g_assert_nonnull(output);

	g_assert_true(strstr(output, "ID") != NULL);
	g_assert_true(strstr(output, "TYPE") != NULL);
	g_assert_true(strstr(output, "TITLE") != NULL);
	g_assert_true(strstr(output, "REPO") != NULL);
	g_assert_true(strstr(output, "99001") != NULL);
	g_assert_true(strstr(output, "PullRequest") != NULL);
	g_assert_true(strstr(output, "New comment on PR #42") != NULL);
	g_assert_true(strstr(output, "owner/repo") != NULL);
}

/*
 * test_formatter_key_table: verify KEY resource table columns
 * (ID, TITLE, FINGERPRINT)
 */
static void
test_formatter_key_table(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;
	g_autoptr(GPtrArray) resources = NULL;
	g_autofree gchar *output = NULL;
	GctlResource *key;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_TABLE);
	resources = g_ptr_array_new_with_free_func(
		(GDestroyNotify)gctl_resource_free);

	key = gctl_resource_new(GCTL_RESOURCE_KIND_KEY);
	gctl_resource_set_title(key, "My SSH Key");
	gctl_resource_set_extra(key, "key_id", "5001");
	gctl_resource_set_extra(key, "fingerprint", "SHA256:abc123xyz");
	g_ptr_array_add(resources, key);

	output = gctl_output_formatter_format_resources(fmt, resources);
	g_assert_nonnull(output);

	g_assert_true(strstr(output, "ID") != NULL);
	g_assert_true(strstr(output, "TITLE") != NULL);
	g_assert_true(strstr(output, "FINGERPRINT") != NULL);
	g_assert_true(strstr(output, "5001") != NULL);
	g_assert_true(strstr(output, "My SSH Key") != NULL);
	g_assert_true(strstr(output, "SHA256:abc123xyz") != NULL);
}

/*
 * test_formatter_webhook_table: verify WEBHOOK resource table columns
 * (ID, URL, EVENTS, ACTIVE)
 */
static void
test_formatter_webhook_table(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;
	g_autoptr(GPtrArray) resources = NULL;
	g_autofree gchar *output = NULL;
	GctlResource *hook;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_TABLE);
	resources = g_ptr_array_new_with_free_func(
		(GDestroyNotify)gctl_resource_free);

	hook = gctl_resource_new(GCTL_RESOURCE_KIND_WEBHOOK);
	gctl_resource_set_url(hook, "https://example.com/hook");
	gctl_resource_set_extra(hook, "webhook_id", "7001");
	gctl_resource_set_extra(hook, "events", "push,pull_request");
	gctl_resource_set_extra(hook, "active", "true");
	g_ptr_array_add(resources, hook);

	output = gctl_output_formatter_format_resources(fmt, resources);
	g_assert_nonnull(output);

	g_assert_true(strstr(output, "ID") != NULL);
	g_assert_true(strstr(output, "URL") != NULL);
	g_assert_true(strstr(output, "EVENTS") != NULL);
	g_assert_true(strstr(output, "ACTIVE") != NULL);
	g_assert_true(strstr(output, "7001") != NULL);
	g_assert_true(strstr(output, "https://example.com/hook") != NULL);
	g_assert_true(strstr(output, "push,pull_request") != NULL);
	g_assert_true(strstr(output, "true") != NULL);
}

/*
 * test_formatter_mirror_table: verify MIRROR resource table columns
 * (ID, URL, DIRECTION, INTERVAL)
 */
static void
test_formatter_mirror_table(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;
	g_autoptr(GPtrArray) resources = NULL;
	g_autofree gchar *output = NULL;
	GctlResource *mirror;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_TABLE);
	resources = g_ptr_array_new_with_free_func(
		(GDestroyNotify)gctl_resource_free);

	mirror = gctl_resource_new(GCTL_RESOURCE_KIND_MIRROR);
	gctl_resource_set_url(mirror, "https://mirror.example.com/repo.git");
	gctl_resource_set_extra(mirror, "mirror_id", "3001");
	gctl_resource_set_extra(mirror, "direction", "push");
	gctl_resource_set_extra(mirror, "interval", "8h");
	g_ptr_array_add(resources, mirror);

	output = gctl_output_formatter_format_resources(fmt, resources);
	g_assert_nonnull(output);

	g_assert_true(strstr(output, "ID") != NULL);
	g_assert_true(strstr(output, "URL") != NULL);
	g_assert_true(strstr(output, "DIRECTION") != NULL);
	g_assert_true(strstr(output, "INTERVAL") != NULL);
	g_assert_true(strstr(output, "3001") != NULL);
	g_assert_true(strstr(output, "push") != NULL);
	g_assert_true(strstr(output, "8h") != NULL);
}

/*
 * test_formatter_release_table: verify RELEASE resource table columns
 * (TAG, TITLE, DATE)
 */
static void
test_formatter_release_table(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;
	g_autoptr(GPtrArray) resources = NULL;
	g_autofree gchar *output = NULL;
	GctlResource *release;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_TABLE);
	resources = g_ptr_array_new_with_free_func(
		(GDestroyNotify)gctl_resource_free);

	release = gctl_resource_new(GCTL_RESOURCE_KIND_RELEASE);
	gctl_resource_set_title(release, "Release v1.0.0");
	gctl_resource_set_created_at(release, "2026-03-23");
	gctl_resource_set_extra(release, "tag", "v1.0.0");
	g_ptr_array_add(resources, release);

	output = gctl_output_formatter_format_resources(fmt, resources);
	g_assert_nonnull(output);

	g_assert_true(strstr(output, "TAG") != NULL);
	g_assert_true(strstr(output, "TITLE") != NULL);
	g_assert_true(strstr(output, "DATE") != NULL);
	g_assert_true(strstr(output, "v1.0.0") != NULL);
	g_assert_true(strstr(output, "Release v1.0.0") != NULL);
	g_assert_true(strstr(output, "2026-03-23") != NULL);
}

/* ── New tests: Empty resources in all formats ────────────────────── */

/*
 * test_formatter_empty_csv: empty resources should produce empty
 * string for CSV format
 */
static void
test_formatter_empty_csv(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;
	g_autoptr(GPtrArray) resources = NULL;
	g_autofree gchar *output = NULL;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_CSV);
	resources = g_ptr_array_new();

	output = gctl_output_formatter_format_resources(fmt, resources);
	g_assert_nonnull(output);
	/* CSV with no data should produce empty string */
	g_assert_cmpstr(output, ==, "");
}

/*
 * test_formatter_empty_json: empty resources should produce
 * empty JSON array
 */
static void
test_formatter_empty_json(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;
	g_autoptr(GPtrArray) resources = NULL;
	g_autofree gchar *output = NULL;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_JSON);
	resources = g_ptr_array_new();

	output = gctl_output_formatter_format_resources(fmt, resources);
	g_assert_nonnull(output);
	/* JSON empty array: should start with '[' */
	g_assert_true(output[0] == '[');
	/* Should contain no object braces */
	g_assert_true(strstr(output, "\"kind\"") == NULL);
}

/*
 * test_formatter_empty_yaml: empty resources should produce
 * empty string for YAML format
 */
static void
test_formatter_empty_yaml(void)
{
	g_autoptr(GctlOutputFormatter) fmt = NULL;
	g_autoptr(GPtrArray) resources = NULL;
	g_autofree gchar *output = NULL;

	fmt = gctl_output_formatter_new(GCTL_OUTPUT_FORMAT_YAML);
	resources = g_ptr_array_new();

	output = gctl_output_formatter_format_resources(fmt, resources);
	g_assert_nonnull(output);
	g_assert_cmpstr(output, ==, "");
}

/*
 * test_formatter_null_fields_all_formats: ensure no crash when
 * formatting a resource with all NULL fields in CSV, JSON, YAML
 */
static void
test_formatter_null_fields_all_formats(void)
{
	GctlOutputFormat formats[] = {
		GCTL_OUTPUT_FORMAT_TABLE,
		GCTL_OUTPUT_FORMAT_JSON,
		GCTL_OUTPUT_FORMAT_YAML,
		GCTL_OUTPUT_FORMAT_CSV,
	};
	guint i;

	for (i = 0; i < G_N_ELEMENTS(formats); i++) {
		g_autoptr(GctlOutputFormatter) fmt = NULL;
		g_autoptr(GPtrArray) resources = NULL;
		g_autofree gchar *output = NULL;
		GctlResource *pr;

		fmt = gctl_output_formatter_new(formats[i]);
		resources = g_ptr_array_new_with_free_func(
			(GDestroyNotify)gctl_resource_free);

		pr = gctl_resource_new(GCTL_RESOURCE_KIND_PR);
		gctl_resource_set_number(pr, 1);
		/* All string fields left as NULL */
		g_ptr_array_add(resources, pr);

		output = gctl_output_formatter_format_resources(fmt, resources);
		g_assert_nonnull(output);
	}
}

int
main(
	int     argc,
	char  **argv
){
	g_test_init(&argc, &argv, NULL);

	/* Original tests */
	g_test_add_func("/output-formatter/new", test_formatter_new);
	g_test_add_func("/output-formatter/set-format", test_formatter_set_format);
	g_test_add_func("/output-formatter/format-empty", test_formatter_format_empty);
	g_test_add_func("/output-formatter/format-pr-table", test_formatter_format_pr_table);
	g_test_add_func("/output-formatter/format-pr-json", test_formatter_format_pr_json);
	g_test_add_func("/output-formatter/format-repo", test_formatter_format_repo);
	g_test_add_func("/output-formatter/format-single", test_formatter_format_single);
	g_test_add_func("/output-formatter/null-fields", test_formatter_null_fields);

	/* CSV format tests */
	g_test_add_func("/output-formatter/format-csv", test_formatter_format_csv);
	g_test_add_func("/output-formatter/csv-escaping", test_formatter_csv_escaping);
	g_test_add_func("/output-formatter/csv-quotes-in-field", test_formatter_csv_quotes_in_field);

	/* YAML format tests */
	g_test_add_func("/output-formatter/format-yaml", test_formatter_format_yaml);
	g_test_add_func("/output-formatter/yaml-multiple", test_formatter_yaml_multiple);

	/* All resource kinds table tests */
	g_test_add_func("/output-formatter/ci-table", test_formatter_ci_table);
	g_test_add_func("/output-formatter/commit-table", test_formatter_commit_table);
	g_test_add_func("/output-formatter/label-table", test_formatter_label_table);
	g_test_add_func("/output-formatter/notification-table", test_formatter_notification_table);
	g_test_add_func("/output-formatter/key-table", test_formatter_key_table);
	g_test_add_func("/output-formatter/webhook-table", test_formatter_webhook_table);
	g_test_add_func("/output-formatter/mirror-table", test_formatter_mirror_table);
	g_test_add_func("/output-formatter/release-table", test_formatter_release_table);

	/* Empty resources in all formats */
	g_test_add_func("/output-formatter/empty-csv", test_formatter_empty_csv);
	g_test_add_func("/output-formatter/empty-json", test_formatter_empty_json);
	g_test_add_func("/output-formatter/empty-yaml", test_formatter_empty_yaml);
	g_test_add_func("/output-formatter/null-fields-all-formats", test_formatter_null_fields_all_formats);

	return g_test_run();
}
