/*
 * test-resource.c - Tests for GctlResource boxed type
 *
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define GCTL_COMPILATION
#include <gitctl.h>

/* test_resource_new: create a resource, verify kind */
static void
test_resource_new(void)
{
	g_autoptr(GctlResource) res = NULL;

	res = gctl_resource_new(GCTL_RESOURCE_KIND_PR);
	g_assert_nonnull(res);
	g_assert_cmpint(gctl_resource_get_kind(res), ==, GCTL_RESOURCE_KIND_PR);

	/* Verify default values */
	g_assert_cmpint(gctl_resource_get_number(res), ==, 0);
	g_assert_null(gctl_resource_get_title(res));
	g_assert_null(gctl_resource_get_state(res));
}

/* test_resource_set_get_title: set title, verify get */
static void
test_resource_set_get_title(void)
{
	g_autoptr(GctlResource) res = NULL;

	res = gctl_resource_new(GCTL_RESOURCE_KIND_ISSUE);

	gctl_resource_set_title(res, "Fix segfault on startup");
	g_assert_cmpstr(gctl_resource_get_title(res), ==, "Fix segfault on startup");

	/* Overwrite with a different value */
	gctl_resource_set_title(res, "Updated title");
	g_assert_cmpstr(gctl_resource_get_title(res), ==, "Updated title");

	/* Set to NULL */
	gctl_resource_set_title(res, NULL);
	g_assert_null(gctl_resource_get_title(res));
}

/* test_resource_set_get_number: set number, verify get */
static void
test_resource_set_get_number(void)
{
	g_autoptr(GctlResource) res = NULL;

	res = gctl_resource_new(GCTL_RESOURCE_KIND_PR);

	gctl_resource_set_number(res, 42);
	g_assert_cmpint(gctl_resource_get_number(res), ==, 42);

	gctl_resource_set_number(res, 0);
	g_assert_cmpint(gctl_resource_get_number(res), ==, 0);

	gctl_resource_set_number(res, 9999);
	g_assert_cmpint(gctl_resource_get_number(res), ==, 9999);
}

/* test_resource_set_get_state: set state, verify get */
static void
test_resource_set_get_state(void)
{
	g_autoptr(GctlResource) res = NULL;

	res = gctl_resource_new(GCTL_RESOURCE_KIND_PR);

	gctl_resource_set_state(res, "open");
	g_assert_cmpstr(gctl_resource_get_state(res), ==, "open");

	gctl_resource_set_state(res, "closed");
	g_assert_cmpstr(gctl_resource_get_state(res), ==, "closed");

	gctl_resource_set_state(res, "merged");
	g_assert_cmpstr(gctl_resource_get_state(res), ==, "merged");
}

/* test_resource_extra: set_extra "key"="val", verify get_extra returns "val" */
static void
test_resource_extra(void)
{
	g_autoptr(GctlResource) res = NULL;

	res = gctl_resource_new(GCTL_RESOURCE_KIND_ISSUE);

	gctl_resource_set_extra(res, "key", "val");
	g_assert_cmpstr(gctl_resource_get_extra(res, "key"), ==, "val");

	/* Overwrite */
	gctl_resource_set_extra(res, "key", "new-val");
	g_assert_cmpstr(gctl_resource_get_extra(res, "key"), ==, "new-val");

	/* Add a second key */
	gctl_resource_set_extra(res, "labels", "bug,critical");
	g_assert_cmpstr(gctl_resource_get_extra(res, "labels"), ==, "bug,critical");

	/* Nonexistent key returns NULL */
	g_assert_null(gctl_resource_get_extra(res, "nonexistent"));

	/* Remove key by setting NULL */
	gctl_resource_set_extra(res, "key", NULL);
	g_assert_null(gctl_resource_get_extra(res, "key"));
}

/* test_resource_copy: create, set fields, copy, verify copy has same values */
static void
test_resource_copy(void)
{
	g_autoptr(GctlResource) original = NULL;
	g_autoptr(GctlResource) copy = NULL;

	original = gctl_resource_new(GCTL_RESOURCE_KIND_RELEASE);
	gctl_resource_set_number(original, 7);
	gctl_resource_set_title(original, "v1.0.0");
	gctl_resource_set_state(original, "published");
	gctl_resource_set_author(original, "zach");
	gctl_resource_set_url(original, "https://github.com/user/repo/releases/tag/v1.0.0");
	gctl_resource_set_extra(original, "tag", "v1.0.0");

	copy = gctl_resource_copy(original);
	g_assert_nonnull(copy);

	/* Verify all fields match */
	g_assert_cmpint(gctl_resource_get_kind(copy), ==, GCTL_RESOURCE_KIND_RELEASE);
	g_assert_cmpint(gctl_resource_get_number(copy), ==, 7);
	g_assert_cmpstr(gctl_resource_get_title(copy), ==, "v1.0.0");
	g_assert_cmpstr(gctl_resource_get_state(copy), ==, "published");
	g_assert_cmpstr(gctl_resource_get_author(copy), ==, "zach");
	g_assert_cmpstr(gctl_resource_get_url(copy), ==,
	                "https://github.com/user/repo/releases/tag/v1.0.0");
	g_assert_cmpstr(gctl_resource_get_extra(copy, "tag"), ==, "v1.0.0");

	/* Verify the copy is independent -- mutating original does not affect copy */
	gctl_resource_set_title(original, "CHANGED");
	g_assert_cmpstr(gctl_resource_get_title(copy), ==, "v1.0.0");
}

/* test_resource_free: create and free (just verify no crash) */
static void
test_resource_free(void)
{
	GctlResource *res;

	res = gctl_resource_new(GCTL_RESOURCE_KIND_REPO);
	gctl_resource_set_title(res, "test-repo");
	gctl_resource_set_description(res, "A test repository");
	gctl_resource_set_extra(res, "visibility", "public");

	/* Should not crash */
	gctl_resource_free(res);

	/* Freeing NULL should also be safe */
	gctl_resource_free(NULL);
}

int
main(
	int     argc,
	char  **argv
){
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/resource/new", test_resource_new);
	g_test_add_func("/resource/set-get-title", test_resource_set_get_title);
	g_test_add_func("/resource/set-get-number", test_resource_set_get_number);
	g_test_add_func("/resource/set-get-state", test_resource_set_get_state);
	g_test_add_func("/resource/extra", test_resource_extra);
	g_test_add_func("/resource/copy", test_resource_copy);
	g_test_add_func("/resource/free", test_resource_free);

	return g_test_run();
}
