/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-types.h - Forward declarations and common types */

#ifndef GCTL_TYPES_H
#define GCTL_TYPES_H

#if !defined(GCTL_INSIDE) && !defined(GCTL_COMPILATION)
#error "Only <gitctl.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

/* ---- Core type forward declarations ---- */

typedef struct _GctlApp                  GctlApp;
typedef struct _GctlAppClass             GctlAppClass;

typedef struct _GctlExecutor             GctlExecutor;
typedef struct _GctlContextResolver      GctlContextResolver;
typedef struct _GctlConfig               GctlConfig;
typedef struct _GctlOutputFormatter      GctlOutputFormatter;

/* ---- Boxed type forward declarations ---- */

typedef struct _GctlCommandResult        GctlCommandResult;
typedef struct _GctlForgeContext         GctlForgeContext;
typedef struct _GctlResource             GctlResource;

/* ---- Interface forward declarations ---- */

typedef struct _GctlForge                GctlForge;
typedef struct _GctlForgeInterface       GctlForgeInterface;

/* ---- Module forward declarations (derivable) ---- */

typedef struct _GctlModule               GctlModule;
typedef struct _GctlModuleClass          GctlModuleClass;

/* ---- Module manager ---- */

typedef struct _GctlModuleManager        GctlModuleManager;

G_END_DECLS

#endif /* GCTL_TYPES_H */
