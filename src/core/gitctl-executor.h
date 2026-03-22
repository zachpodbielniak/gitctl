/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-executor.h - Subprocess execution with dry-run and timeout support */

#ifndef GCTL_EXECUTOR_H
#define GCTL_EXECUTOR_H

#if !defined(GCTL_INSIDE) && !defined(GCTL_COMPILATION)
#error "Only <gitctl.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define GCTL_TYPE_EXECUTOR (gctl_executor_get_type())

G_DECLARE_FINAL_TYPE(GctlExecutor, gctl_executor, GCTL, EXECUTOR, GObject)

/* Forward declaration — defined in boxed/gitctl-command-result.h */
typedef struct _GctlCommandResult GctlCommandResult;

/**
 * gctl_executor_new:
 *
 * Creates a new #GctlExecutor with default settings (dry_run=%FALSE,
 * timeout=30 seconds, verbose=%FALSE).
 *
 * Returns: (transfer full): a newly created #GctlExecutor
 */
GctlExecutor *
gctl_executor_new(void);

/**
 * gctl_executor_run:
 * @self: a #GctlExecutor
 * @argv: (array zero-terminated=1): the argument vector for the subprocess
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Runs a subprocess described by @argv and captures its stdout and stderr.
 *
 * When dry_run is enabled, the command is printed to stdout prefixed with
 * "[dry-run]" and a synthetic #GctlCommandResult with exit_code=0 is
 * returned without spawning a process.
 *
 * On spawn failure, @error is set to %GCTL_ERROR_EXECUTOR_SPAWN and
 * %NULL is returned.  On non-zero exit the result is still returned so
 * the caller can inspect it.
 *
 * Returns: (transfer full) (nullable): a #GctlCommandResult, or %NULL on
 *     spawn failure
 */
GctlCommandResult *
gctl_executor_run(
	GctlExecutor         *self,
	const gchar * const  *argv,
	GError              **error
);

/**
 * gctl_executor_run_simple:
 * @self: a #GctlExecutor
 * @argv: (array zero-terminated=1): the argument vector for the subprocess
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Convenience wrapper around gctl_executor_run() that returns only the
 * captured stdout text.  If the subprocess exits with a non-zero status,
 * @error is set to %GCTL_ERROR_EXECUTOR_FAILED containing the stderr
 * output and %NULL is returned.
 *
 * Returns: (transfer full) (nullable): the stdout output, or %NULL on error
 */
gchar *
gctl_executor_run_simple(
	GctlExecutor         *self,
	const gchar * const  *argv,
	GError              **error
);

/**
 * gctl_executor_set_dry_run:
 * @self: a #GctlExecutor
 * @dry_run: %TRUE to enable dry-run mode
 *
 * Sets whether the executor operates in dry-run mode.  In dry-run mode
 * commands are printed but never executed.
 */
void
gctl_executor_set_dry_run(
	GctlExecutor  *self,
	gboolean       dry_run
);

/**
 * gctl_executor_get_dry_run:
 * @self: a #GctlExecutor
 *
 * Returns whether dry-run mode is enabled.
 *
 * Returns: %TRUE if dry-run mode is active
 */
gboolean
gctl_executor_get_dry_run(GctlExecutor *self);

/**
 * gctl_executor_set_timeout:
 * @self: a #GctlExecutor
 * @seconds: timeout in seconds (must be > 0)
 *
 * Sets the subprocess timeout in seconds.
 */
void
gctl_executor_set_timeout(
	GctlExecutor  *self,
	gint           seconds
);

/**
 * gctl_executor_get_timeout:
 * @self: a #GctlExecutor
 *
 * Returns the current subprocess timeout in seconds.
 *
 * Returns: the timeout value
 */
gint
gctl_executor_get_timeout(GctlExecutor *self);

/**
 * gctl_executor_set_verbose:
 * @self: a #GctlExecutor
 * @verbose: %TRUE to enable verbose output
 *
 * Sets whether the executor prints each command to stderr before
 * executing it.
 */
void
gctl_executor_set_verbose(
	GctlExecutor  *self,
	gboolean       verbose
);

/**
 * gctl_executor_get_verbose:
 * @self: a #GctlExecutor
 *
 * Returns whether verbose mode is enabled.
 *
 * Returns: %TRUE if verbose mode is active
 */
gboolean
gctl_executor_get_verbose(GctlExecutor *self);

G_END_DECLS

#endif /* GCTL_EXECUTOR_H */
