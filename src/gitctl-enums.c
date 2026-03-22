/*
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/* gitctl-enums.c - Enumeration type registration and helpers */

#define GCTL_COMPILATION
#include "gitctl.h"

/* ── GctlForgeType ────────────────────────────────────────────────── */

static const GEnumValue gctl_forge_type_values[] = {
    { GCTL_FORGE_TYPE_UNKNOWN, "GCTL_FORGE_TYPE_UNKNOWN", "unknown" },
    { GCTL_FORGE_TYPE_GITHUB,  "GCTL_FORGE_TYPE_GITHUB",  "github"  },
    { GCTL_FORGE_TYPE_GITLAB,  "GCTL_FORGE_TYPE_GITLAB",  "gitlab"  },
    { GCTL_FORGE_TYPE_FORGEJO, "GCTL_FORGE_TYPE_FORGEJO", "forgejo" },
    { GCTL_FORGE_TYPE_GITEA,   "GCTL_FORGE_TYPE_GITEA",   "gitea"   },
    { 0, NULL, NULL }
};

GType
gctl_forge_type_get_type(void)
{
    static gsize g_type_id = 0;

    if (g_once_init_enter(&g_type_id)) {
        GType id;

        id = g_enum_register_static("GctlForgeType", gctl_forge_type_values);
        g_once_init_leave(&g_type_id, id);
    }

    return (GType)g_type_id;
}

const gchar *
gctl_forge_type_to_string(GctlForgeType forge_type)
{
    switch (forge_type) {
    case GCTL_FORGE_TYPE_GITHUB:  return "github";
    case GCTL_FORGE_TYPE_GITLAB:  return "gitlab";
    case GCTL_FORGE_TYPE_FORGEJO: return "forgejo";
    case GCTL_FORGE_TYPE_GITEA:   return "gitea";
    default:                      return "unknown";
    }
}

GctlForgeType
gctl_forge_type_from_string(const gchar *name)
{
    if (name == NULL) return GCTL_FORGE_TYPE_UNKNOWN;
    if (g_ascii_strcasecmp(name, "github") == 0)  return GCTL_FORGE_TYPE_GITHUB;
    if (g_ascii_strcasecmp(name, "gitlab") == 0)  return GCTL_FORGE_TYPE_GITLAB;
    if (g_ascii_strcasecmp(name, "forgejo") == 0) return GCTL_FORGE_TYPE_FORGEJO;
    if (g_ascii_strcasecmp(name, "gitea") == 0)   return GCTL_FORGE_TYPE_GITEA;
    return GCTL_FORGE_TYPE_UNKNOWN;
}

/* ── GctlOutputFormat ─────────────────────────────────────────────── */

static const GEnumValue gctl_output_format_values[] = {
    { GCTL_OUTPUT_FORMAT_TABLE, "GCTL_OUTPUT_FORMAT_TABLE", "table" },
    { GCTL_OUTPUT_FORMAT_JSON,  "GCTL_OUTPUT_FORMAT_JSON",  "json"  },
    { GCTL_OUTPUT_FORMAT_YAML,  "GCTL_OUTPUT_FORMAT_YAML",  "yaml"  },
    { GCTL_OUTPUT_FORMAT_CSV,   "GCTL_OUTPUT_FORMAT_CSV",   "csv"   },
    { 0, NULL, NULL }
};

GType
gctl_output_format_get_type(void)
{
    static gsize g_type_id = 0;

    if (g_once_init_enter(&g_type_id)) {
        GType id;

        id = g_enum_register_static("GctlOutputFormat",
                                    gctl_output_format_values);
        g_once_init_leave(&g_type_id, id);
    }

    return (GType)g_type_id;
}

/* ── GctlResourceKind ─────────────────────────────────────────────── */

static const GEnumValue gctl_resource_kind_values[] = {
    { GCTL_RESOURCE_KIND_PR,      "GCTL_RESOURCE_KIND_PR",      "pr"      },
    { GCTL_RESOURCE_KIND_ISSUE,   "GCTL_RESOURCE_KIND_ISSUE",   "issue"   },
    { GCTL_RESOURCE_KIND_REPO,    "GCTL_RESOURCE_KIND_REPO",    "repo"    },
    { GCTL_RESOURCE_KIND_RELEASE, "GCTL_RESOURCE_KIND_RELEASE", "release" },
    { GCTL_RESOURCE_KIND_MIRROR,  "GCTL_RESOURCE_KIND_MIRROR",  "mirror"  },
    { 0, NULL, NULL }
};

