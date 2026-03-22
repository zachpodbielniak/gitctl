/*
 * main.c - gitctl entry point
 *
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Provides the command-line interface for gitctl. Uses GOptionContext
 * for global argument parsing and dispatches to noun-specific handlers.
 *
 * Usage: gitctl [global-options] <noun> <verb> [args] [flags]
 */

#define GCTL_COMPILATION

#include <glib.h>
#include <glib-object.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gitctl-types.h"
#include "gitctl-enums.h"
#include "gitctl-error.h"
#include "gitctl-version.h"
#include "core/gitctl-app.h"
#include "core/gitctl-config.h"
#include "core/gitctl-context-resolver.h"
#include "commands/gitctl-cmd-pr.h"
#include "commands/gitctl-cmd-issue.h"
#include "commands/gitctl-cmd-repo.h"
#include "commands/gitctl-cmd-release.h"
#include "commands/gitctl-cmd-mirror.h"
#include "commands/gitctl-cmd-api.h"
#include "commands/gitctl-cmd-config.h"
#include "commands/gitctl-cmd-completion.h"

/* ===== License text ===== */

static const gchar *license_text =
	"gitctl " GCTL_VERSION "\n"
	"Copyright (C) 2026 Zach Podbielniak\n"
	"\n"
	"This program is free software: you can redistribute it and/or modify\n"
	"it under the terms of the GNU Affero General Public License as published by\n"
	"the Free Software Foundation, either version 3 of the License, or\n"
	"(at your option) any later version.\n"
	"\n"
	"This program is distributed in the hope that it will be useful,\n"
	"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
	"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
	"GNU Affero General Public License for more details.\n"
	"\n"
	"You should have received a copy of the GNU Affero General Public License\n"
	"along with this program.  If not, see <https://www.gnu.org/licenses/>.\n";

/* ===== Signal handling ===== */

static volatile sig_atomic_t g_interrupted = 0;

static void
signal_handler(int signum)
{
	(void)signum;
	g_interrupted = 1;
}

/* ===== Global options ===== */

static gboolean opt_version = FALSE;
static gboolean opt_license = FALSE;
static gboolean opt_dry_run = FALSE;
static gboolean opt_verbose = FALSE;
static gchar *opt_output = NULL;
static gchar *opt_forge = NULL;
static gchar *opt_remote = NULL;
static gchar *opt_config = NULL;

static GOptionEntry global_entries[] = {
	{
		"version", 'v', 0, G_OPTION_ARG_NONE, &opt_version,
		"Show version", NULL
	},
	{
		"license", 0, 0, G_OPTION_ARG_NONE, &opt_license,
		"Show license (AGPLv3)", NULL
	},
	{
		"dry-run", 'n', 0, G_OPTION_ARG_NONE, &opt_dry_run,
		"Show commands without executing", NULL
	},
	{
		"verbose", 0, 0, G_OPTION_ARG_NONE, &opt_verbose,
		"Enable verbose output", NULL
	},
	{
		"output", 'o', 0, G_OPTION_ARG_STRING, &opt_output,
		"Output format (table, json, yaml, csv)", "FORMAT"
	},
	{
		"forge", 'f', 0, G_OPTION_ARG_STRING, &opt_forge,
		"Force forge type (github, gitlab, forgejo, gitea)", "TYPE"
	},
	{
		"remote", 'r', 0, G_OPTION_ARG_STRING, &opt_remote,
		"Git remote to use (default: origin)", "NAME"
	},
	{
		"config", 'c', 0, G_OPTION_ARG_STRING, &opt_config,
		"Configuration file path", "PATH"
	},
	{ NULL }
};

/* ===== Command dispatch table ===== */

typedef struct {
	const gchar *name;
	const gchar *description;
	gint (*handler)(GctlApp *app, gint argc, gchar **argv);
} GctlCommand;

