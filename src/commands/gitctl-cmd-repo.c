/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
 * gitctl-cmd-repo.c - Repository command handler
 *
 * Implements the "repo" command with verb dispatch for: list, get,
 * create, fork, clone, delete, browse, star, unstar, and migrate.
 *
 * All verbs parse their options and delegate to gctl_cmd_execute_verb().
 */

#define GCTL_COMPILATION

#include <string.h>
#include <json-glib/json-glib.h>

#include "commands/gitctl-cmd-common-private.h"
#include "commands/gitctl-cmd-repo.h"
#include "commands/gitctl-cmd-mirror.h"

/* ── Verb dispatch table ─────────────────────────────────────────────── */

static const GctlVerbEntry repo_verbs[] = {
	{ "list",   "List repositories",                GCTL_VERB_LIST   },
	{ "get",    "View a single repository",         GCTL_VERB_GET    },
	{ "create", "Create a new repository",          GCTL_VERB_CREATE },
	{ "edit",   "Edit repository settings",         GCTL_VERB_EDIT   },
	{ "fork",   "Fork a repository",                GCTL_VERB_FORK   },
	{ "clone",  "Clone a repository",               GCTL_VERB_CLONE  },
	{ "delete", "Delete a repository",              GCTL_VERB_DELETE },
	{ "browse", "Open the repository in browser",   GCTL_VERB_BROWSE },
	{ "star",   "Star a repository",               GCTL_VERB_STAR   },
	{ "unstar",   "Remove star from repository",     GCTL_VERB_UNSTAR  },
	{ "migrate",  "Migrate a repository to another forge", GCTL_VERB_MIGRATE },
};

static const gsize N_REPO_VERBS = G_N_ELEMENTS(repo_verbs);

/* ── Usage printer ───────────────────────────────────────────────────── */

/**
 * print_usage:
 *
 * Prints the usage summary for the "repo" command, listing all
 * available verbs and their descriptions.
 */
static void
print_usage(void)
{
	gctl_cmd_print_verb_table("repo", repo_verbs, N_REPO_VERBS);
	g_printerr("  %-14s %s\n", "mirror",
	           "Manage push/pull mirrors (see: gitctl mirror --help)");
}

/* ── repo list ───────────────────────────────────────────────────────── */

/**
 * cmd_repo_list:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl repo list".  Parses --limit and --visibility options.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_repo_list(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	gint limit = 30;
	gchar *visibility = NULL;
	gchar *owner = NULL;
	gchar *language = NULL;
	gchar *topic = NULL;
	gchar *sort = NULL;
	gboolean use_pager = FALSE;
	gint ret;

	GOptionEntry entries[] = {
		{ "limit", 'l', 0, G_OPTION_ARG_INT, &limit,
		  "Maximum number of results (default: 30)", "N" },
		{ "visibility", 0, 0, G_OPTION_ARG_STRING, &visibility,
		  "Filter by visibility (public/private/all)", "VIS" },
		{ "owner", 'O', 0, G_OPTION_ARG_STRING, &owner,
		  "List repos for a user or organization", "OWNER" },
		{ "language", 'L', 0, G_OPTION_ARG_STRING, &language,
		  "Filter by programming language", "LANG" },
		{ "topic", 0, 0, G_OPTION_ARG_STRING, &topic,
		  "Filter by topic/tag", "TOPIC" },
		{ "sort", 's', 0, G_OPTION_ARG_STRING, &sort,
		  "Sort by: name, created, updated, stars", "FIELD" },
		{ "pager", 0, 0, G_OPTION_ARG_NONE, &use_pager,
		  "Pipe output through $PAGER", NULL },
		{ NULL }
	};

	opt_context = g_option_context_new("[owner] - list repositories");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	/*
	 * Accept owner as a positional arg for convenience:
	 *   gitctl repo list immutablue
	 *   gitctl repo list https://gitlab.com/immutablue
	 *   gitctl repo list https://git.podbielniak.com/zachpodbielniak
	 *
	 * If the arg looks like a URL (contains "://"), parse it to
	 * extract the host and owner, and auto-detect the forge type.
	 * This lets you browse any forge without --forge.
	 */
	if (owner == NULL && argc >= 2 && argv[1] != NULL &&
	    argv[1][0] != '-')
	{
		const gchar *arg = argv[1];

		if (strstr(arg, "://") != NULL)
		{
			g_autoptr(GUri) uri = NULL;
			g_autoptr(GError) uri_err = NULL;

			uri = g_uri_parse(arg, G_URI_FLAGS_NONE, &uri_err);
			if (uri != NULL)
			{
				const gchar *host;
				const gchar *path;

				host = g_uri_get_host(uri);
				path = g_uri_get_path(uri);

				/* Extract owner from path: /owner or /owner/ */
				if (path != NULL && *path == '/')
					path++;
				if (path != NULL && *path != '\0')
				{
					g_autofree gchar *path_copy = g_strdup(path);
					gchar *slash;

					/* Strip trailing slash */
					slash = strrchr(path_copy, '/');
					if (slash != NULL && *(slash + 1) == '\0')
						*slash = '\0';

					/* If path has a slash, take only the first segment
					 * (the owner/org name) */
					slash = strchr(path_copy, '/');
					if (slash != NULL)
						*slash = '\0';

					owner = g_strdup(path_copy);
				}

				/*
				 * Auto-detect forge from the host and force it
				 * on the resolver so execute_verb targets the
				 * right instance.
				 */
				if (host != NULL)
				{
					GctlConfig *cfg;
					GctlForgeType url_ft;

					cfg = gctl_app_get_config(app);
					url_ft = gctl_config_get_forge_for_host(cfg, host);

					if (url_ft != GCTL_FORGE_TYPE_UNKNOWN)
					{
						GctlContextResolver *res;

						res = gctl_app_get_resolver(app);
						gctl_context_resolver_set_forced_forge(
							res, url_ft);
					}
					else
					{
						g_printerr("warning: unknown forge for "
						           "host '%s'\n", host);
					}
				}
			}
			else
			{
				g_printerr("warning: could not parse URL '%s'\n",
				           arg);
			}
		}
		else
		{
			owner = g_strdup(arg);
		}
	}

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	{
		gchar *limit_str;

		limit_str = g_strdup_printf("%d", limit);
		g_hash_table_insert(params, g_strdup("limit"), limit_str);
	}

	if (visibility != NULL)
		g_hash_table_insert(params, g_strdup("visibility"),
		                    g_strdup(visibility));
	if (owner != NULL)
		g_hash_table_insert(params, g_strdup("owner"),
		                    g_strdup(owner));
	if (language != NULL)
		g_hash_table_insert(params, g_strdup("language"),
		                    g_strdup(language));
	if (topic != NULL)
		g_hash_table_insert(params, g_strdup("topic"),
		                    g_strdup(topic));
	if (sort != NULL)
		g_hash_table_insert(params, g_strdup("sort"),
		                    g_strdup(sort));

	if (use_pager)
		g_hash_table_insert(params, g_strdup("pager"), g_strdup("true"));

	ret = gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_REPO,
	                            GCTL_VERB_LIST, NULL, params);

	g_free(visibility);
	g_free(owner);
	g_free(language);
	g_free(topic);
	g_free(sort);

	return ret;
}

/* ── repo get ────────────────────────────────────────────────────────── */

/**
 * cmd_repo_get:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl repo get <owner/repo>".
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_repo_get(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	const gchar *owner_repo;

	GOptionEntry entries[] = {
		{ NULL }
	};

	opt_context = g_option_context_new("<owner/repo> - view a repository");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	if (argc < 2)
	{
		g_printerr("error: repository identifier required (owner/repo)\n");
		g_printerr("Usage: gitctl repo get <owner/repo>\n");
		return 1;
	}

	owner_repo = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_REPO,
	                             GCTL_VERB_GET, owner_repo, params);
}

/* ── repo create ─────────────────────────────────────────────────────── */

/* Mirror-to option variables (used by cmd_repo_create) */
static gchar **opt_mirror_to = NULL;
static gchar *opt_token_github = NULL;
static gchar *opt_token_gitlab = NULL;
static gchar *opt_token_forgejo = NULL;
static gchar *opt_token_gitea = NULL;

/* ── URL parsing helper (shared with mirror command) ─────────────────── */

/**
 * repo_parse_mirror_url:
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
repo_parse_mirror_url(
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

		if (count < 1 || segments[0][0] == '\0')
			return FALSE;

		*host  = g_strdup(uri_host);
		*owner = g_strdup(segments[0]);

		if (count >= 2 && segments[1][0] != '\0')
		{
			repo_name = g_strdup(segments[1]);
			if (g_str_has_suffix(repo_name, ".git"))
				repo_name[strlen(repo_name) - 4] = '\0';
			*repo = repo_name;
		}
		else
		{
			*repo = NULL;
		}

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

/* ── Push mirror body builder ────────────────────────────────────────── */

/**
 * repo_build_push_mirror_body:
 * @forge_type: the source forge type
 * @url: the remote mirror URL
 * @token: (nullable): authentication token
 * @interval: the sync interval string (e.g. "8h0m0s")
 * @mask_token: if %TRUE, replaces the token with "***" (for dry-run)
 *
 * Builds a JSON request body for creating a push mirror on the source
 * forge, targeting the given URL.
 *
 * Returns: (transfer full) (nullable): a JSON string, or %NULL if the
 *     forge type does not support push mirrors
 */
