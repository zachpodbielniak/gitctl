/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-enums.h - Enumeration types for gitctl */

#ifndef GCTL_ENUMS_H
#define GCTL_ENUMS_H

#if !defined(GCTL_INSIDE) && !defined(GCTL_COMPILATION)
#error "Only <gitctl.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * GctlForgeType:
 * @GCTL_FORGE_TYPE_UNKNOWN: Unknown or undetected forge type.
 * @GCTL_FORGE_TYPE_GITHUB: GitHub (uses `gh` CLI).
 * @GCTL_FORGE_TYPE_GITLAB: GitLab (uses `glab` CLI).
 * @GCTL_FORGE_TYPE_FORGEJO: Forgejo (uses `fj` CLI).
 * @GCTL_FORGE_TYPE_GITEA: Gitea (uses `tea` CLI).
 *
 * Identifies the type of git forge a repository is hosted on.
 */
typedef enum
{
    GCTL_FORGE_TYPE_UNKNOWN = 0,
    GCTL_FORGE_TYPE_GITHUB,
    GCTL_FORGE_TYPE_GITLAB,
    GCTL_FORGE_TYPE_FORGEJO,
    GCTL_FORGE_TYPE_GITEA,
} GctlForgeType;

GType gctl_forge_type_get_type (void) G_GNUC_CONST;
#define GCTL_TYPE_FORGE_TYPE (gctl_forge_type_get_type())

/**
 * GctlOutputFormat:
 * @GCTL_OUTPUT_FORMAT_TABLE: Human-readable table (default).
 * @GCTL_OUTPUT_FORMAT_JSON: JSON output.
 * @GCTL_OUTPUT_FORMAT_YAML: YAML output.
 * @GCTL_OUTPUT_FORMAT_CSV: CSV output.
 *
 * Output format for command results.
 */
typedef enum
{
    GCTL_OUTPUT_FORMAT_TABLE = 0,
    GCTL_OUTPUT_FORMAT_JSON,
    GCTL_OUTPUT_FORMAT_YAML,
    GCTL_OUTPUT_FORMAT_CSV,
} GctlOutputFormat;

GType gctl_output_format_get_type (void) G_GNUC_CONST;
#define GCTL_TYPE_OUTPUT_FORMAT (gctl_output_format_get_type())

/**
 * GctlResourceKind:
 * @GCTL_RESOURCE_KIND_PR: Pull request (merge request on GitLab).
 * @GCTL_RESOURCE_KIND_ISSUE: Issue.
 * @GCTL_RESOURCE_KIND_REPO: Repository.
 * @GCTL_RESOURCE_KIND_RELEASE: Release.
 * @GCTL_RESOURCE_KIND_MIRROR: Repository mirror (push or pull).
 * @GCTL_RESOURCE_KIND_CI: CI pipeline / workflow run.
 * @GCTL_RESOURCE_KIND_COMMIT: Git commit.
 * @GCTL_RESOURCE_KIND_LABEL: Issue/PR label.
 * @GCTL_RESOURCE_KIND_NOTIFICATION: Forge notification.
 * @GCTL_RESOURCE_KIND_KEY: SSH or deploy key.
 * @GCTL_RESOURCE_KIND_WEBHOOK: Repository webhook.
 *
 * The type of forge resource being operated on.
 */
typedef enum
{
    GCTL_RESOURCE_KIND_PR = 0,
    GCTL_RESOURCE_KIND_ISSUE,
    GCTL_RESOURCE_KIND_REPO,
    GCTL_RESOURCE_KIND_RELEASE,
    GCTL_RESOURCE_KIND_MIRROR,
    GCTL_RESOURCE_KIND_CI,
    GCTL_RESOURCE_KIND_COMMIT,
    GCTL_RESOURCE_KIND_LABEL,
    GCTL_RESOURCE_KIND_NOTIFICATION,
    GCTL_RESOURCE_KIND_KEY,
    GCTL_RESOURCE_KIND_WEBHOOK,
} GctlResourceKind;

