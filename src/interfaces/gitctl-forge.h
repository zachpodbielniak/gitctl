/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-forge.h - GInterface for forge backends (GitHub, GitLab, etc.) */

#ifndef GCTL_FORGE_H
#define GCTL_FORGE_H

#if !defined(GCTL_INSIDE) && !defined(GCTL_COMPILATION)
#error "Only <gitctl.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

/* Forward declarations — defined in boxed/ headers */
typedef struct _GctlForgeContext  GctlForgeContext;
typedef struct _GctlResource      GctlResource;

#define GCTL_TYPE_FORGE (gctl_forge_get_type())

G_DECLARE_INTERFACE(GctlForge, gctl_forge, GCTL, FORGE, GObject)

/**
 * GctlForgeInterface:
 * @parent_iface: the parent interface
 * @get_name: returns the human-readable forge name
 * @get_cli_tool: returns the CLI binary name (e.g. "gh", "glab")
 * @get_forge_type: returns the #GctlForgeType for this forge
 * @can_handle_url: returns %TRUE if this forge can handle @remote_url
 * @is_available: returns %TRUE if the CLI tool is found in PATH
 * @build_argv: builds a NULL-terminated argv array for the given operation
 * @parse_list_output: parses CLI JSON output into a #GPtrArray of #GctlResource
 * @parse_get_output: parses CLI JSON output into a single #GctlResource
 * @build_api_argv: builds argv for a raw API passthrough call
 *
 * The virtual function table that all forge backend implementations must
 * provide.  Each method corresponds to a public wrapper function in the
 * gctl_forge_*() namespace.
 */
struct _GctlForgeInterface
{
	GTypeInterface parent_iface;

	/* Identity */
	const gchar      *(*get_name)           (GctlForge *self);
	const gchar      *(*get_cli_tool)       (GctlForge *self);
	GctlForgeType     (*get_forge_type)     (GctlForge *self);

	/* Detection — returns TRUE if this forge can handle the given remote URL */
	gboolean          (*can_handle_url)     (GctlForge    *self,
	                                         const gchar  *remote_url);

	/* Check if the CLI tool is available in PATH */
	gboolean          (*is_available)       (GctlForge *self);

	/*
	 * Build command argv for a given operation.
	 * @params is a string->string GHashTable of operation parameters.
	 * Returns a NULL-terminated argv array, or NULL + GError.
	 */
	gchar           **(*build_argv)         (GctlForge          *self,
	                                         GctlResourceKind    resource,
	                                         GctlVerb            verb,
	                                         GctlForgeContext   *context,
	                                         GHashTable         *params,
	                                         GError            **error);

	/* Parse CLI JSON output into normalized resources */
	GPtrArray        *(*parse_list_output)  (GctlForge          *self,
	                                         GctlResourceKind    resource,
	                                         const gchar        *raw_output,
	                                         GError            **error);

	GctlResource     *(*parse_get_output)   (GctlForge          *self,
	                                         GctlResourceKind    resource,
	                                         const gchar        *raw_output,
	                                         GError            **error);

	/* API passthrough — build argv for raw API call */
	gchar           **(*build_api_argv)     (GctlForge    *self,
	                                         const gchar  *method,
	                                         const gchar  *endpoint,
	                                         const gchar  *body,
	                                         GError      **error);

	/*< private >*/
	gpointer padding[8];
};

/**
 * gctl_forge_get_name:
 * @self: a #GctlForge
 *
 * Returns the human-readable name of this forge backend (e.g. "GitHub",
 * "GitLab").
 *
 * Returns: (transfer none): the forge name
 */
const gchar *
gctl_forge_get_name(GctlForge *self);

/**
 * gctl_forge_get_cli_tool:
 * @self: a #GctlForge
 *
 * Returns the name of the CLI binary this forge delegates to (e.g. "gh",
 * "glab").
 *
 * Returns: (transfer none): the CLI tool name
 */