static gchar *
repo_build_push_mirror_body(
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

/* ── Mirror setup helper ─────────────────────────────────────────────── */

/**
 * get_token_for_forge:
 * @forge_type: the destination forge type
 *
 * Returns the appropriate token for a given forge type based on the
 * --token-* options passed on the command line.
 *
 * Returns: (transfer none) (nullable): the token string, or %NULL
 */
static const gchar *
get_token_for_forge(GctlForgeType forge_type)
{
	switch (forge_type)
	{
		case GCTL_FORGE_TYPE_GITHUB:  return opt_token_github;
		case GCTL_FORGE_TYPE_GITLAB:  return opt_token_gitlab;
		case GCTL_FORGE_TYPE_FORGEJO: return opt_token_forgejo;
		case GCTL_FORGE_TYPE_GITEA:   return opt_token_gitea;
		default:                      return NULL;
	}
}

/**
 * setup_mirror_to:
 * @app: the #GctlApp instance
 * @mirror_url: the destination mirror URL
 * @source_context: the source forge context (the repo we just created)
 * @sync_on_commit: whether to sync on every push
 *
 * Sets up a push mirror from the source repository to the destination
 * specified by @mirror_url.  This involves:
 *
 * 1. Parsing the mirror URL to extract host, owner, and repo
 * 2. Looking up the destination forge type from config
 * 3. Creating a blank repo on the destination (ignoring errors)
 * 4. Adding a push mirror on the source forge pointing to the dest
 *
 * On any failure, a warning is printed and execution continues.
 */
static void
setup_mirror_to(
	GctlApp          *app,
	const gchar      *mirror_url,
	GctlForgeContext *source_context,
	gboolean          sync_on_commit
){
	GctlConfig *config;
	GctlExecutor *executor;
	GctlModuleManager *mm;
	g_autofree gchar *dest_host = NULL;
	g_autofree gchar *dest_owner = NULL;
	g_autofree gchar *dest_repo = NULL;
	GctlForgeType dest_forge_type;
	GctlForge *dest_forge;
	GctlForgeType source_forge_type;
	GctlForge *source_forge;
	const gchar *token;
	gboolean verbose;
	gboolean is_dry_run;

	config = gctl_app_get_config(app);
	executor = gctl_app_get_executor(app);
	mm = gctl_app_get_module_manager(app);
	verbose = gctl_app_get_verbose(app);
	is_dry_run = gctl_executor_get_dry_run(executor);

	/* Step 1: Parse the mirror URL */
	if (!repo_parse_mirror_url(mirror_url, &dest_host, &dest_owner, &dest_repo))
	{
		g_printerr("warning: could not parse mirror URL '%s', skipping\n",
		           mirror_url);
		return;
	}

	if (verbose)
		g_printerr("note: setting up mirror to %s/%s on %s\n",
		           dest_owner, dest_repo, dest_host);

	/* Step 2: Look up dest forge type */
	dest_forge_type = gctl_config_get_forge_for_host(config, dest_host);
	if (dest_forge_type == GCTL_FORGE_TYPE_UNKNOWN)
	{
		g_printerr("warning: unknown forge for host '%s', skipping mirror\n",
		           dest_host);
		return;
	}

	/* Step 3: Find the dest forge module */
	dest_forge = gctl_module_manager_find_forge(mm, dest_forge_type);
	if (dest_forge == NULL)
	{
		g_printerr("warning: no module for %s, skipping mirror\n",
		           gctl_forge_type_to_string(dest_forge_type));
		return;
	}

	/* Step 4: Get the appropriate token */
	token = get_token_for_forge(dest_forge_type);

	/* Step 5: Create the dest repo (ignore errors -- may already exist) */
	{
		g_autoptr(GctlForgeContext) dest_context = NULL;
		g_autoptr(GHashTable) create_params = NULL;
		g_autoptr(GError) create_error = NULL;
		g_autoptr(GctlCommandResult) create_result = NULL;
		const gchar *dest_cli;
		gchar **create_argv;

		dest_cli = gctl_config_get_cli_path(config, dest_forge_type);

		dest_context = gctl_forge_context_new(
		    dest_forge_type, mirror_url,
		    dest_owner, dest_repo,
		    dest_host, dest_cli);

		create_params = g_hash_table_new_full(
		    g_str_hash, g_str_equal, g_free, g_free);
		g_hash_table_insert(create_params,
		                    g_strdup("name"), g_strdup(dest_repo));
		g_hash_table_insert(create_params,
		                    g_strdup("private"), g_strdup("true"));

		if (verbose)
			g_printerr("note: creating destination repo %s/%s on %s\n",
			           dest_owner, dest_repo,
			           gctl_forge_type_to_string(dest_forge_type));

		create_argv = gctl_forge_build_argv(
		    dest_forge, GCTL_RESOURCE_KIND_REPO,
		    GCTL_VERB_CREATE, dest_context,
		    create_params, &create_error);

		if (create_argv == NULL)
		{
			g_printerr("warning: could not build repo create command for "
			           "%s: %s\n",
			           gctl_forge_type_to_string(dest_forge_type),
			           create_error ? create_error->message : "unknown error");
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
				g_printerr("warning: repo creation on %s failed "
				           "(may already exist), continuing\n",
				           gctl_forge_type_to_string(dest_forge_type));
			}
		}
	}

	/* Step 6: Add push mirror on the SOURCE forge */
	source_forge_type = gctl_forge_context_get_forge_type(source_context);
	source_forge = gctl_module_manager_find_forge(mm, source_forge_type);

	if (source_forge == NULL)
	{
		g_printerr("warning: no module for source forge %s, "
		           "cannot add push mirror\n",
		           gctl_forge_type_to_string(source_forge_type));
		return;
	}

	{
		g_autofree gchar *body = NULL;
		g_autofree gchar *endpoint = NULL;
		g_autoptr(GError) mirror_err = NULL;
		g_autoptr(GctlCommandResult) mirror_result = NULL;
		gchar **mirror_argv;
		const gchar *mirror_username;
		const gchar *mirror_token;
		const gchar *env_token;

		/*
		 * Auto-detect username from the destination URL owner
		 * and token from the destination forge's env var.
		 * This matches the behavior of gitctl mirror add.
		 */
		mirror_username = dest_owner;

		env_token = NULL;
		switch (dest_forge_type) {
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

		/* Use the per-forge token flag if available, else env var */
		mirror_token = token;
		if (mirror_token == NULL)
			mirror_token = env_token;

		body = repo_build_push_mirror_body(
		    source_forge_type, mirror_url,
		    mirror_username, mirror_token,
		    "8h0m0s", sync_on_commit, is_dry_run);

		if (body == NULL)
		{
			g_printerr("warning: push mirrors not supported on %s, "
			           "skipping mirror to %s\n",
			           gctl_forge_type_to_string(source_forge_type),
			           mirror_url);
			return;
		}

		/* Build the push mirror API endpoint for the source repo */
		{
			const gchar *src_owner;
			const gchar *src_repo;

			src_owner = gctl_forge_context_get_owner(source_context);
			src_repo = gctl_forge_context_get_repo_name(source_context);

			/*
			 * If owner is unknown (no git remote and no --repo),
			 * try GITCTL_USER env var.
			 */
			if (src_owner == NULL || *src_owner == '\0')
				src_owner = g_getenv("GITCTL_USER");

			if (src_owner == NULL || *src_owner == '\0' ||
			    src_repo == NULL || *src_repo == '\0')
			{
				g_printerr("warning: cannot determine owner/repo for "
				           "push mirror. Set GITCTL_USER or use "
				           "--repo owner/repo\n");
				return;
			}

			endpoint = g_strdup_printf("/repos/%s/%s/push_mirrors",
			                           src_owner, src_repo);
		}

		if (verbose)
			g_printerr("note: adding push mirror: POST %s\n", endpoint);

		mirror_argv = gctl_forge_build_api_argv(
		    source_forge, "POST", endpoint, body,
		    source_context, &mirror_err);

		if (mirror_argv == NULL)
		{
			g_printerr("warning: could not build mirror API call: %s\n",
			           mirror_err ? mirror_err->message : "unknown error");
			return;
		}

		mirror_result = gctl_executor_run(
		    executor,
		    (const gchar * const *)mirror_argv,
		    &mirror_err);
		g_strfreev(mirror_argv);

		if (mirror_result == NULL ||
		    gctl_command_result_get_exit_code(mirror_result) != 0)
		{
			const gchar *stderr_text = NULL;
			if (mirror_result != NULL)
				stderr_text = gctl_command_result_get_stderr(mirror_result);
			g_printerr("warning: push mirror setup failed for %s",
			           mirror_url);
			if (stderr_text != NULL && stderr_text[0] != '\0')
				g_printerr(": %s", stderr_text);
			g_printerr("\n");
		}
		else if (verbose)
		{
			g_printerr("note: push mirror to %s configured successfully\n",
			           mirror_url);
		}
	}
}

/**
 * cmd_repo_create:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl repo create <name>".  Parses --private,
 * --description, --clone, --mirror-to, and --token-* options.
 *
 * When --mirror-to is specified, after the primary repo creation
 * succeeds, a push mirror is set up to each destination URL.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_repo_create(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	gboolean is_private = FALSE;
	gchar *description = NULL;
	gchar *default_branch = NULL;
	gboolean clone_after = FALSE;
	gboolean clone_ssh = FALSE;
	gboolean sync_on_commit = FALSE;
	const gchar *repo_name;
	gint ret;

	/* Reset mirror-to statics for safety */
	opt_mirror_to = NULL;
	opt_token_github = NULL;
	opt_token_gitlab = NULL;
	opt_token_forgejo = NULL;
	opt_token_gitea = NULL;

	GOptionEntry entries[] = {
		{ "private", 'p', 0, G_OPTION_ARG_NONE, &is_private,
		  "Create as private repository", NULL },
		{ "description", 'd', 0, G_OPTION_ARG_STRING, &description,
		  "Repository description", "DESC" },
		{ "default-branch", 'b', 0, G_OPTION_ARG_STRING, &default_branch,
		  "Default branch name (default: from config or forge default)",
		  "BRANCH" },
		{ "clone", 'c', 0, G_OPTION_ARG_NONE, &clone_after,
		  "Clone the repo after creating (HTTPS)", NULL },
		{ "clone-ssh", 0, 0, G_OPTION_ARG_NONE, &clone_ssh,
		  "Clone the repo after creating (SSH)", NULL },
		{ "mirror-to", 0, 0, G_OPTION_ARG_STRING_ARRAY, &opt_mirror_to,
		  "Push mirror to this URL after creation (repeatable)", "URL" },
		{ "sync-on-commit", 0, 0, G_OPTION_ARG_NONE, &sync_on_commit,
		  "Sync mirrors on every push", NULL },
		{ "token-github", 0, 0, G_OPTION_ARG_STRING, &opt_token_github,
		  "GitHub token for mirror destination", "TOKEN" },
		{ "token-gitlab", 0, 0, G_OPTION_ARG_STRING, &opt_token_gitlab,
		  "GitLab token for mirror destination", "TOKEN" },
		{ "token-forgejo", 0, 0, G_OPTION_ARG_STRING, &opt_token_forgejo,
		  "Forgejo token for mirror destination", "TOKEN" },
		{ "token-gitea", 0, 0, G_OPTION_ARG_STRING, &opt_token_gitea,
		  "Gitea token for mirror destination", "TOKEN" },
		{ NULL }
	};

	opt_context = g_option_context_new("<name> - create a repository");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	if (argc < 2)
	{
		g_printerr("error: repository name required\n");
		g_printerr("Usage: gitctl repo create <name> [options]\n");
		return 1;
	}

	repo_name = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	g_hash_table_insert(params, g_strdup("name"), g_strdup(repo_name));

	if (is_private)
		g_hash_table_insert(params, g_strdup("private"), g_strdup("true"));

	if (description != NULL)
		g_hash_table_insert(params, g_strdup("description"),
		                    g_strdup(description));

	/*
	 * Default branch: --default-branch overrides config, which
	 * overrides the forge's default (usually "main").
	 */
	{
		const gchar *branch;

		branch = default_branch;
		if (branch == NULL) {
			GctlConfig *cfg;

			cfg = gctl_app_get_config(app);
			if (cfg != NULL)
				branch = gctl_config_get_default_branch(cfg);
		}

		if (branch != NULL)
			g_hash_table_insert(params, g_strdup("default_branch"),
			                    g_strdup(branch));
	}

	ret = gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_REPO,
	                            GCTL_VERB_CREATE, repo_name, params);

	/*
	 * Post-create: set default branch if specified.
	 *
	 * None of the forge CLIs (gh, glab, fj, tea) support
	 * --default-branch at creation time, so we issue a separate
	 * repo-edit call to set it after the repo exists.
	 */
	if (ret == 0) {
		const gchar *branch;

		branch = g_hash_table_lookup(params, "default_branch");
		if (branch != NULL) {
			g_autoptr(GHashTable) edit_params = NULL;

			edit_params = g_hash_table_new_full(
			    g_str_hash, g_str_equal, g_free, g_free);
			g_hash_table_insert(edit_params,
			    g_strdup("default_branch"),
			    g_strdup(branch));

			gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_REPO,
			                      GCTL_VERB_EDIT, repo_name,
			                      edit_params);
		}
	}

	/* Post-create: set up push mirrors if --mirror-to was specified */
	if (ret == 0 && opt_mirror_to != NULL)
	{
		GctlConfig *config;
		g_autoptr(GctlForgeContext) source_context = NULL;
		gint m;

		config = gctl_app_get_config(app);

		/*
		 * Build the source context for mirror setup.  We need the
		 * owner (who the repo was created under) and the repo name.
		 *
		 * Try these sources in order:
		 * 1. -R owner/repo flag (resolver's forced_repo)
		 * 2. Owner from the first --mirror-to URL (common case:
		 *    same owner across forges)
		 * 3. GITCTL_USER env var
		 */
		{
			GctlContextResolver *res;
			GctlForgeType ft;
			const gchar *host;
			const gchar *cli;
			g_autofree gchar *owner = NULL;

			res = gctl_app_get_resolver(app);
			ft = gctl_context_resolver_get_forced_forge(res);
			if (ft == GCTL_FORGE_TYPE_UNKNOWN)
				ft = gctl_config_get_default_forge(config);
			host = gctl_config_get_default_host(config, ft);
			cli = gctl_config_get_cli_path(config, ft);

			/* Try -R flag first */
			{
				g_autoptr(GctlForgeContext) tmp_ctx = NULL;
				g_autoptr(GError) tmp_err = NULL;
				const gchar *def_remote;

				def_remote = gctl_config_get_default_remote(config);
				tmp_ctx = gctl_context_resolver_resolve(
				    res, def_remote, &tmp_err);
				if (tmp_ctx != NULL) {
					const gchar *resolved_owner;
					resolved_owner = gctl_forge_context_get_owner(
					    tmp_ctx);
					if (resolved_owner != NULL &&
					    *resolved_owner != '\0')
						owner = g_strdup(resolved_owner);
				}
			}

			/* Try first --mirror-to URL owner */
			if (owner == NULL && opt_mirror_to != NULL &&
			    opt_mirror_to[0] != NULL)
			{
				g_autofree gchar *mhost = NULL;
				g_autofree gchar *mowner = NULL;
				g_autofree gchar *mrepo = NULL;

				if (repo_parse_mirror_url(opt_mirror_to[0],
				    &mhost, &mowner, &mrepo))
				{
					if (mowner != NULL && *mowner != '\0')
						owner = g_strdup(mowner);
				}
			}

			/* Try GITCTL_USER env var */
			if (owner == NULL) {
				const gchar *env_user;
				env_user = g_getenv("GITCTL_USER");
				if (env_user != NULL)
					owner = g_strdup(env_user);
			}

			source_context = gctl_forge_context_new(
			    ft, NULL, owner, repo_name, host, cli);
		}

		for (m = 0; opt_mirror_to[m] != NULL; m++)
		{
			setup_mirror_to(app, opt_mirror_to[m], source_context,
			               sync_on_commit);
		}
	}

	/*
	 * Post-create: clone the repo into the current directory.
	 * --clone (defaults to https) or --clone=ssh for SSH URL.
	 */
	if (ret == 0 && (clone_after || clone_ssh))
	{
		GctlConfig *cfg;
		GctlContextResolver *res;
		GctlForgeType ft;
		const gchar *host;
		g_autofree gchar *clone_owner = NULL;
		g_autofree gchar *clone_url = NULL;
		g_autoptr(GSubprocess) clone_proc = NULL;
		g_autoptr(GError) clone_err = NULL;
		gboolean use_ssh;

		cfg = gctl_app_get_config(app);
		res = gctl_app_get_resolver(app);
		ft = gctl_context_resolver_get_forced_forge(res);
		if (ft == GCTL_FORGE_TYPE_UNKNOWN)
			ft = gctl_config_get_default_forge(cfg);
		host = gctl_config_get_default_host(cfg, ft);

		/* Determine owner — same logic as mirror setup */
		{
			g_autoptr(GctlForgeContext) tmp_ctx = NULL;
			g_autoptr(GError) tmp_err = NULL;
			const gchar *def_remote;

			def_remote = gctl_config_get_default_remote(cfg);
			tmp_ctx = gctl_context_resolver_resolve(
			    res, def_remote, &tmp_err);
			if (tmp_ctx != NULL) {
				const gchar *ro;
				ro = gctl_forge_context_get_owner(tmp_ctx);
				if (ro != NULL && *ro != '\0')
					clone_owner = g_strdup(ro);
			}
		}
		if (clone_owner == NULL && opt_mirror_to != NULL &&
		    opt_mirror_to[0] != NULL)
		{
			g_autofree gchar *mh = NULL;
			g_autofree gchar *mo = NULL;
			g_autofree gchar *mr = NULL;

			if (repo_parse_mirror_url(opt_mirror_to[0],
			                          &mh, &mo, &mr))
			{
				if (mo != NULL && *mo != '\0')
					clone_owner = g_strdup(mo);
			}
		}
		if (clone_owner == NULL) {
			const gchar *eu;
			eu = g_getenv("GITCTL_USER");
			if (eu != NULL)
				clone_owner = g_strdup(eu);
		}

		use_ssh = clone_ssh;

		if (host != NULL)
		{
			if (use_ssh)
			{
				/*
				 * Use ssh_host from config if set (e.g.
				 * git-ssh.podbielniak.com vs git.podbielniak.com).
				 * Falls back to the default host.
				 */
				const gchar *ssh_host;

				ssh_host = gctl_config_get_ssh_host(cfg, ft);
				if (ssh_host == NULL)
					ssh_host = host;

				if (clone_owner != NULL)
					clone_url = g_strdup_printf(
						"git@%s:%s/%s.git",
						ssh_host, clone_owner, repo_name);
				else
					clone_url = g_strdup_printf(
						"git@%s:%s.git",
						ssh_host, repo_name);
			}
			else
			{
				if (clone_owner != NULL)
					clone_url = g_strdup_printf(
						"https://%s/%s/%s.git",
						host, clone_owner, repo_name);
				else
					clone_url = g_strdup_printf(
						"https://%s/%s.git",
						host, repo_name);
			}
		}

		if (clone_url != NULL)
		{
			if (gctl_executor_get_dry_run(
			        gctl_app_get_executor(app)))
			{
				g_print("[dry-run] 'git' 'clone' '%s'\n",
				        clone_url);
			}
			else
			{
				g_printerr("Cloning %s...\n", clone_url);
				clone_proc = g_subprocess_new(
				    G_SUBPROCESS_FLAGS_NONE,
				    &clone_err,
				    "git", "clone", clone_url, NULL);

				if (clone_proc != NULL)
				{
					g_subprocess_wait(clone_proc, NULL,
					                  &clone_err);
					if (!g_subprocess_get_successful(clone_proc))
						g_printerr("warning: clone failed\n");
				}
				else
				{
					g_printerr("warning: could not spawn "
					           "git clone: %s\n",
					           clone_err ? clone_err->message
					                     : "unknown error");
				}
			}
		}
		else
		{
			g_printerr("warning: could not determine clone URL "
			           "(set GITCTL_USER or use -R)\n");
		}
	}

	g_free(description);
	g_free(default_branch);
	g_strfreev(opt_mirror_to);
	g_free(opt_token_github);
	g_free(opt_token_gitlab);
	g_free(opt_token_forgejo);
	g_free(opt_token_gitea);

	/* Reset statics */
	opt_mirror_to = NULL;
	opt_token_github = NULL;
	opt_token_gitlab = NULL;
	opt_token_forgejo = NULL;
	opt_token_gitea = NULL;

	return ret;
}

