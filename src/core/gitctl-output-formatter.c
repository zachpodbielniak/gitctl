/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-output-formatter.c - Multi-format resource output rendering */

#define GCTL_COMPILATION
#include "gitctl.h"

#include <json-glib/json-glib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

/* ── Private structure ────────────────────────────────────────────── */

struct _GctlOutputFormatter
{
	GObject parent_instance;

	GctlOutputFormat  format;
	gboolean          colorize;
};

G_DEFINE_TYPE(GctlOutputFormatter, gctl_output_formatter, G_TYPE_OBJECT)

/* ── GObject vfuncs ───────────────────────────────────────────────── */

static void
gctl_output_formatter_class_init(GctlOutputFormatterClass *klass)
{
	/* No special finalize needed — no heap fields beyond GObject */
	(void)klass;
}

static void
gctl_output_formatter_init(GctlOutputFormatter *self)
{
	self->format   = GCTL_OUTPUT_FORMAT_TABLE;
	self->colorize = isatty(STDOUT_FILENO) ? TRUE : FALSE;
}

/* ── ANSI color helpers ───────────────────────────────────────────── */

#define ANSI_RESET  "\033[0m"
#define ANSI_BOLD   "\033[1m"
#define ANSI_GREEN  "\033[32m"
#define ANSI_RED    "\033[31m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_CYAN   "\033[36m"

/*
 * state_color:
 * @state: the resource state string (e.g. "open", "closed", "merged")
 * @colorize: whether color output is enabled
 *
 * Returns an ANSI color prefix appropriate for the given state.
 */
static const gchar *
state_color(
	const gchar *state,
	gboolean     colorize
){
	if (!colorize || state == NULL)
		return "";

	if (g_ascii_strcasecmp(state, "open") == 0)
		return ANSI_GREEN;
	if (g_ascii_strcasecmp(state, "closed") == 0)
		return ANSI_RED;
	if (g_ascii_strcasecmp(state, "merged") == 0)
		return ANSI_YELLOW;

	return "";
}

/*
 * reset_if_color:
 * @colorize: whether color output is enabled
 *
 * Returns the ANSI reset sequence if @colorize is %TRUE, else "".
 */
static const gchar *
reset_if_color(gboolean colorize)
{
	return colorize ? ANSI_RESET : "";
}

/* ── CSV quoting helper ───────────────────────────────────────────── */

/*
 * csv_quote:
 * @value: (nullable): the string to quote
 *
 * Returns a properly quoted CSV field.  If @value contains commas,
 * double quotes, or newlines, the field is wrapped in double quotes
 * with internal double quotes doubled.
 *
 * Returns: (transfer full): the quoted string
 */
static gchar *
csv_quote(const gchar *value)
{
	GString *buf;

	if (value == NULL)
		return g_strdup("");

	if (strchr(value, ',') == NULL &&
	    strchr(value, '"') == NULL &&
	    strchr(value, '\n') == NULL)
	{
		return g_strdup(value);
	}

	buf = g_string_new("\"");
	while (*value != '\0') {
		if (*value == '"')
			g_string_append_c(buf, '"');
		g_string_append_c(buf, *value);
		value++;
	}
	g_string_append_c(buf, '"');

	return g_string_free(buf, FALSE);
}

/* ── Table formatting ─────────────────────────────────────────────── */

/*
 * get_columns_for_kind:
 * @kind: the resource kind
 * @n_cols: (out): number of columns returned
 *
 * Returns a static array of column header strings appropriate for
 * the given resource kind.
 */