GType gctl_resource_kind_get_type (void) G_GNUC_CONST;
#define GCTL_TYPE_RESOURCE_KIND (gctl_resource_kind_get_type())

/**
 * GctlVerb:
 * @GCTL_VERB_LIST: List resources.
 * @GCTL_VERB_GET: View a single resource.
 * @GCTL_VERB_CREATE: Create a new resource.
 * @GCTL_VERB_EDIT: Edit an existing resource.
 * @GCTL_VERB_CLOSE: Close a resource (issue, PR).
 * @GCTL_VERB_REOPEN: Reopen a closed resource.
 * @GCTL_VERB_MERGE: Merge a pull request.
 * @GCTL_VERB_COMMENT: Comment on a resource.
 * @GCTL_VERB_CHECKOUT: Checkout a PR branch locally.
 * @GCTL_VERB_REVIEW: Review a pull request.
 * @GCTL_VERB_DELETE: Delete a resource.
 * @GCTL_VERB_FORK: Fork a repository.
 * @GCTL_VERB_CLONE: Clone a repository.
 * @GCTL_VERB_BROWSE: Open resource in web browser.
 * @GCTL_VERB_SYNC: Trigger mirror synchronization.
 * @GCTL_VERB_DIFF: View diff of a pull request.
 * @GCTL_VERB_LOG: View logs (CI pipeline output).
 * @GCTL_VERB_READ: Mark a notification as read.
 * @GCTL_VERB_STAR: Star/favourite a repository.
 * @GCTL_VERB_UNSTAR: Remove star from a repository.
 *
 * The action to perform on a forge resource.
 */
typedef enum
{
    GCTL_VERB_LIST = 0,
    GCTL_VERB_GET,
    GCTL_VERB_CREATE,
    GCTL_VERB_EDIT,
    GCTL_VERB_CLOSE,
    GCTL_VERB_REOPEN,
    GCTL_VERB_MERGE,
    GCTL_VERB_COMMENT,
    GCTL_VERB_CHECKOUT,
    GCTL_VERB_REVIEW,
    GCTL_VERB_DELETE,
    GCTL_VERB_FORK,
    GCTL_VERB_CLONE,
    GCTL_VERB_BROWSE,
    GCTL_VERB_SYNC,
    GCTL_VERB_DIFF,
    GCTL_VERB_LOG,
    GCTL_VERB_READ,
    GCTL_VERB_STAR,
    GCTL_VERB_UNSTAR,
} GctlVerb;

GType gctl_verb_get_type (void) G_GNUC_CONST;
#define GCTL_TYPE_VERB (gctl_verb_get_type())

/**
 * gctl_resource_kind_to_string:
 * @kind: a #GctlResourceKind
 *
 * Returns the string name for a resource kind.
 *
 * Returns: (transfer none): the resource kind name
 */
const gchar *gctl_resource_kind_to_string (GctlResourceKind kind);

/**
 * gctl_forge_type_to_string:
 * @forge_type: a #GctlForgeType
 *
 * Returns the string name for a forge type.
 *
 * Returns: (transfer none): the forge type name
 */
const gchar *gctl_forge_type_to_string (GctlForgeType forge_type);

/**
 * gctl_forge_type_from_string:
 * @name: the forge type name
 *
 * Parses a forge type from its string name.
 *
 * Returns: the #GctlForgeType, or %GCTL_FORGE_TYPE_UNKNOWN
 */
GctlForgeType gctl_forge_type_from_string (const gchar *name);

/**
 * gctl_verb_to_string:
 * @verb: a #GctlVerb
 *
 * Returns the string name for a verb.
 *
 * Returns: (transfer none): the verb name
 */
const gchar *gctl_verb_to_string (GctlVerb verb);

/**
 * gctl_verb_from_string:
 * @name: the verb name
 *
 * Parses a verb from its string name. Returns -1 if not recognized.
 *
 * Returns: the #GctlVerb, or -1
 */
gint gctl_verb_from_string (const gchar *name);

G_END_DECLS

#endif /* GCTL_ENUMS_H */
