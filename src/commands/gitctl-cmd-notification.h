/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-cmd-notification.h - Notification command handler */

#ifndef GCTL_CMD_NOTIFICATION_H
#define GCTL_CMD_NOTIFICATION_H

#if !defined(GCTL_INSIDE) && !defined(GCTL_COMPILATION)
#error "Only <gitctl.h> can be included directly."
#endif

#include <glib.h>
#include "gitctl-types.h"

G_BEGIN_DECLS

/**
 * gctl_cmd_notification:
 * @app: the #GctlApp instance
 * @argc: argument count (verb + verb-specific args)
 * @argv: (array length=argc): argument vector, where argv[0] is the verb
 *
 * Main entry point for the "notification" command.  Dispatches to
 * the appropriate verb handler: list or read.
 *
 * Returns: 0 on success, 1 on error
 */
gint
gctl_cmd_notification(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
);

G_END_DECLS

#endif /* GCTL_CMD_NOTIFICATION_H */
