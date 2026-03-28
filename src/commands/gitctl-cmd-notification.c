/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
 * gitctl-cmd-notification.c - Notification command handler
 *
 * Implements the "notification" command with verb dispatch for: list
 * and read.
 *
 * Maps to: gh api /notifications, glab api /todos, API fallback for
 * fj/tea.
 */

#define GCTL_COMPILATION

#include <string.h>

#include "commands/gitctl-cmd-common-private.h"
#include "commands/gitctl-cmd-notification.h"

/* ── Verb dispatch table ─────────────────────────────────────────────── */

static const GctlVerbEntry notification_verbs[] = {
	{ "list",  "List notifications",           GCTL_VERB_LIST },
	{ "read",  "Mark notification(s) as read", GCTL_VERB_READ },
};

static const gsize N_NOTIFICATION_VERBS = G_N_ELEMENTS(notification_verbs);

/* ── Usage printer ───────────────────────────────────────────────────── */

/**
 * print_usage:
 *
 * Prints the usage summary for the "notification" command, listing all
 * available verbs and their descriptions.
 */
static void
print_usage(void)
{
	gctl_cmd_print_verb_table("notification",
	                          notification_verbs, N_NOTIFICATION_VERBS);
}

/* ── notification list ───────────────────────────────────────────────── */

/**
 * cmd_notification_list:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl notification list".  Parses --unread flag.
 * By default shows only unread notifications.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_notification_list(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	gboolean unread = TRUE;
	gboolean use_pager = FALSE;
	gint ret;

	GOptionEntry entries[] = {
		{ "unread", 'u', 0, G_OPTION_ARG_NONE, &unread,
		  "Show only unread notifications (default)", NULL },
		{ "pager", 0, 0, G_OPTION_ARG_NONE, &use_pager,
		  "Pipe output through $PAGER", NULL },
		{ NULL }
	};

	opt_context = g_option_context_new("- list notifications");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	if (unread)
		g_hash_table_insert(params, g_strdup("unread"), g_strdup("true"));

	if (use_pager)
		g_hash_table_insert(params, g_strdup("pager"), g_strdup("true"));

	ret = gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_NOTIFICATION,
	                            GCTL_VERB_LIST, NULL, params);

	return ret;
}

/* ── notification read ───────────────────────────────────────────────── */

/**
 * cmd_notification_read:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl notification read [id]".  If an ID is provided,
 * marks that specific notification as read.  Otherwise, marks all
 * notifications as read.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_notification_read(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *notif_id;

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	notif_id = (argc >= 2) ? argv[1] : NULL;

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_NOTIFICATION,
	                             GCTL_VERB_READ, notif_id, params);
}

/* ── Main entry point ────────────────────────────────────────────────── */

/**
 * gctl_cmd_notification:
 * @app: the #GctlApp instance
 * @argc: argument count (verb + verb-specific args)
 * @argv: (array length=argc): argument vector, where argv[0] is the verb
 *
 * Main entry point for the "notification" command.  Extracts the verb
 * from argv[0], looks it up in the dispatch table, and delegates to
 * the appropriate verb handler.
 *
 * Returns: 0 on success, 1 on error
 */
gint
gctl_cmd_notification(
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
	entry = gctl_cmd_find_verb(notification_verbs, N_NOTIFICATION_VERBS,
	                           verb_name);

	if (entry == NULL)
	{
		g_printerr("error: unknown verb '%s' for notification command\n",
		           verb_name);
		print_usage();
		return 1;
	}

	switch (entry->verb)
	{
		case GCTL_VERB_LIST:
			return cmd_notification_list(app, argc, argv);
		case GCTL_VERB_READ:
			return cmd_notification_read(app, argc, argv);
		default:
			g_printerr("error: verb '%s' not implemented for notification\n",
			           verb_name);
			return 1;
	}
}
