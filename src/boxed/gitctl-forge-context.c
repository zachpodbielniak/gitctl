/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-forge-context.c - Boxed type describing the detected forge environment */

#define GCTL_COMPILATION
#include "gitctl.h"

/* ── Boxed type registration ───────────────────────────────────────── */

G_DEFINE_BOXED_TYPE(
	GctlForgeContext,
	gctl_forge_context,
	gctl_forge_context_copy,
	gctl_forge_context_free
)

/* ── Constructor ───────────────────────────────────────────────────── */

/**
 * gctl_forge_context_new:
 * @forge_type: the #GctlForgeType
 * @remote_url: (nullable): the full remote URL
 * @owner: (nullable): the repository owner or namespace
 * @repo_name: (nullable): the repository name
 * @host: (nullable): the hostname of the forge
 * @cli_tool: (nullable): resolved path to the CLI tool (gh, glab, fj, tea)
 *
 * Creates a new #GctlForgeContext.  All strings are deep-copied so the
 * caller retains ownership of the originals.
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
){
	GctlForgeContext *self;

	self = g_new0(GctlForgeContext, 1);

	self->forge_type  = forge_type;
	self->remote_url  = g_strdup(remote_url);
	self->owner       = g_strdup(owner);
	self->repo_name   = g_strdup(repo_name);
	self->host        = g_strdup(host);
	self->cli_tool    = g_strdup(cli_tool);

	return self;
}

/* ── Copy / Free ───────────────────────────────────────────────────── */

/**
 * gctl_forge_context_copy:
 * @src: (not nullable): the #GctlForgeContext to copy
 *
 * Creates a deep copy of @src, duplicating all string fields.
 *
 * Returns: (transfer full): a newly allocated copy of @src
 */
GctlForgeContext *
gctl_forge_context_copy(const GctlForgeContext *src)
{
	g_return_val_if_fail(src != NULL, NULL);

	return gctl_forge_context_new(
		src->forge_type,
		src->remote_url,
		src->owner,
		src->repo_name,
		src->host,
		src->cli_tool
	);
}

/**
 * gctl_forge_context_free:
 * @self: (nullable): a #GctlForgeContext, or %NULL
 *
 * Frees all memory associated with @self.  If @self is %NULL this
 * function is a no-op.
 */
void
gctl_forge_context_free(GctlForgeContext *self)
{
	if (self == NULL)
		return;

	g_free(self->remote_url);
	g_free(self->owner);
	g_free(self->repo_name);
	g_free(self->host);
	g_free(self->cli_tool);
	g_free(self);
}

/* ── Accessors ─────────────────────────────────────────────────────── */

/**
 * gctl_forge_context_get_forge_type:
 * @self: a #GctlForgeContext
 *
 * Returns the forge type.
 *
 * Returns: the #GctlForgeType
 */
GctlForgeType
gctl_forge_context_get_forge_type(const GctlForgeContext *self)
{
	g_return_val_if_fail(self != NULL, GCTL_FORGE_TYPE_UNKNOWN);

	return self->forge_type;
}

/**
 * gctl_forge_context_get_remote_url:
 * @self: a #GctlForgeContext
 *
 * Returns the full remote URL.
 *
 * Returns: (transfer none) (nullable): the remote URL
 */
const gchar *
gctl_forge_context_get_remote_url(const GctlForgeContext *self)
{
	g_return_val_if_fail(self != NULL, NULL);

	return self->remote_url;
}

/**
 * gctl_forge_context_get_owner:
 * @self: a #GctlForgeContext
 *
 * Returns the repository owner or namespace.
 *
 * Returns: (transfer none) (nullable): the owner
 */
const gchar *
gctl_forge_context_get_owner(const GctlForgeContext *self)
{
	g_return_val_if_fail(self != NULL, NULL);

	return self->owner;
}

/**
 * gctl_forge_context_get_repo_name:
 * @self: a #GctlForgeContext
 *
 * Returns the repository name.
 *
 * Returns: (transfer none) (nullable): the repository name
 */
const gchar *
gctl_forge_context_get_repo_name(const GctlForgeContext *self)
{
	g_return_val_if_fail(self != NULL, NULL);

	return self->repo_name;
}

/**
 * gctl_forge_context_get_host:
 * @self: a #GctlForgeContext
 *
 * Returns the hostname of the forge.
 *
 * Returns: (transfer none) (nullable): the hostname
 */
const gchar *
gctl_forge_context_get_host(const GctlForgeContext *self)
{
	g_return_val_if_fail(self != NULL, NULL);

	return self->host;
}

/**
 * gctl_forge_context_get_cli_tool:
 * @self: a #GctlForgeContext
 *
 * Returns the resolved path to the CLI tool.
 *
 * Returns: (transfer none) (nullable): the CLI tool path
 */
const gchar *
gctl_forge_context_get_cli_tool(const GctlForgeContext *self)
{
	g_return_val_if_fail(self != NULL, NULL);

	return self->cli_tool;
}

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
gctl_forge_context_get_owner_repo(const GctlForgeContext *self)
{
	g_return_val_if_fail(self != NULL, NULL);

	return g_strdup_printf("%s/%s",
	                       self->owner != NULL ? self->owner : "",
	                       self->repo_name != NULL ? self->repo_name : "");
}
