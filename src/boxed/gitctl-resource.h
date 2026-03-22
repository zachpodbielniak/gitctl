/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-resource.h - Boxed type for a forge resource (PR, issue, repo, etc.) */

#ifndef GCTL_RESOURCE_H
#define GCTL_RESOURCE_H

#if !defined(GCTL_INSIDE) && !defined(GCTL_COMPILATION)
#error "Only <gitctl.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * GCTL_TYPE_RESOURCE:
 *
 * The #GType for #GctlResource.
 */
#define GCTL_TYPE_RESOURCE (gctl_resource_get_type())

/**
 * GctlResource:
 *
 * A boxed type representing a single forge resource such as a pull
 * request, issue, repository, or release.  Fields are public so
 * internal code can access them directly; external consumers should
 * use the accessor API.
 */
struct _GctlResource
{
	GctlResourceKind  kind;
	gint              number;
	gchar            *title;
	gchar            *state;
	gchar            *author;
	gchar            *url;
	gchar            *created_at;
	gchar            *updated_at;
	gchar            *description;
	GHashTable       *extra;
};

/**
 * gctl_resource_get_type:
 *
 * Registers and returns the #GType for #GctlResource.
 *
 * Returns: the #GType
 */
GType
gctl_resource_get_type(void) G_GNUC_CONST;

/**
 * gctl_resource_new:
 * @kind: the #GctlResourceKind for this resource
 *
 * Creates a new #GctlResource of the given @kind with all string
 * fields set to %NULL, number set to 0, and an empty extras table.
 *
 * Returns: (transfer full): a newly allocated #GctlResource
 */
GctlResource *
gctl_resource_new(GctlResourceKind kind);

/**
 * gctl_resource_copy:
 * @src: (not nullable): the #GctlResource to copy
 *
 * Creates a deep copy of @src, duplicating all strings and the
 * extras hash table.
 *
 * Returns: (transfer full): a newly allocated copy of @src
 */
GctlResource *
gctl_resource_copy(const GctlResource *src);

/**
 * gctl_resource_free:
 * @self: (nullable): a #GctlResource, or %NULL
 *
 * Frees all memory associated with @self, including all strings and
 * the extras hash table.  If @self is %NULL this function is a no-op.
 */
void
gctl_resource_free(GctlResource *self);

/* ── Kind accessor ─────────────────────────────────────────────────── */

/**
 * gctl_resource_get_kind:
 * @self: a #GctlResource
 *
 * Returns the resource kind.
 *
 * Returns: the #GctlResourceKind
 */
GctlResourceKind
gctl_resource_get_kind(const GctlResource *self);

/* ── Number ────────────────────────────────────────────────────────── */

/**
 * gctl_resource_set_number:
 * @self: a #GctlResource
 * @number: the PR/issue number, or 0 for repos
 *
 * Sets the numeric identifier for this resource.
 */
void
gctl_resource_set_number(
	GctlResource  *self,
	gint           number
);

/**
 * gctl_resource_get_number:
 * @self: a #GctlResource
 *
 * Returns the numeric identifier.
 *
 * Returns: the number
 */
gint
gctl_resource_get_number(const GctlResource *self);

/* ── Title ─────────────────────────────────────────────────────────── */

/**
 * gctl_resource_set_title:
 * @self: a #GctlResource
 * @title: (nullable): the title string (will be copied)
 *
 * Sets the title, replacing any previous value.
 */
void
gctl_resource_set_title(
	GctlResource  *self,
	const gchar   *title
);

/**
 * gctl_resource_get_title:
 * @self: a #GctlResource
 *
 * Returns the title.
 *
 * Returns: (transfer none) (nullable): the title
 */
const gchar *
gctl_resource_get_title(const GctlResource *self);

/* ── State ─────────────────────────────────────────────────────────── */

/**
 * gctl_resource_set_state:
 * @self: a #GctlResource
 * @state: (nullable): the state string (e.g. "open", "closed", "merged")
 *
 * Sets the state, replacing any previous value.
 */
void
gctl_resource_set_state(
	GctlResource  *self,
	const gchar   *state
);

/**
 * gctl_resource_get_state:
 * @self: a #GctlResource
 *
 * Returns the state string.
 *
 * Returns: (transfer none) (nullable): the state
 */
const gchar *
gctl_resource_get_state(const GctlResource *self);

