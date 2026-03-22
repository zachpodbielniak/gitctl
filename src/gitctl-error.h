/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-error.h - Error domain and codes */

#ifndef GCTL_ERROR_H
#define GCTL_ERROR_H

#if !defined(GCTL_INSIDE) && !defined(GCTL_COMPILATION)
#error "Only <gitctl.h> can be included directly."
#endif

#include <glib.h>

G_BEGIN_DECLS

/**
 * GCTL_ERROR:
 *
 * Error domain for gitctl operations.
 * Errors in this domain will be from the #GctlError enumeration.
 */
#define GCTL_ERROR (gctl_error_quark())

GQuark gctl_error_quark (void);

/**
 * GctlError:
 * @GCTL_ERROR_GENERAL: General error.
 * @GCTL_ERROR_CONFIG_PARSE: Failed to parse configuration file.
 * @GCTL_ERROR_CONFIG_INVALID: Configuration value is invalid.
 * @GCTL_ERROR_MODULE_LOAD: Failed to load a module .so file.
 * @GCTL_ERROR_MODULE_SYMBOL: Missing entry point symbol in module.
 * @GCTL_ERROR_MODULE_TYPE: Module type does not implement required interface.
 * @GCTL_ERROR_MODULE_REGISTER: Module registration failed.
 * @GCTL_ERROR_FORGE_UNAVAILABLE: Forge CLI tool not found in PATH.
 * @GCTL_ERROR_FORGE_UNSUPPORTED: Operation not supported by this forge.
 * @GCTL_ERROR_FORGE_DETECT: Could not auto-detect forge type.
 * @GCTL_ERROR_EXECUTOR_SPAWN: Failed to spawn subprocess.
 * @GCTL_ERROR_EXECUTOR_TIMEOUT: Subprocess timed out.
 * @GCTL_ERROR_EXECUTOR_FAILED: Subprocess exited with non-zero status.
 * @GCTL_ERROR_PARSE_OUTPUT: Failed to parse forge CLI output.
 * @GCTL_ERROR_INVALID_ARGS: Invalid command-line arguments.
 * @GCTL_ERROR_NO_REPO: Not inside a git repository.
 * @GCTL_ERROR_NO_REMOTE: No git remote configured.
 *
 * Error codes for the %GCTL_ERROR domain.
 */
typedef enum
{
    GCTL_ERROR_GENERAL = 1,
    GCTL_ERROR_CONFIG_PARSE,
    GCTL_ERROR_CONFIG_INVALID,
    GCTL_ERROR_MODULE_LOAD,
    GCTL_ERROR_MODULE_SYMBOL,
    GCTL_ERROR_MODULE_TYPE,
    GCTL_ERROR_MODULE_REGISTER,
    GCTL_ERROR_FORGE_UNAVAILABLE,
    GCTL_ERROR_FORGE_UNSUPPORTED,
    GCTL_ERROR_FORGE_DETECT,
    GCTL_ERROR_EXECUTOR_SPAWN,
    GCTL_ERROR_EXECUTOR_TIMEOUT,
    GCTL_ERROR_EXECUTOR_FAILED,
    GCTL_ERROR_PARSE_OUTPUT,
    GCTL_ERROR_INVALID_ARGS,
    GCTL_ERROR_NO_REPO,
    GCTL_ERROR_NO_REMOTE,
} GctlError;

G_END_DECLS

#endif /* GCTL_ERROR_H */
