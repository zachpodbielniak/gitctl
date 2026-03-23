/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-context-resolver.h - Forge detection from git remote URLs */

#ifndef GCTL_CONTEXT_RESOLVER_H
#define GCTL_CONTEXT_RESOLVER_H

#if !defined(GCTL_INSIDE) && !defined(GCTL_COMPILATION)
#error "Only <gitctl.h> can be included directly."
#endif

#include <glib-object.h>
#include "gitctl-enums.h"
#include "gitctl-types.h"

G_BEGIN_DECLS

#define GCTL_TYPE_CONTEXT_RESOLVER (gctl_context_resolver_get_type())

G_DECLARE_FINAL_TYPE(GctlContextResolver, gctl_context_resolver,
                     GCTL, CONTEXT_RESOLVER, GObject)

/* Forward declaration — defined in boxed/gitctl-forge-context.h */
typedef struct _GctlForgeContext GctlForgeContext;

/**
 * gctl_context_resolver_new:
 * @config: (transfer none): a #GctlConfig to use for host lookups
 *
 * Creates a new #GctlContextResolver that will use @config for
 * mapping hostnames to forge types and CLI tool paths.  The resolver
 * does not take ownership of @config.
 *
 * Returns: (transfer full): a newly created #GctlContextResolver
 */
GctlContextResolver *
gctl_context_resolver_new(GctlConfig *config);

/**
 * gctl_context_resolver_set_forced_forge:
 * @self: a #GctlContextResolver
 * @forge_type: the #GctlForgeType to force, or
 *     %GCTL_FORGE_TYPE_UNKNOWN to clear the override
 *
 * Forces the resolver to use @forge_type instead of auto-detecting
 * from the remote URL hostname.  The remote URL is still queried for
 * owner and repository name extraction.
 */
void
gctl_context_resolver_set_forced_forge(
	GctlContextResolver  *self,
	GctlForgeType         forge_type
);

/**
 * gctl_context_resolver_get_forced_forge:
 * @self: a #GctlContextResolver
 *
 * Returns the forced forge type, or %GCTL_FORGE_TYPE_UNKNOWN if none.
 *
 * Returns: the forced #GctlForgeType
 */
GctlForgeType
gctl_context_resolver_get_forced_forge(GctlContextResolver *self);

/**
 * gctl_context_resolver_resolve:
 * @self: a #GctlContextResolver
 * @remote_name: the git remote name to inspect (e.g. "origin")
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Resolves the forge context for the given git remote.  This runs
 * `git remote get-url @remote_name` to obtain the URL, parses
 * out the hostname, owner, and repository name, then maps the
 * hostname to a #GctlForgeType via the configuration.
 *
 * If the hostname is not recognized and no forced forge has been
 * set, @error is set to %GCTL_ERROR_FORGE_DETECT.
 *
 * Returns: (transfer full) (nullable): a newly allocated
 *     #GctlForgeContext on success, or %NULL on error
 */
GctlForgeContext *
gctl_context_resolver_resolve(
	GctlContextResolver  *self,
	const gchar          *remote_name,
	GError              **error
);

/**
 * gctl_context_resolver_set_forced_repo:
 * @self: a #GctlContextResolver
 * @owner: the repository owner
 * @repo: the repository name
 *
 * Forces the resolver to use @owner and @repo instead of parsing
 * them from the git remote URL.  The remote URL is still queried
 * for hostname detection (unless a forced forge is also set).
 *
 * Pass %NULL for both to clear the override.
 */
void
gctl_context_resolver_set_forced_repo(
	GctlContextResolver  *self,
	const gchar          *owner,
	const gchar          *repo
);

G_END_DECLS

#endif /* GCTL_CONTEXT_RESOLVER_H */
