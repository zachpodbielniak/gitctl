/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
 * gitctl-cmd-mirror.c - Mirror command handler
 *
 * Implements the "mirror" command with verb dispatch for: list, add,
 * remove, sync, and get.
 *
 * The "add" verb constructs a JSON body for the forge API and
 * optionally creates the destination repository first via
 * --create-repo.
 *
 * All verbs parse their options and delegate to gctl_cmd_execute_verb().
 */

#define GCTL_COMPILATION

#include <string.h>
#include <json-glib/json-glib.h>

#include "commands/gitctl-cmd-common-private.h"
#include "commands/gitctl-cmd-mirror.h"

/* ── Verb dispatch table ─────────────────────────────────────────────── */

static const GctlVerbEntry mirror_verbs[] = {
	{ "list",   "List configured mirrors",         GCTL_VERB_LIST   },
	{ "add",    "Add a new mirror",                GCTL_VERB_CREATE },
	{ "remove", "Remove a mirror",                 GCTL_VERB_DELETE },
	{ "sync",   "Trigger mirror synchronization",  GCTL_VERB_SYNC   },
	{ "get",    "Show mirror details",             GCTL_VERB_GET    },
};

static const gsize N_MIRROR_VERBS = G_N_ELEMENTS(mirror_verbs);

/* ── Usage printer ───────────────────────────────────────────────────── */

/**
 * print_usage:
 *
 * Prints the usage summary for the "mirror" command, listing all
 * available verbs and their descriptions.
 */
static void
print_usage(void)
{
	gctl_cmd_print_verb_table("mirror", mirror_verbs, N_MIRROR_VERBS);
}

/* ── URL parsing helper ──────────────────────────────────────────────── */

/**
 * parse_mirror_url:
 * @url: a remote repository URL (HTTPS or SSH)
 * @host: (out) (transfer full): the hostname
 * @owner: (out) (transfer full): the repository owner
 * @repo: (out) (transfer full): the repository name (without .git suffix)
 *
 * Parses a mirror destination URL into its component parts.  Handles
 * HTTPS URLs (https://host/owner/repo.git) and SSH URLs
 * (git@host:owner/repo.git).
 *
 * Returns: %TRUE on success, %FALSE if the URL could not be parsed
 */
static gboolean
parse_mirror_url(
	const gchar  *url,
	gchar       **host,
	gchar       **owner,
	gchar       **repo
){
	/* HTTPS / HTTP URLs */
	if (g_str_has_prefix(url, "https://") ||
	    g_str_has_prefix(url, "http://"))
	{
		g_autoptr(GUri) uri = NULL;
		const gchar *uri_host;
		const gchar *path;
		g_auto(GStrv) segments = NULL;
		guint count;
		gchar *repo_name;

		uri = g_uri_parse(url, G_URI_FLAGS_NONE, NULL);
		if (uri == NULL)
			return FALSE;

		uri_host = g_uri_get_host(uri);
		path = g_uri_get_path(uri);

		if (uri_host == NULL || path == NULL || *path == '\0')
			return FALSE;

		if (*path == '/')
			path++;

		segments = g_strsplit(path, "/", 0);
		count = g_strv_length(segments);

		if (count < 2)
			return FALSE;

		repo_name = g_strdup(segments[1]);
		if (g_str_has_suffix(repo_name, ".git"))
			repo_name[strlen(repo_name) - 4] = '\0';

		*host  = g_strdup(uri_host);
		*owner = g_strdup(segments[0]);
		*repo  = repo_name;

		return TRUE;
	}

	/* SSH URLs: git@host:owner/repo.git */
	if (g_str_has_prefix(url, "git@") || strchr(url, ':') != NULL)
	{
		const gchar *at_sign;
		const gchar *colon;
		g_auto(GStrv) parts = NULL;
		guint count;
		gchar *repo_name;

		at_sign = strchr(url, '@');
		if (at_sign == NULL)
			return FALSE;

		colon = strchr(at_sign + 1, ':');
		if (colon == NULL)
			return FALSE;

		*host = g_strndup(at_sign + 1, (gsize)(colon - at_sign - 1));

		parts = g_strsplit(colon + 1, "/", 0);
		count = g_strv_length(parts);

		if (count < 2)
		{
			g_free(*host);
			*host = NULL;
			return FALSE;
		}

		repo_name = g_strdup(parts[1]);
		if (g_str_has_suffix(repo_name, ".git"))
			repo_name[strlen(repo_name) - 4] = '\0';

		*owner = g_strdup(parts[0]);
		*repo  = repo_name;

		return TRUE;
	}

	return FALSE;
}

