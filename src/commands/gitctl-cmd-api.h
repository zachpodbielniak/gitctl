/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-cmd-api.h - Raw API passthrough command handler */

#ifndef GCTL_CMD_API_H
#define GCTL_CMD_API_H

#if !defined(GCTL_INSIDE) && !defined(GCTL_COMPILATION)
#error "Only <gitctl.h> can be included directly."
#endif

#include <glib.h>
#include "gitctl-types.h"

G_BEGIN_DECLS

/**
 * gctl_cmd_api:
 * @app: the #GctlApp instance
 * @argc: argument count
 * @argv: (array length=argc): argument vector, where argv[0] is the HTTP
 *     method and argv[1] is the API endpoint
 *
 * Main entry point for the "api" command.  Unlike other command handlers,
 * this does NOT use the verb dispatch pattern.  Instead it passes a raw
 * HTTP method and endpoint through to the forge CLI's API interface.
 *
 * Usage: gitctl api <METHOD> <endpoint> [--body JSON]
 *
 * Returns: 0 on success, 1 on error
 */
gint
gctl_cmd_api(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
);

G_END_DECLS

#endif /* GCTL_CMD_API_H */