/* ── repo edit ────────────────────────────────────────────────────────── */

/**
 * cmd_repo_edit:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl repo edit [options]".  Edits repository settings
 * such as visibility, description, default branch, and feature toggles.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_repo_edit(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	gchar *description = NULL;
	gchar *visibility = NULL;
	gchar *default_branch = NULL;
	gchar *homepage = NULL;
	gboolean enable_issues = FALSE;
	gboolean disable_issues = FALSE;
	gboolean enable_wiki = FALSE;
	gboolean disable_wiki = FALSE;
	gboolean enable_projects = FALSE;
	gboolean disable_projects = FALSE;
	gboolean archive = FALSE;
	gboolean unarchive = FALSE;
	gint ret;

	GOptionEntry entries[] = {
		{ "description", 'd', 0, G_OPTION_ARG_STRING, &description,
		  "Repository description", "DESC" },
		{ "visibility", 0, 0, G_OPTION_ARG_STRING, &visibility,
		  "Visibility: public, private, or internal", "VIS" },
		{ "default-branch", 0, 0, G_OPTION_ARG_STRING, &default_branch,
		  "Default branch name", "BRANCH" },
		{ "homepage", 0, 0, G_OPTION_ARG_STRING, &homepage,
		  "Repository homepage URL", "URL" },
		{ "enable-issues", 0, 0, G_OPTION_ARG_NONE, &enable_issues,
		  "Enable issues", NULL },
		{ "disable-issues", 0, 0, G_OPTION_ARG_NONE, &disable_issues,
		  "Disable issues", NULL },
		{ "enable-wiki", 0, 0, G_OPTION_ARG_NONE, &enable_wiki,
		  "Enable wiki", NULL },
		{ "disable-wiki", 0, 0, G_OPTION_ARG_NONE, &disable_wiki,
		  "Disable wiki", NULL },
		{ "enable-projects", 0, 0, G_OPTION_ARG_NONE, &enable_projects,
		  "Enable projects", NULL },
		{ "disable-projects", 0, 0, G_OPTION_ARG_NONE, &disable_projects,
		  "Disable projects", NULL },
		{ "archive", 0, 0, G_OPTION_ARG_NONE, &archive,
		  "Archive the repository", NULL },
		{ "unarchive", 0, 0, G_OPTION_ARG_NONE, &unarchive,
		  "Unarchive the repository", NULL },
		{ NULL }
	};

	opt_context = g_option_context_new("- edit repository settings");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	/* At least one option must be specified */
	if (description == NULL && visibility == NULL &&
	    default_branch == NULL && homepage == NULL &&
	    !enable_issues && !disable_issues &&
	    !enable_wiki && !disable_wiki &&
	    !enable_projects && !disable_projects &&
	    !archive && !unarchive)
	{
		g_printerr("error: at least one setting must be specified\n");
		g_printerr("Usage: gitctl repo edit [options]\n");
		g_printerr("  -d, --description DESC    Set description\n");
		g_printerr("  --visibility VIS          Set public/private/internal\n");
		g_printerr("  --default-branch BRANCH   Set default branch\n");
		g_printerr("  --homepage URL            Set homepage URL\n");
		g_printerr("  --enable-issues           Enable issues\n");
		g_printerr("  --disable-issues          Disable issues\n");
		g_printerr("  --enable-wiki             Enable wiki\n");
		g_printerr("  --disable-wiki            Disable wiki\n");
		g_printerr("  --enable-projects         Enable projects\n");
		g_printerr("  --disable-projects        Disable projects\n");
		g_printerr("  --archive                 Archive the repository\n");
		g_printerr("  --unarchive               Unarchive the repository\n");
		return 1;
	}

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	if (description != NULL)
		g_hash_table_insert(params, g_strdup("description"),
		                    g_strdup(description));
	if (visibility != NULL)
		g_hash_table_insert(params, g_strdup("visibility"),
		                    g_strdup(visibility));
	if (default_branch != NULL)
		g_hash_table_insert(params, g_strdup("default_branch"),
		                    g_strdup(default_branch));
	if (homepage != NULL)
		g_hash_table_insert(params, g_strdup("homepage"),
		                    g_strdup(homepage));
	if (enable_issues)
		g_hash_table_insert(params, g_strdup("enable_issues"),
		                    g_strdup("true"));
	if (disable_issues)
		g_hash_table_insert(params, g_strdup("enable_issues"),
		                    g_strdup("false"));
	if (enable_wiki)
		g_hash_table_insert(params, g_strdup("enable_wiki"),
		                    g_strdup("true"));
	if (disable_wiki)
		g_hash_table_insert(params, g_strdup("enable_wiki"),
		                    g_strdup("false"));
	if (enable_projects)
		g_hash_table_insert(params, g_strdup("enable_projects"),
		                    g_strdup("true"));
	if (disable_projects)
		g_hash_table_insert(params, g_strdup("enable_projects"),
		                    g_strdup("false"));
	if (archive)
		g_hash_table_insert(params, g_strdup("archive"),
		                    g_strdup("true"));
	if (unarchive)
		g_hash_table_insert(params, g_strdup("archive"),
		                    g_strdup("false"));

	ret = gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_REPO,
	                            GCTL_VERB_EDIT, NULL, params);

	g_free(description);
	g_free(visibility);
	g_free(default_branch);
	g_free(homepage);

	return ret;
}

