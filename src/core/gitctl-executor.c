/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-executor.c - Subprocess execution with dry-run and timeout support */

#define GCTL_COMPILATION
#include "gitctl.h"

#include <gio/gio.h>
#include <signal.h>

/* ── Private structure ─────────────────────────────────────────────── */

struct _GctlExecutor
{
	GObject   parent_instance;

	gboolean  dry_run;
	gint      timeout_seconds;
	gboolean  verbose;
};

G_DEFINE_FINAL_TYPE(GctlExecutor, gctl_executor, G_TYPE_OBJECT)

/* ── GObject property IDs ──────────────────────────────────────────── */

enum
{
	PROP_0,
	PROP_DRY_RUN,
	PROP_TIMEOUT,
	PROP_VERBOSE,
	N_PROPS
};

static GParamSpec *properties[N_PROPS];

/* ── Helpers ───────────────────────────────────────────────────────── */

/**
 * format_argv:
 * @argv: (array zero-terminated=1): the argument vector
 *
 * Joins @argv into a single space-separated string with shell-style
 * quoting applied to each element so the result is safe to display.
 *
 * Returns: (transfer full): the formatted command string
 */
static gchar *
format_argv(const gchar * const *argv)
{
	GString *buf;
	gint     i;

	buf = g_string_new(NULL);

	for (i = 0; argv[i] != NULL; i++) {
		g_autofree gchar *quoted = NULL;

		if (i > 0)
			g_string_append_c(buf, ' ');

		quoted = g_shell_quote(argv[i]);
		g_string_append(buf, quoted);
	}

	return g_string_free(buf, FALSE);
}

/* ── GObject vfuncs ────────────────────────────────────────────────── */