/* ── JSON body builder ───────────────────────────────────────────────── */

/**
 * build_push_mirror_body:
 * @forge_type: the destination forge type
 * @url: the remote mirror URL
 * @username: (nullable): authentication username
 * @token: (nullable): authentication token / password
 * @interval: the sync interval string (e.g. "8h0m0s")
 * @sync_on_commit: whether to sync on every push
 * @mask_token: if %TRUE, replaces the token with "***" (for dry-run)
 *
 * Builds a JSON request body for creating a push mirror.  The body
 * format depends on the forge type:
 *
 * - Forgejo/Gitea: uses remote_address, remote_username, remote_password,
 *   interval, sync_on_commit keys
 * - GitLab: uses url and enabled keys
 * - GitHub: returns %NULL (push mirrors not supported via API)
 *
 * Returns: (transfer full) (nullable): a JSON string, or %NULL if the
 *     forge type does not support push mirrors
 */
static gchar *
build_push_mirror_body(
	GctlForgeType  forge_type,
	const gchar   *url,
	const gchar   *username,
	const gchar   *token,
	const gchar   *interval,
	gboolean       sync_on_commit,
	gboolean       mask_token
){
	g_autoptr(JsonBuilder) builder = NULL;
	g_autoptr(JsonGenerator) gen = NULL;
	g_autoptr(JsonNode) root = NULL;
	gchar *json_str;

	builder = json_builder_new();

	if (forge_type == GCTL_FORGE_TYPE_FORGEJO ||
	    forge_type == GCTL_FORGE_TYPE_GITEA)
	{
		json_builder_begin_object(builder);

		json_builder_set_member_name(builder, "remote_address");
		json_builder_add_string_value(builder, url);

		if (username != NULL)
		{
			json_builder_set_member_name(builder, "remote_username");
			json_builder_add_string_value(builder, username);
		}

		if (token != NULL)
		{
			json_builder_set_member_name(builder, "remote_password");
			json_builder_add_string_value(builder,
			                              mask_token ? "***" : token);
		}

		json_builder_set_member_name(builder, "interval");
		json_builder_add_string_value(builder, interval);

		json_builder_set_member_name(builder, "sync_on_commit");
		json_builder_add_boolean_value(builder, sync_on_commit);

		json_builder_end_object(builder);
	}
	else if (forge_type == GCTL_FORGE_TYPE_GITLAB)
	{
		json_builder_begin_object(builder);

		json_builder_set_member_name(builder, "url");
		json_builder_add_string_value(builder, url);

		json_builder_set_member_name(builder, "enabled");
		json_builder_add_boolean_value(builder, TRUE);

		json_builder_end_object(builder);
	}
	else
	{
		/* GitHub does not support push mirrors via API */
		return NULL;
	}

	root = json_builder_get_root(builder);
	gen = json_generator_new();
	json_generator_set_root(gen, root);

	json_str = json_generator_to_data(gen, NULL);
	return json_str;
}

/* ── mirror list ─────────────────────────────────────────────────────── */

/**
 * cmd_mirror_list:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl mirror list".  No special arguments.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_mirror_list(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;

	GOptionEntry entries[] = {
		{ NULL }
	};

	opt_context = g_option_context_new("- list configured mirrors");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_MIRROR,
	                             GCTL_VERB_LIST, NULL, params);
}

/* ── mirror add ──────────────────────────────────────────────────────── */