/* ── repo fork ───────────────────────────────────────────────────────── */

/**
 * cmd_repo_fork:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl repo fork <owner/repo>".
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_repo_fork(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *owner_repo;

	if (argc < 2)
	{
		g_printerr("error: repository identifier required (owner/repo)\n");
		g_printerr("Usage: gitctl repo fork <owner/repo>\n");
		return 1;
	}

	owner_repo = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_REPO,
	                             GCTL_VERB_FORK, owner_repo, params);
}

/* ── repo clone ──────────────────────────────────────────────────────── */

/**
 * cmd_repo_clone:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl repo clone <owner/repo>".
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_repo_clone(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *owner_repo;

	if (argc < 2)
	{
		g_printerr("error: repository identifier required (owner/repo)\n");
		g_printerr("Usage: gitctl repo clone <owner/repo>\n");
		return 1;
	}

	owner_repo = argv[1];
	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_REPO,
	                             GCTL_VERB_CLONE, owner_repo, params);
}

/* ── repo delete ─────────────────────────────────────────────────────── */

/**
 * cmd_repo_delete:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl repo delete <owner/repo>".  Requires --yes / -y
 * to confirm the destructive operation (skipped in --dry-run mode).
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_repo_delete(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	const gchar *owner_repo;
	gboolean opt_yes = FALSE;

	GOptionEntry entries[] = {
		{ "yes", 'y', 0, G_OPTION_ARG_NONE, &opt_yes,
		  "Skip confirmation prompt (DANGEROUS)", NULL },
		{ NULL }
	};

	opt_context = g_option_context_new("<owner/repo> - delete a repository");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		return 1;
	}

	if (argc < 2)
	{
		g_printerr("error: repository identifier required (owner/repo)\n");
		g_printerr("Usage: gitctl repo delete [--yes] <owner/repo>\n");
		return 1;
	}

	owner_repo = argv[1];

	/* Require explicit --yes unless running in dry-run mode */
	if (!opt_yes && !gctl_app_get_dry_run(app))
	{
		g_printerr("warning: this will PERMANENTLY delete repository '%s'\n",
		           owner_repo);
		g_printerr("Run with --yes to confirm, or --dry-run to preview.\n");
		return 1;
	}

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	if (opt_yes)
		g_hash_table_insert(params, g_strdup("confirm"), g_strdup("true"));

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_REPO,
	                             GCTL_VERB_DELETE, owner_repo, params);
}

/* ── repo browse ─────────────────────────────────────────────────────── */

/**
 * cmd_repo_browse:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl repo browse".  Opens the repository in the browser.
 * No arguments required; uses the current repo context.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_repo_browse(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;

	(void)argc;
	(void)argv;

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_REPO,
	                             GCTL_VERB_BROWSE, NULL, params);
}

/* ── repo star ───────────────────────────────────────────────────────── */

/**
 * cmd_repo_star:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl repo star [owner/repo]".  Stars the specified
 * repository, or the current repository if no argument is given.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_repo_star(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *owner_repo;

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	owner_repo = (argc >= 2) ? argv[1] : NULL;

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_REPO,
	                             GCTL_VERB_STAR, owner_repo, params);
}

/* ── repo unstar ─────────────────────────────────────────────────────── */

/**
 * cmd_repo_unstar:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl repo unstar [owner/repo]".  Removes the star from
 * the specified repository, or the current repository if no argument
 * is given.
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_repo_unstar(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GHashTable) params = NULL;
	const gchar *owner_repo;

	params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	owner_repo = (argc >= 2) ? argv[1] : NULL;

	return gctl_cmd_execute_verb(app, GCTL_RESOURCE_KIND_REPO,
	                             GCTL_VERB_UNSTAR, owner_repo, params);
}

/* ── repo migrate ────────────────────────────────────────────────────── */

/* Static option variables for the migrate verb */
static gchar *opt_migrate_to_forge = NULL;
static gchar *opt_migrate_name = NULL;
static gchar *opt_migrate_owner = NULL;
static gboolean opt_migrate_private = FALSE;
static gchar *opt_migrate_include = NULL;
static gchar *opt_migrate_service = NULL;
static gchar *opt_migrate_token_src = NULL;
static gboolean opt_migrate_mirror = FALSE;
static gboolean opt_migrate_mirror_back = FALSE;
static gboolean opt_migrate_sync_on_commit = FALSE;
static gchar **opt_migrate_mirror_to = NULL;
static gboolean opt_migrate_mass = FALSE;

/**
 * infer_service_from_forge:
 * @forge_type: the #GctlForgeType of the source forge
 *
 * Maps a forge type to the service name expected by migration CLIs
 * (e.g. forgejo/gitea --service flag).
 *
 * Returns: (transfer none): the service string, or "git" as fallback
 */
