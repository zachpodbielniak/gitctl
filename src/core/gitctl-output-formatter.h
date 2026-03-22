/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-output-formatter.h - Multi-format resource output rendering */

#ifndef GCTL_OUTPUT_FORMATTER_H
#define GCTL_OUTPUT_FORMATTER_H

#if !defined(GCTL_INSIDE) && !defined(GCTL_COMPILATION)
#error "Only <gitctl.h> can be included directly."
#endif

#include <glib-object.h>
#include "gitctl-enums.h"
#include "gitctl-types.h"

G_BEGIN_DECLS

#define GCTL_TYPE_OUTPUT_FORMATTER (gctl_output_formatter_get_type())

G_DECLARE_FINAL_TYPE(GctlOutputFormatter, gctl_output_formatter,
                     GCTL, OUTPUT_FORMATTER, GObject)

/* Forward declaration — defined in boxed/gitctl-resource.h */
typedef struct _GctlResource GctlResource;

/**
 * gctl_output_formatter_new:
 * @format: the #GctlOutputFormat to use for rendering
 *
 * Creates a new #GctlOutputFormatter.  The colorize flag defaults to
 * %TRUE if stdout is a terminal (checked via `isatty()`), otherwise
 * %FALSE.
 *
 * Returns: (transfer full): a newly created #GctlOutputFormatter
 */
GctlOutputFormatter *
gctl_output_formatter_new(GctlOutputFormat format);

/**
 * gctl_output_formatter_set_format:
 * @self: a #GctlOutputFormatter
 * @format: the #GctlOutputFormat to set
 *
 * Changes the output format used by the formatter.
 */
void
gctl_output_formatter_set_format(
	GctlOutputFormatter  *self,
	GctlOutputFormat      format
);

/**
 * gctl_output_formatter_get_format:
 * @self: a #GctlOutputFormatter
 *
 * Returns the current output format.
 *
 * Returns: the #GctlOutputFormat
 */
GctlOutputFormat
gctl_output_formatter_get_format(GctlOutputFormatter *self);

/**
 * gctl_output_formatter_format_resources:
 * @self: a #GctlOutputFormatter
 * @resources: (element-type GctlResource): array of #GctlResource pointers
 *
 * Formats an array of resources into a single output string according
 * to the configured format (table, JSON, YAML, or CSV).
 *
 * Returns: (transfer full): the formatted output string
 */
gchar *
gctl_output_formatter_format_resources(
	GctlOutputFormatter  *self,
	GPtrArray            *resources
);

/**
 * gctl_output_formatter_format_resource:
 * @self: a #GctlOutputFormatter
 * @resource: a #GctlResource
 *
 * Formats a single resource into an output string according to the
 * configured format.
 *
 * Returns: (transfer full): the formatted output string
 */
gchar *
gctl_output_formatter_format_resource(
	GctlOutputFormatter  *self,
	GctlResource         *resource
);

/**
 * gctl_output_formatter_print_resources:
 * @self: a #GctlOutputFormatter
 * @resources: (element-type GctlResource): array of #GctlResource pointers
 *
 * Convenience method that formats @resources and writes the result to
 * stdout.
 */
void
gctl_output_formatter_print_resources(
	GctlOutputFormatter  *self,
	GPtrArray            *resources
);

/**
 * gctl_output_formatter_print_resource:
 * @self: a #GctlOutputFormatter
 * @resource: a #GctlResource
 *
 * Convenience method that formats @resource and writes the result to
 * stdout.
 */
void
gctl_output_formatter_print_resource(
	GctlOutputFormatter  *self,
	GctlResource         *resource
);

G_END_DECLS

#endif /* GCTL_OUTPUT_FORMATTER_H */
