/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-module-manager.c - Module loader and manager implementation */

#define GCTL_COMPILATION
#include "gitctl.h"

#include <gmodule.h>

/* ── Private structure ────────────────────────────────────────────── */

struct _GctlModuleManager
{
	GObject parent_instance;

	GPtrArray *modules;         /* element-type GctlModule, owned refs */
	GPtrArray *loaded_gmodules; /* element-type GModule*, for cleanup  */
};

G_DEFINE_TYPE(GctlModuleManager, gctl_module_manager, G_TYPE_OBJECT)

/* ── GObject vfuncs ───────────────────────────────────────────────── */

static void
gctl_module_manager_dispose(GObject *object)
{
	GctlModuleManager *self;

	self = GCTL_MODULE_MANAGER(object);

	/*
	 * Deactivate all modules before dropping references.  This
	 * gives each module a chance to clean up resources.
	 */
	if (self->modules != NULL) {
		gctl_module_manager_deactivate_all(self);
		g_clear_pointer(&self->modules, g_ptr_array_unref);
	}

	G_OBJECT_CLASS(gctl_module_manager_parent_class)->dispose(object);
}

static void
gctl_module_manager_finalize(GObject *object)
{
	GctlModuleManager *self;

	self = GCTL_MODULE_MANAGER(object);

	/*
	 * Close loaded GModule handles.  This must happen after
	 * dispose has released all module GObject instances, because
	 * the GType code lives inside the shared libraries.
	 */
	if (self->loaded_gmodules != NULL) {
		guint i;

		for (i = 0; i < self->loaded_gmodules->len; i++) {
			GModule *gmod;

			gmod = (GModule *)g_ptr_array_index(self->loaded_gmodules, i);
			if (gmod != NULL)
				g_module_close(gmod);
		}
		g_ptr_array_unref(self->loaded_gmodules);
		self->loaded_gmodules = NULL;
	}

	G_OBJECT_CLASS(gctl_module_manager_parent_class)->finalize(object);
}

/* ── Class and instance init ──────────────────────────────────────── */

static void
gctl_module_manager_class_init(GctlModuleManagerClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);

	object_class->dispose  = gctl_module_manager_dispose;
	object_class->finalize = gctl_module_manager_finalize;
}

static void
gctl_module_manager_init(GctlModuleManager *self)
{
	self->modules = g_ptr_array_new_with_free_func(g_object_unref);
	self->loaded_gmodules = g_ptr_array_new();
}

/* ── Public API ───────────────────────────────────────────────────── */

/**
 * gctl_module_manager_new:
 *
 * Creates a new #GctlModuleManager with an empty module list.
 *
 * Returns: (transfer full): a new #GctlModuleManager
 */
GctlModuleManager *
gctl_module_manager_new(void)
{
	return (GctlModuleManager *)g_object_new(GCTL_TYPE_MODULE_MANAGER, NULL);
}

/**
 * gctl_module_manager_load_from_directory:
 * @self: a #GctlModuleManager
 * @dir_path: the path to a directory containing module .so files
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Scans @dir_path for shared library files (`.so` suffix), loads each
 * one with g_module_open(), and looks for a `gctl_module_register`
 * entry point symbol.  The returned GType must be a subclass of
 * #GctlModule.
 *
 * Modules that fail to load are logged as warnings and skipped.
 *
 * Returns: %TRUE if the directory was scanned successfully, %FALSE
 *     if @dir_path could not be opened
 */
gboolean
gctl_module_manager_load_from_directory(
	GctlModuleManager *self,
	const gchar       *dir_path,
	GError           **error
){
	GDir        *dir;
	const gchar *entry;

	g_return_val_if_fail(GCTL_IS_MODULE_MANAGER(self), FALSE);
	g_return_val_if_fail(dir_path != NULL, FALSE);

	dir = g_dir_open(dir_path, 0, error);
	if (dir == NULL)
		return FALSE;

	while ((entry = g_dir_read_name(dir)) != NULL) {
		g_autofree gchar *full_path = NULL;
		GModule          *gmod;
		gpointer          symbol;
		GType             mod_type;
		GctlModule       *mod;

		/* Only consider files with .so suffix */
		if (!g_str_has_suffix(entry, ".so"))
			continue;

		full_path = g_build_filename(dir_path, entry, NULL);

		/* Attempt to open the shared library */
		gmod = g_module_open(full_path, G_MODULE_BIND_LAZY);
		if (gmod == NULL) {
			g_warning("gitctl: failed to load module '%s': %s",
			          entry, g_module_error());
			continue;
		}

		/* Look for the registration entry point */
		if (!g_module_symbol(gmod, "gctl_module_register", &symbol)) {
			g_warning("gitctl: module '%s' has no gctl_module_register symbol: %s",
			          entry, g_module_error());
			g_module_close(gmod);
			continue;
		}

		/*
		 * Call the register function.  It must return a GType that
		 * is a subclass of GctlModule.
		 */
		{
			GType (*register_func)(void) = (GType (*)(void))symbol;

			mod_type = register_func();
		}

		if (!g_type_is_a(mod_type, GCTL_TYPE_MODULE)) {
			g_warning("gitctl: module '%s' registered type '%s' which is "
			          "not a GctlModule subclass",
			          entry, g_type_name(mod_type));
			g_module_close(gmod);
			continue;
		}

		/* Instantiate and register the module */
		mod = (GctlModule *)g_object_new(mod_type, NULL);

		g_ptr_array_add(self->modules, mod);
		g_ptr_array_add(self->loaded_gmodules, gmod);

		g_debug("gitctl: loaded module '%s' from %s",
		        gctl_module_get_name(mod), entry);
	}

	g_dir_close(dir);
	return TRUE;
}

