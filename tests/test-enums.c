/*
 * test-enums.c - Tests for enum helpers
 *
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define GCTL_COMPILATION
#include <gitctl.h>

/* test_forge_type_to_string: verify all conversions */
static void
test_forge_type_to_string(void)
{
	g_assert_cmpstr(gctl_forge_type_to_string(GCTL_FORGE_TYPE_GITHUB),  ==, "github");
	g_assert_cmpstr(gctl_forge_type_to_string(GCTL_FORGE_TYPE_GITLAB),  ==, "gitlab");
	g_assert_cmpstr(gctl_forge_type_to_string(GCTL_FORGE_TYPE_FORGEJO), ==, "forgejo");
	g_assert_cmpstr(gctl_forge_type_to_string(GCTL_FORGE_TYPE_GITEA),   ==, "gitea");
	g_assert_cmpstr(gctl_forge_type_to_string(GCTL_FORGE_TYPE_UNKNOWN), ==, "unknown");

	/* Out-of-range value should also return "unknown" */
	g_assert_cmpstr(gctl_forge_type_to_string((GctlForgeType)99), ==, "unknown");
}

/* test_forge_type_from_string: verify all conversions including case insensitivity */
static void
test_forge_type_from_string(void)
{
	/* Exact lowercase */
	g_assert_cmpint(gctl_forge_type_from_string("github"),  ==, GCTL_FORGE_TYPE_GITHUB);
	g_assert_cmpint(gctl_forge_type_from_string("gitlab"),  ==, GCTL_FORGE_TYPE_GITLAB);
	g_assert_cmpint(gctl_forge_type_from_string("forgejo"), ==, GCTL_FORGE_TYPE_FORGEJO);
	g_assert_cmpint(gctl_forge_type_from_string("gitea"),   ==, GCTL_FORGE_TYPE_GITEA);

	/* Case insensitivity */
	g_assert_cmpint(gctl_forge_type_from_string("GitHub"),  ==, GCTL_FORGE_TYPE_GITHUB);
	g_assert_cmpint(gctl_forge_type_from_string("GITLAB"),  ==, GCTL_FORGE_TYPE_GITLAB);
	g_assert_cmpint(gctl_forge_type_from_string("Forgejo"), ==, GCTL_FORGE_TYPE_FORGEJO);
	g_assert_cmpint(gctl_forge_type_from_string("GITEA"),   ==, GCTL_FORGE_TYPE_GITEA);

	/* Unknown strings */
	g_assert_cmpint(gctl_forge_type_from_string("bitbucket"), ==, GCTL_FORGE_TYPE_UNKNOWN);
	g_assert_cmpint(gctl_forge_type_from_string(""),          ==, GCTL_FORGE_TYPE_UNKNOWN);
	g_assert_cmpint(gctl_forge_type_from_string(NULL),        ==, GCTL_FORGE_TYPE_UNKNOWN);
}

/* test_verb_to_string: verify all verb conversions */
static void
test_verb_to_string(void)
{
	g_assert_cmpstr(gctl_verb_to_string(GCTL_VERB_LIST),     ==, "list");
	g_assert_cmpstr(gctl_verb_to_string(GCTL_VERB_GET),      ==, "get");
	g_assert_cmpstr(gctl_verb_to_string(GCTL_VERB_CREATE),   ==, "create");
	g_assert_cmpstr(gctl_verb_to_string(GCTL_VERB_EDIT),     ==, "edit");
	g_assert_cmpstr(gctl_verb_to_string(GCTL_VERB_CLOSE),    ==, "close");
	g_assert_cmpstr(gctl_verb_to_string(GCTL_VERB_REOPEN),   ==, "reopen");
	g_assert_cmpstr(gctl_verb_to_string(GCTL_VERB_MERGE),    ==, "merge");
	g_assert_cmpstr(gctl_verb_to_string(GCTL_VERB_COMMENT),  ==, "comment");
	g_assert_cmpstr(gctl_verb_to_string(GCTL_VERB_CHECKOUT), ==, "checkout");
	g_assert_cmpstr(gctl_verb_to_string(GCTL_VERB_REVIEW),   ==, "review");
	g_assert_cmpstr(gctl_verb_to_string(GCTL_VERB_DELETE),   ==, "delete");
	g_assert_cmpstr(gctl_verb_to_string(GCTL_VERB_FORK),     ==, "fork");
	g_assert_cmpstr(gctl_verb_to_string(GCTL_VERB_CLONE),    ==, "clone");
	g_assert_cmpstr(gctl_verb_to_string(GCTL_VERB_BROWSE),   ==, "browse");

	/* Out-of-range value should return "unknown" */
	g_assert_cmpstr(gctl_verb_to_string((GctlVerb)99), ==, "unknown");
}