static const GctlCommand commands[] = {
	{ "pr",      "Manage pull requests",       gctl_cmd_pr      },
	{ "issue",   "Manage issues",              gctl_cmd_issue   },
	{ "repo",    "Manage repositories",        gctl_cmd_repo    },
	{ "release", "Manage releases",            gctl_cmd_release },
	{ "mirror",  "Manage repository mirrors",  gctl_cmd_mirror  },
	{ "api",     "Make raw API requests",      gctl_cmd_api     },
	{ "config",  "Manage configuration",       gctl_cmd_config  },
	{ "completion", "Generate shell completions", gctl_cmd_completion },
	{ NULL, NULL, NULL }
};

/* ===== Help output ===== */

static void
print_usage(void)
{
	const GctlCommand *cmd;

	g_print("Usage: gitctl [options] <command> <verb> [args]\n");
	g_print("\n");
	g_print("A kubectl-like CLI for managing git repositories across forges.\n");
	g_print("Supports GitHub (gh), GitLab (glab), Forgejo (fj), and Gitea (tea).\n");
	g_print("\n");
	g_print("Commands:\n");

	for (cmd = commands; cmd->name != NULL; cmd++) {
		g_print("  %-12s %s\n", cmd->name, cmd->description);
	}

	g_print("\n");
	g_print("Global options:\n");
	g_print("  -h, --help              Show help\n");
	g_print("  -v, --version           Show version\n");
	g_print("  --license               Show license (AGPLv3)\n");
	g_print("  -n, --dry-run           Show commands without executing\n");
	g_print("  --verbose               Enable verbose output\n");
	g_print("  -o, --output FORMAT     Output format (table, json, yaml, csv)\n");
	g_print("  -f, --forge TYPE        Force forge (github, gitlab, forgejo, gitea)\n");
	g_print("  -r, --remote NAME       Git remote (default: origin)\n");
	g_print("  -c, --config PATH       Config file path\n");
	g_print("\n");
	g_print("Environment variables (overridden by CLI flags):\n");
	g_print("  GITCTL_CONFIG               Configuration file path\n");
	g_print("  GITCTL_FORGE                Force forge type\n");
	g_print("  GITCTL_REMOTE               Git remote name\n");
	g_print("  GITCTL_OUTPUT               Output format\n");
	g_print("\n");
	g_print("Examples:\n");
	g_print("  gitctl pr list                           List open pull requests\n");
	g_print("  gitctl pr get 123                        View PR #123\n");
	g_print("  gitctl pr create --title \"Fix bug\"       Create a pull request\n");
	g_print("  gitctl issue list --state closed          List closed issues\n");
	g_print("  gitctl repo create myproject --private    Create a private repo\n");
	g_print("  gitctl --dry-run pr merge 123             Show merge command\n");
	g_print("  gitctl -o json pr list                    List PRs as JSON\n");
	g_print("  gitctl -f gitlab pr list                  Force GitLab forge\n");
	g_print("  gitctl api GET /repos/{owner}/{repo}      Raw API request\n");
	g_print("  GITCTL_FORGE=gitlab gitctl pr list        Use env var for forge\n");
	g_print("\n");
	g_print("Run 'gitctl <command> --help' for command-specific help.\n");
}

/**
 * parse_output_format:
 * @name: the output format string
 *
 * Parses an output format name into the enum value.
 *
 * Returns: the format, or -1 if invalid
 */
static gint
parse_output_format(const gchar *name)
{
	if (name == NULL) return -1;
	if (g_ascii_strcasecmp(name, "table") == 0) return GCTL_OUTPUT_FORMAT_TABLE;
	if (g_ascii_strcasecmp(name, "json") == 0)  return GCTL_OUTPUT_FORMAT_JSON;
	if (g_ascii_strcasecmp(name, "yaml") == 0)  return GCTL_OUTPUT_FORMAT_YAML;
	if (g_ascii_strcasecmp(name, "csv") == 0)   return GCTL_OUTPUT_FORMAT_CSV;
	return -1;
}

