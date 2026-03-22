/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-app.c - Main application object implementation */

#define GCTL_COMPILATION
#include "gitctl.h"

/* ── Private data ────────────────────────────────────────────────────── */

typedef struct _GctlAppPrivate
{
	GctlExecutor        *executor;
	GctlContextResolver *resolver;
	GctlConfig          *config;
	GctlOutputFormatter *formatter;
	GctlModuleManager   *module_manager;

	gboolean             running;
	gboolean             dry_run;
	GctlOutputFormat     output_format;
	gboolean             verbose;
} GctlAppPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GctlApp, gctl_app, G_TYPE_OBJECT)

/* ── Property identifiers ────────────────────────────────────────────── */

enum
{
	PROP_0,
	PROP_RUNNING,
	PROP_DRY_RUN,
	PROP_OUTPUT_FORMAT,
	PROP_VERBOSE,
	N_PROPS
};

static GParamSpec *props[N_PROPS] = { NULL };

/* ── Signal identifiers ──────────────────────────────────────────────── */

enum
{
	SIGNAL_STARTED,
	SIGNAL_STOPPED,
	N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

/* ── GObject virtual methods ─────────────────────────────────────────── */

static void
gctl_app_get_property(
	GObject    *object,
	guint       prop_id,
	GValue     *value,
	GParamSpec *pspec
){
	GctlAppPrivate *priv;

	priv = gctl_app_get_instance_private(GCTL_APP(object));

	switch (prop_id) {
	case PROP_RUNNING:
		g_value_set_boolean(value, priv->running);
		break;
	case PROP_DRY_RUN:
		g_value_set_boolean(value, priv->dry_run);
		break;
	case PROP_OUTPUT_FORMAT:
		g_value_set_enum(value, priv->output_format);
		break;
	case PROP_VERBOSE:
		g_value_set_boolean(value, priv->verbose);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
gctl_app_set_property(
	GObject      *object,
	guint         prop_id,
	const GValue *value,
	GParamSpec   *pspec
){
	GctlApp *self;

	self = GCTL_APP(object);

	switch (prop_id) {
	case PROP_DRY_RUN:
		gctl_app_set_dry_run(self, g_value_get_boolean(value));
		break;
	case PROP_OUTPUT_FORMAT:
		gctl_app_set_output_format(self, (GctlOutputFormat)g_value_get_enum(value));
		break;
	case PROP_VERBOSE:
		gctl_app_set_verbose(self, g_value_get_boolean(value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
gctl_app_dispose(GObject *object)
{
	GctlAppPrivate *priv;

	priv = gctl_app_get_instance_private(GCTL_APP(object));

	/* Emit stopped before tearing down subsystems */
	if (priv->running) {
		priv->running = FALSE;
		g_signal_emit(object, signals[SIGNAL_STOPPED], 0);
	}

	g_clear_object(&priv->module_manager);
	g_clear_object(&priv->formatter);
	g_clear_object(&priv->resolver);
	g_clear_object(&priv->config);
	g_clear_object(&priv->executor);

	G_OBJECT_CLASS(gctl_app_parent_class)->dispose(object);
}

static void
gctl_app_finalize(GObject *object)
{
	/*
	 * All owned references are released in dispose.  This method is
	 * provided in case subclasses or future changes add plain-data
	 * fields that need freeing.
	 */

	G_OBJECT_CLASS(gctl_app_parent_class)->finalize(object);
}

/* ── Class and instance init ─────────────────────────────────────────── */

static void
gctl_app_class_init(GctlAppClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);

	object_class->get_property = gctl_app_get_property;
	object_class->set_property = gctl_app_set_property;
	object_class->dispose      = gctl_app_dispose;
	object_class->finalize     = gctl_app_finalize;

	/* ── Properties ─────────────────────────────────────────────── */

	/**
	 * GctlApp:running:
	 *
	 * Whether the application has been initialized and is running.
	 */
	props[PROP_RUNNING] =
		g_param_spec_boolean(
			"running",
			"Running",
			"Whether the application is initialized and running",
			FALSE,
			G_PARAM_READABLE | G_PARAM_STATIC_STRINGS
		);

	/**
	 * GctlApp:dry-run:
	 *
	 * When %TRUE the executor will log commands instead of running them.
	 */
	props[PROP_DRY_RUN] =
		g_param_spec_boolean(
			"dry-run",
			"Dry Run",
			"Log commands instead of executing them",
			FALSE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS
		);

	/**
	 * GctlApp:output-format:
	 *
	 * The output format used when rendering command results.
	 */
	props[PROP_OUTPUT_FORMAT] =
		g_param_spec_enum(
			"output-format",
			"Output Format",
			"The output format for command results",
			GCTL_TYPE_OUTPUT_FORMAT,
			GCTL_OUTPUT_FORMAT_TABLE,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
		);

	/**
	 * GctlApp:verbose:
	 *
	 * When %TRUE additional diagnostic messages are emitted.
	 */
	props[PROP_VERBOSE] =
		g_param_spec_boolean(
			"verbose",
			"Verbose",
			"Enable verbose diagnostic output",
			FALSE,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
		);

	g_object_class_install_properties(object_class, N_PROPS, props);

	/* ── Signals ────────────────────────────────────────────────── */

	/**
	 * GctlApp::started:
	 * @self: the #GctlApp that emitted the signal
	 *
	 * Emitted after gctl_app_initialize() completes successfully and
	 * all subsystems are ready.
	 */
	signals[SIGNAL_STARTED] =
		g_signal_new(
			"started",
			G_TYPE_FROM_CLASS(klass),
			G_SIGNAL_RUN_LAST,
			0,              /* class offset (no default handler) */
			NULL, NULL,     /* accumulator, accu_data */
			NULL,           /* c_marshaller (use generic) */
			G_TYPE_NONE,
			0               /* n_params */
		);

	/**
	 * GctlApp::stopped:
	 * @self: the #GctlApp that emitted the signal
	 *
	 * Emitted when the application is shutting down, before subsystem
	 * objects are released.
	 */
	signals[SIGNAL_STOPPED] =
		g_signal_new(
			"stopped",
			G_TYPE_FROM_CLASS(klass),
			G_SIGNAL_RUN_LAST,
			0,
			NULL, NULL,
			NULL,
			G_TYPE_NONE,
			0
		);
}

static void
gctl_app_init(GctlApp *self)
{
	GctlAppPrivate *priv;

	priv = gctl_app_get_instance_private(self);

	priv->executor       = NULL;
	priv->resolver       = NULL;
	priv->config         = NULL;
	priv->formatter      = NULL;
	priv->module_manager = NULL;

	priv->running       = FALSE;
	priv->dry_run       = FALSE;
	priv->output_format = GCTL_OUTPUT_FORMAT_TABLE;
	priv->verbose       = FALSE;
}

/* ── Public API ──────────────────────────────────────────────────────── */

/**
 * gctl_app_new:
 *
 * Creates a new #GctlApp instance.  Call gctl_app_initialize() before
 * using any of the subsystem accessors.
 *
 * Returns: (transfer full): a new #GctlApp
 */
GctlApp *
gctl_app_new(void)
{
	return (GctlApp *)g_object_new(GCTL_TYPE_APP, NULL);
}

/**
 * gctl_app_initialize:
 * @self: a #GctlApp
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Performs full application bootstrap: loads configuration, creates the
 * executor and output formatter, detects the forge context, and loads
 * modules.  Emits #GctlApp::started on success.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean
gctl_app_initialize(
	GctlApp  *self,
	GError  **error
){
	GctlAppPrivate *priv;

	g_return_val_if_fail(GCTL_IS_APP(self), FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	priv = gctl_app_get_instance_private(self);

	if (priv->running) {
		g_set_error_literal(
			error,
			GCTL_ERROR,
			GCTL_ERROR_GENERAL,
			"Application is already initialized"
		);
		return FALSE;
	}

	/* 1. Configuration (needed by everything else) */
	priv->config = gctl_config_new();
	if (priv->config == NULL) {
		g_set_error_literal(
			error,
			GCTL_ERROR,
			GCTL_ERROR_CONFIG_PARSE,
			"Failed to create configuration object"
		);
		return FALSE;
	}

	/* 2. Executor (subprocess runner) */
	priv->executor = gctl_executor_new();
	gctl_executor_set_dry_run(priv->executor, priv->dry_run);

	/* 3. Context resolver (forge detection) */
	priv->resolver = gctl_context_resolver_new(priv->config);

	/* 4. Output formatter */
	priv->formatter = gctl_output_formatter_new(priv->output_format);

	/* 5. Module manager (plugin loading) */
	priv->module_manager = gctl_module_manager_new();

	/*
	 * Load forge backend modules from the development build directory
	 * first (for uninstalled use), then from the installed module
	 * directory.  Errors are non-fatal — we log them and continue.
	 */
	{
		g_autoptr(GError) mod_error = NULL;

#ifdef GCTL_DEV_MODULE_DIR
		gctl_module_manager_load_from_directory(
			priv->module_manager, GCTL_DEV_MODULE_DIR, &mod_error);
		if (mod_error != NULL) {
			g_debug("dev module dir: %s", mod_error->message);
			g_clear_error(&mod_error);
		}
#endif

#ifdef GCTL_MODULEDIR
		gctl_module_manager_load_from_directory(
			priv->module_manager, GCTL_MODULEDIR, &mod_error);
		if (mod_error != NULL) {
			g_debug("installed module dir: %s", mod_error->message);
			g_clear_error(&mod_error);
		}
#endif

		gctl_module_manager_activate_all(priv->module_manager);
	}

	/* Mark as running and notify listeners */
	priv->running = TRUE;
	g_object_notify_by_pspec(G_OBJECT(self), props[PROP_RUNNING]);
	g_signal_emit(self, signals[SIGNAL_STARTED], 0);

	return TRUE;
}

/**
 * gctl_app_get_executor:
 * @self: a #GctlApp
 *
 * Retrieves the subprocess executor owned by @self.
 *
 * Returns: (transfer none) (nullable): the #GctlExecutor, or %NULL
 *   if the app has not been initialized
 */
GctlExecutor *
gctl_app_get_executor(GctlApp *self)
{
	GctlAppPrivate *priv;

	g_return_val_if_fail(GCTL_IS_APP(self), NULL);

	priv = gctl_app_get_instance_private(self);
	return priv->executor;
}

/**
 * gctl_app_get_resolver:
 * @self: a #GctlApp
 *
 * Retrieves the context resolver owned by @self.
 *
 * Returns: (transfer none) (nullable): the #GctlContextResolver, or
 *   %NULL if the app has not been initialized
 */
GctlContextResolver *
gctl_app_get_resolver(GctlApp *self)
{
	GctlAppPrivate *priv;

	g_return_val_if_fail(GCTL_IS_APP(self), NULL);

	priv = gctl_app_get_instance_private(self);
	return priv->resolver;
}

/**
 * gctl_app_get_config:
 * @self: a #GctlApp
 *
 * Retrieves the configuration object owned by @self.
 *
 * Returns: (transfer none) (nullable): the #GctlConfig, or %NULL if
 *   the app has not been initialized
 */
GctlConfig *
gctl_app_get_config(GctlApp *self)
{
	GctlAppPrivate *priv;

	g_return_val_if_fail(GCTL_IS_APP(self), NULL);

	priv = gctl_app_get_instance_private(self);
	return priv->config;
}

/**
 * gctl_app_get_formatter:
 * @self: a #GctlApp
 *
 * Retrieves the output formatter owned by @self.
 *
 * Returns: (transfer none) (nullable): the #GctlOutputFormatter, or
 *   %NULL if the app has not been initialized
 */
GctlOutputFormatter *
gctl_app_get_formatter(GctlApp *self)
{
	GctlAppPrivate *priv;

	g_return_val_if_fail(GCTL_IS_APP(self), NULL);

	priv = gctl_app_get_instance_private(self);
	return priv->formatter;
}

/**
 * gctl_app_get_module_manager:
 * @self: a #GctlApp
 *
 * Retrieves the module manager owned by @self.
 *
 * Returns: (transfer none) (nullable): the #GctlModuleManager, or
 *   %NULL if the app has not been initialized
 */
GctlModuleManager *
gctl_app_get_module_manager(GctlApp *self)
{
	GctlAppPrivate *priv;

	g_return_val_if_fail(GCTL_IS_APP(self), NULL);

	priv = gctl_app_get_instance_private(self);
	return priv->module_manager;
}

/**
 * gctl_app_get_dry_run:
 * @self: a #GctlApp
 *
 * Returns whether the application is in dry-run mode.
 *
 * Returns: %TRUE if dry-run mode is active
 */
gboolean
gctl_app_get_dry_run(GctlApp *self)
{
	GctlAppPrivate *priv;

	g_return_val_if_fail(GCTL_IS_APP(self), FALSE);

	priv = gctl_app_get_instance_private(self);
	return priv->dry_run;
}

/**
 * gctl_app_set_dry_run:
 * @self: a #GctlApp
 * @dry_run: whether to enable dry-run mode
 *
 * Sets the dry-run flag on the application.  When an executor is
 * present its dry-run state is kept in sync.
 */
void
gctl_app_set_dry_run(
	GctlApp  *self,
	gboolean  dry_run
){
	GctlAppPrivate *priv;

	g_return_if_fail(GCTL_IS_APP(self));

	priv = gctl_app_get_instance_private(self);

	dry_run = !!dry_run;
	if (priv->dry_run == dry_run)
		return;

	priv->dry_run = dry_run;

	/* Keep the executor in sync */
	if (priv->executor != NULL)
		gctl_executor_set_dry_run(priv->executor, dry_run);

	g_object_notify_by_pspec(G_OBJECT(self), props[PROP_DRY_RUN]);
}

/**
 * gctl_app_get_output_format:
 * @self: a #GctlApp
 *
 * Returns the currently selected output format.
 *
 * Returns: the #GctlOutputFormat
 */
GctlOutputFormat
gctl_app_get_output_format(GctlApp *self)
{
	GctlAppPrivate *priv;

	g_return_val_if_fail(GCTL_IS_APP(self), GCTL_OUTPUT_FORMAT_TABLE);

	priv = gctl_app_get_instance_private(self);
	return priv->output_format;
}

/**
 * gctl_app_set_output_format:
 * @self: a #GctlApp
 * @format: the desired #GctlOutputFormat
 *
 * Sets the output format used by the formatter.
 */
void
gctl_app_set_output_format(
	GctlApp          *self,
	GctlOutputFormat  format
){
	GctlAppPrivate *priv;

	g_return_if_fail(GCTL_IS_APP(self));

	priv = gctl_app_get_instance_private(self);

	if (priv->output_format == format)
		return;

	priv->output_format = format;
	g_object_notify_by_pspec(G_OBJECT(self), props[PROP_OUTPUT_FORMAT]);
}

/**
 * gctl_app_get_verbose:
 * @self: a #GctlApp
 *
 * Returns whether verbose logging is enabled.
 *
 * Returns: %TRUE if verbose mode is active
 */
gboolean
gctl_app_get_verbose(GctlApp *self)
{
	GctlAppPrivate *priv;

	g_return_val_if_fail(GCTL_IS_APP(self), FALSE);

	priv = gctl_app_get_instance_private(self);
	return priv->verbose;
}

/**
 * gctl_app_set_verbose:
 * @self: a #GctlApp
 * @verbose: whether to enable verbose logging
 *
 * Sets the verbose flag on the application.
 */
void
gctl_app_set_verbose(
	GctlApp  *self,
	gboolean  verbose
){
	GctlAppPrivate *priv;

	g_return_if_fail(GCTL_IS_APP(self));

	priv = gctl_app_get_instance_private(self);

	verbose = !!verbose;
	if (priv->verbose == verbose)
		return;

	priv->verbose = verbose;
	g_object_notify_by_pspec(G_OBJECT(self), props[PROP_VERBOSE]);
}
