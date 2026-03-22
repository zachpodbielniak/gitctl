/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-module-manager.h - Module loader and manager */

#ifndef GCTL_MODULE_MANAGER_H
#define GCTL_MODULE_MANAGER_H

#if !defined(GCTL_INSIDE) && !defined(GCTL_COMPILATION)
#error "Only <gitctl.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define GCTL_TYPE_MODULE_MANAGER (gctl_module_manager_get_type())

G_DECLARE_FINAL_TYPE(GctlModuleManager, gctl_module_manager, GCTL, MODULE_MANAGER, GObject)

/**
 * gctl_module_manager_new:
 *
 * Creates a new #GctlModuleManager with an empty module list.
 *
 * Returns: (transfer full): a new #GctlModuleManager
 */
GctlModuleManager *
gctl_module_manager_new(void);

/**
 * gctl_module_manager_load_from_directory:
 * @self: a #GctlModuleManager
 * @dir_path: the path to a directory containing module .so files
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Scans @dir_path for shared library files (`.so` suffix), loads each
 * one with g_module_open(), and looks for a `gctl_module_register`
 * entry point symbol.  The symbol must be a function returning a
 * #GType that is a subclass of #GctlModule.
 *
 * Modules that fail to load are logged as warnings and skipped; the
 * scan continues with the next file.  The error parameter is only set
 * if the directory itself cannot be opened.
 *
 * Returns: %TRUE if the directory was scanned (even if individual
 *     modules failed), %FALSE if @dir_path could not be opened
 */
gboolean
gctl_module_manager_load_from_directory(
	GctlModuleManager *self,
	const gchar       *dir_path,
	GError           **error
);

/**
 * gctl_module_manager_register:
 * @self: a #GctlModuleManager
 * @module: (transfer none): the #GctlModule to register
 *
 * Adds a pre-built module to the manager's internal list.  A
 * reference is taken on @module.  This is useful for statically
 * linked modules that do not need to be loaded from a shared
 * library.
 *
 * Returns: %TRUE if the module was added, %FALSE if @module is
 *     %NULL or not a valid #GctlModule
 */
gboolean
gctl_module_manager_register(
	GctlModuleManager *self,
	GctlModule        *module
);

/**
 * gctl_module_manager_activate_all:
 * @self: a #GctlModuleManager
 *
 * Calls gctl_module_activate() on every registered module.  Modules
 * that fail to activate are logged as warnings.
 */
void
gctl_module_manager_activate_all(GctlModuleManager *self);

/**
 * gctl_module_manager_deactivate_all:
 * @self: a #GctlModuleManager
 *
 * Calls gctl_module_deactivate() on every registered module.
 */
void
gctl_module_manager_deactivate_all(GctlModuleManager *self);

/**
 * gctl_module_manager_get_modules:
 * @self: a #GctlModuleManager
 *
 * Returns the array of registered #GctlModule instances.  The
 * returned array is owned by the manager and must not be freed.
 *
 * Returns: (transfer none) (element-type GctlModule): the module array
 */
GPtrArray *
gctl_module_manager_get_modules(GctlModuleManager *self);

/**
 * gctl_module_manager_find_forge:
 * @self: a #GctlModuleManager
 * @forge_type: the #GctlForgeType to search for
 *
 * Iterates over all registered modules looking for one that
 * implements the #GctlForge interface and whose
 * gctl_forge_get_forge_type() matches @forge_type.
 *
 * Returns: (transfer none) (nullable): the matching #GctlForge, or
 *     %NULL if no module provides the requested forge type
 */
GctlForge *
gctl_module_manager_find_forge(
	GctlModuleManager *self,
	GctlForgeType      forge_type
);

/**
 * gctl_module_manager_find_forge_for_url:
 * @self: a #GctlModuleManager
 * @remote_url: the git remote URL to match against
 *
 * Iterates over all registered modules that implement the #GctlForge
 * interface and calls gctl_forge_can_handle_url() on each until one
 * returns %TRUE.
 *
 * Returns: (transfer none) (nullable): the matching #GctlForge, or
 *     %NULL if no module can handle @remote_url
 */
GctlForge *
gctl_module_manager_find_forge_for_url(
	GctlModuleManager *self,
	const gchar       *remote_url
);

G_END_DECLS

#endif /* GCTL_MODULE_MANAGER_H */
