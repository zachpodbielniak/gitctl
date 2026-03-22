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

	return g_test_run();
}
