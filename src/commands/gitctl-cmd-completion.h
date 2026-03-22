/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-cmd-completion.h - Shell completion generation */

#ifndef GCTL_CMD_COMPLETION_H
#define GCTL_CMD_COMPLETION_H

#if !defined(GCTL_INSIDE) && !defined(GCTL_COMPILATION)
#error "Only <gitctl.h> can be included directly."
#endif

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GctlApp GctlApp;

gint gctl_cmd_completion (GctlApp *app, gint argc, gchar **argv);

G_END_DECLS

#endif /* GCTL_CMD_COMPLETION_H */
