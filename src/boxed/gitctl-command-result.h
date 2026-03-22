/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-command-result.h - Boxed type for subprocess execution results */

#ifndef GCTL_COMMAND_RESULT_H
#define GCTL_COMMAND_RESULT_H

#if !defined(GCTL_INSIDE) && !defined(GCTL_COMPILATION)
#error "Only <gitctl.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * GCTL_TYPE_COMMAND_RESULT:
 *
 * The #GType for #GctlCommandResult.
 */
#define GCTL_TYPE_COMMAND_RESULT (gctl_command_result_get_type())

/**
 * GctlCommandResult:
 *
 * An opaque boxed type that holds the result of a subprocess execution,
 * including exit code, captured stdout/stderr, the argv that was run,
 * and elapsed wall-clock time.
 *
 * The struct fields are public so internal callers (e.g. #GctlExecutor)
 * can access them directly.
 */
struct _GctlCommandResult
{
	gint      exit_code;
	gchar    *stdout_text;
	gchar    *stderr_text;
	gchar   **argv;
	gdouble   elapsed_seconds;
};

/**
 * gctl_command_result_get_type:
 *
 * Registers and returns the #GType for #GctlCommandResult.
 *
 * Returns: the #GType
 */
GType
gctl_command_result_get_type(void) G_GNUC_CONST;

/**
 * gctl_command_result_new:
 * @exit_code: the process exit code
 * @stdout_text: (nullable): captured standard output text
 * @stderr_text: (nullable): captured standard error text
 * @argv: (array zero-terminated=1) (nullable): the command that was run
 * @elapsed_seconds: wall-clock time the command took, in seconds
 *
 * Creates a new #GctlCommandResult.  All strings and the argv vector
 * are deep-copied.
 *
 * Returns: (transfer full): a newly allocated #GctlCommandResult
 */
GctlCommandResult *
gctl_command_result_new(
	gint                  exit_code,
	const gchar          *stdout_text,
	const gchar          *stderr_text,
	const gchar * const  *argv,
	gdouble               elapsed_seconds
);

/**
 * gctl_command_result_copy:
 * @src: (not nullable): the #GctlCommandResult to copy
 *
 * Creates a deep copy of @src, duplicating all strings and the argv
 * vector.
 *
 * Returns: (transfer full): a newly allocated copy of @src
 */
GctlCommandResult *
gctl_command_result_copy(const GctlCommandResult *src);

/**
 * gctl_command_result_free:
 * @self: (nullable): a #GctlCommandResult, or %NULL
 *
 * Frees all memory associated with @self.  If @self is %NULL this
 * function is a no-op.
 */
void
gctl_command_result_free(GctlCommandResult *self);

/**
 * gctl_command_result_get_exit_code:
 * @self: a #GctlCommandResult
 *
 * Returns the process exit code.
 *
 * Returns: the exit code
 */
gint
gctl_command_result_get_exit_code(const GctlCommandResult *self);

/**
 * gctl_command_result_get_stdout:
 * @self: a #GctlCommandResult
 *
 * Returns the captured standard output text.
 *
 * Returns: (transfer none) (nullable): the stdout text
 */
const gchar *
gctl_command_result_get_stdout(const GctlCommandResult *self);

/**
 * gctl_command_result_get_stderr:
 * @self: a #GctlCommandResult
 *
 * Returns the captured standard error text.
 *
 * Returns: (transfer none) (nullable): the stderr text
 */
const gchar *
gctl_command_result_get_stderr(const GctlCommandResult *self);

/**
 * gctl_command_result_get_argv:
 * @self: a #GctlCommandResult
 *
 * Returns the NULL-terminated argument vector of the command that
 * was executed.
 *
 * Returns: (transfer none) (array zero-terminated=1) (nullable): the argv
 */
const gchar * const *
gctl_command_result_get_argv(const GctlCommandResult *self);

/**
 * gctl_command_result_get_elapsed:
 * @self: a #GctlCommandResult
 *
 * Returns the elapsed wall-clock time for the subprocess, in seconds.
 *
 * Returns: elapsed seconds
 */
gdouble
gctl_command_result_get_elapsed(const GctlCommandResult *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GctlCommandResult, gctl_command_result_free)

G_END_DECLS

#endif /* GCTL_COMMAND_RESULT_H */
