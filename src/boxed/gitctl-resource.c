/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-resource.c - Boxed type for a forge resource (PR, issue, repo, etc.) */

#define GCTL_COMPILATION
#include "gitctl.h"

/* ── Boxed type registration ───────────────────────────────────────── */

G_DEFINE_BOXED_TYPE(
	GctlResource,
	gctl_resource,
	gctl_resource_copy,
	gctl_resource_free
)

/* ── Helpers ───────────────────────────────────────────────────────── */

/**
 * copy_extra_entry:
 * @key: (not nullable): the hash table key
 * @value: (not nullable): the hash table value
 * @user_data: (not nullable): the destination #GHashTable
 *
 * GHFunc callback that copies a single string->string entry into
 * the destination hash table.
 */
static void
copy_extra_entry(
	gpointer key,
	gpointer value,
	gpointer user_data
){
	GHashTable *dest = (GHashTable *)user_data;

	g_hash_table_insert(
		dest,
		g_strdup((const gchar *)key),
		g_strdup((const gchar *)value)
	);
}

/* ── Constructor ───────────────────────────────────────────────────── */

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
gctl_resource_new(GctlResourceKind kind)
{
	GctlResource *self;

	self = g_new0(GctlResource, 1);

	self->kind        = kind;
	self->number      = 0;
	self->title       = NULL;
	self->state       = NULL;
	self->author      = NULL;
	self->url         = NULL;
	self->created_at  = NULL;
	self->updated_at  = NULL;
	self->description = NULL;
	self->extra       = g_hash_table_new_full(
		g_str_hash, g_str_equal, g_free, g_free
	);

	return self;
}

/* ── Copy / Free ───────────────────────────────────────────────────── */

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
gctl_resource_copy(const GctlResource *src)
{
	GctlResource *dest;

	g_return_val_if_fail(src != NULL, NULL);

	dest = gctl_resource_new(src->kind);

	dest->number      = src->number;
	dest->title       = g_strdup(src->title);
	dest->state       = g_strdup(src->state);
	dest->author      = g_strdup(src->author);
	dest->url         = g_strdup(src->url);
	dest->created_at  = g_strdup(src->created_at);
	dest->updated_at  = g_strdup(src->updated_at);
	dest->description = g_strdup(src->description);

	/* Deep-copy the extras table */
	if (src->extra != NULL)
		g_hash_table_foreach(src->extra, copy_extra_entry, dest->extra);

	return dest;
}

/**
 * gctl_resource_free:
 * @self: (nullable): a #GctlResource, or %NULL
 *
 * Frees all memory associated with @self, including all strings and
 * the extras hash table.  If @self is %NULL this function is a no-op.
 */
void
gctl_resource_free(GctlResource *self)
{
	if (self == NULL)
		return;

	g_free(self->title);
	g_free(self->state);
	g_free(self->author);
	g_free(self->url);
	g_free(self->created_at);
	g_free(self->updated_at);
	g_free(self->description);

	if (self->extra != NULL)
		g_hash_table_unref(self->extra);

	g_free(self);
}

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
gctl_resource_get_kind(const GctlResource *self)
{
	g_return_val_if_fail(self != NULL, GCTL_RESOURCE_KIND_PR);

	return self->kind;
}

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
){
	g_return_if_fail(self != NULL);

	self->number = number;
}

/**
 * gctl_resource_get_number:
 * @self: a #GctlResource
 *
 * Returns the numeric identifier.
 *
 * Returns: the number
 */
gint
gctl_resource_get_number(const GctlResource *self)
{
	g_return_val_if_fail(self != NULL, 0);

	return self->number;
}

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
){
	g_return_if_fail(self != NULL);

	g_free(self->title);
	self->title = g_strdup(title);
}

/**
 * gctl_resource_get_title:
 * @self: a #GctlResource
 *
 * Returns the title.
 *
 * Returns: (transfer none) (nullable): the title
 */
const gchar *
gctl_resource_get_title(const GctlResource *self)
{
	g_return_val_if_fail(self != NULL, NULL);

	return self->title;
}

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
){
	g_return_if_fail(self != NULL);

	g_free(self->state);
	self->state = g_strdup(state);
}

/**
 * gctl_resource_get_state:
 * @self: a #GctlResource
 *
 * Returns the state string.
 *
 * Returns: (transfer none) (nullable): the state
 */
const gchar *
gctl_resource_get_state(const GctlResource *self)
{
	g_return_val_if_fail(self != NULL, NULL);

	return self->state;
}

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
){
	g_return_if_fail(self != NULL);

	g_free(self->author);
	self->author = g_strdup(author);
}

/**
 * gctl_resource_get_author:
 * @self: a #GctlResource
 *
 * Returns the author.
 *
 * Returns: (transfer none) (nullable): the author
 */
const gchar *
gctl_resource_get_author(const GctlResource *self)
{
	g_return_val_if_fail(self != NULL, NULL);

	return self->author;
}

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
){
	g_return_if_fail(self != NULL);

	g_free(self->url);
	self->url = g_strdup(url);
}

/**
 * gctl_resource_get_url:
 * @self: a #GctlResource
 *
 * Returns the URL.
 *
 * Returns: (transfer none) (nullable): the URL
 */
const gchar *
gctl_resource_get_url(const GctlResource *self)
{
	g_return_val_if_fail(self != NULL, NULL);

	return self->url;
}

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
){
	g_return_if_fail(self != NULL);

	g_free(self->created_at);
	self->created_at = g_strdup(created_at);
}

/**
 * gctl_resource_get_created_at:
 * @self: a #GctlResource
 *
 * Returns the creation timestamp.
 *
 * Returns: (transfer none) (nullable): the created-at timestamp
 */
const gchar *
gctl_resource_get_created_at(const GctlResource *self)
{
	g_return_val_if_fail(self != NULL, NULL);

	return self->created_at;
}

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
){
	g_return_if_fail(self != NULL);

	g_free(self->updated_at);
	self->updated_at = g_strdup(updated_at);
}

/**
 * gctl_resource_get_updated_at:
 * @self: a #GctlResource
 *
 * Returns the update timestamp.
 *
 * Returns: (transfer none) (nullable): the updated-at timestamp
 */
const gchar *
gctl_resource_get_updated_at(const GctlResource *self)
{
	g_return_val_if_fail(self != NULL, NULL);

	return self->updated_at;
}

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
){
	g_return_if_fail(self != NULL);

	g_free(self->description);
	self->description = g_strdup(description);
}

/**
 * gctl_resource_get_description:
 * @self: a #GctlResource
 *
 * Returns the description.
 *
 * Returns: (transfer none) (nullable): the description
 */
const gchar *
gctl_resource_get_description(const GctlResource *self)
{
	g_return_val_if_fail(self != NULL, NULL);

	return self->description;
}

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
){
	g_return_if_fail(self != NULL);
	g_return_if_fail(key != NULL);

	if (value != NULL) {
		g_hash_table_insert(
			self->extra,
			g_strdup(key),
			g_strdup(value)
		);
	} else {
		g_hash_table_remove(self->extra, key);
	}
}

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
){
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(key != NULL, NULL);

	return (const gchar *)g_hash_table_lookup(self->extra, key);
}

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
gctl_resource_get_extra_table(const GctlResource *self)
{
	g_return_val_if_fail(self != NULL, NULL);

	return self->extra;
}
