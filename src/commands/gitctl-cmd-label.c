/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
 * gitctl-cmd-label.c - Label management command handler
 *
 * Implements the "label" command with verb dispatch for: list, create,
 * and delete.
 *
 * Maps to: gh label list, glab label list, API fallback for fj/tea.
 */

#define GCTL_COMPILATION

#include <string.h>

#include "commands/gitctl-cmd-common-private.h"
#include "commands/gitctl-cmd-label.h"

/* ── Verb dispatch table ─────────────────────────────────────────────── */

static const GctlVerbEntry label_verbs[] = {
	{ "list",   "List labels",          GCTL_VERB_LIST   },
	{ "create", "Create a new label",   GCTL_VERB_CREATE },
	{ "delete", "Delete a label",       GCTL_VERB_DELETE },
};

static const gsize N_LABEL_VERBS = G_N_ELEMENTS(label_verbs);

/* ── Usage printer ───────────────────────────────────────────────────── */

/**
 * print_usage:
 *
 * Prints the usage summary for the "label" command, listing all
 * available verbs and their descriptions.
 */
static void
print_usage(void)
{
	gctl_cmd_print_verb_table("label", label_verbs, N_LABEL_VERBS);
}

/* ── label list ──────────────────────────────────────────────────────── */

/**
 * cmd_label_list:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl label list".
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_label_list(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	gboolean use_pager = FALSE;

	GOptionEntry entries[] = {
		{ "pager", 0, 0, G_OPTION_ARG_NONE, &use_pager,
		  "Pipe output through $PAGER", NULL },
		{ NULL }
	};

	opt_context = g_option_context_new("- list labels");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	if (use_pager)
		g_hash_table_insert(params, g_strdup("pager"), g_strdup("true"));

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_LABEL,
	                             GCTL_VERB_LIST, NULL, params);
}

/* ── label create ────────────────────────────────────────────────────── */

/**
 * cmd_label_create:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl label create --name <name> --color <hex>".
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_label_create(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	gchar *name = NULL;
	gchar *color = NULL;
	gchar *description = NULL;
	gint ret;

	GOptionEntry entries[] = {
		{ "name", 'n', 0, G_OPTION_ARG_STRING, &name,
		  "Label name", "NAME" },
		{ "color", 'c', 0, G_OPTION_ARG_STRING, &color,
		  "Label color (hex, e.g. ff0000)", "HEX" },
		{ "description", 'd', 0, G_OPTION_ARG_STRING, &description,
		  "Label description", "DESC" },
		{ NULL }
	};

	opt_context = g_option_context_new("- create a label");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	if (name == NULL)
	{
		g_printerr("error: --name is required\n");
		g_printerr("Usage: gitctl label create --name <name> --color <hex>\n");
		return 1;
	}

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_insert(params, g_strdup("name"), g_strdup(name));

	if (color != NULL)
		g_hash_table_insert(params, g_strdup("color"), g_strdup(color));

	if (description != NULL)
		g_hash_table_insert(params, g_strdup("description"),
		                    g_strdup(description));

	ret = gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_LABEL,
	                            GCTL_VERB_CREATE, name, params);

	g_free(name);
	g_free(color);
	g_free(description);

	return ret;
}

/* ── label delete ────────────────────────────────────────────────────── */

/**
 * cmd_label_delete:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl label delete <name>".  Requires --yes / -y
 * to confirm the destructive operation (skipped in --dry-run mode).
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_label_delete(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	const gchar *label_name;
	gboolean opt_yes = FALSE;

	GOptionEntry entries[] = {
		{ "yes", 'y', 0, G_OPTION_ARG_NONE, &opt_yes,
		  "Skip confirmation prompt (DANGEROUS)", NULL },
		{ NULL }
	};

	opt_context = g_option_context_new("<name> - delete a label");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	if (argc < 2)
	{
		g_printerr("error: label name required\n");
		g_printerr("Usage: gitctl label delete [--yes] <name>\n");
		return 1;
	}

	label_name = argv[1];

	/* Require explicit --yes unless running in dry-run mode */
	if (!opt_yes && !gctl_app_get_dry_run(app))
	{
		g_printerr("warning: this will PERMANENTLY delete label '%s'\n",
		           label_name);
		g_printerr("Run with --yes to confirm, or --dry-run to preview.\n");
		return 1;
	}

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	if (opt_yes)
		g_hash_table_insert(params, g_strdup("confirm"), g_strdup("true"));

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_LABEL,
	                             GCTL_VERB_DELETE, label_name, params);
}

/* ── Main entry point ────────────────────────────────────────────────── */

/**
 * gctl_cmd_label:
 * @app: the #GctlApp instance
 * @argc: argument count (verb + verb-specific args)
 * @argv: (array length=argc): argument vector, where argv[0] is the verb
 *
 * Main entry point for the "label" command.  Extracts the verb from
 * argv[0], looks it up in the dispatch table, and delegates to the
 * appropriate verb handler.
 *
 * Returns: 0 on success, 1 on error
 */
gint
gctl_cmd_label(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	const GctlVerbEntry *entry;
	const gchar *verb_name;

	if (argc < 1)
	{
		print_usage();
		return 0;
	}

	/* Handle --help / -h before verb lookup */
	if (g_strcmp0(argv[0], "--help") == 0 ||
	    g_strcmp0(argv[0], "-h") == 0)
	{
		print_usage();
		return 0;
	}

	verb_name = argv[0];
	entry = gctl_cmd_find_verb(label_verbs, N_LABEL_VERBS, verb_name);

	if (entry == NULL)
	{
		g_printerr("error: unknown verb '%s' for label command\n",
		           verb_name);
		print_usage();
		return 1;
	}

	switch (entry->verb)
	{
		case GCTL_VERB_LIST:
			return cmd_label_list(app, argc, argv);
		case GCTL_VERB_CREATE:
			return cmd_label_create(app, argc, argv);
		case GCTL_VERB_DELETE:
			return cmd_label_delete(app, argc, argv);
		default:
			g_printerr("error: verb '%s' not implemented for label\n",
			           verb_name);
			return 1;
	}
}