/* test_verb_from_string: verify conversions, verify -1 for unknown */
static void
test_verb_from_string(void)
{
	/* Exact lowercase */
	g_assert_cmpint(gctl_verb_from_string("list"),     ==, GCTL_VERB_LIST);
	g_assert_cmpint(gctl_verb_from_string("get"),      ==, GCTL_VERB_GET);
	g_assert_cmpint(gctl_verb_from_string("create"),   ==, GCTL_VERB_CREATE);
	g_assert_cmpint(gctl_verb_from_string("edit"),     ==, GCTL_VERB_EDIT);
	g_assert_cmpint(gctl_verb_from_string("close"),    ==, GCTL_VERB_CLOSE);
	g_assert_cmpint(gctl_verb_from_string("reopen"),   ==, GCTL_VERB_REOPEN);
	g_assert_cmpint(gctl_verb_from_string("merge"),    ==, GCTL_VERB_MERGE);
	g_assert_cmpint(gctl_verb_from_string("comment"),  ==, GCTL_VERB_COMMENT);
	g_assert_cmpint(gctl_verb_from_string("checkout"), ==, GCTL_VERB_CHECKOUT);
	g_assert_cmpint(gctl_verb_from_string("review"),   ==, GCTL_VERB_REVIEW);
	g_assert_cmpint(gctl_verb_from_string("delete"),   ==, GCTL_VERB_DELETE);
	g_assert_cmpint(gctl_verb_from_string("fork"),     ==, GCTL_VERB_FORK);
	g_assert_cmpint(gctl_verb_from_string("clone"),    ==, GCTL_VERB_CLONE);
	g_assert_cmpint(gctl_verb_from_string("browse"),   ==, GCTL_VERB_BROWSE);

	/* Case insensitivity */
	g_assert_cmpint(gctl_verb_from_string("LIST"),     ==, GCTL_VERB_LIST);
	g_assert_cmpint(gctl_verb_from_string("Create"),   ==, GCTL_VERB_CREATE);
	g_assert_cmpint(gctl_verb_from_string("MERGE"),    ==, GCTL_VERB_MERGE);

	/* Unknown strings return -1 */
	g_assert_cmpint(gctl_verb_from_string("destroy"), ==, -1);
	g_assert_cmpint(gctl_verb_from_string(""),        ==, -1);
	g_assert_cmpint(gctl_verb_from_string(NULL),      ==, -1);
}

/* test_resource_kind_to_string: verify all resource kind conversions */
static void
test_resource_kind_to_string(void)
{
	g_assert_cmpstr(gctl_resource_kind_to_string(GCTL_RESOURCE_KIND_PR),           ==, "pr");
	g_assert_cmpstr(gctl_resource_kind_to_string(GCTL_RESOURCE_KIND_ISSUE),        ==, "issue");
	g_assert_cmpstr(gctl_resource_kind_to_string(GCTL_RESOURCE_KIND_REPO),         ==, "repo");
	g_assert_cmpstr(gctl_resource_kind_to_string(GCTL_RESOURCE_KIND_RELEASE),      ==, "release");
	g_assert_cmpstr(gctl_resource_kind_to_string(GCTL_RESOURCE_KIND_MIRROR),       ==, "mirror");
	g_assert_cmpstr(gctl_resource_kind_to_string(GCTL_RESOURCE_KIND_CI),           ==, "ci");
	g_assert_cmpstr(gctl_resource_kind_to_string(GCTL_RESOURCE_KIND_COMMIT),       ==, "commit");
	g_assert_cmpstr(gctl_resource_kind_to_string(GCTL_RESOURCE_KIND_LABEL),        ==, "label");
	g_assert_cmpstr(gctl_resource_kind_to_string(GCTL_RESOURCE_KIND_NOTIFICATION), ==, "notification");
	g_assert_cmpstr(gctl_resource_kind_to_string(GCTL_RESOURCE_KIND_KEY),          ==, "key");
	g_assert_cmpstr(gctl_resource_kind_to_string(GCTL_RESOURCE_KIND_WEBHOOK),      ==, "webhook");

	/* Out-of-range value should return "unknown" */
	g_assert_cmpstr(gctl_resource_kind_to_string((GctlResourceKind)99), ==, "unknown");
}

/* test_resource_kind_gtype: verify the GType is properly registered */
static void
test_resource_kind_gtype(void)
{
	GType type;

	type = GCTL_TYPE_RESOURCE_KIND;
	g_assert_true(G_TYPE_IS_ENUM(type));
}