/**
 * cmd_mirror_add:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl mirror add --url <url> [options]".  Builds a JSON
 * request body and optionally creates the destination repository first
 * when --create-repo is specified.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_mirror_add(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	GctlExecutor *executor;
	GctlContextResolver *resolver;
	GctlModuleManager *module_manager;
	GctlConfig *config;
	g_autoptr(GctlForgeContext) context = NULL;
	GctlForgeType source_forge_type;
	const gchar *default_remote;
	gchar *mirror_url = NULL;
	gchar *direction = NULL;
	gchar *interval = NULL;
	gboolean sync_on_commit = FALSE;
	gchar *mirror_token = NULL;
	gchar *mirror_username = NULL;
	gboolean create_repo = FALSE;
	g_autofree gchar *body = NULL;
	gboolean is_dry_run;
	gint ret;

	GOptionEntry entries[] = {
		{ "url", 'u', 0, G_OPTION_ARG_STRING, &mirror_url,
		  "Remote repository URL for the mirror (required)", "URL" },
		{ "direction", 'd', 0, G_OPTION_ARG_STRING, &direction,
		  "Mirror direction: push or pull (default: push)", "DIR" },
		{ "interval", 'i', 0, G_OPTION_ARG_STRING, &interval,
		  "Sync interval (default: 8h0m0s)", "INTERVAL" },
		{ "sync-on-commit", 0, 0, G_OPTION_ARG_NONE, &sync_on_commit,
		  "Sync on every push", NULL },
		{ "token", 't', 0, G_OPTION_ARG_STRING, &mirror_token,
		  "Auth token for the remote", "TOKEN" },
		{ "username", 0, 0, G_OPTION_ARG_STRING, &mirror_username,
		  "Username for the remote", "USER" },
		{ "create-repo", 0, 0, G_OPTION_ARG_NONE, &create_repo,
		  "Create the destination repo first", NULL },
		{ NULL }
	};

	opt_context = g_option_context_new("- add a new mirror");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		ret = 1;
		goto out;
	}

	if (mirror_url == NULL)
	{
		g_printerr("error: --url is required\n");
		g_printerr("Usage: gitctl mirror add --url <url> [options]\n");
		ret = 1;
		goto out;
	}

	/* Apply defaults */
	if (direction == NULL)
		direction = g_strdup("push");
	if (interval == NULL)
		interval = g_strdup("8h0m0s");

	/*
	 * Auto-detect username and token from the mirror URL when not
	 * explicitly provided.  Parse the URL to determine the destination
	 * forge, then use the corresponding env var for the token and
	 * extract the owner from the URL path as the username.
	 */
	{
		g_autofree gchar *dest_host = NULL;
		g_autofree gchar *dest_owner = NULL;
		g_autofree gchar *dest_repo = NULL;
		GctlConfig *cfg;

		cfg = gctl_app_get_config(app);

		if (parse_mirror_url(mirror_url, &dest_host,
		                     &dest_owner, &dest_repo))
		{
			/* Auto-detect username from URL owner */
			if (mirror_username == NULL && dest_owner != NULL)
				mirror_username = g_strdup(dest_owner);

			/* Auto-detect token from destination forge env var */
			if (mirror_token == NULL && cfg != NULL)
			{
				GctlForgeType dest_ft;
				const gchar *env_token = NULL;

				dest_ft = gctl_config_get_forge_for_host(
					cfg, dest_host);

				switch (dest_ft) {
				case GCTL_FORGE_TYPE_GITHUB:
					env_token = g_getenv("GITHUB_TOKEN");
					break;
				case GCTL_FORGE_TYPE_GITLAB:
					env_token = g_getenv("GITLAB_TOKEN");
					break;
				case GCTL_FORGE_TYPE_FORGEJO:
					env_token = g_getenv("FORGEJO_TOKEN");
					break;
				case GCTL_FORGE_TYPE_GITEA:
					env_token = g_getenv("GITEA_TOKEN");
					break;
				default:
					break;
				}

				if (env_token != NULL && *env_token != '\0') {
					mirror_token = g_strdup(env_token);
					if (gctl_app_get_verbose(app))
						g_printerr("note: using %s token "
						           "from env var for mirror "
						           "destination\n",
						           gctl_forge_type_to_string(
						               dest_ft));
				}
			}
		}
	}

	/* Resolve the source forge context */
	executor = gctl_app_get_executor(app);
	resolver = gctl_app_get_resolver(app);
	module_manager = gctl_app_get_module_manager(app);
	config = gctl_app_get_config(app);
	default_remote = gctl_config_get_default_remote(config);

	context = gctl_context_resolver_resolve(resolver, default_remote, &error);
	if (context == NULL)
	{
		g_printerr("error: failed to resolve forge context: %s\n",
		           error ? error->message : "unknown error");
		ret = 1;
		goto out;
	}

	source_forge_type = gctl_forge_context_get_forge_type(context);
	is_dry_run = gctl_executor_get_dry_run(executor);

	/*
	 * --create-repo: optionally create the destination repository
	 * before setting up the mirror.
	 */
	if (create_repo)
	{
		g_autofree gchar *dest_host = NULL;
		g_autofree gchar *dest_owner = NULL;
		g_autofree gchar *dest_repo = NULL;

		if (!parse_mirror_url(mirror_url, &dest_host, &dest_owner, &dest_repo))
		{
			g_printerr("warning: could not parse --url for --create-repo, "
			           "skipping repo creation\n");
		}
		else
		{
			GctlForgeType dest_forge_type;
			GctlForge *dest_forge;

			dest_forge_type = gctl_config_get_forge_for_host(config, dest_host);

			if (dest_forge_type == GCTL_FORGE_TYPE_UNKNOWN)
			{
				g_printerr("warning: unknown forge for host '%s', "
				           "skipping repo creation\n", dest_host);
			}
			else
			{
				dest_forge = gctl_module_manager_find_forge(
				    module_manager, dest_forge_type);

				if (dest_forge == NULL)
				{
					g_printerr("warning: no module for %s, "
					           "skipping repo creation\n",
					           gctl_forge_type_to_string(dest_forge_type));
				}
				else
				{
					g_autoptr(GctlForgeContext) dest_context = NULL;
					g_autoptr(GHashTable) create_params = NULL;
					g_autoptr(GError) create_error = NULL;
					g_autoptr(GctlCommandResult) create_result = NULL;
					const gchar *dest_cli;
					gchar **create_argv;

					dest_cli = gctl_config_get_cli_path(
					    config, dest_forge_type);

					dest_context = gctl_forge_context_new(
					    dest_forge_type, mirror_url,
					    dest_owner, dest_repo,
					    dest_host, dest_cli);

					create_params = g_hash_table_new_full(
					    g_str_hash, g_str_equal, g_free, g_free);
					g_hash_table_insert(create_params,
					                    g_strdup("private"),
					                    g_strdup("true"));

					create_argv = gctl_forge_build_argv(
					    dest_forge, GCTL_RESOURCE_KIND_REPO,
					    GCTL_VERB_CREATE, dest_context,
					    create_params, &create_error);

					if (create_argv == NULL)
					{
						g_printerr("warning: could not build repo create "
						           "command: %s\n",
						           create_error ? create_error->message
						                        : "unknown error");
					}
					else
					{
						create_result = gctl_executor_run(
						    executor,
						    (const gchar * const *)create_argv,
						    &create_error);
						g_strfreev(create_argv);

						if (create_result == NULL ||
						    gctl_command_result_get_exit_code(create_result) != 0)
						{
							g_printerr("warning: repo creation failed "
							           "(may already exist), continuing "
							           "with mirror setup\n");
						}
					}
				}
			}
		}
	}

	/*
	 * Build the JSON body for the push mirror API call.
	 * In dry-run mode, mask the token so it does not leak.
	 */
	body = build_push_mirror_body(source_forge_type, mirror_url,
	                              mirror_username, mirror_token,
	                              interval, sync_on_commit, is_dry_run);

	if (body == NULL)
	{
		g_printerr("error: push mirrors are not supported for %s\n",
		           gctl_forge_type_to_string(source_forge_type));
		ret = 1;
		goto out;
	}

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_insert(params, g_strdup("body"), g_strdup(body));

	ret = gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_MIRROR,
	                            GCTL_VERB_CREATE, NULL, params);

