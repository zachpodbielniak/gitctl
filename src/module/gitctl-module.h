/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-module.h - Derivable base class for forge backend modules */

#ifndef GCTL_MODULE_H
#define GCTL_MODULE_H

#if !defined(GCTL_INSIDE) && !defined(GCTL_COMPILATION)
#error "Only <gitctl.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define GCTL_TYPE_MODULE (gctl_module_get_type())

G_DECLARE_DERIVABLE_TYPE(GctlModule, gctl_module, GCTL, MODULE, GObject)

/**
 * GctlModuleClass:
 * @parent_class: the parent class structure
 * @activate: virtual method called to activate the module; return %TRUE
 *     on success
 * @deactivate: virtual method called to deactivate the module
 * @get_name: virtual method returning the module's human-readable name
 * @get_description: virtual method returning a short description of the
 *     module
 *
 * The class structure for #GctlModule.  Forge backend modules subclass
 * this type and override the virtual methods.  Padding is reserved for
 * future expansion so the ABI stays stable across minor releases.
 */
struct _GctlModuleClass
{
	GObjectClass parent_class;

	gboolean     (*activate)        (GctlModule *self);
	void         (*deactivate)      (GctlModule *self);
	const gchar *(*get_name)        (GctlModule *self);
	const gchar *(*get_description) (GctlModule *self);

	/*< private >*/
	gpointer padding[7];
};

/**
 * gctl_module_activate:
 * @self: a #GctlModule
 *
 * Activates the module by calling the subclass activate() virtual
 * method.  If the virtual method returns %TRUE the module is marked
 * as active.
 *
 * Returns: %TRUE if activation succeeded
 */
gboolean
gctl_module_activate(GctlModule *self);

/**
 * gctl_module_deactivate:
 * @self: a #GctlModule
 *
 * Deactivates the module by calling the subclass deactivate() virtual
 * method and marking the module as inactive.
 */
void
gctl_module_deactivate(GctlModule *self);

/**
 * gctl_module_get_name:
 * @self: a #GctlModule
 *
 * Returns the human-readable name of this module.  If the subclass
 * does not override get_name(), a default string is returned.
 *
 * Returns: (transfer none): the module name
 */
const gchar *
gctl_module_get_name(GctlModule *self);

/**
 * gctl_module_get_description:
 * @self: a #GctlModule
 *
 * Returns a short description of what this module provides.  If the
 * subclass does not override get_description(), a default string is
 * returned.
 *
 * Returns: (transfer none): the module description
 */
const gchar *
gctl_module_get_description(GctlModule *self);

/**
 * gctl_module_get_priority:
 * @self: a #GctlModule
 *
 * Returns the load priority of the module.  Lower values mean higher
 * priority.  The default priority is 0.
 *
 * Returns: the module priority
 */
gint
gctl_module_get_priority(GctlModule *self);

/**
 * gctl_module_set_priority:
 * @self: a #GctlModule
 * @priority: the new priority value
 *
 * Sets the load priority of the module.  Lower values mean higher
 * priority.
 */
void
gctl_module_set_priority(
	GctlModule *self,
	gint        priority
);

/**
 * gctl_module_is_active:
 * @self: a #GctlModule
 *
 * Returns whether the module is currently active.
 *
 * Returns: %TRUE if the module has been successfully activated
 */
gboolean
gctl_module_is_active(GctlModule *self);

G_END_DECLS

#endif /* GCTL_MODULE_H */