/* test_verb_new_values: verify to_string for the new verb values */
static void
test_verb_new_values(void)
{
	g_assert_cmpstr(gctl_verb_to_string(GCTL_VERB_SYNC),    ==, "sync");
	g_assert_cmpstr(gctl_verb_to_string(GCTL_VERB_DIFF),    ==, "diff");
	g_assert_cmpstr(gctl_verb_to_string(GCTL_VERB_LOG),     ==, "log");
	g_assert_cmpstr(gctl_verb_to_string(GCTL_VERB_READ),    ==, "read");
	g_assert_cmpstr(gctl_verb_to_string(GCTL_VERB_STAR),    ==, "star");
	g_assert_cmpstr(gctl_verb_to_string(GCTL_VERB_UNSTAR),  ==, "unstar");
	g_assert_cmpstr(gctl_verb_to_string(GCTL_VERB_MIGRATE), ==, "migrate");
}

/* test_verb_new_from_string: verify from_string for the new verb values */
static void
test_verb_new_from_string(void)
{
	g_assert_cmpint(gctl_verb_from_string("sync"),    ==, GCTL_VERB_SYNC);
	g_assert_cmpint(gctl_verb_from_string("diff"),    ==, GCTL_VERB_DIFF);
	g_assert_cmpint(gctl_verb_from_string("log"),     ==, GCTL_VERB_LOG);
	g_assert_cmpint(gctl_verb_from_string("read"),    ==, GCTL_VERB_READ);
	g_assert_cmpint(gctl_verb_from_string("star"),    ==, GCTL_VERB_STAR);
	g_assert_cmpint(gctl_verb_from_string("unstar"),  ==, GCTL_VERB_UNSTAR);
	g_assert_cmpint(gctl_verb_from_string("migrate"), ==, GCTL_VERB_MIGRATE);

	/* Case insensitivity for new verbs */
	g_assert_cmpint(gctl_verb_from_string("SYNC"),    ==, GCTL_VERB_SYNC);
	g_assert_cmpint(gctl_verb_from_string("Diff"),    ==, GCTL_VERB_DIFF);
	g_assert_cmpint(gctl_verb_from_string("MIGRATE"), ==, GCTL_VERB_MIGRATE);
}

/* test_forge_type_unknown: verify UNKNOWN string is "unknown" */
static void
test_forge_type_unknown(void)
{
	g_assert_cmpstr(gctl_forge_type_to_string(GCTL_FORGE_TYPE_UNKNOWN),
	                ==, "unknown");
	g_assert_cmpint(gctl_forge_type_from_string("unknown"),
	                ==, GCTL_FORGE_TYPE_UNKNOWN);
}

/*
 * test_verb_unknown_garbage: verify -1 from_string for garbage input
 * including completely nonsensical strings
 */
static void
test_verb_unknown_garbage(void)
{
	g_assert_cmpint(gctl_verb_from_string("xyzzy"),     ==, -1);
	g_assert_cmpint(gctl_verb_from_string("123"),       ==, -1);
	g_assert_cmpint(gctl_verb_from_string("list-all"),  ==, -1);
	g_assert_cmpint(gctl_verb_from_string(" list"),     ==, -1);
}

/* test_forge_type_gtype: verify the GType is properly registered */
static void
test_forge_type_gtype(void)
{
	GType type;

	type = GCTL_TYPE_FORGE_TYPE;
	g_assert_true(G_TYPE_IS_ENUM(type));
}

/* test_verb_gtype: verify the GType is properly registered */
static void
test_verb_gtype(void)
{
	GType type;

	type = GCTL_TYPE_VERB;
	g_assert_true(G_TYPE_IS_ENUM(type));
}

/* test_output_format_gtype: verify the GType is properly registered */
static void
test_output_format_gtype(void)
{
	GType type;

	type = GCTL_TYPE_OUTPUT_FORMAT;
	g_assert_true(G_TYPE_IS_ENUM(type));
}

int
main(
	int     argc,
	char  **argv
){
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/enums/forge-type-to-string", test_forge_type_to_string);
	g_test_add_func("/enums/forge-type-from-string", test_forge_type_from_string);
	g_test_add_func("/enums/verb-to-string", test_verb_to_string);
	g_test_add_func("/enums/verb-from-string", test_verb_from_string);
	g_test_add_func("/enums/resource-kind-to-string", test_resource_kind_to_string);
	g_test_add_func("/enums/resource-kind-gtype", test_resource_kind_gtype);
	g_test_add_func("/enums/verb-new-values", test_verb_new_values);
	g_test_add_func("/enums/verb-new-from-string", test_verb_new_from_string);
	g_test_add_func("/enums/forge-type-unknown", test_forge_type_unknown);
	g_test_add_func("/enums/verb-unknown-garbage", test_verb_unknown_garbage);
	g_test_add_func("/enums/forge-type-gtype", test_forge_type_gtype);
	g_test_add_func("/enums/verb-gtype", test_verb_gtype);
	g_test_add_func("/enums/output-format-gtype", test_output_format_gtype);

	return g_test_run();
}
