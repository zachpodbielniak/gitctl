/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-app.h - Main application object */

#ifndef GCTL_APP_H
#define GCTL_APP_H

#if !defined(GCTL_INSIDE) && !defined(GCTL_COMPILATION)
#error "Only <gitctl.h> can be included directly."
#endif

#include <glib-object.h>
#include "gitctl-enums.h"
#include "gitctl-types.h"

G_BEGIN_DECLS

#define GCTL_TYPE_APP (gctl_app_get_type())

G_DECLARE_DERIVABLE_TYPE(GctlApp, gctl_app, GCTL, APP, GObject)

/**
 * GctlAppClass:
 * @parent_class: the parent class structure
 *
 * The class structure for #GctlApp.  Padding is reserved for future
 * virtual methods so the ABI stays stable across minor releases.
 */
struct _GctlAppClass
{
	GObjectClass parent_class;

	/*< private >*/
	gpointer padding[7];
};

/**
 * gctl_app_new:
 *
 * Creates a new #GctlApp instance.  Call gctl_app_initialize() before
 * using any of the subsystem accessors.
 *
 * Returns: (transfer full): a new #GctlApp
 */
GctlApp *
gctl_app_new(void);

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
);

/**
 * gctl_app_get_executor:
 * @self: a #GctlApp
 *
 * Retrieves the subprocess executor owned by @self.
 * The caller must not free the returned pointer.
 *
 * Returns: (transfer none) (nullable): the #GctlExecutor, or %NULL
 *   if the app has not been initialized
 */
GctlExecutor *
gctl_app_get_executor(GctlApp *self);

/**
 * gctl_app_get_resolver:
 * @self: a #GctlApp
 *
 * Retrieves the context resolver owned by @self.
 * The caller must not free the returned pointer.
 *
 * Returns: (transfer none) (nullable): the #GctlContextResolver, or
 *   %NULL if the app has not been initialized
 */
GctlContextResolver *
gctl_app_get_resolver(GctlApp *self);

/**
 * gctl_app_get_config:
 * @self: a #GctlApp
 *
 * Retrieves the configuration object owned by @self.
 * The caller must not free the returned pointer.
 *
 * Returns: (transfer none) (nullable): the #GctlConfig, or %NULL if
 *   the app has not been initialized
 */
GctlConfig *
gctl_app_get_config(GctlApp *self);

/**
 * gctl_app_get_formatter:
 * @self: a #GctlApp
 *
 * Retrieves the output formatter owned by @self.
 * The caller must not free the returned pointer.
 *
 * Returns: (transfer none) (nullable): the #GctlOutputFormatter, or
 *   %NULL if the app has not been initialized
 */
GctlOutputFormatter *
gctl_app_get_formatter(GctlApp *self);

/**
 * gctl_app_get_module_manager:
 * @self: a #GctlApp
 *
 * Retrieves the module manager owned by @self.
 * The caller must not free the returned pointer.
 *
 * Returns: (transfer none) (nullable): the #GctlModuleManager, or
 *   %NULL if the app has not been initialized
 */
GctlModuleManager *
gctl_app_get_module_manager(GctlApp *self);

/**
 * gctl_app_get_dry_run:
 * @self: a #GctlApp
 *
 * Returns whether the application is in dry-run mode.
 *
 * Returns: %TRUE if dry-run mode is active
 */
gboolean
gctl_app_get_dry_run(GctlApp *self);

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
);

/**
 * gctl_app_get_output_format:
 * @self: a #GctlApp
 *
 * Returns the currently selected output format.
 *
 * Returns: the #GctlOutputFormat
 */
GctlOutputFormat
gctl_app_get_output_format(GctlApp *self);

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
);

/**
 * gctl_app_get_verbose:
 * @self: a #GctlApp
 *
 * Returns whether verbose logging is enabled.
 *
 * Returns: %TRUE if verbose mode is active
 */
gboolean
gctl_app_get_verbose(GctlApp *self);

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
);

G_END_DECLS

#endif /* GCTL_APP_H */
