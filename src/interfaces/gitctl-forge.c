/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-forge.c - GInterface implementation for forge backends */

#define GCTL_COMPILATION
#include "gitctl.h"

/* ── Interface boilerplate ─────────────────────────────────────────── */

G_DEFINE_INTERFACE(GctlForge, gctl_forge, G_TYPE_OBJECT)

static void
gctl_forge_default_init(GctlForgeInterface *iface)
{
	/* No default implementations — every method must be provided by
	 * the concrete forge backend. */
	(void)iface;
}

/* ── Public wrapper functions ──────────────────────────────────────── */

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
gctl_forge_get_name(GctlForge *self)
{
	g_return_val_if_fail(GCTL_IS_FORGE(self), NULL);

	return GCTL_FORGE_GET_IFACE(self)->get_name(self);
}

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
gctl_forge_get_cli_tool(GctlForge *self)
{
	g_return_val_if_fail(GCTL_IS_FORGE(self), NULL);

	return GCTL_FORGE_GET_IFACE(self)->get_cli_tool(self);
}

/**
 * gctl_forge_get_forge_type:
 * @self: a #GctlForge
 *
 * Returns the #GctlForgeType that identifies this forge backend.
 *
 * Returns: the forge type
 */
GctlForgeType
gctl_forge_get_forge_type(GctlForge *self)
{
	g_return_val_if_fail(GCTL_IS_FORGE(self), GCTL_FORGE_TYPE_UNKNOWN);

	return GCTL_FORGE_GET_IFACE(self)->get_forge_type(self);
}

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
){
	g_return_val_if_fail(GCTL_IS_FORGE(self), FALSE);
	g_return_val_if_fail(remote_url != NULL, FALSE);

	return GCTL_FORGE_GET_IFACE(self)->can_handle_url(self, remote_url);
}

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
gctl_forge_is_available(GctlForge *self)
{
	g_return_val_if_fail(GCTL_IS_FORGE(self), FALSE);

	return GCTL_FORGE_GET_IFACE(self)->is_available(self);
}

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
){
	GctlForgeInterface *iface;

	g_return_val_if_fail(GCTL_IS_FORGE(self), NULL);
	g_return_val_if_fail(context != NULL, NULL);

	iface = GCTL_FORGE_GET_IFACE(self);

	if (iface->build_argv == NULL) {
		g_set_error(
			error,
			GCTL_ERROR,
			GCTL_ERROR_FORGE_UNSUPPORTED,
			"Forge '%s' does not support building command argv",
			gctl_forge_get_name(self)
		);
		return NULL;
	}

	return iface->build_argv(self, resource, verb, context, params, error);
}

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
){
	g_return_val_if_fail(GCTL_IS_FORGE(self), NULL);
	g_return_val_if_fail(raw_output != NULL, NULL);

	return GCTL_FORGE_GET_IFACE(self)->parse_list_output(
		self, resource, raw_output, error
	);
}

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
){
	g_return_val_if_fail(GCTL_IS_FORGE(self), NULL);
	g_return_val_if_fail(raw_output != NULL, NULL);

	return GCTL_FORGE_GET_IFACE(self)->parse_get_output(
		self, resource, raw_output, error
	);
}

/**
 * gctl_forge_build_api_argv:
 * @self: a #GctlForge
 * @method: the HTTP method (e.g. "GET", "POST")
 * @endpoint: the API endpoint path
 * @body: (nullable): optional JSON request body, or %NULL
 * @context: (transfer none) (nullable): the forge context (owner, repo, host, etc.)
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
	GctlForge          *self,
	const gchar        *method,
	const gchar        *endpoint,
	const gchar        *body,
	GctlForgeContext   *context,
	GError            **error
){
	GctlForgeInterface *iface;

	g_return_val_if_fail(GCTL_IS_FORGE(self), NULL);
	g_return_val_if_fail(method != NULL, NULL);
	g_return_val_if_fail(endpoint != NULL, NULL);

	iface = GCTL_FORGE_GET_IFACE(self);

	if (iface->build_api_argv == NULL) {
		g_set_error(
			error,
			GCTL_ERROR,
			GCTL_ERROR_FORGE_UNSUPPORTED,
			"Forge '%s' does not support raw API passthrough",
			gctl_forge_get_name(self)
		);
		return NULL;
	}

	return iface->build_api_argv(self, method, endpoint, body, context, error);
}
