/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
 * gitctl-cmd-key.c - SSH/deploy key command handler
 *
 * Implements the "key" command with verb dispatch for: list, add, and
 * remove.
 *
 * Maps to: gh ssh-key list, glab ssh-key list, API fallback for fj/tea.
 */

#define GCTL_COMPILATION

#include <string.h>

#include "commands/gitctl-cmd-common-private.h"
#include "commands/gitctl-cmd-key.h"

/* ── Verb dispatch table ─────────────────────────────────────────────── */

static const GctlVerbEntry key_verbs[] = {
	{ "list",   "List SSH/deploy keys",    GCTL_VERB_LIST   },
	{ "add",    "Add an SSH/deploy key",   GCTL_VERB_CREATE },
	{ "remove", "Remove an SSH/deploy key", GCTL_VERB_DELETE },
};

static const gsize N_KEY_VERBS = G_N_ELEMENTS(key_verbs);

/* ── Usage printer ───────────────────────────────────────────────────── */

/**
 * print_usage:
 *
 * Prints the usage summary for the "key" command, listing all
 * available verbs and their descriptions.
 */
static void
print_usage(void)
{
	gctl_cmd_print_verb_table("key", key_verbs, N_KEY_VERBS);
}

/* ── key list ────────────────────────────────────────────────────────── */

/**
 * cmd_key_list:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl key list".
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_key_list(
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

	opt_context = g_option_context_new("- list SSH/deploy keys");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	if (use_pager)
		g_hash_table_insert(params, g_strdup("pager"), g_strdup("true"));

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_KEY,
	                             GCTL_VERB_LIST, NULL, params);
}

/* ── key add ─────────────────────────────────────────────────────────── */

/**
 * cmd_key_add:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl key add --title <title> --key <pubkey>".
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_key_add(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	gchar *title = NULL;
	gchar *key = NULL;
	gint ret;

	GOptionEntry entries[] = {
		{ "title", 't', 0, G_OPTION_ARG_STRING, &title,
		  "Key title", "TITLE" },
		{ "key", 'k', 0, G_OPTION_ARG_STRING, &key,
		  "Public key string", "PUBKEY" },
		{ NULL }
	};

	opt_context = g_option_context_new("- add an SSH/deploy key");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	if (title == NULL || key == NULL)
	{
		g_printerr("error: --title and --key are required\n");
		g_printerr("Usage: gitctl key add --title <title> --key <pubkey>\n");
		g_free(title);
		g_free(key);
		return 1;
	}

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_insert(params, g_strdup("title"), g_strdup(title));
	g_hash_table_insert(params, g_strdup("key"), g_strdup(key));

	ret = gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_KEY,
	                            GCTL_VERB_CREATE, NULL, params);

	g_free(title);
	g_free(key);

	return ret;
}

/* ── key remove ──────────────────────────────────────────────────────── */

/**
 * cmd_key_remove:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl key remove <id>".  Requires --yes / -y
 * to confirm the destructive operation (skipped in --dry-run mode).
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_key_remove(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	const gchar *key_id;
	gboolean opt_yes = FALSE;

	GOptionEntry entries[] = {
		{ "yes", 'y', 0, G_OPTION_ARG_NONE, &opt_yes,
		  "Skip confirmation prompt (DANGEROUS)", NULL },
		{ NULL }
	};

	opt_context = g_option_context_new("<id> - remove an SSH/deploy key");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	if (argc < 2)
	{
		g_printerr("error: key ID required\n");
		g_printerr("Usage: gitctl key remove [--yes] <id>\n");
		return 1;
	}

	key_id = argv[1];

	/* Require explicit --yes unless running in dry-run mode */
	if (!opt_yes && !gctl_app_get_dry_run(app))
	{
		g_printerr("warning: this will PERMANENTLY remove SSH key '%s'\n",
		           key_id);
		g_printerr("Run with --yes to confirm, or --dry-run to preview.\n");
		return 1;
	}

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	if (opt_yes)
		g_hash_table_insert(params, g_strdup("confirm"), g_strdup("true"));

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_KEY,
	                             GCTL_VERB_DELETE, key_id, params);
}

/* ── Main entry point ────────────────────────────────────────────────── */

/**
 * gctl_cmd_key:
 * @app: the #GctlApp instance
 * @argc: argument count (verb + verb-specific args)
 * @argv: (array length=argc): argument vector, where argv[0] is the verb
 *
 * Main entry point for the "key" command.  Extracts the verb from
 * argv[0], looks it up in the dispatch table, and delegates to the
 * appropriate verb handler.
 *
 * Returns: 0 on success, 1 on error
 */
gint
gctl_cmd_key(
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
	entry = gctl_cmd_find_verb(key_verbs, N_KEY_VERBS, verb_name);

	if (entry == NULL)
	{
		g_printerr("error: unknown verb '%s' for key command\n",
		           verb_name);
		print_usage();
		return 1;
	}

	switch (entry->verb)
	{
		case GCTL_VERB_LIST:
			return cmd_key_list(app, argc, argv);
		case GCTL_VERB_CREATE:
			return cmd_key_add(app, argc, argv);
		case GCTL_VERB_DELETE:
			return cmd_key_remove(app, argc, argv);
		default:
			g_printerr("error: verb '%s' not implemented for key\n",
			           verb_name);
			return 1;
	}
}
