/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-types.c - Error quark and version info implementation */

#define GCTL_COMPILATION
#include "gitctl.h"

/**
 * gctl_error_quark:
 *
 * Returns the #GQuark for the gitctl error domain.
 *
 * Returns: the error quark
 */
GQuark
gctl_error_quark(void)
{
    return g_quark_from_static_string("gitctl-error-quark");
}

/*
 * Version info — placed here since this is always compiled.
 */

void
gctl_get_version(
    guint *major,
    guint *minor,
    guint *micro
){
    if (major) *major = GCTL_VERSION_MAJOR;
    if (minor) *minor = GCTL_VERSION_MINOR;
    if (micro) *micro = GCTL_VERSION_MICRO;
}

const gchar *
gctl_get_version_string(void)
{
    return GCTL_VERSION_STRING;
}
