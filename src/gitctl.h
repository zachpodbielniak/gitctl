/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl.h - Umbrella header for gitctl */

#ifndef GCTL_H
#define GCTL_H

/**
 * SECTION:gitctl
 * @title: Gitctl
 * @short_description: kubectl-like CLI for managing git repositories across forges
 *
 * To use the library, include only this header:
 * |[<!-- language="C" -->
 * #include <gitctl.h>
 * ]|
 */

#define GCTL_INSIDE

/* Common types and enumerations */
#include "gitctl-types.h"
#include "gitctl-enums.h"
#include "gitctl-error.h"
#include "gitctl-version.h"

/* Core types */
#include "core/gitctl-app.h"
#include "core/gitctl-executor.h"
#include "core/gitctl-context-resolver.h"
#include "core/gitctl-config.h"
#include "core/gitctl-output-formatter.h"

/* Boxed types */
#include "boxed/gitctl-command-result.h"
#include "boxed/gitctl-forge-context.h"
#include "boxed/gitctl-resource.h"

/* Interfaces */
#include "interfaces/gitctl-forge.h"

/* Command handlers */
#include "commands/gitctl-cmd-pr.h"
#include "commands/gitctl-cmd-issue.h"
#include "commands/gitctl-cmd-repo.h"
#include "commands/gitctl-cmd-release.h"
#include "commands/gitctl-cmd-mirror.h"
#include "commands/gitctl-cmd-api.h"
#include "commands/gitctl-cmd-config.h"
#include "commands/gitctl-cmd-completion.h"
#include "commands/gitctl-cmd-status.h"
#include "commands/gitctl-cmd-ci.h"
#include "commands/gitctl-cmd-commit.h"
#include "commands/gitctl-cmd-label.h"
#include "commands/gitctl-cmd-notification.h"
#include "commands/gitctl-cmd-key.h"
#include "commands/gitctl-cmd-webhook.h"

/* Module system */
#include "module/gitctl-module.h"
#include "module/gitctl-module-manager.h"

#undef GCTL_INSIDE

#endif /* GCTL_H */