static const gchar **
get_columns_for_kind(
	GctlResourceKind  kind,
	guint            *n_cols
){
	static const gchar *pr_cols[]      = { "#", "TITLE", "STATE", "AUTHOR", NULL };
	static const gchar *issue_cols[]   = { "#", "TITLE", "STATE", "AUTHOR", NULL };
	static const gchar *repo_cols[]    = { "NAME", "DESCRIPTION", "VISIBILITY", NULL };
	static const gchar *release_cols[] = { "TAG", "TITLE", "DATE", NULL };
	static const gchar *mirror_cols[]  = { "ID", "URL", "DIRECTION", "INTERVAL", NULL };

	switch (kind) {
	case GCTL_RESOURCE_KIND_PR:
		*n_cols = 4;
		return pr_cols;
	case GCTL_RESOURCE_KIND_ISSUE:
		*n_cols = 4;
		return issue_cols;
	case GCTL_RESOURCE_KIND_REPO:
		*n_cols = 3;
		return repo_cols;
	case GCTL_RESOURCE_KIND_RELEASE:
		*n_cols = 3;
		return release_cols;
	case GCTL_RESOURCE_KIND_MIRROR:
		*n_cols = 4;
		return mirror_cols;
	default:
		*n_cols = 4;
		return pr_cols;
	}
}

/*
 * get_resource_field:
 * @resource: a #GctlResource
 * @kind: the resource kind
 * @col: the column index
 *
 * Extracts the field value for a given column of a resource.  The
 * returned string is owned by the resource and must not be freed.
 *
 * Returns: (transfer none): the field value string, or ""
 */
static const gchar *
get_resource_field(
	GctlResource     *resource,
	GctlResourceKind  kind,
	guint             col
){
	switch (kind) {
	case GCTL_RESOURCE_KIND_PR:
	case GCTL_RESOURCE_KIND_ISSUE:
		switch (col) {
		case 0: {
			/*
			 * Convert the numeric identifier to a string.  We use a
			 * thread-local static buffer so the returned pointer stays
			 * valid until the next call from the same thread.
			 */
			static _Thread_local gchar num_buf[32];

			g_snprintf(num_buf, sizeof(num_buf), "#%d",
			           gctl_resource_get_number(resource));
			return num_buf;
		}
		case 1: return gctl_resource_get_title(resource);
		case 2: return gctl_resource_get_state(resource);
		case 3: return gctl_resource_get_author(resource);
		}
		break;
	case GCTL_RESOURCE_KIND_REPO:
		switch (col) {
		case 0: return gctl_resource_get_title(resource);
		case 1: {
			const gchar *desc;

			desc = gctl_resource_get_extra(resource, "description");
			return desc ? desc : "";
		}
		case 2: {
			const gchar *vis;

			vis = gctl_resource_get_extra(resource, "visibility");
			return vis ? vis : "";
		}
		}
		break;
	case GCTL_RESOURCE_KIND_RELEASE:
		switch (col) {
		case 0: {
			const gchar *tag;

			tag = gctl_resource_get_extra(resource, "tag");
			return tag ? tag : "";
		}
		case 1: return gctl_resource_get_title(resource);
		case 2: return gctl_resource_get_created_at(resource);
		}
		break;
	case GCTL_RESOURCE_KIND_MIRROR:
		switch (col) {
		case 0: {
			const gchar *id;

			id = gctl_resource_get_extra(resource, "mirror_id");
			return id ? id : "";
		}
		case 1: return gctl_resource_get_url(resource);
		case 2: {
			const gchar *dir;

			dir = gctl_resource_get_extra(resource, "direction");
			return dir ? dir : "push";
		}
		case 3: {
			const gchar *interval;

			interval = gctl_resource_get_extra(resource, "interval");
			return interval ? interval : "";
		}
		}
		break;
	}

	return "";
}

/*
 * format_as_table:
 * @self: the formatter
 * @resources: array of #GctlResource pointers
 *
 * Renders the resources as a padded, column-aligned table.
 *
 * Returns: (transfer full): the table string
 */