int
main(
	int     argc,
	char  **argv
){
	g_autoptr(GOptionContext) context = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GctlApp) app = NULL;
	const GctlCommand *cmd;
	const gchar *command_name;
	gint i;
	gint sub_argc;
	gchar **sub_argv;

	/*
	 * Parse global options. We stop at the first non-option argument
	 * (the command name) by using G_OPTION_CONTEXT_NO_HELP and
	 * manually handling --help.
	 */
	context = g_option_context_new("<command> [verb] [args]");
	g_option_context_set_summary(context,
		"kubectl-like CLI for managing git repositories across forges");
	g_option_context_add_main_entries(context, global_entries, NULL);
	g_option_context_set_ignore_unknown_options(context, TRUE);
	g_option_context_set_help_enabled(context, FALSE);

	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		g_printerr("Error: %s\n", error->message);
		g_printerr("Run 'gitctl --help' for usage.\n");
		return 1;
	}

	/* Environment variable defaults (CLI flags override) */
	if (opt_config == NULL)
		opt_config = g_strdup(g_getenv("GITCTL_CONFIG"));
	if (opt_forge == NULL)
		opt_forge = g_strdup(g_getenv("GITCTL_FORGE"));
	if (opt_remote == NULL)
		opt_remote = g_strdup(g_getenv("GITCTL_REMOTE"));
	if (opt_output == NULL)
		opt_output = g_strdup(g_getenv("GITCTL_OUTPUT"));

	/* Handle --version */
	if (opt_version) {
		g_print("gitctl %s\n", GCTL_VERSION);
		return 0;
	}

	/* Handle --license */
	if (opt_license) {
		g_print("%s", license_text);
		return 0;
	}

	/* After GOptionContext parsing, remaining args are in argv[1..argc-1].
	 * The first remaining arg is the command name. */
	if (argc < 2) {
		print_usage();
		return 0;
	}

	command_name = argv[1];

	/* Check for --help / -h as first argument */
	if (g_strcmp0(command_name, "--help") == 0 ||
	    g_strcmp0(command_name, "-h") == 0) {
		print_usage();
		return 0;
	}

	/* Create and initialize the app (before command lookup, for alias support) */
	app = gctl_app_new();

	/* Apply global options */
	if (opt_dry_run)
		gctl_app_set_dry_run(app, TRUE);

	if (opt_verbose)
		gctl_app_set_verbose(app, TRUE);

	if (opt_output) {
		gint fmt;

		fmt = parse_output_format(opt_output);
		if (fmt < 0) {
			g_printerr("Error: unknown output format '%s'\n", opt_output);
			g_printerr("Valid formats: table, json, yaml, csv\n");
			return 1;
		}
		gctl_app_set_output_format(app, (GctlOutputFormat)fmt);
	}

	/* Initialize the app (loads config, modules, etc.) */
	if (!gctl_app_initialize(app, &error)) {
		g_printerr("Error: initialization failed: %s\n", error->message);
		return 1;
	}

	/* Apply --config override (or GITCTL_CONFIG env var) */
	if (opt_config) {
		GctlConfig *config = gctl_app_get_config(app);
		g_autoptr(GError) cfg_err = NULL;
		if (!gctl_config_load(config, opt_config, &cfg_err)) {
			g_printerr("warning: failed to load config '%s': %s\n",
			           opt_config, cfg_err->message);
		}
	}

	/* Apply --remote override (or GITCTL_REMOTE env var) */
	if (opt_remote) {
		GctlConfig *config = gctl_app_get_config(app);
		gctl_config_set_default_remote(config, opt_remote);
	}

	/* If --forge was specified, force the forge type on the resolver */
	if (opt_forge) {
		GctlForgeType forge_type;
		GctlContextResolver *resolver;

		forge_type = gctl_forge_type_from_string(opt_forge);
		if (forge_type == GCTL_FORGE_TYPE_UNKNOWN) {
			g_printerr("Error: unknown forge type '%s'\n", opt_forge);
			g_printerr("Valid forges: github, gitlab, forgejo, gitea\n");
			return 1;
		}

		resolver = gctl_app_get_resolver(app);
		gctl_context_resolver_set_forced_forge(resolver, forge_type);
	}

	/* Install signal handlers for graceful interruption */
	{
		struct sigaction sa;
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = signal_handler;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sigaction(SIGINT, &sa, NULL);
		sigaction(SIGTERM, &sa, NULL);
	}

	/* Compute sub_argv before alias expansion (may be rebuilt below) */
	sub_argc = argc - 2;
	sub_argv = argv + 2;

	/*
	 * Alias expansion — check if the command name is a user-defined alias.
	 * The app must be initialized before this point so the config (which
	 * contains alias definitions) is loaded.  The command lookup must
	 * happen AFTER this so the expanded command name is used.
	 *
	 * Example: alias "prl" = "pr list"
	 * User runs:  gitctl prl --state closed
	 * Expands to: gitctl pr list --state closed
	 */
	{
		GctlConfig *alias_config;
		const gchar *alias_value;
		gchar *expanded_command_name = NULL;
		gchar **expanded_sub_argv = NULL;

		alias_config = gctl_app_get_config(app);
		alias_value = NULL;

		if (alias_config != NULL)
			alias_value = gctl_config_get_alias(alias_config, command_name);

		if (alias_value != NULL)
		{
			gchar **alias_parts;
			gint n_parts;

			alias_parts = g_strsplit(alias_value, " ", -1);
			n_parts = (gint)g_strv_length(alias_parts);

			if (n_parts >= 1)
			{
				/*
				 * Replace command_name with the first word of the alias.
				 * The remaining alias words become the new verb + args,
				 * followed by any extra args the user passed after the alias.
				 *
				 * command_name becomes alias_parts[0] (e.g. "pr")
				 * sub_argv becomes ["list", "--state", "closed"]
				 */
				gint alias_extra;
				gint orig_sub_argc;
				gint new_sub_argc;
				gint j;

				expanded_command_name = g_strdup(alias_parts[0]);
				command_name = expanded_command_name;

				alias_extra = n_parts - 1;
				orig_sub_argc = sub_argc;
				new_sub_argc = alias_extra + orig_sub_argc;

				expanded_sub_argv = g_new0(gchar *, (gsize)(new_sub_argc + 1));

				/* Copy alias extra words (strdup for ownership) */
				for (j = 0; j < alias_extra; j++)
					expanded_sub_argv[j] = g_strdup(alias_parts[1 + j]);

				/* Copy original sub_argv entries (strdup for ownership) */
				for (j = 0; j < orig_sub_argc; j++)
					expanded_sub_argv[alias_extra + j] = g_strdup(sub_argv[j]);

				sub_argc = new_sub_argc;
				sub_argv = expanded_sub_argv;

				if (gctl_app_get_verbose(app))
					g_printerr("alias: %s -> %s\n",
					           argv[1], alias_value);
			}

			g_strfreev(alias_parts);
		}

		/* Look up the command (after alias expansion) */
		cmd = NULL;
		for (i = 0; commands[i].name != NULL; i++) {
			if (g_strcmp0(commands[i].name, command_name) == 0) {
				cmd = &commands[i];
				break;
			}
		}

		if (cmd == NULL) {
			g_printerr("Error: unknown command '%s'\n", command_name);
			g_printerr("Run 'gitctl --help' for a list of commands.\n");
			g_free(expanded_command_name);
			g_strfreev(expanded_sub_argv);
			return 1;
		}

		/* Dispatch to the command handler */
		{
			gint ret;

			ret = cmd->handler(app, sub_argc, sub_argv);

			g_free(expanded_command_name);
			g_strfreev(expanded_sub_argv);

			if (g_interrupted)
				return 128 + SIGINT;
			return ret;
		}
	}
}
