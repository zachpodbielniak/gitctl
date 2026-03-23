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

/* test_resource_description: set/get description */
static void
test_resource_description(void)
{
	g_autoptr(GctlResource) res = NULL;

	res = gctl_resource_new(GCTL_RESOURCE_KIND_REPO);

	/* Initially NULL */
	g_assert_null(gctl_resource_get_description(res));

	gctl_resource_set_description(res, "A test repository");
	g_assert_cmpstr(gctl_resource_get_description(res),
	                ==, "A test repository");

	/* Overwrite */
	gctl_resource_set_description(res, "Updated description");
	g_assert_cmpstr(gctl_resource_get_description(res),
	                ==, "Updated description");

	/* Set to NULL */
	gctl_resource_set_description(res, NULL);
	g_assert_null(gctl_resource_get_description(res));
}

/* test_resource_url: set/get url */
static void
test_resource_url(void)
{
	g_autoptr(GctlResource) res = NULL;

	res = gctl_resource_new(GCTL_RESOURCE_KIND_PR);

	/* Initially NULL */
	g_assert_null(gctl_resource_get_url(res));

	gctl_resource_set_url(res, "https://github.com/user/repo/pull/1");
	g_assert_cmpstr(gctl_resource_get_url(res),
	                ==, "https://github.com/user/repo/pull/1");

	/* Overwrite */
	gctl_resource_set_url(res, "https://github.com/user/repo/pull/2");
	g_assert_cmpstr(gctl_resource_get_url(res),
	                ==, "https://github.com/user/repo/pull/2");
}

/* test_resource_created_at: set/get created_at */
static void
test_resource_created_at(void)
{
	g_autoptr(GctlResource) res = NULL;

	res = gctl_resource_new(GCTL_RESOURCE_KIND_RELEASE);

	/* Initially NULL */
	g_assert_null(gctl_resource_get_created_at(res));

	gctl_resource_set_created_at(res, "2026-03-23T10:00:00Z");
	g_assert_cmpstr(gctl_resource_get_created_at(res),
	                ==, "2026-03-23T10:00:00Z");

	/* Set to NULL */
	gctl_resource_set_created_at(res, NULL);
	g_assert_null(gctl_resource_get_created_at(res));
}

/* test_resource_updated_at: set/get updated_at */
static void
test_resource_updated_at(void)
{
	g_autoptr(GctlResource) res = NULL;

	res = gctl_resource_new(GCTL_RESOURCE_KIND_ISSUE);

	/* Initially NULL */
	g_assert_null(gctl_resource_get_updated_at(res));

	gctl_resource_set_updated_at(res, "2026-03-23T12:00:00Z");
	g_assert_cmpstr(gctl_resource_get_updated_at(res),
	                ==, "2026-03-23T12:00:00Z");

	/* Overwrite */
	gctl_resource_set_updated_at(res, "2026-03-24T08:00:00Z");
	g_assert_cmpstr(gctl_resource_get_updated_at(res),
	                ==, "2026-03-24T08:00:00Z");
}

/* test_resource_extra_overwrite: set extra key twice, verify second value */
static void
test_resource_extra_overwrite(void)
{
	g_autoptr(GctlResource) res = NULL;

	res = gctl_resource_new(GCTL_RESOURCE_KIND_PR);

	gctl_resource_set_extra(res, "branch", "main");
	g_assert_cmpstr(gctl_resource_get_extra(res, "branch"), ==, "main");

	gctl_resource_set_extra(res, "branch", "develop");
	g_assert_cmpstr(gctl_resource_get_extra(res, "branch"), ==, "develop");
}

/* test_resource_extra_null_value: set_extra with NULL value removes key */
static void
test_resource_extra_null_value(void)
{
	g_autoptr(GctlResource) res = NULL;

	res = gctl_resource_new(GCTL_RESOURCE_KIND_ISSUE);

	gctl_resource_set_extra(res, "label", "bug");
	g_assert_cmpstr(gctl_resource_get_extra(res, "label"), ==, "bug");

	/* Setting NULL should remove the key */
	gctl_resource_set_extra(res, "label", NULL);
	g_assert_null(gctl_resource_get_extra(res, "label"));
}

/* test_resource_copy_with_extra: create with extras, copy, verify extras copied */
static void
test_resource_copy_with_extra(void)
{
	g_autoptr(GctlResource) original = NULL;
	g_autoptr(GctlResource) copy = NULL;

	original = gctl_resource_new(GCTL_RESOURCE_KIND_CI);
	gctl_resource_set_title(original, "Pipeline #123");
	gctl_resource_set_extra(original, "run_id", "12345");
	gctl_resource_set_extra(original, "branch", "feature-x");
	gctl_resource_set_extra(original, "status", "success");

	copy = gctl_resource_copy(original);
	g_assert_nonnull(copy);

	/* Verify extras are copied */
	g_assert_cmpstr(gctl_resource_get_extra(copy, "run_id"), ==, "12345");
	g_assert_cmpstr(gctl_resource_get_extra(copy, "branch"), ==, "feature-x");
	g_assert_cmpstr(gctl_resource_get_extra(copy, "status"), ==, "success");

	/* Verify the copy is independent */
	gctl_resource_set_extra(original, "status", "failed");
	g_assert_cmpstr(gctl_resource_get_extra(copy, "status"), ==, "success");
}

/* test_resource_all_kinds: create one of each resource kind, verify */
static void
test_resource_all_kinds(void)
{
	GctlResourceKind kinds[] = {
		GCTL_RESOURCE_KIND_PR,
		GCTL_RESOURCE_KIND_ISSUE,
		GCTL_RESOURCE_KIND_REPO,
		GCTL_RESOURCE_KIND_RELEASE,
		GCTL_RESOURCE_KIND_MIRROR,
		GCTL_RESOURCE_KIND_CI,
		GCTL_RESOURCE_KIND_COMMIT,
		GCTL_RESOURCE_KIND_LABEL,
		GCTL_RESOURCE_KIND_NOTIFICATION,
		GCTL_RESOURCE_KIND_KEY,
		GCTL_RESOURCE_KIND_WEBHOOK,
	};
	guint i;

	for (i = 0; i < G_N_ELEMENTS(kinds); i++) {
		g_autoptr(GctlResource) res = NULL;

		res = gctl_resource_new(kinds[i]);
		g_assert_nonnull(res);
		g_assert_cmpint(gctl_resource_get_kind(res), ==, kinds[i]);
	}
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
	g_test_add_func("/resource/description", test_resource_description);
	g_test_add_func("/resource/url", test_resource_url);
	g_test_add_func("/resource/created-at", test_resource_created_at);
	g_test_add_func("/resource/updated-at", test_resource_updated_at);
	g_test_add_func("/resource/extra-overwrite", test_resource_extra_overwrite);
	g_test_add_func("/resource/extra-null-value", test_resource_extra_null_value);
	g_test_add_func("/resource/copy-with-extra", test_resource_copy_with_extra);
	g_test_add_func("/resource/all-kinds", test_resource_all_kinds);

	return g_test_run();
}