static const gchar *
infer_service_from_forge(GctlForgeType forge_type)
{
	switch (forge_type)
	{
		case GCTL_FORGE_TYPE_GITHUB:  return "github";
		case GCTL_FORGE_TYPE_GITLAB:  return "gitlab";
		case GCTL_FORGE_TYPE_FORGEJO: return "forgejo";
		case GCTL_FORGE_TYPE_GITEA:   return "gitea";
		default:                      return "git";
	}
}

/* ── Mass-migrate helper ─────────────────────────────────────────────── */

/**
 * cmd_repo_mass_migrate:
 * @app: the #GctlApp instance
 * @source_host: the hostname of the source forge
 * @source_owner: the org or user name on the source forge
 * @executor: the #GctlExecutor instance
 * @config: the #GctlConfig instance
 * @mm: the #GctlModuleManager instance
 * @verbose: whether to print verbose messages
 *
 * Handles mass migration of all repositories from a source org/user
 * to the destination forge.  Lists repos via the source forge API,
 * then migrates each one using the destination forge's native migrate
 * command.  Optionally sets up --mirror-back and --mirror-to for each
 * migrated repo.
 *
 * Returns: 0 on success, 1 if any repo failed
 */
static gint
cmd_repo_mass_migrate(
	GctlApp           *app,
	const gchar       *source_host,
	const gchar       *source_owner,
	GctlExecutor      *executor,
	GctlConfig        *config,
	GctlModuleManager *mm,
	gboolean           verbose
){
	GctlForgeType source_forge_type;
	GctlForgeType dest_forge_type;
	GctlForge *source_forge;
	GctlForge *dest_forge;
	const gchar *dest_host;
	const gchar *dest_cli;
	g_autoptr(GPtrArray) repos = NULL;
	g_autoptr(GError) error = NULL;
	gboolean is_dry_run;
	guint total, succeeded, failed;
	guint i;

	is_dry_run = gctl_executor_get_dry_run(executor);

	/* Determine source and destination forge types */
	source_forge_type = gctl_config_get_forge_for_host(config, source_host);
	dest_forge_type = gctl_forge_type_from_string(opt_migrate_to_forge);

	if (source_forge_type == GCTL_FORGE_TYPE_UNKNOWN)
	{
		g_printerr("error: unknown forge for host '%s'\n", source_host);
		return 1;
	}
	if (dest_forge_type == GCTL_FORGE_TYPE_UNKNOWN)
	{
		g_printerr("error: unknown destination forge '%s'\n",
		           opt_migrate_to_forge);
		return 1;
	}

	source_forge = gctl_module_manager_find_forge(mm, source_forge_type);
	dest_forge = gctl_module_manager_find_forge(mm, dest_forge_type);
	dest_host = gctl_config_get_default_host(config, dest_forge_type);
	dest_cli = gctl_config_get_cli_path(config, dest_forge_type);

	if (source_forge == NULL || dest_forge == NULL)
	{
		g_printerr("error: forge module not available for %s or %s\n",
		           gctl_forge_type_to_string(source_forge_type),
		           gctl_forge_type_to_string(dest_forge_type));
		return 1;
	}

	/*
	 * Step 1: List all repos from the source org/user.
	 *
	 * Build the list command using the source forge's build_argv.
	 * If the forge doesn't support list via CLI, the API fallback
	 * path in the forge module (e.g. Forgejo curl) handles it.
	 */
	{
		g_autoptr(GctlForgeContext) src_context = NULL;
		g_autoptr(GHashTable) list_params = NULL;
		g_autoptr(GctlCommandResult) list_result = NULL;
		const gchar *src_cli;
		gchar **list_argv;

		src_cli = gctl_config_get_cli_path(config, source_forge_type);
		src_context = gctl_forge_context_new(
		    source_forge_type, NULL, source_owner, NULL,
		    source_host, src_cli);

		list_params = g_hash_table_new_full(
		    g_str_hash, g_str_equal, g_free, g_free);
		g_hash_table_insert(list_params, g_strdup("owner"),
		                    g_strdup(source_owner));
		g_hash_table_insert(list_params, g_strdup("limit"),
		                    g_strdup("100"));

		list_argv = gctl_forge_build_argv(
		    source_forge, GCTL_RESOURCE_KIND_REPO,
		    GCTL_VERB_LIST, src_context, list_params, &error);

		/*
		 * If the forge doesn't support CLI list, try API fallback.
		 * Build a direct API call to list repos for the owner.
		 */
		if (list_argv == NULL &&
		    error != NULL &&
		    error->domain == GCTL_ERROR &&
		    error->code == GCTL_ERROR_FORGE_UNSUPPORTED)
		{
			g_autofree gchar *endpoint = NULL;

			g_clear_error(&error);

			endpoint = g_strdup_printf(
			    "/users/%s/repos?limit=100", source_owner);

			if (verbose)
				g_printerr("note: listing repos via API: "
				           "GET %s\n", endpoint);

			list_argv = gctl_forge_build_api_argv(
			    source_forge, "GET", endpoint, NULL,
			    src_context, &error);
		}

		if (list_argv == NULL)
		{
			g_printerr("error: could not list repos: %s\n",
			           error ? error->message : "unknown");
			return 1;
		}

		list_result = gctl_executor_run(
		    executor, (const gchar * const *)list_argv, &error);
		g_strfreev(list_argv);

		if (list_result == NULL ||
		    gctl_command_result_get_exit_code(list_result) != 0)
		{
			g_printerr("error: failed to list repos from %s/%s\n",
			           source_host, source_owner);
			if (list_result != NULL)
			{
				const gchar *err_text;
				err_text = gctl_command_result_get_stderr(list_result);
				if (err_text != NULL && *err_text != '\0')
					g_printerr("  %s\n", err_text);
			}
			return 1;
		}

		/*
		 * In dry-run mode the executor returns empty output.
		 * Print a summary of what would happen and exit.
		 */
		if (is_dry_run)
		{
			g_printerr("note: [dry-run] would list repos from "
			           "%s/%s and migrate each to %s\n",
			           source_host, source_owner,
			           opt_migrate_to_forge);
			g_printerr("note: each repo would be migrated with:");
			if (opt_migrate_include != NULL)
				g_printerr(" --include %s", opt_migrate_include);
			if (opt_migrate_mirror)
				g_printerr(" --mirror");
			if (opt_migrate_mirror_back)
				g_printerr(" --mirror-back");
			if (opt_migrate_sync_on_commit)
				g_printerr(" --sync-on-commit");
			if (opt_migrate_mirror_to != NULL)
			{
				gint m;
				for (m = 0; opt_migrate_mirror_to[m] != NULL; m++)
					g_printerr(" --mirror-to %s/<reponame>",
					           opt_migrate_mirror_to[m]);
			}
			g_printerr("\n");
			return 0;
		}

		/* Parse the repo list output into GctlResource objects */
		repos = gctl_forge_parse_list_output(
		    source_forge, GCTL_RESOURCE_KIND_REPO,
		    gctl_command_result_get_stdout(list_result), &error);

		if (repos == NULL || repos->len == 0)
		{
			g_printerr("note: no repos found in %s/%s\n",
			           source_host, source_owner);
			return 0;
		}
	}

	/* Step 2: Print summary of repos to be migrated */
	total = repos->len;
	g_printerr("Found %u repos in %s/%s to migrate to %s:\n",
	           total, source_host, source_owner, opt_migrate_to_forge);
	for (i = 0; i < total; i++)
	{
		GctlResource *r;
		const gchar *vis;

		r = (GctlResource *)g_ptr_array_index(repos, i);
		vis = gctl_resource_get_state(r);
		g_printerr("  %-30s %s\n",
		           gctl_resource_get_title(r),
		           vis ? vis : "");
	}
	g_printerr("\n");

	/* Step 3: Migrate each repo */
	succeeded = 0;
	failed = 0;

	for (i = 0; i < total; i++)
	{
		GctlResource *r;
		const gchar *rname;
		g_autofree gchar *repo_source_url = NULL;
		g_autofree gchar *inferred_service = NULL;
		g_autoptr(GHashTable) params = NULL;
		g_autoptr(GctlForgeContext) dest_ctx = NULL;
		g_autoptr(GError) mig_err = NULL;
		g_autoptr(GctlCommandResult) mig_result = NULL;
		gchar **mig_argv;
		const gchar *owner_on_dest;

		r = (GctlResource *)g_ptr_array_index(repos, i);
		rname = gctl_resource_get_title(r);

		g_printerr("[%u/%u] Migrating %s...\n", i + 1, total, rname);

		/* Build the full source repo URL */
		repo_source_url = g_strdup_printf("https://%s/%s/%s",
		                                  source_host, source_owner,
		                                  rname);

		/* Owner on destination defaults to source owner */
		owner_on_dest = (opt_migrate_owner != NULL)
		    ? opt_migrate_owner : source_owner;

		/* Build params for this repo's migration */
		params = g_hash_table_new_full(
		    g_str_hash, g_str_equal, g_free, g_free);
		g_hash_table_insert(params, g_strdup("source_url"),
		                    g_strdup(repo_source_url));
		g_hash_table_insert(params, g_strdup("name"),
		                    g_strdup(rname));
		g_hash_table_insert(params, g_strdup("repo_owner"),
		                    g_strdup(owner_on_dest));

		/*
		 * Preserve source repo visibility by default.
		 * --private overrides all repos to private.
		 * Otherwise, check the source resource's state field
		 * which holds "private" or "public" from the list output.
		 */
		if (opt_migrate_private) {
			g_hash_table_insert(params, g_strdup("private"),
			                    g_strdup("true"));
		} else {
			const gchar *vis;

			vis = gctl_resource_get_state(r);
			if (vis != NULL &&
			    (g_ascii_strcasecmp(vis, "private") == 0 ||
			     g_ascii_strcasecmp(vis, "PRIVATE") == 0))
			{
				g_hash_table_insert(params, g_strdup("private"),
				                    g_strdup("true"));
			}
			/* public repos: don't set "private" flag */
		}
		if (opt_migrate_mirror)
			g_hash_table_insert(params, g_strdup("mirror"),
			                    g_strdup("true"));
		if (opt_migrate_include != NULL)
			g_hash_table_insert(params, g_strdup("include"),
			                    g_strdup(opt_migrate_include));

		/* Infer service type from source forge */
		inferred_service = g_strdup(
		    infer_service_from_forge(source_forge_type));
		if (opt_migrate_service != NULL)
			g_hash_table_insert(params, g_strdup("service"),
			                    g_strdup(opt_migrate_service));
		else
			g_hash_table_insert(params, g_strdup("service"),
			                    g_strdup(inferred_service));

		/* Source token: use explicit --token or fall back to env */
		{
			const gchar *src_token;

			src_token = opt_migrate_token_src;
			if (src_token == NULL)
			{
				switch (source_forge_type) {
				case GCTL_FORGE_TYPE_GITHUB:
					src_token = g_getenv("GITHUB_TOKEN");
					break;
				case GCTL_FORGE_TYPE_GITLAB:
					src_token = g_getenv("GITLAB_TOKEN");
					break;
				case GCTL_FORGE_TYPE_FORGEJO:
					src_token = g_getenv("FORGEJO_TOKEN");
					break;
				case GCTL_FORGE_TYPE_GITEA:
					src_token = g_getenv("GITEA_TOKEN");
					break;
				default:
					break;
				}
			}
			if (src_token != NULL)
				g_hash_table_insert(params,
				    g_strdup("source_token"),
				    g_strdup(src_token));
		}

		/* Build destination context for this repo */
		dest_ctx = gctl_forge_context_new(
		    dest_forge_type, NULL, owner_on_dest, rname,
		    dest_host, dest_cli);

		/* Try forge-native migrate */
		mig_argv = gctl_forge_build_argv(
		    dest_forge, GCTL_RESOURCE_KIND_REPO,
		    GCTL_VERB_MIGRATE, dest_ctx, params, &mig_err);

		if (mig_argv == NULL)
		{
			g_printerr("  error: %s -- skipping\n",
			           mig_err ? mig_err->message : "unsupported");
			failed++;
			continue;
		}

		mig_result = gctl_executor_run(
		    executor, (const gchar * const *)mig_argv, &mig_err);
		g_strfreev(mig_argv);

		if (mig_result == NULL ||
		    gctl_command_result_get_exit_code(mig_result) != 0)
		{
			const gchar *err_text = NULL;

			if (mig_result != NULL)
				err_text = gctl_command_result_get_stderr(
				    mig_result);
			g_printerr("  error: migration failed");
			if (err_text != NULL && *err_text != '\0')
				g_printerr(": %s", err_text);
			g_printerr(" -- skipping\n");
			failed++;
			continue;
		}

		g_printerr("  migrated successfully\n");
		succeeded++;

		/*
		 * Post-migrate: set up push mirror from destination back
		 * to the source (--mirror-back).
		 */
		if (opt_migrate_mirror_back)
		{
			g_autofree gchar *mb_body = NULL;
			g_autofree gchar *mb_endpoint = NULL;
			const gchar *mb_username;
			const gchar *mb_token = NULL;

			mb_username = source_owner;

			switch (source_forge_type) {
			case GCTL_FORGE_TYPE_GITHUB:
				mb_token = g_getenv("GITHUB_TOKEN");
				break;
			case GCTL_FORGE_TYPE_GITLAB:
				mb_token = g_getenv("GITLAB_TOKEN");
				break;
			case GCTL_FORGE_TYPE_FORGEJO:
				mb_token = g_getenv("FORGEJO_TOKEN");
				break;
			case GCTL_FORGE_TYPE_GITEA:
				mb_token = g_getenv("GITEA_TOKEN");
				break;
			default:
				break;
			}

			mb_body = repo_build_push_mirror_body(
			    dest_forge_type, repo_source_url,
			    mb_username, mb_token,
			    "8h0m0s", opt_migrate_sync_on_commit,
			    is_dry_run);

			if (mb_body != NULL)
			{
				g_autoptr(GError) mb_err = NULL;
				g_autoptr(GctlCommandResult) mb_result = NULL;
				gchar **mb_argv;
				GctlForge *dest_forge_mod;

				mb_endpoint = g_strdup_printf(
				    "/repos/%s/%s/push_mirrors",
				    owner_on_dest, rname);

				dest_forge_mod =
				    gctl_module_manager_find_forge(
				        mm, dest_forge_type);

				if (dest_forge_mod != NULL)
				{
					mb_argv = gctl_forge_build_api_argv(
					    dest_forge_mod, "POST",
					    mb_endpoint, mb_body,
					    dest_ctx, &mb_err);

					if (mb_argv != NULL)
					{
						mb_result = gctl_executor_run(
						    executor,
						    (const gchar * const *)mb_argv,
						    &mb_err);
						g_strfreev(mb_argv);
					}
				}

				if (verbose)
					g_printerr("  mirror-back to %s "
					           "configured\n",
					           repo_source_url);
			}
			else if (verbose)
			{
				g_printerr("  warning: push mirrors not "
				           "supported on %s\n",
				           gctl_forge_type_to_string(
				               dest_forge_type));
			}
		}

		/*
		 * Post-migrate: set up push mirrors to additional
		 * destinations (--mirror-to).  Each mirror-to URL is
		 * treated as an org-level URL; the repo name is appended.
		 */
		if (opt_migrate_mirror_to != NULL)
		{
			gint m;

			for (m = 0; opt_migrate_mirror_to[m] != NULL; m++)
			{
				g_autofree gchar *per_repo_url = NULL;

				per_repo_url = g_strdup_printf("%s/%s",
				    opt_migrate_mirror_to[m], rname);

				if (verbose)
					g_printerr("  mirror-to %s\n",
					           per_repo_url);

				setup_mirror_to(app, per_repo_url, dest_ctx,
				               opt_migrate_sync_on_commit);
			}
		}
	}

	/* Step 4: Summary */
	g_printerr("\nMass migration complete: %u/%u succeeded",
	           succeeded, total);
	if (failed > 0)
		g_printerr(", %u failed", failed);
	g_printerr("\n");

	return (failed > 0) ? 1 : 0;
}