out:
	g_free(mirror_url);
	g_free(direction);
	g_free(interval);
	g_free(mirror_token);
	g_free(mirror_username);

	return ret;
}

/* ── mirror remove ───────────────────────────────────────────────────── */

/**
 * cmd_mirror_remove:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl mirror remove <mirror-id>".
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_mirror_remove(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *mirror_id;

	if (argc < 2)
	{
		g_printerr("error: mirror identifier required\n");
		g_printerr("Usage: gitctl mirror remove <mirror-id>\n");
		return 1;
	}

	mirror_id = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_insert(params, g_strdup("mirror_id"), g_strdup(mirror_id));

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_MIRROR,
	                             GCTL_VERB_DELETE, NULL, params);
}

/* ── mirror sync ─────────────────────────────────────────────────────── */

/**
 * cmd_mirror_sync:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl mirror sync [<mirror-id>]".  When a mirror-id is
 * provided, syncs that specific mirror; otherwise triggers a sync
 * for all mirrors.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_mirror_sync(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *mirror_id;

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	if (argc >= 2)
	{
		mirror_id = argv[1];
		g_hash_table_insert(params, g_strdup("mirror_id"),
		                    g_strdup(mirror_id));
	}

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_MIRROR,
	                             GCTL_VERB_SYNC, NULL, params);
}

/* ── mirror get ──────────────────────────────────────────────────────── */

