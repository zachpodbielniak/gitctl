/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
 * gitctl-cmd-webhook.c - Webhook management command handler
 *
 * Implements the "webhook" command with verb dispatch for: list, create,
 * delete, and get.
 *
 * Maps to: API on all forges (/repos/{owner}/{repo}/hooks).
 */

#define GCTL_COMPILATION

#include <string.h>
#include <json-glib/json-glib.h>

#include "commands/gitctl-cmd-common-private.h"
#include "commands/gitctl-cmd-webhook.h"

/* ── Verb dispatch table ─────────────────────────────────────────────── */

static const GctlVerbEntry webhook_verbs[] = {
	{ "list",   "List webhooks",       GCTL_VERB_LIST   },
	{ "create", "Create a webhook",    GCTL_VERB_CREATE },
	{ "delete", "Delete a webhook",    GCTL_VERB_DELETE },
	{ "get",    "View webhook details", GCTL_VERB_GET    },
};

static const gsize N_WEBHOOK_VERBS = G_N_ELEMENTS(webhook_verbs);

/* ── Usage printer ───────────────────────────────────────────────────── */

/**
 * print_usage:
 *
 * Prints the usage summary for the "webhook" command, listing all
 * available verbs and their descriptions.
 */
static void
print_usage(void)
{
	gctl_cmd_print_verb_table("webhook", webhook_verbs, N_WEBHOOK_VERBS);
}

/* ── webhook list ────────────────────────────────────────────────────── */

/**
 * cmd_webhook_list:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl webhook list".
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_webhook_list(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;

	(void)argc;
	(void)argv;

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_WEBHOOK,
	                             GCTL_VERB_LIST, NULL, params);
}

/* ── webhook create ──────────────────────────────────────────────────── */

/**
 * cmd_webhook_create:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl webhook create --url <url> --events <comma-separated>".
 * Builds a JSON body for the API request.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_webhook_create(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	gchar *url = NULL;
	gchar *events = NULL;
	gchar *secret = NULL;
	gchar *content_type = NULL;
	gint ret;

	GOptionEntry entries[] = {
		{ "url", 'u', 0, G_OPTION_ARG_STRING, &url,
		  "Webhook URL", "URL" },
		{ "events", 'e', 0, G_OPTION_ARG_STRING, &events,
		  "Comma-separated event types", "EVENTS" },
		{ "secret", 's', 0, G_OPTION_ARG_STRING, &secret,
		  "Webhook secret", "SECRET" },
		{ "content-type", 0, 0, G_OPTION_ARG_STRING, &content_type,
		  "Content type (json/form, default: json)", "TYPE" },
		{ NULL }
	};

	opt_context = g_option_context_new("- create a webhook");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	if (url == NULL)
	{
		g_printerr("error: --url is required\n");
		g_printerr("Usage: gitctl webhook create --url <url> "
		           "--events <comma-separated>\n");
		g_free(url);
		g_free(events);
		g_free(secret);
		g_free(content_type);
		return 1;
	}

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_insert(params, g_strdup("url"), g_strdup(url));

	if (events != NULL)
		g_hash_table_insert(params, g_strdup("events"), g_strdup(events));

	if (secret != NULL)
		g_hash_table_insert(params, g_strdup("secret"), g_strdup(secret));

	if (content_type != NULL)
		g_hash_table_insert(params, g_strdup("content_type"),
		                    g_strdup(content_type));

	ret = gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_WEBHOOK,
	                            GCTL_VERB_CREATE, NULL, params);

	g_free(url);
	g_free(events);
	g_free(secret);
	g_free(content_type);

	return ret;
}

/* ── webhook delete ──────────────────────────────────────────────────── */

/**
 * cmd_webhook_delete:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl webhook delete <id>".
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_webhook_delete(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *webhook_id;

	if (argc < 2)
	{
		g_printerr("error: webhook ID required\n");
		g_printerr("Usage: gitctl webhook delete <id>\n");
		return 1;
	}

	webhook_id = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_WEBHOOK,
	                             GCTL_VERB_DELETE, webhook_id, params);
}

/* ── webhook get ─────────────────────────────────────────────────────── */

/**
 * cmd_webhook_get:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl webhook get <id>".  Views details of a specific
 * webhook.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_webhook_get(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *webhook_id;

	if (argc < 2)
	{
		g_printerr("error: webhook ID required\n");
		g_printerr("Usage: gitctl webhook get <id>\n");
		return 1;
	}

	webhook_id = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_WEBHOOK,
	                             GCTL_VERB_GET, webhook_id, params);
}

/* ── Main entry point ────────────────────────────────────────────────── */

/**
 * gctl_cmd_webhook:
 * @app: the #GctlApp instance
 * @argc: argument count (verb + verb-specific args)
 * @argv: (array length=argc): argument vector, where argv[0] is the verb
 *
 * Main entry point for the "webhook" command.  Extracts the verb from
 * argv[0], looks it up in the dispatch table, and delegates to the
 * appropriate verb handler.
 *
 * Returns: 0 on success, 1 on error
 */
gint
gctl_cmd_webhook(
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
	entry = gctl_cmd_find_verb(webhook_verbs, N_WEBHOOK_VERBS, verb_name);

	if (entry == NULL)
	{
		g_printerr("error: unknown verb '%s' for webhook command\n",
		           verb_name);
		print_usage();
		return 1;
	}

	switch (entry->verb)
	{
		case GCTL_VERB_LIST:
			return cmd_webhook_list(app, argc, argv);
		case GCTL_VERB_CREATE:
			return cmd_webhook_create(app, argc, argv);
		case GCTL_VERB_DELETE:
			return cmd_webhook_delete(app, argc, argv);
		case GCTL_VERB_GET:
			return cmd_webhook_get(app, argc, argv);
		default:
			g_printerr("error: verb '%s' not implemented for webhook\n",
			           verb_name);
			return 1;
	}
}
