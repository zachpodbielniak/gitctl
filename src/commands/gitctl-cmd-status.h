/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-cmd-status.h - Repository status command handler */

#ifndef GCTL_CMD_STATUS_H
#define GCTL_CMD_STATUS_H

#if !defined(GCTL_INSIDE) && !defined(GCTL_COMPILATION)
#error "Only <gitctl.h> can be included directly."
#endif

#include <glib.h>
#include "gitctl-types.h"

G_BEGIN_DECLS

/**
 * gctl_cmd_status:
 * @app: the #GctlApp instance
 * @argc: argument count
 * @argv: (array length=argc): argument vector
 *
 * Main entry point for the "status" command.  Shows an overview of
 * the current repository: detected forge type, owner/repo, open PR
 * and issue counts, and recent activity.
 *
 * This command does NOT follow the noun-verb pattern.
 *
 * Returns: 0 on success, 1 on error
 */
gint
gctl_cmd_status(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
);

G_END_DECLS

#endif /* GCTL_CMD_STATUS_H */
