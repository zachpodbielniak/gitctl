/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-module.c - Derivable base class for forge backend modules */

#define GCTL_COMPILATION
#include "gitctl.h"

/* ── Private data ────────────────────────────────────────────────────── */

typedef struct _GctlModulePrivate
{
	gint     priority;
	gboolean active;
} GctlModulePrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GctlModule, gctl_module, G_TYPE_OBJECT)

/* ── Property identifiers ────────────────────────────────────────────── */

enum
{
	PROP_0,
	PROP_PRIORITY,
	PROP_ACTIVE,
	N_PROPS
};

static GParamSpec *props[N_PROPS] = { NULL };

/* ── GObject virtual methods ─────────────────────────────────────────── */

static void
gctl_module_get_property(
	GObject    *object,
	guint       prop_id,
	GValue     *value,
	GParamSpec *pspec
){
	GctlModulePrivate *priv;

	priv = gctl_module_get_instance_private(GCTL_MODULE(object));

	switch (prop_id) {
	case PROP_PRIORITY:
		g_value_set_int(value, priv->priority);
		break;
	case PROP_ACTIVE:
		g_value_set_boolean(value, priv->active);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
gctl_module_set_property(
	GObject      *object,
	guint         prop_id,
	const GValue *value,
	GParamSpec   *pspec
){
	GctlModule *self;

	self = GCTL_MODULE(object);

	switch (prop_id) {
	case PROP_PRIORITY:
		gctl_module_set_priority(self, g_value_get_int(value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

/* ── Class and instance init ─────────────────────────────────────────── */

static void
gctl_module_class_init(GctlModuleClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);

	object_class->get_property = gctl_module_get_property;
	object_class->set_property = gctl_module_set_property;

	/* Default virtual methods are NULL — subclasses override them */
	klass->activate        = NULL;
	klass->deactivate      = NULL;
	klass->get_name        = NULL;
	klass->get_description = NULL;

	/* ── Properties ─────────────────────────────────────────────── */

	/**
	 * GctlModule:priority:
	 *
	 * The load priority of the module.  Lower values mean higher
	 * priority.  The default is 0.
	 */
	props[PROP_PRIORITY] =
		g_param_spec_int(
			"priority",
			"Priority",
			"Load priority of the module (lower = higher priority)",
			G_MININT, G_MAXINT,
			0,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS
		);

	/**
	 * GctlModule:active:
	 *
	 * Whether the module is currently active.  This property is
	 * read-only; call gctl_module_activate() or
	 * gctl_module_deactivate() to change the state.
	 */
	props[PROP_ACTIVE] =
		g_param_spec_boolean(
			"active",
			"Active",
			"Whether the module is currently active",
			FALSE,
			G_PARAM_READABLE | G_PARAM_STATIC_STRINGS
		);

	g_object_class_install_properties(object_class, N_PROPS, props);
}

static void
gctl_module_init(GctlModule *self)
{
	GctlModulePrivate *priv;

	priv = gctl_module_get_instance_private(self);

	priv->priority = 0;
	priv->active   = FALSE;
}

/* ── Public API ──────────────────────────────────────────────────────── */

/**
 * gctl_module_activate:
 * @self: a #GctlModule
 *
 * Activates the module by calling the subclass activate() virtual
 * method.  If the virtual method returns %TRUE the module is marked
 * as active.  If the subclass does not implement activate(), the
 * module is simply marked as active.
 *
 * Returns: %TRUE if activation succeeded
 */
gboolean
gctl_module_activate(GctlModule *self)
{
	GctlModuleClass   *klass;
	GctlModulePrivate *priv;
	gboolean           result;

	g_return_val_if_fail(GCTL_IS_MODULE(self), FALSE);

	priv  = gctl_module_get_instance_private(self);
	klass = GCTL_MODULE_GET_CLASS(self);

	if (priv->active)
		return TRUE;

	/*
	 * If the subclass provides an activate vfunc, call it and use
	 * its return value.  Otherwise assume activation succeeds.
	 */
	if (klass->activate != NULL)
		result = klass->activate(self);
	else
		result = TRUE;

	if (result) {
		priv->active = TRUE;
		g_object_notify_by_pspec(G_OBJECT(self), props[PROP_ACTIVE]);
	}

	return result;
}

/**
 * gctl_module_deactivate:
 * @self: a #GctlModule
 *
 * Deactivates the module by calling the subclass deactivate() virtual
 * method and marking the module as inactive.
 */
void
gctl_module_deactivate(GctlModule *self)
{
	GctlModuleClass   *klass;
	GctlModulePrivate *priv;

	g_return_if_fail(GCTL_IS_MODULE(self));

	priv  = gctl_module_get_instance_private(self);
	klass = GCTL_MODULE_GET_CLASS(self);

	if (!priv->active)
		return;

	if (klass->deactivate != NULL)
		klass->deactivate(self);

	priv->active = FALSE;
	g_object_notify_by_pspec(G_OBJECT(self), props[PROP_ACTIVE]);
}

/**
 * gctl_module_get_name:
 * @self: a #GctlModule
 *
 * Returns the human-readable name of this module.  If the subclass
 * does not override get_name(), the string "Unknown Module" is
 * returned.
 *
 * Returns: (transfer none): the module name
 */
const gchar *
gctl_module_get_name(GctlModule *self)
{
	GctlModuleClass *klass;

	g_return_val_if_fail(GCTL_IS_MODULE(self), NULL);

	klass = GCTL_MODULE_GET_CLASS(self);

	if (klass->get_name != NULL)
		return klass->get_name(self);

	return "Unknown Module";
}

/**
 * gctl_module_get_description:
 * @self: a #GctlModule
 *
 * Returns a short description of what this module provides.  If the
 * subclass does not override get_description(), the string "No
 * description available" is returned.
 *
 * Returns: (transfer none): the module description
 */
const gchar *
gctl_module_get_description(GctlModule *self)
{
	GctlModuleClass *klass;

	g_return_val_if_fail(GCTL_IS_MODULE(self), NULL);

	klass = GCTL_MODULE_GET_CLASS(self);

	if (klass->get_description != NULL)
		return klass->get_description(self);

	return "No description available";
}

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
gctl_module_get_priority(GctlModule *self)
{
	GctlModulePrivate *priv;

	g_return_val_if_fail(GCTL_IS_MODULE(self), 0);

	priv = gctl_module_get_instance_private(self);
	return priv->priority;
}

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
){
	GctlModulePrivate *priv;

	g_return_if_fail(GCTL_IS_MODULE(self));

	priv = gctl_module_get_instance_private(self);

	if (priv->priority == priority)
		return;

	priv->priority = priority;
	g_object_notify_by_pspec(G_OBJECT(self), props[PROP_PRIORITY]);
}

/**
 * gctl_module_is_active:
 * @self: a #GctlModule
 *
 * Returns whether the module is currently active.
 *
 * Returns: %TRUE if the module has been successfully activated
 */
gboolean
gctl_module_is_active(GctlModule *self)
{
	GctlModulePrivate *priv;

	g_return_val_if_fail(GCTL_IS_MODULE(self), FALSE);

	priv = gctl_module_get_instance_private(self);
	return priv->active;
}