static void
gctl_executor_get_property(
	GObject     *object,
	guint        prop_id,
	GValue      *value,
	GParamSpec  *pspec
){
	GctlExecutor *self = GCTL_EXECUTOR(object);

	switch (prop_id) {
	case PROP_DRY_RUN:
		g_value_set_boolean(value, self->dry_run);
		break;
	case PROP_TIMEOUT:
		g_value_set_int(value, self->timeout_seconds);
		break;
	case PROP_VERBOSE:
		g_value_set_boolean(value, self->verbose);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
gctl_executor_set_property(
	GObject       *object,
	guint          prop_id,
	const GValue  *value,
	GParamSpec    *pspec
){
	GctlExecutor *self = GCTL_EXECUTOR(object);

	switch (prop_id) {
	case PROP_DRY_RUN:
		self->dry_run = g_value_get_boolean(value);
		break;
	case PROP_TIMEOUT:
		self->timeout_seconds = g_value_get_int(value);
		break;
	case PROP_VERBOSE:
		self->verbose = g_value_get_boolean(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
gctl_executor_class_init(GctlExecutorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->get_property = gctl_executor_get_property;
	object_class->set_property = gctl_executor_set_property;

	/**
	 * GctlExecutor:dry-run:
	 *
	 * Whether the executor operates in dry-run mode.  When %TRUE,
	 * commands are printed but never spawned.
	 */
	properties[PROP_DRY_RUN] =
		g_param_spec_boolean(
			"dry-run",
			"Dry run",
			"Print commands instead of executing them",
			FALSE,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
		);

	/**
	 * GctlExecutor:timeout:
	 *
	 * Subprocess timeout in seconds.  A value of 0 means no timeout.
	 */
	properties[PROP_TIMEOUT] =
		g_param_spec_int(
			"timeout",
			"Timeout",
			"Subprocess timeout in seconds",
			0, G_MAXINT, 30,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
		);

	/**
	 * GctlExecutor:verbose:
	 *
	 * Whether to print each command to stderr before execution.
	 */
	properties[PROP_VERBOSE] =
		g_param_spec_boolean(
			"verbose",
			"Verbose",
			"Print commands to stderr before execution",
			FALSE,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
		);

	g_object_class_install_properties(object_class, N_PROPS, properties);
}

static void
gctl_executor_init(GctlExecutor *self)
{
	self->dry_run         = FALSE;
	self->timeout_seconds = 30;
	self->verbose         = FALSE;
}

/* ── Public API ────────────────────────────────────────────────────── */

/**
 * gctl_executor_new:
 *
 * Creates a new #GctlExecutor with default settings (dry_run=%FALSE,
 * timeout=30 seconds, verbose=%FALSE).
 *
 * Returns: (transfer full): a newly created #GctlExecutor
 */
GctlExecutor *
gctl_executor_new(void)
{
	return g_object_new(GCTL_TYPE_EXECUTOR, NULL);
}

/**
 * gctl_executor_run:
 * @self: a #GctlExecutor
 * @argv: (array zero-terminated=1): the argument vector for the subprocess
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Runs a subprocess described by @argv and captures its stdout and stderr.
 *
 * When dry_run is enabled the command is printed to stdout and a synthetic
 * #GctlCommandResult with exit_code=0 is returned without spawning a
 * process.
 *
 * On spawn failure @error is set to %GCTL_ERROR_EXECUTOR_SPAWN and
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
){
	g_autofree gchar *cmd_str = NULL;
	g_autoptr(GSubprocess) proc = NULL;
	g_autofree gchar *out_text = NULL;
	g_autofree gchar *err_text = NULL;
	GSubprocessFlags flags;
	GTimer *timer = NULL;
	gdouble elapsed;
	gint exit_code;

	g_return_val_if_fail(GCTL_IS_EXECUTOR(self), NULL);
	g_return_val_if_fail(argv != NULL && argv[0] != NULL, NULL);

	cmd_str = format_argv(argv);

	/* Dry-run mode: print the command and return a synthetic result */
	if (self->dry_run) {
		g_print("[dry-run] %s\n", cmd_str);
		return gctl_command_result_new(0, "", "", argv, 0.0);
	}

	/* Verbose mode: echo the command to stderr */
	if (self->verbose)
		g_printerr("[exec] %s\n", cmd_str);

	/* Spawn the subprocess with stdout and stderr pipes */
	flags = G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE;

	proc = g_subprocess_newv(argv, flags, error);
	if (proc == NULL) {
		/* Wrap the GError in our domain if not already set */
		if (error != NULL && *error != NULL) {
			g_autofree gchar *orig_msg = g_strdup((*error)->message);
			g_clear_error(error);
			g_set_error(
				error,
				GCTL_ERROR,
				GCTL_ERROR_EXECUTOR_SPAWN,
				"Failed to spawn '%s': %s",
				argv[0], orig_msg
			);
		}
		return NULL;
	}

	/* Communicate: capture stdout and stderr */
	timer = g_timer_new();

	if (!g_subprocess_communicate_utf8(proc, NULL, NULL,
	                                   &out_text, &err_text, error))
	{
		g_timer_destroy(timer);
		return NULL;
	}

	elapsed = g_timer_elapsed(timer, NULL);
	g_timer_destroy(timer);

	/* Check for user interruption.
	 * g_interrupted is defined in main.c and exported via --export-dynamic.
	 * We use a weak reference so the library links cleanly on its own;
	 * the symbol is resolved at load time when linked into the gitctl
	 * executable. */
	{
		extern volatile sig_atomic_t g_interrupted __attribute__((weak));
		if (&g_interrupted != NULL && g_interrupted) {
			g_set_error_literal(error, GCTL_ERROR, GCTL_ERROR_GENERAL,
			                    "Interrupted");
			/* result may be partially valid -- return NULL to signal error */
			return NULL;
		}
	}

	exit_code = g_subprocess_get_exit_status(proc);

	/*
	 * Build the result.  Even on non-zero exit we return the result
	 * so the caller can decide how to handle it.
	 */
	return gctl_command_result_new(
		exit_code,
		out_text != NULL ? out_text : "",
		err_text != NULL ? err_text : "",
		argv,
		elapsed
	);
}

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
){
	GctlCommandResult *result = NULL;
	gchar *stdout_copy = NULL;

	g_return_val_if_fail(GCTL_IS_EXECUTOR(self), NULL);
	g_return_val_if_fail(argv != NULL && argv[0] != NULL, NULL);

	result = gctl_executor_run(self, argv, error);
	if (result == NULL)
		return NULL;

	/*
	 * Access the result fields directly.  The struct definition lives
	 * in boxed/gitctl-command-result.h but we have it via the umbrella
	 * header.  We duplicate the stdout text so we can free the result.
	 */
	if (result->exit_code != 0) {
		g_autofree gchar *cmd_str = format_argv(argv);

		g_set_error(
			error,
			GCTL_ERROR,
			GCTL_ERROR_EXECUTOR_FAILED,
			"Command '%s' exited with status %d: %s",
			cmd_str,
			result->exit_code,
			result->stderr_text != NULL ? result->stderr_text : ""
		);

		gctl_command_result_free(result);
		return NULL;
	}

	stdout_copy = g_strdup(result->stdout_text);
	gctl_command_result_free(result);

	return stdout_copy;
}

/* ── Property accessors ────────────────────────────────────────────── */

/**
 * gctl_executor_set_dry_run:
 * @self: a #GctlExecutor
 * @dry_run: %TRUE to enable dry-run mode
 *
 * Sets whether the executor operates in dry-run mode.
 */
void
gctl_executor_set_dry_run(
	GctlExecutor  *self,
	gboolean       dry_run
){
	g_return_if_fail(GCTL_IS_EXECUTOR(self));

	if (self->dry_run != dry_run) {
		self->dry_run = dry_run;
		g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_DRY_RUN]);
	}
}

/**
 * gctl_executor_get_dry_run:
 * @self: a #GctlExecutor
 *
 * Returns whether dry-run mode is enabled.
 *
 * Returns: %TRUE if dry-run mode is active
 */
gboolean
gctl_executor_get_dry_run(GctlExecutor *self)
{
	g_return_val_if_fail(GCTL_IS_EXECUTOR(self), FALSE);

	return self->dry_run;
}

/**
 * gctl_executor_set_timeout:
 * @self: a #GctlExecutor
 * @seconds: timeout in seconds (must be >= 0)
 *
 * Sets the subprocess timeout in seconds.
 */
void
gctl_executor_set_timeout(
	GctlExecutor  *self,
	gint           seconds
){
	g_return_if_fail(GCTL_IS_EXECUTOR(self));
	g_return_if_fail(seconds >= 0);

	if (self->timeout_seconds != seconds) {
		self->timeout_seconds = seconds;
		g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_TIMEOUT]);
	}
}

/**
 * gctl_executor_get_timeout:
 * @self: a #GctlExecutor
 *
 * Returns the current subprocess timeout in seconds.
 *
 * Returns: the timeout value
 */
gint
gctl_executor_get_timeout(GctlExecutor *self)
{
	g_return_val_if_fail(GCTL_IS_EXECUTOR(self), 30);

	return self->timeout_seconds;
}

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
){
	g_return_if_fail(GCTL_IS_EXECUTOR(self));

	if (self->verbose != verbose) {
		self->verbose = verbose;
		g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_VERBOSE]);
	}
}

/**
 * gctl_executor_get_verbose:
 * @self: a #GctlExecutor
 *
 * Returns whether verbose mode is enabled.
 *
 * Returns: %TRUE if verbose mode is active
 */
gboolean
gctl_executor_get_verbose(GctlExecutor *self)
{
	g_return_val_if_fail(GCTL_IS_EXECUTOR(self), FALSE);

	return self->verbose;
}