/**
 * gctl_module_manager_register:
 * @self: a #GctlModuleManager
 * @module: (transfer none): the #GctlModule to register
 *
 * Adds a pre-built module to the manager's internal list.  A
 * reference is taken on @module.
 *
 * Returns: %TRUE if the module was added, %FALSE if @module is
 *     %NULL or not a valid #GctlModule
 */
gboolean
gctl_module_manager_register(
	GctlModuleManager *self,
	GctlModule        *module
){
	g_return_val_if_fail(GCTL_IS_MODULE_MANAGER(self), FALSE);
	g_return_val_if_fail(GCTL_IS_MODULE(module), FALSE);

	g_ptr_array_add(self->modules, g_object_ref(module));

	g_debug("gitctl: registered module '%s'",
	        gctl_module_get_name(module));

	return TRUE;
}

/**
 * gctl_module_manager_activate_all:
 * @self: a #GctlModuleManager
 *
 * Calls gctl_module_activate() on every registered module.  Modules
 * that fail to activate are logged as warnings.
 */
void
gctl_module_manager_activate_all(GctlModuleManager *self)
{
	guint i;

	g_return_if_fail(GCTL_IS_MODULE_MANAGER(self));

	for (i = 0; i < self->modules->len; i++) {
		GctlModule *mod;

		mod = (GctlModule *)g_ptr_array_index(self->modules, i);

		if (!gctl_module_activate(mod)) {
			g_warning("gitctl: failed to activate module '%s'",
			          gctl_module_get_name(mod));
		}
	}
}

/**
 * gctl_module_manager_deactivate_all:
 * @self: a #GctlModuleManager
 *
 * Calls gctl_module_deactivate() on every registered module.
 */
void
gctl_module_manager_deactivate_all(GctlModuleManager *self)
{
	guint i;

	g_return_if_fail(GCTL_IS_MODULE_MANAGER(self));

	for (i = 0; i < self->modules->len; i++) {
		GctlModule *mod;

		mod = (GctlModule *)g_ptr_array_index(self->modules, i);
		gctl_module_deactivate(mod);
	}
}

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
gctl_module_manager_get_modules(GctlModuleManager *self)
{
	g_return_val_if_fail(GCTL_IS_MODULE_MANAGER(self), NULL);

	return self->modules;
}

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
){
	guint i;

	g_return_val_if_fail(GCTL_IS_MODULE_MANAGER(self), NULL);

	for (i = 0; i < self->modules->len; i++) {
		GctlModule *mod;
		GctlForge  *forge;

		mod = (GctlModule *)g_ptr_array_index(self->modules, i);

		/* Check if this module also implements GctlForge */
		if (!g_type_is_a(G_OBJECT_TYPE(mod), GCTL_TYPE_FORGE))
			continue;

		forge = GCTL_FORGE(mod);

		if (gctl_forge_get_forge_type(forge) == forge_type)
			return forge;
	}

	return NULL;
}

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
){
	guint i;

	g_return_val_if_fail(GCTL_IS_MODULE_MANAGER(self), NULL);
	g_return_val_if_fail(remote_url != NULL, NULL);

	for (i = 0; i < self->modules->len; i++) {
		GctlModule *mod;
		GctlForge  *forge;

		mod = (GctlModule *)g_ptr_array_index(self->modules, i);

		/* Check if this module also implements GctlForge */
		if (!g_type_is_a(G_OBJECT_TYPE(mod), GCTL_TYPE_FORGE))
			continue;

		forge = GCTL_FORGE(mod);

		if (gctl_forge_can_handle_url(forge, remote_url))
			return forge;
	}

	return NULL;
}