GType
gctl_resource_kind_get_type(void)
{
    static gsize g_type_id = 0;

    if (g_once_init_enter(&g_type_id)) {
        GType id;

        id = g_enum_register_static("GctlResourceKind",
                                    gctl_resource_kind_values);
        g_once_init_leave(&g_type_id, id);
    }

    return (GType)g_type_id;
}

const gchar *
gctl_resource_kind_to_string(GctlResourceKind kind)
{
    switch (kind) {
    case GCTL_RESOURCE_KIND_PR:      return "pr";
    case GCTL_RESOURCE_KIND_ISSUE:   return "issue";
    case GCTL_RESOURCE_KIND_REPO:    return "repo";
    case GCTL_RESOURCE_KIND_RELEASE: return "release";
    case GCTL_RESOURCE_KIND_MIRROR:  return "mirror";
    default:                         return "unknown";
    }
}

/* ── GctlVerb ─────────────────────────────────────────────────────── */

static const GEnumValue gctl_verb_values[] = {
    { GCTL_VERB_LIST,     "GCTL_VERB_LIST",     "list"     },
    { GCTL_VERB_GET,      "GCTL_VERB_GET",      "get"      },
    { GCTL_VERB_CREATE,   "GCTL_VERB_CREATE",   "create"   },
    { GCTL_VERB_EDIT,     "GCTL_VERB_EDIT",     "edit"     },
    { GCTL_VERB_CLOSE,    "GCTL_VERB_CLOSE",    "close"    },
    { GCTL_VERB_REOPEN,   "GCTL_VERB_REOPEN",   "reopen"  },
    { GCTL_VERB_MERGE,    "GCTL_VERB_MERGE",    "merge"    },
    { GCTL_VERB_COMMENT,  "GCTL_VERB_COMMENT",  "comment"  },
    { GCTL_VERB_CHECKOUT, "GCTL_VERB_CHECKOUT", "checkout" },
    { GCTL_VERB_REVIEW,   "GCTL_VERB_REVIEW",   "review"   },
    { GCTL_VERB_DELETE,   "GCTL_VERB_DELETE",    "delete"   },
    { GCTL_VERB_FORK,     "GCTL_VERB_FORK",     "fork"     },
    { GCTL_VERB_CLONE,    "GCTL_VERB_CLONE",    "clone"    },
    { GCTL_VERB_BROWSE,   "GCTL_VERB_BROWSE",   "browse"   },
    { GCTL_VERB_SYNC,     "GCTL_VERB_SYNC",     "sync"     },
    { 0, NULL, NULL }
};

GType
gctl_verb_get_type(void)
{
    static gsize g_type_id = 0;

    if (g_once_init_enter(&g_type_id)) {
        GType id;

        id = g_enum_register_static("GctlVerb", gctl_verb_values);
        g_once_init_leave(&g_type_id, id);
    }

    return (GType)g_type_id;
}

const gchar *
gctl_verb_to_string(GctlVerb verb)
{
    switch (verb) {
    case GCTL_VERB_LIST:     return "list";
    case GCTL_VERB_GET:      return "get";
    case GCTL_VERB_CREATE:   return "create";
    case GCTL_VERB_EDIT:     return "edit";
    case GCTL_VERB_CLOSE:    return "close";
    case GCTL_VERB_REOPEN:   return "reopen";
    case GCTL_VERB_MERGE:    return "merge";
    case GCTL_VERB_COMMENT:  return "comment";
    case GCTL_VERB_CHECKOUT: return "checkout";
    case GCTL_VERB_REVIEW:   return "review";
    case GCTL_VERB_DELETE:   return "delete";
    case GCTL_VERB_FORK:     return "fork";
    case GCTL_VERB_CLONE:    return "clone";
    case GCTL_VERB_BROWSE:   return "browse";
    case GCTL_VERB_SYNC:     return "sync";
    default:                 return "unknown";
    }
}

gint
gctl_verb_from_string(const gchar *name)
{
    gint i;

    if (name == NULL) return -1;

    for (i = 0; gctl_verb_values[i].value_name != NULL; i++) {
        if (g_ascii_strcasecmp(name, gctl_verb_values[i].value_nick) == 0)
            return gctl_verb_values[i].value;
    }

    return -1;
}