static gchar *
format_as_table(
	GctlOutputFormatter  *self,
	GPtrArray            *resources
){
	GString *buf;
	const gchar **headers;
	guint n_cols;
	guint *widths;
	guint i;
	guint j;
	GctlResourceKind kind;

	if (resources->len == 0)
		return g_strdup("(no results)\n");

	/* Determine columns from the first resource's kind */
	kind    = gctl_resource_get_kind((GctlResource *)g_ptr_array_index(resources, 0));
	headers = get_columns_for_kind(kind, &n_cols);

	/* Calculate column widths */
	widths = g_new0(guint, n_cols);
	for (j = 0; j < n_cols; j++)
		widths[j] = (guint)strlen(headers[j]);

	for (i = 0; i < resources->len; i++) {
		GctlResource *res;

		res = (GctlResource *)g_ptr_array_index(resources, i);
		for (j = 0; j < n_cols; j++) {
			const gchar *val;
			guint len;

			val = get_resource_field(res, kind, j);
			len = (val != NULL) ? (guint)strlen(val) : 0;
			if (len > widths[j])
				widths[j] = len;
		}
	}

	buf = g_string_new(NULL);

	/* Header line */
	for (j = 0; j < n_cols; j++) {
		if (j > 0)
			g_string_append(buf, "  ");
		if (self->colorize)
			g_string_append(buf, ANSI_BOLD);
		g_string_append_printf(buf, "%-*s", (gint)widths[j], headers[j]);
		if (self->colorize)
			g_string_append(buf, ANSI_RESET);
	}
	g_string_append_c(buf, '\n');

	/* Separator line */
	for (j = 0; j < n_cols; j++) {
		guint k;

		if (j > 0)
			g_string_append(buf, "  ");
		for (k = 0; k < widths[j]; k++)
			g_string_append_c(buf, '-');
	}
	g_string_append_c(buf, '\n');

	/* Data rows */
	for (i = 0; i < resources->len; i++) {
		GctlResource *res;

		res = (GctlResource *)g_ptr_array_index(resources, i);
		for (j = 0; j < n_cols; j++) {
			const gchar *val;

			val = get_resource_field(res, kind, j);
			if (val == NULL)
				val = "";

			if (j > 0)
				g_string_append(buf, "  ");

			/*
			 * Apply color to state columns:
			 * col 2 for PR/Issue kinds.
			 */
			if (j == 2 &&
			    (kind == GCTL_RESOURCE_KIND_PR ||
			     kind == GCTL_RESOURCE_KIND_ISSUE))
			{
				g_string_append(buf, state_color(val, self->colorize));
				g_string_append_printf(buf, "%-*s", (gint)widths[j], val);
				g_string_append(buf, reset_if_color(self->colorize));
			} else {
				g_string_append_printf(buf, "%-*s", (gint)widths[j], val);
			}
		}
		g_string_append_c(buf, '\n');
	}

	g_free(widths);

	return g_string_free(buf, FALSE);
}

/* ── JSON formatting ──────────────────────────────────────────────── */

/*
 * format_as_json:
 * @resources: array of #GctlResource pointers
 *
 * Builds a JSON array of resource objects using json-glib.
 *
 * Returns: (transfer full): the JSON string
 */