/**
 * cmd_repo_migrate:
 * @app: the #GctlApp instance
 * @argc: remaining argument count after verb
 * @argv: remaining argument vector after verb
 *
 * Handles "gitctl repo migrate <source-url> --to <forge> [options]".
 *
 * Migrates a repository from one forge to another.  For forges that
 * support native migration (Forgejo, Gitea), the forge CLI's migrate
 * command is invoked directly.  For forges without native migrate
 * support (GitHub, GitLab), a fallback creates a blank repo and
 * prints instructions for manual mirror setup.
 *
 * When --mass-migrate is specified and the source URL points to an
 * org/user (not a specific repo), all repos in that org are migrated.
 *
 * Optionally sets up push mirrors back to the source (--mirror-back)
 * and to additional destinations (--mirror-to).
 *
 * Returns: 0 on success, 1 on error
 */
static gint
cmd_repo_migrate(
	GctlApp  *app,
	gint      argc,
	gchar   **argv
){
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GError) error = NULL;
	GctlConfig *config;
	GctlExecutor *executor;
	GctlModuleManager *mm;
	GctlForgeType dest_forge_type;
	GctlForgeType source_forge_type;
	GctlForge *dest_forge;
	const gchar *source_url;
	const gchar *dest_host;
	const gchar *dest_cli;
	g_autofree gchar *repo_name = NULL;
	g_autofree gchar *source_host = NULL;
	g_autofree gchar *source_owner = NULL;
	g_autofree gchar *source_repo = NULL;
	g_autofree gchar *inferred_service = NULL;
	gboolean verbose;
	gint ret;

	/* Reset statics for safety */
	opt_migrate_to_forge = NULL;
	opt_migrate_name = NULL;
	opt_migrate_owner = NULL;
	opt_migrate_private = FALSE;
	opt_migrate_include = NULL;
	opt_migrate_service = NULL;
	opt_migrate_token_src = NULL;
	opt_migrate_mirror = FALSE;
	opt_migrate_mirror_back = FALSE;
	opt_migrate_sync_on_commit = FALSE;
	opt_migrate_mirror_to = NULL;
	opt_migrate_mass = FALSE;

	GOptionEntry entries[] = {
		{ "to", 't', 0, G_OPTION_ARG_STRING, &opt_migrate_to_forge,
		  "Destination forge type (github, gitlab, forgejo, gitea)",
		  "FORGE" },
		{ "name", 'n', 0, G_OPTION_ARG_STRING, &opt_migrate_name,
		  "Repository name on destination (default: source repo name)",
		  "NAME" },
		{ "owner", 'o', 0, G_OPTION_ARG_STRING, &opt_migrate_owner,
		  "Owner on destination (default: source owner / authenticated user)",
		  "OWNER" },
		{ "private", 'p', 0, G_OPTION_ARG_NONE, &opt_migrate_private,
		  "Create as private repository", NULL },
		{ "include", 'i', 0, G_OPTION_ARG_STRING, &opt_migrate_include,
		  "Items to include: all, or comma-separated list "
		  "(lfs,wiki,issues,prs,milestones,labels,releases)", "ITEMS" },
		{ "service", 's', 0, G_OPTION_ARG_STRING, &opt_migrate_service,
		  "Source service type (git, github, gitlab, forgejo, gitea)",
		  "TYPE" },
		{ "token", 0, 0, G_OPTION_ARG_STRING, &opt_migrate_token_src,
		  "Authentication token for accessing the source repository",
		  "TOKEN" },
		{ "mirror", 'm', 0, G_OPTION_ARG_NONE, &opt_migrate_mirror,
		  "Create as a mirror (pull mirror) instead of a one-time "
		  "migration", NULL },
		{ "mirror-back", 0, 0, G_OPTION_ARG_NONE,
		  &opt_migrate_mirror_back,
		  "Set up push mirror from destination back to source after "
		  "migration", NULL },
		{ "mirror-to", 0, 0, G_OPTION_ARG_STRING_ARRAY,
		  &opt_migrate_mirror_to,
		  "Set up push mirror to this URL after migration (repeatable)",
		  "URL" },
		{ "sync-on-commit", 0, 0, G_OPTION_ARG_NONE,
		  &opt_migrate_sync_on_commit,
		  "Sync mirrors on every push", NULL },
		{ "token-github", 0, 0, G_OPTION_ARG_STRING, &opt_token_github,
		  "GitHub token for mirror destination", "TOKEN" },
		{ "token-gitlab", 0, 0, G_OPTION_ARG_STRING, &opt_token_gitlab,
		  "GitLab token for mirror destination", "TOKEN" },
		{ "token-forgejo", 0, 0, G_OPTION_ARG_STRING, &opt_token_forgejo,
		  "Forgejo token for mirror destination", "TOKEN" },
		{ "token-gitea", 0, 0, G_OPTION_ARG_STRING, &opt_token_gitea,
		  "Gitea token for mirror destination", "TOKEN" },
		{ "mass-migrate", 0, 0, G_OPTION_ARG_NONE, &opt_migrate_mass,
		  "Migrate ALL repos from a user/org (source URL must be an "
		  "org/user URL, not a specific repo)", NULL },
		{ NULL }
	};

	opt_context = g_option_context_new(
	    "<source-url> --to <forge> - migrate a repository");
	g_option_context_add_main_entries(opt_context, entries, NULL);

	if (!g_option_context_parse(opt_context, &argc, &argv, &error))
	{
		g_printerr("error: %s\n", error->message);
		ret = 1;
		goto cleanup;
	}

	/* Validate required arguments */
	if (argc < 2)
	{
		g_printerr("error: source repository URL required\n");
		g_printerr("Usage: gitctl repo migrate <source-url> "
		           "--to <forge> [options]\n");
		ret = 1;
		goto cleanup;
	}

	if (opt_migrate_to_forge == NULL)
	{
		g_printerr("error: --to <forge> is required\n");
		g_printerr("Usage: gitctl repo migrate <source-url> "
		           "--to <forge> [options]\n");
		ret = 1;
		goto cleanup;
	}

	source_url = argv[1];
	config = gctl_app_get_config(app);
	executor = gctl_app_get_executor(app);
	mm = gctl_app_get_module_manager(app);
	verbose = gctl_app_get_verbose(app);

	/* Determine destination forge type */
	dest_forge_type = gctl_forge_type_from_string(opt_migrate_to_forge);
	if (dest_forge_type == GCTL_FORGE_TYPE_UNKNOWN)
	{
		g_printerr("error: unknown forge type '%s'\n",
		           opt_migrate_to_forge);
		g_printerr("Supported: github, gitlab, forgejo, gitea\n");
		ret = 1;
		goto cleanup;
	}

	/* Find the destination forge module */
	dest_forge = gctl_module_manager_find_forge(mm, dest_forge_type);
	if (dest_forge == NULL)
	{
		g_printerr("error: no module available for %s\n",
		           gctl_forge_type_to_string(dest_forge_type));
		ret = 1;
		goto cleanup;
	}

	/* Parse the source URL to extract host, owner, repo name */
	if (!repo_parse_mirror_url(source_url, &source_host,
	                           &source_owner, &source_repo))
	{
		g_printerr("error: could not parse source URL '%s'\n",
		           source_url);
		ret = 1;
		goto cleanup;
	}

	/*
	 * Mass migration: if --mass-migrate is set and the source URL
	 * points to an org/user (no repo segment), delegate to the
	 * mass-migrate handler which lists and migrates all repos.
	 */
	if (opt_migrate_mass)
	{
		if (source_repo != NULL && *source_repo != '\0')
		{
			g_printerr("error: --mass-migrate expects an org/user "
			           "URL, not a repo URL\n");
			g_printerr("  Use: gitctl repo migrate "
			           "https://gitlab.com/immutablue "
			           "--to forgejo --mass-migrate\n");
			ret = 1;
			goto cleanup;
		}

		ret = cmd_repo_mass_migrate(app, source_host,
		                            source_owner, executor,
		                            config, mm, verbose);
		goto cleanup;
	}

	/* Determine the repository name on the destination */
	if (opt_migrate_name != NULL)
		repo_name = g_strdup(opt_migrate_name);
	else
		repo_name = g_strdup(source_repo);

	if (repo_name == NULL || repo_name[0] == '\0')
	{
		g_printerr("error: could not determine repository name; "
		           "use --name to specify it\n");
		ret = 1;
		goto cleanup;
	}

	/*
	 * Determine the owner on the destination.  Defaults to the
	 * source URL's owner, which preserves org-to-org migrations.
	 * Override with --owner if the destination org is different.
	 */
	if (opt_migrate_owner == NULL)
		opt_migrate_owner = g_strdup(source_owner);

	/* Detect source forge type from the hostname in config */
	source_forge_type = gctl_config_get_forge_for_host(config,
	                                                   source_host);

	/* Infer --service from source forge type if not provided */
	if (opt_migrate_service == NULL && source_forge_type !=
	    GCTL_FORGE_TYPE_UNKNOWN)
	{
		inferred_service = g_strdup(
		    infer_service_from_forge(source_forge_type));
	}

	/* Get destination host and CLI path from config */
	dest_host = gctl_config_get_default_host(config, dest_forge_type);
	dest_cli = gctl_config_get_cli_path(config, dest_forge_type);

	if (verbose)
	{
		g_printerr("note: migrating %s -> %s (repo: %s)\n",
		           source_url,
		           gctl_forge_type_to_string(dest_forge_type),
		           repo_name);
		g_printerr("note: source forge: %s, service: %s\n",
		           gctl_forge_type_to_string(source_forge_type),
		           opt_migrate_service != NULL
		               ? opt_migrate_service
		               : (inferred_service != NULL
		                   ? inferred_service : "(auto)"));
	}

	/* Build params hash for the forge module */
	params = g_hash_table_new_full(g_str_hash, g_str_equal,
	                               g_free, g_free);
	g_hash_table_insert(params, g_strdup("source_url"),
	                    g_strdup(source_url));
	g_hash_table_insert(params, g_strdup("name"),
	                    g_strdup(repo_name));

	if (opt_migrate_owner != NULL)
		g_hash_table_insert(params, g_strdup("repo_owner"),
		                    g_strdup(opt_migrate_owner));

	if (opt_migrate_private)
		g_hash_table_insert(params, g_strdup("private"),
		                    g_strdup("true"));

	if (opt_migrate_mirror)
		g_hash_table_insert(params, g_strdup("mirror"),
		                    g_strdup("true"));

	if (opt_migrate_include != NULL)
		g_hash_table_insert(params, g_strdup("include"),
		                    g_strdup(opt_migrate_include));

	if (opt_migrate_service != NULL)
		g_hash_table_insert(params, g_strdup("service"),
		                    g_strdup(opt_migrate_service));
	else if (inferred_service != NULL)
		g_hash_table_insert(params, g_strdup("service"),
		                    g_strdup(inferred_service));

	if (opt_migrate_token_src != NULL) {
		g_hash_table_insert(params, g_strdup("source_token"),
		                    g_strdup(opt_migrate_token_src));
	} else {
		/*
		 * Fall back to forge-specific environment variables for the
		 * SOURCE forge token.  This token is needed by the destination
		 * forge's migration worker to pull issues, PRs, etc. from
		 * the source via its API.
		 */
		const gchar *env_token = NULL;

		switch (source_forge_type) {
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
			g_hash_table_insert(params, g_strdup("source_token"),
			                    g_strdup(env_token));
			if (verbose)
				g_printerr("note: using %s_TOKEN env var for "
				           "source authentication\n",
				           gctl_forge_type_to_string(
				               source_forge_type));
		}
	}

	/*
	 * Build the destination forge context for the newly created repo.
	 * We extract the owner from the source URL's owner by default.
	 * For forges like Forgejo/Gitea the CLI will use the authenticated
	 * user if no owner is specified.
	 */
	{
		g_autoptr(GctlForgeContext) dest_context = NULL;
		g_autoptr(GctlCommandResult) migrate_result = NULL;
		g_autoptr(GError) build_err = NULL;
		gchar **migrate_argv;

		dest_context = gctl_forge_context_new(
		    dest_forge_type,
		    NULL,           /* remote_url: not yet known */
		    source_owner,   /* owner: reuse source owner as default */
		    repo_name,
		    dest_host,
		    dest_cli);

		/* Try the forge's native migrate command */
		migrate_argv = gctl_forge_build_argv(
		    dest_forge, GCTL_RESOURCE_KIND_REPO,
		    GCTL_VERB_MIGRATE, dest_context,
		    params, &build_err);

		if (migrate_argv == NULL &&
		    build_err != NULL &&
		    build_err->domain == GCTL_ERROR &&
		    build_err->code == GCTL_ERROR_FORGE_UNSUPPORTED)
		{
			/*
			 * Fallback for forges without native migrate:
			 * Create a blank repo, then instruct the user to set
			 * up mirroring manually or use git clone + push.
			 */
			g_autoptr(GHashTable) create_params = NULL;
			g_autoptr(GError) create_err = NULL;
			g_autoptr(GctlCommandResult) create_result = NULL;
			gchar **create_argv;

			g_printerr("note: %s does not support native migration; "
			           "creating blank repo and using clone+push\n",
			           gctl_forge_type_to_string(dest_forge_type));

			create_params = g_hash_table_new_full(
			    g_str_hash, g_str_equal, g_free, g_free);
			g_hash_table_insert(create_params, g_strdup("name"),
			                    g_strdup(repo_name));
			if (opt_migrate_private)
				g_hash_table_insert(create_params,
				                    g_strdup("private"),
				                    g_strdup("true"));

			create_argv = gctl_forge_build_argv(
			    dest_forge, GCTL_RESOURCE_KIND_REPO,
			    GCTL_VERB_CREATE, dest_context,
			    create_params, &create_err);

			if (create_argv == NULL)
			{
				g_printerr("error: could not build repo create "
				           "command for %s: %s\n",
				           gctl_forge_type_to_string(
				               dest_forge_type),
				           create_err ? create_err->message
				                      : "unknown error");
				ret = 1;
				goto cleanup;
			}

			create_result = gctl_executor_run(
			    executor,
			    (const gchar * const *)create_argv,
			    &create_err);
			g_strfreev(create_argv);

			if (create_result == NULL ||
			    gctl_command_result_get_exit_code(
			        create_result) != 0)
			{
				const gchar *stderr_text = NULL;
				if (create_result != NULL)
					stderr_text =
					    gctl_command_result_get_stderr(
					        create_result);
				g_printerr("error: failed to create repo on "
				           "%s",
				           gctl_forge_type_to_string(
				               dest_forge_type));
				if (stderr_text != NULL &&
				    stderr_text[0] != '\0')
					g_printerr(": %s", stderr_text);
				g_printerr("\n");
				ret = 1;
				goto cleanup;
			}

			/*
			 * Now clone from source and push to destination.
			 * Build: git clone --bare <source-url> /tmp/repo.git
			 *        cd /tmp/repo.git
			 *        git push --mirror <dest-url>
			 *
			 * For simplicity in a first pass, print instructions
			 * for the user to finish the migration manually.
			 */
			g_printerr("note: blank repo created on %s. To "
			           "complete migration, run:\n",
			           gctl_forge_type_to_string(
			               dest_forge_type));
			g_printerr("  git clone --bare %s /tmp/%s.git\n",
			           source_url, repo_name);
			g_printerr("  cd /tmp/%s.git\n", repo_name);
			g_printerr("  git push --mirror <dest-remote-url>\n");
			g_printerr("  rm -rf /tmp/%s.git\n", repo_name);

			ret = 0;
		}
		else if (migrate_argv == NULL)
		{
			g_printerr("error: failed to build migrate command: "
			           "%s\n",
			           build_err ? build_err->message
			                     : "unknown error");
			ret = 1;
			goto cleanup;
		}
		else
		{
			/* Execute the native migrate command */
			migrate_result = gctl_executor_run(
			    executor,
			    (const gchar * const *)migrate_argv,
			    &error);
			g_strfreev(migrate_argv);

			if (migrate_result == NULL ||
			    gctl_command_result_get_exit_code(
			        migrate_result) != 0)
			{
				const gchar *stderr_text = NULL;
				if (migrate_result != NULL)
					stderr_text =
					    gctl_command_result_get_stderr(
					        migrate_result);
				g_printerr("error: migration failed");
				if (stderr_text != NULL &&
				    stderr_text[0] != '\0')
					g_printerr(": %s", stderr_text);
				g_printerr("\n");
				ret = 1;
				goto cleanup;
			}

			if (verbose)
				g_printerr("note: migration to %s completed "
				           "successfully\n",
				           gctl_forge_type_to_string(
				               dest_forge_type));

			ret = 0;
		}

		/*
		 * Post-migrate: set up push mirror from destination back
		 * to the source (--mirror-back).
		 *
		 * Unlike --mirror-to, we do NOT create the source repo
		 * (it already exists — that's where we migrated from).
		 * We only add the push mirror on the NEW dest repo.
		 */
		if (ret == 0 && opt_migrate_mirror_back)
		{
			g_autoptr(GctlForgeContext) new_dest_context = NULL;
			GctlForge *src_forge_mod;
			g_autofree gchar *mirror_body = NULL;
			g_autofree gchar *mirror_endpoint = NULL;
			const gchar *mirror_username;
			const gchar *mirror_token_val;
			const gchar *env_tok = NULL;

			if (verbose)
				g_printerr("note: setting up push mirror "
				           "back to source %s\n",
				           source_url);

			new_dest_context = gctl_forge_context_new(
			    dest_forge_type,
			    NULL,
			    source_owner,
			    repo_name,
			    dest_host,
			    dest_cli);

			/* Auto-detect username/token for the source forge */
			mirror_username = source_owner;

			switch (source_forge_type) {
			case GCTL_FORGE_TYPE_GITHUB:
				env_tok = g_getenv("GITHUB_TOKEN");
				break;
			case GCTL_FORGE_TYPE_GITLAB:
				env_tok = g_getenv("GITLAB_TOKEN");
				break;
			case GCTL_FORGE_TYPE_FORGEJO:
				env_tok = g_getenv("FORGEJO_TOKEN");
				break;
			case GCTL_FORGE_TYPE_GITEA:
				env_tok = g_getenv("GITEA_TOKEN");
				break;
			default:
				break;
			}
			mirror_token_val = env_tok;

			/* Build the push mirror body */
			mirror_body = repo_build_push_mirror_body(
			    dest_forge_type, source_url,
			    mirror_username, mirror_token_val,
			    "8h0m0s", opt_migrate_sync_on_commit,
			    gctl_executor_get_dry_run(executor));

			if (mirror_body != NULL)
			{
				g_autoptr(GError) mirror_err = NULL;
				g_autoptr(GctlCommandResult) mirror_result = NULL;
				gchar **mirror_argv;
				const gchar *own;
				const gchar *rname;

				own = source_owner;
				rname = repo_name;
				if (own == NULL || *own == '\0')
					own = g_getenv("GITCTL_USER");

				if (own != NULL && rname != NULL)
				{
					mirror_endpoint = g_strdup_printf(
					    "/repos/%s/%s/push_mirrors",
					    own, rname);

					src_forge_mod =
					    gctl_module_manager_find_forge(
					        mm, dest_forge_type);

					if (src_forge_mod != NULL)
					{
						mirror_argv =
						    gctl_forge_build_api_argv(
						        src_forge_mod, "POST",
						        mirror_endpoint,
						        mirror_body,
						        new_dest_context,
						        &mirror_err);

						if (mirror_argv != NULL)
						{
							mirror_result =
							    gctl_executor_run(
							        executor,
							        (const gchar * const *)mirror_argv,
							        &mirror_err);
							g_strfreev(mirror_argv);
						}
					}
				}

				if (verbose)
					g_printerr("note: push mirror back to "
					           "%s configured\n",
					           source_url);
			}
			else
			{
				g_printerr("warning: push mirrors not "
				           "supported on %s\n",
				           gctl_forge_type_to_string(
				               dest_forge_type));
			}
		}

		/*
		 * Post-migrate: set up push mirrors to additional
		 * destinations (--mirror-to)
		 */
		if (ret == 0 && opt_migrate_mirror_to != NULL)
		{
			g_autoptr(GctlForgeContext) new_dest_context = NULL;
			gint m;

			new_dest_context = gctl_forge_context_new(
			    dest_forge_type,
			    NULL,
			    source_owner,
			    repo_name,
			    dest_host,
			    dest_cli);

			for (m = 0; opt_migrate_mirror_to[m] != NULL; m++)
			{
				if (verbose)
					g_printerr("note: setting up push "
					           "mirror to %s\n",
					           opt_migrate_mirror_to[m]);

				setup_mirror_to(app,
				                opt_migrate_mirror_to[m],
				                new_dest_context,
				                opt_migrate_sync_on_commit);
			}
		}
	}

