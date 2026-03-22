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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gitctl-types.h"
#include "gitctl-enums.h"
#include "gitctl-error.h"
#include "gitctl-version.h"
#include "core/gitctl-app.h"
#include "core/gitctl-context-resolver.h"
#include "commands/gitctl-cmd-pr.h"
#include "commands/gitctl-cmd-issue.h"
#include "commands/gitctl-cmd-repo.h"
#include "commands/gitctl-cmd-release.h"
#include "commands/gitctl-cmd-api.h"
#include "commands/gitctl-cmd-config.h"

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
	{ "api",     "Make raw API requests",      gctl_cmd_api     },
	{ "config",  "Manage configuration",       gctl_cmd_config  },
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

	/* Look up the command */
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
		return 1;
	}

	/* Create and initialize the app */
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

	/* Shift argv to pass the subcommand args (skip argv[0] and command name) */
	sub_argc = argc - 2;
	sub_argv = argv + 2;

	/* Dispatch to the command handler */
	return cmd->handler(app, sub_argc, sub_argv);
}