/**
 * cmd_mirror_get:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl mirror get <mirror-id>".
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_mirror_get(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *mirror_id;

	if (argc < 2)
	{
		g_printerr("error: mirror identifier required\n");
		g_printerr("Usage: gitctl mirror get <mirror-id>\n");
		return 1;
	}

	mirror_id = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_insert(params, g_strdup("mirror_id"), g_strdup(mirror_id));

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_MIRROR,
	                             GCTL_VERB_GET, mirror_id, params);
}

/* ── Main entry point ────────────────────────────────────────────────── */

/**
 * gctl_cmd_mirror:
 * @app: the #GctlApp instance
 * @argc: argument count (verb + verb-specific args)
 * @argv: (array length=argc): argument vector, where argv[0] is the verb
 *
 * Main entry point for the "mirror" command.  Extracts the verb from
 * argv[0], looks it up in the dispatch table, and delegates to the
 * appropriate verb handler.
 *
 * Returns: 0 on success, 1 on error
 */
gint
gctl_cmd_mirror(
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
	entry = gctl_cmd_find_verb(mirror_verbs, N_MIRROR_VERBS, verb_name);

	if (entry == NULL)
	{
		g_printerr("error: unknown verb '%s' for mirror command\n",
		           verb_name);
		print_usage();
		return 1;
	}

	switch (entry->verb)
	{
		case GCTL_VERB_LIST:
			return cmd_mirror_list(app, argc, argv);
		case GCTL_VERB_CREATE:
			return cmd_mirror_add(app, argc, argv);
		case GCTL_VERB_DELETE:
			return cmd_mirror_remove(app, argc, argv);
		case GCTL_VERB_SYNC:
			return cmd_mirror_sync(app, argc, argv);
		case GCTL_VERB_GET:
			return cmd_mirror_get(app, argc, argv);
		default:
			g_printerr("error: verb '%s' not implemented for mirror\n",
			           verb_name);
			return 1;
	}
}