static gchar *
format_as_json(GPtrArray *resources)
{
	g_autoptr(JsonBuilder) builder = NULL;
	g_autoptr(JsonGenerator) gen = NULL;
	g_autoptr(JsonNode) root = NULL;
	gchar *json_str;
	guint i;

	builder = json_builder_new();

	json_builder_begin_array(builder);
	for (i = 0; i < resources->len; i++) {
		GctlResource *res;

		res = (GctlResource *)g_ptr_array_index(resources, i);

		json_builder_begin_object(builder);

		json_builder_set_member_name(builder, "kind");
		json_builder_add_string_value(builder,
			gctl_resource_kind_to_string(gctl_resource_get_kind(res)));

		if (gctl_resource_get_number(res) >= 0) {
			json_builder_set_member_name(builder, "number");
			json_builder_add_int_value(builder,
				gctl_resource_get_number(res));
		}

		json_builder_set_member_name(builder, "title");
		json_builder_add_string_value(builder,
			gctl_resource_get_title(res) ? gctl_resource_get_title(res) : "");

		if (gctl_resource_get_state(res) != NULL) {
			json_builder_set_member_name(builder, "state");
			json_builder_add_string_value(builder,
				gctl_resource_get_state(res));
		}

		if (gctl_resource_get_author(res) != NULL) {
			json_builder_set_member_name(builder, "author");
			json_builder_add_string_value(builder,
				gctl_resource_get_author(res));
		}

		if (gctl_resource_get_url(res) != NULL) {
			json_builder_set_member_name(builder, "url");
			json_builder_add_string_value(builder,
				gctl_resource_get_url(res));
		}

		if (gctl_resource_get_created_at(res) != NULL) {
			json_builder_set_member_name(builder, "created_at");
			json_builder_add_string_value(builder,
				gctl_resource_get_created_at(res));
		}

		if (gctl_resource_get_updated_at(res) != NULL) {
			json_builder_set_member_name(builder, "updated_at");
			json_builder_add_string_value(builder,
				gctl_resource_get_updated_at(res));
		}

		json_builder_end_object(builder);
	}
	json_builder_end_array(builder);

	root = json_builder_get_root(builder);

	gen = json_generator_new();
	json_generator_set_pretty(gen, TRUE);
	json_generator_set_indent(gen, 2);
	json_generator_set_root(gen, root);

	json_str = json_generator_to_data(gen, NULL);

	return json_str;
}

/* ── YAML formatting ──────────────────────────────────────────────── */

/*
 * format_as_yaml:
 * @resources: array of #GctlResource pointers
 *
 * Renders resources as simple key: value pairs separated by "---".
 *
 * Returns: (transfer full): the YAML-ish string
 */
static gchar *
format_as_yaml(GPtrArray *resources)
{
	GString *buf;
	guint i;

	buf = g_string_new(NULL);

	for (i = 0; i < resources->len; i++) {
		GctlResource *res;

		res = (GctlResource *)g_ptr_array_index(resources, i);

		if (i > 0)
			g_string_append(buf, "---\n");

		g_string_append_printf(buf, "kind: %s\n",
			gctl_resource_kind_to_string(gctl_resource_get_kind(res)));

		if (gctl_resource_get_number(res) >= 0) {
			g_string_append_printf(buf, "number: %d\n",
				gctl_resource_get_number(res));
		}

		g_string_append_printf(buf, "title: %s\n",
			gctl_resource_get_title(res) ? gctl_resource_get_title(res) : "");

		if (gctl_resource_get_state(res) != NULL) {
			g_string_append_printf(buf, "state: %s\n",
				gctl_resource_get_state(res));
		}

		if (gctl_resource_get_author(res) != NULL) {
			g_string_append_printf(buf, "author: %s\n",
				gctl_resource_get_author(res));
		}

		if (gctl_resource_get_url(res) != NULL) {
			g_string_append_printf(buf, "url: %s\n",
				gctl_resource_get_url(res));
		}

		if (gctl_resource_get_created_at(res) != NULL) {
			g_string_append_printf(buf, "created_at: %s\n",
				gctl_resource_get_created_at(res));
		}

		if (gctl_resource_get_updated_at(res) != NULL) {
			g_string_append_printf(buf, "updated_at: %s\n",
				gctl_resource_get_updated_at(res));
		}
	}

	return g_string_free(buf, FALSE);
}

/* ── CSV formatting ───────────────────────────────────────────────── */

/*
 * format_as_csv:
 * @resources: array of #GctlResource pointers
 *
 * Renders resources as CSV with a header row.  Fields are properly
 * quoted when they contain commas, double quotes, or newlines.
 *
 * Returns: (transfer full): the CSV string
 */
