/*
 * test-command-result.c - Tests for GctlCommandResult boxed type
 *
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define GCTL_COMPILATION
#include <gitctl.h>

/* test_result_new: create with all fields, verify accessors */
static void
test_result_new(void)
{
	g_autoptr(GctlCommandResult) result = NULL;
	const gchar *argv[] = { "echo", "hello", NULL };

	result = gctl_command_result_new(0, "hello\n", "", argv, 0.5);

	g_assert_nonnull(result);
	g_assert_cmpint(gctl_command_result_get_exit_code(result), ==, 0);
	g_assert_cmpstr(gctl_command_result_get_stdout(result), ==, "hello\n");
	g_assert_cmpstr(gctl_command_result_get_stderr(result), ==, "");
	g_assert_true(gctl_command_result_get_elapsed(result) >= 0.4);
	g_assert_true(gctl_command_result_get_elapsed(result) <= 0.6);
}

/* test_result_copy: copy and verify all fields match */
static void
test_result_copy(void)
{
	g_autoptr(GctlCommandResult) original = NULL;
	g_autoptr(GctlCommandResult) copy = NULL;
	const gchar *argv[] = { "git", "status", NULL };
	const gchar * const *copy_argv;

	original = gctl_command_result_new(1, "out text", "err text", argv, 1.23);
	copy = gctl_command_result_copy(original);

	g_assert_nonnull(copy);
	g_assert_cmpint(gctl_command_result_get_exit_code(copy), ==, 1);
	g_assert_cmpstr(gctl_command_result_get_stdout(copy), ==, "out text");
	g_assert_cmpstr(gctl_command_result_get_stderr(copy), ==, "err text");
	g_assert_cmpfloat_with_epsilon(gctl_command_result_get_elapsed(copy),
	                               1.23, 0.001);

	/* Verify argv is preserved */
	copy_argv = gctl_command_result_get_argv(copy);
	g_assert_nonnull(copy_argv);
	g_assert_cmpstr(copy_argv[0], ==, "git");
	g_assert_cmpstr(copy_argv[1], ==, "status");
	g_assert_null(copy_argv[2]);
}

/* test_result_free: create and free (no crash) */
static void
test_result_free(void)
{
	GctlCommandResult *result;
	const gchar *argv[] = { "ls", NULL };

	result = gctl_command_result_new(0, "files", "", argv, 0.1);
	gctl_command_result_free(result);

	/* Freeing NULL should also be safe */
	gctl_command_result_free(NULL);
}

/* test_result_null_fields: create with NULL stdout/stderr, verify getters */
static void
test_result_null_fields(void)
{
	g_autoptr(GctlCommandResult) result = NULL;

	result = gctl_command_result_new(0, NULL, NULL, NULL, 0.0);

	g_assert_nonnull(result);
	g_assert_null(gctl_command_result_get_stdout(result));
	g_assert_null(gctl_command_result_get_stderr(result));
	g_assert_null(gctl_command_result_get_argv(result));
}

/* test_result_zero_exit: create with exit_code 0 */
static void
test_result_zero_exit(void)
{
	g_autoptr(GctlCommandResult) result = NULL;
	const gchar *argv[] = { "true", NULL };

	result = gctl_command_result_new(0, "", "", argv, 0.0);

	g_assert_cmpint(gctl_command_result_get_exit_code(result), ==, 0);
}

/* test_result_nonzero_exit: create with exit_code 1 */
static void
test_result_nonzero_exit(void)
{
	g_autoptr(GctlCommandResult) result = NULL;
	const gchar *argv[] = { "false", NULL };

	result = gctl_command_result_new(1, "", "error occurred", argv, 0.0);

	g_assert_cmpint(gctl_command_result_get_exit_code(result), ==, 1);
	g_assert_cmpstr(gctl_command_result_get_stderr(result),
	                ==, "error occurred");
}

/* test_result_argv: verify argv is preserved correctly */
static void
test_result_argv(void)
{
	g_autoptr(GctlCommandResult) result = NULL;
	const gchar *argv[] = { "git", "remote", "get-url", "origin", NULL };
	const gchar * const *result_argv;

	result = gctl_command_result_new(0, "", "", argv, 0.0);

	result_argv = gctl_command_result_get_argv(result);
	g_assert_nonnull(result_argv);
	g_assert_cmpstr(result_argv[0], ==, "git");
	g_assert_cmpstr(result_argv[1], ==, "remote");
	g_assert_cmpstr(result_argv[2], ==, "get-url");
	g_assert_cmpstr(result_argv[3], ==, "origin");
	g_assert_null(result_argv[4]);
}

int
main(
	int     argc,
	char  **argv
){
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/command-result/new", test_result_new);
	g_test_add_func("/command-result/copy", test_result_copy);
	g_test_add_func("/command-result/free", test_result_free);
	g_test_add_func("/command-result/null-fields", test_result_null_fields);
	g_test_add_func("/command-result/zero-exit", test_result_zero_exit);
	g_test_add_func("/command-result/nonzero-exit", test_result_nonzero_exit);
	g_test_add_func("/command-result/argv", test_result_argv);

	return g_test_run();
}
