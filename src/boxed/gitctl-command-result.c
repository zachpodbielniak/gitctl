/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-command-result.c - Boxed type for subprocess execution results */

#define GCTL_COMPILATION
#include "gitctl.h"

/* ── Boxed type registration ───────────────────────────────────────── */

G_DEFINE_BOXED_TYPE(
	GctlCommandResult,
	gctl_command_result,
	gctl_command_result_copy,
	gctl_command_result_free
)

/* ── Constructor ───────────────────────────────────────────────────── */

/**
 * gctl_command_result_new:
 * @exit_code: the process exit code
 * @stdout_text: (nullable): captured standard output text
 * @stderr_text: (nullable): captured standard error text
 * @argv: (array zero-terminated=1) (nullable): the command that was run
 * @elapsed_seconds: wall-clock time the command took, in seconds
 *
 * Creates a new #GctlCommandResult.  All strings and the argv vector
 * are deep-copied so the caller retains ownership of the originals.
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
){
	GctlCommandResult *self;

	self = g_new0(GctlCommandResult, 1);

	self->exit_code        = exit_code;
	self->stdout_text      = g_strdup(stdout_text);
	self->stderr_text      = g_strdup(stderr_text);
	self->argv             = g_strdupv((gchar **)argv);
	self->elapsed_seconds  = elapsed_seconds;

	return self;
}

/* ── Copy / Free ───────────────────────────────────────────────────── */

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
gctl_command_result_copy(const GctlCommandResult *src)
{
	g_return_val_if_fail(src != NULL, NULL);

	return gctl_command_result_new(
		src->exit_code,
		src->stdout_text,
		src->stderr_text,
		(const gchar * const *)src->argv,
		src->elapsed_seconds
	);
}

/**
 * gctl_command_result_free:
 * @self: (nullable): a #GctlCommandResult, or %NULL
 *
 * Frees all memory associated with @self.  If @self is %NULL this
 * function is a no-op.
 */
void
gctl_command_result_free(GctlCommandResult *self)
{
	if (self == NULL)
		return;

	g_free(self->stdout_text);
	g_free(self->stderr_text);
	g_strfreev(self->argv);
	g_free(self);
}

/* ── Accessors ─────────────────────────────────────────────────────── */

/**
 * gctl_command_result_get_exit_code:
 * @self: a #GctlCommandResult
 *
 * Returns the process exit code.
 *
 * Returns: the exit code
 */
gint
gctl_command_result_get_exit_code(const GctlCommandResult *self)
{
	g_return_val_if_fail(self != NULL, -1);

	return self->exit_code;
}

/**
 * gctl_command_result_get_stdout:
 * @self: a #GctlCommandResult
 *
 * Returns the captured standard output text.
 *
 * Returns: (transfer none) (nullable): the stdout text
 */
const gchar *
gctl_command_result_get_stdout(const GctlCommandResult *self)
{
	g_return_val_if_fail(self != NULL, NULL);

	return self->stdout_text;
}

/**
 * gctl_command_result_get_stderr:
 * @self: a #GctlCommandResult
 *
 * Returns the captured standard error text.
 *
 * Returns: (transfer none) (nullable): the stderr text
 */
const gchar *
gctl_command_result_get_stderr(const GctlCommandResult *self)
{
	g_return_val_if_fail(self != NULL, NULL);

	return self->stderr_text;
}

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
gctl_command_result_get_argv(const GctlCommandResult *self)
{
	g_return_val_if_fail(self != NULL, NULL);

	return (const gchar * const *)self->argv;
}

/**
 * gctl_command_result_get_elapsed:
 * @self: a #GctlCommandResult
 *
 * Returns the elapsed wall-clock time for the subprocess, in seconds.
 *
 * Returns: elapsed seconds
 */
gdouble
gctl_command_result_get_elapsed(const GctlCommandResult *self)
{
	g_return_val_if_fail(self != NULL, 0.0);

	return self->elapsed_seconds;
}
