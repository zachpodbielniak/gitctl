/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-cmd-completion.c - Shell completion script generation */

#define GCTL_COMPILATION
#include "gitctl.h"

static void
print_bash_completion(void)
{
    /* Output a comprehensive bash completion script */
    g_print(
        "# gitctl bash completion\n"
        "# Add to ~/.bashrc: eval \"$(gitctl completion bash)\"\n"
        "\n"
        "_gitctl() {\n"
        "    local cur prev pprev\n"
        "    cur=\"${COMP_WORDS[COMP_CWORD]}\"\n"
        "    prev=\"${COMP_WORDS[COMP_CWORD-1]}\"\n"
        "    if [[ ${COMP_CWORD} -ge 2 ]]; then\n"
        "        pprev=\"${COMP_WORDS[COMP_CWORD-2]}\"\n"
        "    fi\n"
        "\n"
        "    local commands=\"pr issue repo release mirror api config completion status ci commit label notification key webhook\"\n"
        "    local global_flags=\"--help --version --license --dry-run --verbose --output --forge --remote --config --repo\"\n"
        "\n"
        "    local verbs_pr=\"list get create edit close reopen merge checkout comment review browse diff\"\n"
        "    local verbs_issue=\"list get create edit close reopen comment browse\"\n"
        "    local verbs_repo=\"list get create edit delete fork clone browse star unstar migrate\"\n"
        "    local verbs_release=\"list get create delete\"\n"
        "    local verbs_api=\"GET POST PUT PATCH DELETE\"\n"
        "    local verbs_mirror=\"list add remove sync get\"\n"
        "    local verbs_config=\"list get set\"\n"
        "    local verbs_ci=\"list get log browse\"\n"
        "    local verbs_commit=\"list get\"\n"
        "    local verbs_label=\"list create delete\"\n"
        "    local verbs_notification=\"list read\"\n"
        "    local verbs_key=\"list add remove\"\n"
        "    local verbs_webhook=\"list create delete get\"\n"
        "\n"
        "    local flags_list=\"--state --limit --author --label --assignee --pager --help\"\n"
        "    local flags_get=\"--help\"\n"
        "    local flags_create=\"--title --body --description --base --head --draft --private --help\"\n"
        "    local flags_edit=\"--title --body --assignee --label --help\"\n"
        "    local flags_delete=\"--yes -y --help\"\n"
        "    local flags_remove=\"--yes -y --help\"\n"
        "    local flags_merge=\"--method --help\"\n"
        "    local flags_comment=\"--body --help\"\n"
        "    local flags_review=\"--approve --request-changes --comment --body --help\"\n"
        "    local flags_add=\"--url --direction --interval --sync-on-commit --token --username --create-repo --no-create-repo --help\"\n"
        "    local flags_migrate=\"--to --name --private --include --service --token --mirror --mirror-back --mirror-to --mass-migrate --sync-on-commit --owner --token-github --token-gitlab --token-forgejo --token-gitea --help\"\n"
        "\n"
        "    local output_formats=\"table json yaml csv\"\n"
        "    local forge_types=\"github gitlab forgejo gitea\"\n"
        "    local state_values=\"open closed all merged\"\n"
        "\n"
        "    # Complete flag values\n"
        "    case \"${prev}\" in\n"
        "        --output|-o)\n"
        "            COMPREPLY=($(compgen -W \"${output_formats}\" -- \"${cur}\"))\n"
        "            return\n"
        "            ;;\n"
        "        --forge|-f)\n"
        "            COMPREPLY=($(compgen -W \"${forge_types}\" -- \"${cur}\"))\n"
        "            return\n"
        "            ;;\n"
        "        --state)\n"
        "            COMPREPLY=($(compgen -W \"${state_values}\" -- \"${cur}\"))\n"
        "            return\n"
        "            ;;\n"
        "        --config|-c)\n"
        "            COMPREPLY=($(compgen -f -- \"${cur}\"))\n"
        "            return\n"
        "            ;;\n"
        "        --method)\n"
        "            COMPREPLY=($(compgen -W \"merge rebase squash\" -- \"${cur}\"))\n"
        "            return\n"
        "            ;;\n"
        "        --to)\n"
        "            COMPREPLY=($(compgen -W \"${forge_types}\" -- \"${cur}\"))\n"
        "            return\n"
        "            ;;\n"
        "        --service)\n"
        "            COMPREPLY=($(compgen -W \"git github gitlab forgejo gitea\" -- \"${cur}\"))\n"
        "            return\n"
        "            ;;\n"
        "        --include)\n"
        "            COMPREPLY=($(compgen -W \"all lfs wiki issues prs milestones labels releases\" -- \"${cur}\"))\n"
        "            return\n"
        "            ;;\n"
        "    esac\n"
        "\n"
        "    # Determine position context\n"
        "    local cmd_idx=1\n"
        "    local noun=\"\"\n"
        "    local verb=\"\"\n"
        "    local i\n"
        "\n"
        "    # Skip global flags to find the noun\n"
        "    for ((i=1; i < COMP_CWORD; i++)); do\n"
        "        local w=\"${COMP_WORDS[i]}\"\n"
        "        case \"${w}\" in\n"
        "            --output|--forge|--remote|--config|--repo|-o|-f|-r|-c|-R)\n"
        "                ((i++))  # skip the flag's argument\n"
        "                ;;\n"
        "            --dry-run|--verbose|--license|--version|-n|-v)\n"
        "                ;;\n"
        "            -*)\n"
        "                ;;\n"
        "            *)\n"
        "                if [[ -z \"${noun}\" ]]; then\n"
        "                    noun=\"${w}\"\n"
        "                elif [[ -z \"${verb}\" ]]; then\n"
        "                    verb=\"${w}\"\n"
        "                fi\n"
        "                ;;\n"
        "        esac\n"
        "    done\n"
        "\n"
        "    # No noun yet — complete commands + global flags\n"
        "    if [[ -z \"${noun}\" ]]; then\n"
        "        COMPREPLY=($(compgen -W \"${commands} ${global_flags}\" -- \"${cur}\"))\n"
        "        return\n"
        "    fi\n"
        "\n"
        "    # Noun but no verb — complete verbs\n"
        "    if [[ -z \"${verb}\" ]]; then\n"
        "        local verbs_var=\"verbs_${noun}\"\n"
        "        COMPREPLY=($(compgen -W \"${!verbs_var} --help\" -- \"${cur}\"))\n"
        "        return\n"
        "    fi\n"
        "\n"
        "    # Noun + verb present — complete verb-specific flags\n"
        "    case \"${verb}\" in\n"
        "        list)     COMPREPLY=($(compgen -W \"${flags_list}\" -- \"${cur}\")) ;;\n"
        "        get)      COMPREPLY=($(compgen -W \"${flags_get}\" -- \"${cur}\")) ;;\n"
        "        create)   COMPREPLY=($(compgen -W \"${flags_create}\" -- \"${cur}\")) ;;\n"
        "        edit)     COMPREPLY=($(compgen -W \"${flags_edit}\" -- \"${cur}\")) ;;\n"
        "        delete)   COMPREPLY=($(compgen -W \"${flags_delete}\" -- \"${cur}\")) ;;\n"
        "        remove)   COMPREPLY=($(compgen -W \"${flags_remove}\" -- \"${cur}\")) ;;\n"
        "        merge)    COMPREPLY=($(compgen -W \"${flags_merge}\" -- \"${cur}\")) ;;\n"
        "        comment)  COMPREPLY=($(compgen -W \"${flags_comment}\" -- \"${cur}\")) ;;\n"
        "        review)   COMPREPLY=($(compgen -W \"${flags_review}\" -- \"${cur}\")) ;;\n"
        "        add)      COMPREPLY=($(compgen -W \"${flags_add}\" -- \"${cur}\")) ;;\n"
        "        migrate)  COMPREPLY=($(compgen -W \"${flags_migrate}\" -- \"${cur}\")) ;;\n"
        "        *)        COMPREPLY=($(compgen -W \"--help\" -- \"${cur}\")) ;;\n"
        "    esac\n"
        "}\n"
        "\n"
        "complete -F _gitctl gitctl\n"
    );
}

static void
print_usage(void)
{
    g_printerr("Usage: gitctl completion <shell>\n\n");
    g_printerr("Supported shells:\n");
    g_printerr("  bash    Generate bash completion script\n");
    g_printerr("\nExample:\n");
    g_printerr("  eval \"$(gitctl completion bash)\"\n");
}

gint
gctl_cmd_completion(
    GctlApp  *app,
    gint      argc,
    gchar   **argv
){
    (void)app;

    if (argc < 1 ||
        g_strcmp0(argv[0], "--help") == 0 ||
        g_strcmp0(argv[0], "-h") == 0)
    {
        print_usage();
        return (argc < 1) ? 1 : 0;
    }

    if (g_ascii_strcasecmp(argv[0], "bash") == 0)
    {
        print_bash_completion();
        return 0;
    }

    g_printerr("error: unsupported shell '%s'\n", argv[0]);
    g_printerr("Supported: bash\n");
    return 1;
}