cleanup:
	g_free(opt_migrate_to_forge);
	g_free(opt_migrate_name);
	g_free(opt_migrate_owner);
	g_free(opt_migrate_include);
	g_free(opt_migrate_service);
	g_free(opt_migrate_token_src);
	g_strfreev(opt_migrate_mirror_to);
	g_free(opt_token_github);
	g_free(opt_token_gitlab);
	g_free(opt_token_forgejo);
	g_free(opt_token_gitea);

	opt_migrate_to_forge = NULL;
	opt_migrate_name = NULL;
	opt_migrate_include = NULL;
	opt_migrate_service = NULL;
	opt_migrate_token_src = NULL;
	opt_migrate_mirror_to = NULL;
	opt_token_github = NULL;
	opt_token_gitlab = NULL;
	opt_token_forgejo = NULL;
	opt_token_gitea = NULL;

	return ret;
}

/* ── Main entry point ────────────────────────────────────────────────── */

/**
 * gctl_cmd_repo:
 * @app: the #GctlApp instance
 * @argc: argument count (verb + verb-specific args)
 * @argv: (array length=argc): argument vector, where argv[0] is the verb
 *
 * Main entry point for the "repo" command.  Extracts the verb from
 * argv[0], looks it up in the dispatch table, and delegates to the
 * appropriate verb handler.
 *
 * Returns: 0 on success, 1 on error
 */
