/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-forge-context.h - Boxed type describing the detected forge environment */

#ifndef GCTL_FORGE_CONTEXT_H
#define GCTL_FORGE_CONTEXT_H

#if !defined(GCTL_INSIDE) && !defined(GCTL_COMPILATION)
#error "Only <gitctl.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * GCTL_TYPE_FORGE_CONTEXT:
 *
 * The #GType for #GctlForgeContext.
 */
#define GCTL_TYPE_FORGE_CONTEXT (gctl_forge_context_get_type())

/**
 * GctlForgeContext:
 *
 * An opaque boxed type that captures the detected forge environment for
 * a git repository: the forge type, remote URL components, and the
 * resolved path to the corresponding CLI tool.
 */
struct _GctlForgeContext
{
	GctlForgeType  forge_type;
	gchar         *remote_url;
	gchar         *owner;
	gchar         *repo_name;
	gchar         *host;
	gchar         *cli_tool;
};

/**
 * gctl_forge_context_get_type:
 *
 * Registers and returns the #GType for #GctlForgeContext.
 *
 * Returns: the #GType
 */
GType
gctl_forge_context_get_type(void) G_GNUC_CONST;

/**
 * gctl_forge_context_new:
 * @forge_type: the #GctlForgeType
 * @remote_url: (nullable): the full remote URL
 * @owner: (nullable): the repository owner or namespace
 * @repo_name: (nullable): the repository name
 * @host: (nullable): the hostname of the forge
 * @cli_tool: (nullable): resolved path to the CLI tool (gh, glab, fj, tea)
 *
 * Creates a new #GctlForgeContext.  All strings are deep-copied.
 *
 * Returns: (transfer full): a newly allocated #GctlForgeContext
 */
GctlForgeContext *
gctl_forge_context_new(
	GctlForgeType   forge_type,
	const gchar    *remote_url,
	const gchar    *owner,
	const gchar    *repo_name,
	const gchar    *host,
	const gchar    *cli_tool
);

/**
 * gctl_forge_context_copy:
 * @src: (not nullable): the #GctlForgeContext to copy
 *
 * Creates a deep copy of @src, duplicating all string fields.
 *
 * Returns: (transfer full): a newly allocated copy of @src
 */
GctlForgeContext *
gctl_forge_context_copy(const GctlForgeContext *src);

/**
 * gctl_forge_context_free:
 * @self: (nullable): a #GctlForgeContext, or %NULL
 *
 * Frees all memory associated with @self.  If @self is %NULL this
 * function is a no-op.
 */
void
gctl_forge_context_free(GctlForgeContext *self);

/**
 * gctl_forge_context_get_forge_type:
 * @self: a #GctlForgeContext
 *
 * Returns the forge type.
 *
 * Returns: the #GctlForgeType
 */
GctlForgeType
gctl_forge_context_get_forge_type(const GctlForgeContext *self);

/**
 * gctl_forge_context_get_remote_url:
 * @self: a #GctlForgeContext
 *
 * Returns the full remote URL.
 *
 * Returns: (transfer none) (nullable): the remote URL
 */
const gchar *
gctl_forge_context_get_remote_url(const GctlForgeContext *self);

/**
 * gctl_forge_context_get_owner:
 * @self: a #GctlForgeContext
 *
 * Returns the repository owner or namespace.
 *
 * Returns: (transfer none) (nullable): the owner
 */
const gchar *
gctl_forge_context_get_owner(const GctlForgeContext *self);

/**
 * gctl_forge_context_get_repo_name:
 * @self: a #GctlForgeContext
 *
 * Returns the repository name.
 *
 * Returns: (transfer none) (nullable): the repository name
 */
const gchar *
gctl_forge_context_get_repo_name(const GctlForgeContext *self);

/**
 * gctl_forge_context_get_host:
 * @self: a #GctlForgeContext
 *
 * Returns the hostname of the forge.
 *
 * Returns: (transfer none) (nullable): the hostname
 */
const gchar *
gctl_forge_context_get_host(const GctlForgeContext *self);

/**
 * gctl_forge_context_get_cli_tool:
 * @self: a #GctlForgeContext
 *
 * Returns the resolved path to the CLI tool.
 *
 * Returns: (transfer none) (nullable): the CLI tool path
 */
const gchar *
gctl_forge_context_get_cli_tool(const GctlForgeContext *self);

/**
 * gctl_forge_context_get_owner_repo:
 * @self: a #GctlForgeContext
 *
 * Builds and returns the "owner/repo" string by joining the owner
 * and repo_name fields with a forward slash.
 *
 * Returns: (transfer full): a newly allocated "owner/repo" string
 */
gchar *
gctl_forge_context_get_owner_repo(const GctlForgeContext *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GctlForgeContext, gctl_forge_context_free)

G_END_DECLS

#endif /* GCTL_FORGE_CONTEXT_H */