static gchar *
format_as_csv(GPtrArray *resources)
{
	GString *buf;
	guint n_cols;
	const gchar **headers;
	GctlResourceKind kind;
	guint i;
	guint j;

	if (resources->len == 0)
		return g_strdup("");

	kind    = gctl_resource_get_kind((GctlResource *)g_ptr_array_index(resources, 0));
	headers = get_columns_for_kind(kind, &n_cols);

	buf = g_string_new(NULL);

	/* Header row */
	for (j = 0; j < n_cols; j++) {
		g_autofree gchar *q = NULL;

		if (j > 0)
			g_string_append_c(buf, ',');
		q = csv_quote(headers[j]);
		g_string_append(buf, q);
	}
	g_string_append_c(buf, '\n');

	/* Data rows */
	for (i = 0; i < resources->len; i++) {
		GctlResource *res;

		res = (GctlResource *)g_ptr_array_index(resources, i);
		for (j = 0; j < n_cols; j++) {
			const gchar *val;
			g_autofree gchar *q = NULL;

			if (j > 0)
				g_string_append_c(buf, ',');
			val = get_resource_field(res, kind, j);
			q = csv_quote(val);
			g_string_append(buf, q);
		}
		g_string_append_c(buf, '\n');
	}

	return g_string_free(buf, FALSE);
}

/* ── Public API ───────────────────────────────────────────────────── */

GctlOutputFormatter *
gctl_output_formatter_new(GctlOutputFormat format)
{
	GctlOutputFormatter *self;

	self = (GctlOutputFormatter *)g_object_new(
		GCTL_TYPE_OUTPUT_FORMATTER, NULL);
	self->format = format;

	return self;
}

void
gctl_output_formatter_set_format(
	GctlOutputFormatter  *self,
	GctlOutputFormat      format
){
	g_return_if_fail(GCTL_IS_OUTPUT_FORMATTER(self));

	self->format = format;
}

GctlOutputFormat
gctl_output_formatter_get_format(GctlOutputFormatter *self)
{
	g_return_val_if_fail(GCTL_IS_OUTPUT_FORMATTER(self),
	                     GCTL_OUTPUT_FORMAT_TABLE);

	return self->format;
}

gchar *
gctl_output_formatter_format_resources(
	GctlOutputFormatter  *self,
	GPtrArray            *resources
){
	g_return_val_if_fail(GCTL_IS_OUTPUT_FORMATTER(self), NULL);
	g_return_val_if_fail(resources != NULL, NULL);

	switch (self->format) {
	case GCTL_OUTPUT_FORMAT_TABLE:
		return format_as_table(self, resources);
	case GCTL_OUTPUT_FORMAT_JSON:
		return format_as_json(resources);
	case GCTL_OUTPUT_FORMAT_YAML:
		return format_as_yaml(resources);
	case GCTL_OUTPUT_FORMAT_CSV:
		return format_as_csv(resources);
	default:
		return format_as_table(self, resources);
	}
}

gchar *
gctl_output_formatter_format_resource(
	GctlOutputFormatter  *self,
	GctlResource         *resource
){
	g_autoptr(GPtrArray) arr = NULL;

	g_return_val_if_fail(GCTL_IS_OUTPUT_FORMATTER(self), NULL);
	g_return_val_if_fail(resource != NULL, NULL);

	/* Wrap the single resource in a temporary array */
	arr = g_ptr_array_new();
	g_ptr_array_add(arr, resource);

	return gctl_output_formatter_format_resources(self, arr);
}

void
gctl_output_formatter_print_resources(
	GctlOutputFormatter  *self,
	GPtrArray            *resources
){
	g_autofree gchar *output = NULL;

	g_return_if_fail(GCTL_IS_OUTPUT_FORMATTER(self));
	g_return_if_fail(resources != NULL);

	output = gctl_output_formatter_format_resources(self, resources);
	g_print("%s", output);
}

void
gctl_output_formatter_print_resource(
	GctlOutputFormatter  *self,
	GctlResource         *resource
){
	g_autofree gchar *output = NULL;

	g_return_if_fail(GCTL_IS_OUTPUT_FORMATTER(self));
	g_return_if_fail(resource != NULL);

	output = gctl_output_formatter_format_resource(self, resource);
	g_print("%s", output);
}