/* ── Author ────────────────────────────────────────────────────────── */

/**
 * gctl_resource_set_author:
 * @self: a #GctlResource
 * @author: (nullable): the author string (will be copied)
 *
 * Sets the author, replacing any previous value.
 */
void
gctl_resource_set_author(
	GctlResource  *self,
	const gchar   *author
);

/**
 * gctl_resource_get_author:
 * @self: a #GctlResource
 *
 * Returns the author.
 *
 * Returns: (transfer none) (nullable): the author
 */
const gchar *
gctl_resource_get_author(const GctlResource *self);

/* ── URL ───────────────────────────────────────────────────────────── */

/**
 * gctl_resource_set_url:
 * @self: a #GctlResource
 * @url: (nullable): the URL string (will be copied)
 *
 * Sets the URL, replacing any previous value.
 */
void
gctl_resource_set_url(
	GctlResource  *self,
	const gchar   *url
);

/**
 * gctl_resource_get_url:
 * @self: a #GctlResource
 *
 * Returns the URL.
 *
 * Returns: (transfer none) (nullable): the URL
 */
const gchar *
gctl_resource_get_url(const GctlResource *self);

/* ── Created-at ────────────────────────────────────────────────────── */

/**
 * gctl_resource_set_created_at:
 * @self: a #GctlResource
 * @created_at: (nullable): the creation timestamp (will be copied)
 *
 * Sets the creation timestamp, replacing any previous value.
 */
void
gctl_resource_set_created_at(
	GctlResource  *self,
	const gchar   *created_at
);

/**
 * gctl_resource_get_created_at:
 * @self: a #GctlResource
 *
 * Returns the creation timestamp.
 *
 * Returns: (transfer none) (nullable): the created-at timestamp
 */
const gchar *
gctl_resource_get_created_at(const GctlResource *self);

/* ── Updated-at ────────────────────────────────────────────────────── */

/**
 * gctl_resource_set_updated_at:
 * @self: a #GctlResource
 * @updated_at: (nullable): the update timestamp (will be copied)
 *
 * Sets the update timestamp, replacing any previous value.
 */
void
gctl_resource_set_updated_at(
	GctlResource  *self,
	const gchar   *updated_at
);

/**
 * gctl_resource_get_updated_at:
 * @self: a #GctlResource
 *
 * Returns the update timestamp.
 *
 * Returns: (transfer none) (nullable): the updated-at timestamp
 */
const gchar *
gctl_resource_get_updated_at(const GctlResource *self);

/* ── Description ───────────────────────────────────────────────────── */

/**
 * gctl_resource_set_description:
 * @self: a #GctlResource
 * @description: (nullable): the description (will be copied)
 *
 * Sets the description, replacing any previous value.  Used for repo
 * descriptions or PR body summaries.
 */
void
gctl_resource_set_description(
	GctlResource  *self,
	const gchar   *description
);

/**
 * gctl_resource_get_description:
 * @self: a #GctlResource
 *
 * Returns the description.
 *
 * Returns: (transfer none) (nullable): the description
 */
const gchar *
gctl_resource_get_description(const GctlResource *self);

/* ── Extra (string -> string) ──────────────────────────────────────── */

/**
 * gctl_resource_set_extra:
 * @self: a #GctlResource
 * @key: the key (will be copied)
 * @value: (nullable): the value (will be copied, or removed if %NULL)
 *
 * Sets a forge-specific extra field.  If @value is %NULL the key is
 * removed from the table.
 */
void
gctl_resource_set_extra(
	GctlResource  *self,
	const gchar   *key,
	const gchar   *value
);

/**
 * gctl_resource_get_extra:
 * @self: a #GctlResource
 * @key: the key to look up
 *
 * Looks up a forge-specific extra field.
 *
 * Returns: (transfer none) (nullable): the value, or %NULL if not set
 */
const gchar *
gctl_resource_get_extra(
	const GctlResource  *self,
	const gchar         *key
);

/**
 * gctl_resource_get_extra_table:
 * @self: a #GctlResource
 *
 * Returns the underlying extras hash table.  The caller must not
 * unref the returned table.
 *
 * Returns: (transfer none): the extras #GHashTable
 */
GHashTable *
gctl_resource_get_extra_table(const GctlResource *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GctlResource, gctl_resource_free)

G_END_DECLS

#endif /* GCTL_RESOURCE_H */