gint
gctl_cmd_repo(
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

	/*
	 * "gitctl repo mirror ..." delegates to the mirror command handler.
	 * This provides a natural way to manage mirrors via the repo noun:
	 *   gitctl repo mirror list
	 *   gitctl repo mirror add --url <url>
	 *   gitctl repo mirror sync
	 */
	if (g_strcmp0(verb_name, "mirror") == 0)
		return gctl_cmd_mirror(app, argc - 1, argv + 1);

	entry = gctl_cmd_find_verb(repo_verbs, N_REPO_VERBS, verb_name);

	if (entry == NULL)
	{
		g_printerr("error: unknown verb '%s' for repo command\n",
		           verb_name);
		print_usage();
		return 1;
	}

	switch (entry->verb)
	{
		case GCTL_VERB_LIST:
			return cmd_repo_list(app, argc, argv);
		case GCTL_VERB_GET:
			return cmd_repo_get(app, argc, argv);
		case GCTL_VERB_CREATE:
			return cmd_repo_create(app, argc, argv);
		case GCTL_VERB_EDIT:
			return cmd_repo_edit(app, argc, argv);
		case GCTL_VERB_FORK:
			return cmd_repo_fork(app, argc, argv);
		case GCTL_VERB_CLONE:
			return cmd_repo_clone(app, argc, argv);
		case GCTL_VERB_DELETE:
			return cmd_repo_delete(app, argc, argv);
		case GCTL_VERB_BROWSE:
			return cmd_repo_browse(app, argc, argv);
		case GCTL_VERB_STAR:
			return cmd_repo_star(app, argc, argv);
		case GCTL_VERB_UNSTAR:
			return cmd_repo_unstar(app, argc, argv);
		case GCTL_VERB_MIGRATE:
			return cmd_repo_migrate(app, argc, argv);
		default:
			g_printerr("error: verb '%s' not implemented for repo\n",
			           verb_name);
			return 1;
	}
}
