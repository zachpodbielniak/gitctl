/*
 * test-executor.c - Tests for GctlExecutor
 *
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define GCTL_COMPILATION
#include <gitctl.h>

/* test_executor_new: create executor, verify type */
static void
test_executor_new(void)
{
	g_autoptr(GctlExecutor) exec = NULL;

	exec = gctl_executor_new();
	g_assert_nonnull(exec);
	g_assert_true(GCTL_IS_EXECUTOR(exec));
}

/* test_executor_dry_run_default: verify defaults to FALSE */
static void
test_executor_dry_run_default(void)
{
	g_autoptr(GctlExecutor) exec = NULL;

	exec = gctl_executor_new();
	g_assert_false(gctl_executor_get_dry_run(exec));
}

/* test_executor_set_dry_run: set to TRUE, verify */
static void
test_executor_set_dry_run(void)
{
	g_autoptr(GctlExecutor) exec = NULL;

	exec = gctl_executor_new();

	gctl_executor_set_dry_run(exec, TRUE);
	g_assert_true(gctl_executor_get_dry_run(exec));

	gctl_executor_set_dry_run(exec, FALSE);
	g_assert_false(gctl_executor_get_dry_run(exec));
}

/* test_executor_timeout_default: verify defaults to 30 */
static void
test_executor_timeout_default(void)
{
	g_autoptr(GctlExecutor) exec = NULL;

	exec = gctl_executor_new();
	g_assert_cmpint(gctl_executor_get_timeout(exec), ==, 30);
}

/* test_executor_run_simple_echo: run `echo hello`, verify stdout contains "hello" */
static void
test_executor_run_simple_echo(void)
{
	g_autoptr(GctlExecutor) exec = NULL;
	g_autoptr(GctlCommandResult) result = NULL;
	g_autoptr(GError) error = NULL;
	const gchar *argv[] = { "echo", "hello", NULL };
	const gchar *stdout_text;

	exec = gctl_executor_new();
	result = gctl_executor_run(exec, argv, &error);

	g_assert_no_error(error);
	g_assert_nonnull(result);
	g_assert_cmpint(gctl_command_result_get_exit_code(result), ==, 0);

	stdout_text = gctl_command_result_get_stdout(result);
	g_assert_nonnull(stdout_text);

	/* echo outputs "hello\n" -- verify it contains "hello" */
	g_assert_true(g_str_has_prefix(stdout_text, "hello"));
}

/* test_executor_run_exit_code: run `false`, verify exit_code is non-zero */
static void
test_executor_run_exit_code(void)
{
	g_autoptr(GctlExecutor) exec = NULL;
	g_autoptr(GctlCommandResult) result = NULL;
	g_autoptr(GError) error = NULL;
	const gchar *argv[] = { "false", NULL };

	exec = gctl_executor_new();
	result = gctl_executor_run(exec, argv, &error);

	/*
	 * gctl_executor_run returns a result even on non-zero exit,
	 * so we should have a result with a non-zero exit code.
	 */
	g_assert_nonnull(result);
	g_assert_cmpint(gctl_command_result_get_exit_code(result), !=, 0);
}

/*
 * test_executor_dry_run_mode: set dry-run, run a command, verify result
 * has exit_code 0 and no actual execution.
 */
static void
test_executor_dry_run_mode(void)
{
	g_autoptr(GctlExecutor) exec = NULL;
	g_autoptr(GctlCommandResult) result = NULL;
	g_autoptr(GError) error = NULL;

	/*
	 * Use a command that would fail if actually executed; in dry-run
	 * mode it should not be spawned.
	 */
	const gchar *argv[] = { "false", NULL };

	exec = gctl_executor_new();
	gctl_executor_set_dry_run(exec, TRUE);

	result = gctl_executor_run(exec, argv, &error);

	g_assert_no_error(error);
	g_assert_nonnull(result);

	/* Dry-run always returns exit_code 0 */
	g_assert_cmpint(gctl_command_result_get_exit_code(result), ==, 0);
}

int
main(
	int     argc,
	char  **argv
){
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/executor/new", test_executor_new);
	g_test_add_func("/executor/dry-run-default", test_executor_dry_run_default);
	g_test_add_func("/executor/set-dry-run", test_executor_set_dry_run);
	g_test_add_func("/executor/timeout-default", test_executor_timeout_default);
	g_test_add_func("/executor/run-simple-echo", test_executor_run_simple_echo);
	g_test_add_func("/executor/run-exit-code", test_executor_run_exit_code);
	g_test_add_func("/executor/dry-run-mode", test_executor_dry_run_mode);

	return g_test_run();
}
