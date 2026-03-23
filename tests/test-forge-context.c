/*
 * test-forge-context.c - Tests for GctlForgeContext boxed type
 *
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define GCTL_COMPILATION
#include <gitctl.h>

/* test_context_new: create with all fields, verify accessors */
static void
test_context_new(void)
{
	g_autoptr(GctlForgeContext) ctx = NULL;

	ctx = gctl_forge_context_new(
		GCTL_FORGE_TYPE_GITHUB,
		"https://github.com/user/repo.git",
		"user",
		"repo",
		"github.com",
		"gh"
	);

	g_assert_nonnull(ctx);
	g_assert_cmpint(gctl_forge_context_get_forge_type(ctx),
	                ==, GCTL_FORGE_TYPE_GITHUB);
	g_assert_cmpstr(gctl_forge_context_get_remote_url(ctx),
	                ==, "https://github.com/user/repo.git");
	g_assert_cmpstr(gctl_forge_context_get_owner(ctx), ==, "user");
	g_assert_cmpstr(gctl_forge_context_get_repo_name(ctx), ==, "repo");
	g_assert_cmpstr(gctl_forge_context_get_host(ctx), ==, "github.com");
	g_assert_cmpstr(gctl_forge_context_get_cli_tool(ctx), ==, "gh");
}

/* test_context_copy: copy and verify all fields */
static void
test_context_copy(void)
{
	g_autoptr(GctlForgeContext) original = NULL;
	g_autoptr(GctlForgeContext) copy = NULL;

	original = gctl_forge_context_new(
		GCTL_FORGE_TYPE_GITLAB,
		"git@gitlab.com:ns/project.git",
		"ns",
		"project",
		"gitlab.com",
		"glab"
	);

	copy = gctl_forge_context_copy(original);

	g_assert_nonnull(copy);
	g_assert_cmpint(gctl_forge_context_get_forge_type(copy),
	                ==, GCTL_FORGE_TYPE_GITLAB);
	g_assert_cmpstr(gctl_forge_context_get_remote_url(copy),
	                ==, "git@gitlab.com:ns/project.git");
	g_assert_cmpstr(gctl_forge_context_get_owner(copy), ==, "ns");
	g_assert_cmpstr(gctl_forge_context_get_repo_name(copy), ==, "project");
	g_assert_cmpstr(gctl_forge_context_get_host(copy), ==, "gitlab.com");
	g_assert_cmpstr(gctl_forge_context_get_cli_tool(copy), ==, "glab");
}

/* test_context_free: create and free (no crash) */
static void
test_context_free(void)
{
	GctlForgeContext *ctx;

	ctx = gctl_forge_context_new(
		GCTL_FORGE_TYPE_FORGEJO,
		"https://codeberg.org/user/repo",
		"user",
		"repo",
		"codeberg.org",
		"fj"
	);

	/* Should not crash */
	gctl_forge_context_free(ctx);

	/* Freeing NULL should also be safe */
	gctl_forge_context_free(NULL);
}

/* test_context_null_fields: create with some NULL fields */
static void
test_context_null_fields(void)
{
	g_autoptr(GctlForgeContext) ctx = NULL;

	ctx = gctl_forge_context_new(
		GCTL_FORGE_TYPE_GITHUB,
		NULL,   /* no remote URL */
		NULL,   /* no owner */
		NULL,   /* no repo name */
		NULL,   /* no host */
		NULL    /* no CLI tool */
	);

	g_assert_nonnull(ctx);
	g_assert_cmpint(gctl_forge_context_get_forge_type(ctx),
	                ==, GCTL_FORGE_TYPE_GITHUB);
	g_assert_null(gctl_forge_context_get_remote_url(ctx));
	g_assert_null(gctl_forge_context_get_owner(ctx));
	g_assert_null(gctl_forge_context_get_repo_name(ctx));
	g_assert_null(gctl_forge_context_get_host(ctx));
	g_assert_null(gctl_forge_context_get_cli_tool(ctx));
}

/* test_context_owner_repo: verify get_owner_repo returns "owner/repo" */
static void
test_context_owner_repo(void)
{
	g_autoptr(GctlForgeContext) ctx = NULL;
	g_autofree gchar *owner_repo = NULL;

	ctx = gctl_forge_context_new(
		GCTL_FORGE_TYPE_GITHUB,
		"https://github.com/myorg/myrepo.git",
		"myorg",
		"myrepo",
		"github.com",
		"gh"
	);

	owner_repo = gctl_forge_context_get_owner_repo(ctx);
	g_assert_cmpstr(owner_repo, ==, "myorg/myrepo");
}

/*
 * test_context_owner_repo_null: verify get_owner_repo with NULL owner
 * returns "/repo" (empty owner portion)
 */
static void
test_context_owner_repo_null(void)
{
	g_autoptr(GctlForgeContext) ctx = NULL;
	g_autofree gchar *owner_repo = NULL;

	ctx = gctl_forge_context_new(
		GCTL_FORGE_TYPE_GITHUB,
		NULL,
		NULL,    /* NULL owner */
		"repo",
		NULL,
		NULL
	);

	owner_repo = gctl_forge_context_get_owner_repo(ctx);
	g_assert_cmpstr(owner_repo, ==, "/repo");
}

int
main(
	int     argc,
	char  **argv
){
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/forge-context/new", test_context_new);
	g_test_add_func("/forge-context/copy", test_context_copy);
	g_test_add_func("/forge-context/free", test_context_free);
	g_test_add_func("/forge-context/null-fields", test_context_null_fields);
	g_test_add_func("/forge-context/owner-repo", test_context_owner_repo);
	g_test_add_func("/forge-context/owner-repo-null", test_context_owner_repo_null);

	return g_test_run();
}