const gchar *
gctl_forge_get_cli_tool(GctlForge *self);

/**
 * gctl_forge_get_forge_type:
 * @self: a #GctlForge
 *
 * Returns the #GctlForgeType that identifies this forge backend.
 *
 * Returns: the forge type
 */
GctlForgeType
gctl_forge_get_forge_type(GctlForge *self);

/**
 * gctl_forge_can_handle_url:
 * @self: a #GctlForge
 * @remote_url: the git remote URL to test
 *
 * Checks whether this forge backend recognises @remote_url as one of its
 * own.  Typically this is a simple hostname check.
 *
 * Returns: %TRUE if this forge can handle the URL
 */
gboolean
gctl_forge_can_handle_url(
	GctlForge    *self,
	const gchar  *remote_url
);

/**
 * gctl_forge_is_available:
 * @self: a #GctlForge
 *
 * Checks whether the CLI tool required by this forge (as returned by
 * gctl_forge_get_cli_tool()) is present in the user's PATH.
 *
 * Returns: %TRUE if the CLI tool is available
 */
gboolean
gctl_forge_is_available(GctlForge *self);

/**
 * gctl_forge_build_argv:
 * @self: a #GctlForge
 * @resource: the kind of resource to operate on
 * @verb: the action to perform
 * @context: (transfer none): the forge context (owner, repo, remote, etc.)
 * @params: (element-type utf8 utf8) (nullable): operation-specific parameters
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Builds a NULL-terminated argument vector suitable for passing to
 * gctl_executor_run().  The returned array and its strings are owned by
 * the caller.
 *
 * If the forge implementation does not support the requested operation,
 * @error is set to %GCTL_ERROR_FORGE_UNSUPPORTED and %NULL is returned.
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): the argv
 *     array, or %NULL on error
 */
gchar **
gctl_forge_build_argv(
	GctlForge          *self,
	GctlResourceKind    resource,
	GctlVerb            verb,
	GctlForgeContext   *context,
	GHashTable         *params,
	GError            **error
);

/**
 * gctl_forge_parse_list_output:
 * @self: a #GctlForge
 * @resource: the kind of resource that was listed
 * @raw_output: the raw JSON output from the CLI tool
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Parses the JSON output produced by a list command and returns a
 * #GPtrArray of #GctlResource instances.
 *
 * Returns: (transfer full) (element-type GctlResource) (nullable): an array
 *     of resources, or %NULL on error
 */
GPtrArray *
gctl_forge_parse_list_output(
	GctlForge          *self,
	GctlResourceKind    resource,
	const gchar        *raw_output,
	GError            **error
);

/**
 * gctl_forge_parse_get_output:
 * @self: a #GctlForge
 * @resource: the kind of resource that was fetched
 * @raw_output: the raw JSON output from the CLI tool
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Parses the JSON output produced by a get/view command and returns a
 * single #GctlResource.
 *
 * Returns: (transfer full) (nullable): the parsed resource, or %NULL on error
 */
GctlResource *
gctl_forge_parse_get_output(
	GctlForge          *self,
	GctlResourceKind    resource,
	const gchar        *raw_output,
	GError            **error
);

/**
 * gctl_forge_build_api_argv:
 * @self: a #GctlForge
 * @method: the HTTP method (e.g. "GET", "POST")
 * @endpoint: the API endpoint path
 * @body: (nullable): optional JSON request body, or %NULL
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Builds a NULL-terminated argument vector for a raw API passthrough
 * call to the forge's REST API.
 *
 * If the forge implementation does not support raw API calls, @error is
 * set to %GCTL_ERROR_FORGE_UNSUPPORTED and %NULL is returned.
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): the argv
 *     array, or %NULL on error
 */
gchar **
gctl_forge_build_api_argv(
	GctlForge    *self,
	const gchar  *method,
	const gchar  *endpoint,
	const gchar  *body,
	GError      **error
);

G_END_DECLS

#endif /* GCTL_FORGE_H */
