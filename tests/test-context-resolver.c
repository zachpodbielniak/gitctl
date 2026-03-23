/*
 * test-context-resolver.c - Tests for GctlContextResolver
 *
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define GCTL_COMPILATION
#include <gitctl.h>

/* test_resolver_new: create with config, verify non-null */
static void
test_resolver_new(void)
{
	g_autoptr(GctlConfig) config = NULL;
	g_autoptr(GctlContextResolver) resolver = NULL;

	config = gctl_config_new();
	resolver = gctl_context_resolver_new(config);

	g_assert_nonnull(resolver);
	g_assert_true(GCTL_IS_CONTEXT_RESOLVER(resolver));
}

/* test_resolver_forced_forge: set forced forge, verify get returns it */
static void
test_resolver_forced_forge(void)
{
	g_autoptr(GctlConfig) config = NULL;
	g_autoptr(GctlContextResolver) resolver = NULL;

	config = gctl_config_new();
	resolver = gctl_context_resolver_new(config);

	/* Default should be UNKNOWN */
	g_assert_cmpint(gctl_context_resolver_get_forced_forge(resolver),
	                ==, GCTL_FORGE_TYPE_UNKNOWN);

	/* Set to GITLAB, verify */
	gctl_context_resolver_set_forced_forge(resolver, GCTL_FORGE_TYPE_GITLAB);
	g_assert_cmpint(gctl_context_resolver_get_forced_forge(resolver),
	                ==, GCTL_FORGE_TYPE_GITLAB);

	/* Set to FORGEJO, verify */
	gctl_context_resolver_set_forced_forge(resolver, GCTL_FORGE_TYPE_FORGEJO);
	g_assert_cmpint(gctl_context_resolver_get_forced_forge(resolver),
	                ==, GCTL_FORGE_TYPE_FORGEJO);

	/* Clear by setting to UNKNOWN */
	gctl_context_resolver_set_forced_forge(resolver, GCTL_FORGE_TYPE_UNKNOWN);
	g_assert_cmpint(gctl_context_resolver_get_forced_forge(resolver),
	                ==, GCTL_FORGE_TYPE_UNKNOWN);
}

/*
 * test_resolver_forced_repo: set forced repo, verify owner/repo
 * override is applied when resolve is called.
 *
 * We cannot easily test resolve() directly since it requires a git
 * remote, but we can verify the forced forge + forced repo produces
 * a valid context by using a forced forge (which uses default host
 * when the remote URL fails).
 */
static void
test_resolver_forced_repo(void)
{
	g_autoptr(GctlConfig) config = NULL;
	g_autoptr(GctlContextResolver) resolver = NULL;
	g_autoptr(GctlForgeContext) ctx = NULL;
	g_autoptr(GError) error = NULL;

	config = gctl_config_new();
	resolver = gctl_context_resolver_new(config);

	/* Force both forge and repo so resolve works without a real git remote */
	gctl_context_resolver_set_forced_forge(resolver, GCTL_FORGE_TYPE_GITHUB);
	gctl_context_resolver_set_forced_repo(resolver, "testowner", "testrepo");

	/*
	 * Resolve with a bogus remote name.  Since forge is forced, the
	 * resolver will use the config's default host for GitHub and
	 * override owner/repo with our forced values.
	 */
	ctx = gctl_context_resolver_resolve(resolver, "nonexistent-remote", &error);

	g_assert_no_error(error);
	g_assert_nonnull(ctx);
	g_assert_cmpstr(gctl_forge_context_get_owner(ctx), ==, "testowner");
	g_assert_cmpstr(gctl_forge_context_get_repo_name(ctx), ==, "testrepo");
	g_assert_cmpint(gctl_forge_context_get_forge_type(ctx),
	                ==, GCTL_FORGE_TYPE_GITHUB);
	g_assert_cmpstr(gctl_forge_context_get_host(ctx), ==, "github.com");
}

/*
 * test_resolver_resolve_no_remote: resolve with invalid remote, forced
 * forge set.  Should return context with the default host.
 */
static void
test_resolver_resolve_no_remote(void)
{
	g_autoptr(GctlConfig) config = NULL;
	g_autoptr(GctlContextResolver) resolver = NULL;
	g_autoptr(GctlForgeContext) ctx = NULL;
	g_autoptr(GError) error = NULL;

	config = gctl_config_new();
	resolver = gctl_context_resolver_new(config);

	/* Force forge so the resolver does not fail on missing remote */
	gctl_context_resolver_set_forced_forge(resolver, GCTL_FORGE_TYPE_GITLAB);

	ctx = gctl_context_resolver_resolve(resolver, "bogus-remote", &error);

	g_assert_no_error(error);
	g_assert_nonnull(ctx);
	g_assert_cmpint(gctl_forge_context_get_forge_type(ctx),
	                ==, GCTL_FORGE_TYPE_GITLAB);
	g_assert_cmpstr(gctl_forge_context_get_host(ctx), ==, "gitlab.com");
}

/*
 * test_resolver_resolve_no_remote_no_forge: resolve with invalid
 * remote, no forced forge.  Should fail with an error.
 */
static void
test_resolver_resolve_no_remote_no_forge(void)
{
	g_autoptr(GctlConfig) config = NULL;
	g_autoptr(GctlContextResolver) resolver = NULL;
	g_autoptr(GctlForgeContext) ctx = NULL;
	g_autoptr(GError) error = NULL;

	config = gctl_config_new();
	resolver = gctl_context_resolver_new(config);

	/* No forced forge -- should fail when remote does not exist */
	ctx = gctl_context_resolver_resolve(resolver, "bogus-remote", &error);

	g_assert_null(ctx);
	g_assert_nonnull(error);
}

int
main(
	int     argc,
	char  **argv
){
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/context-resolver/new", test_resolver_new);
	g_test_add_func("/context-resolver/forced-forge", test_resolver_forced_forge);
	g_test_add_func("/context-resolver/forced-repo", test_resolver_forced_repo);
	g_test_add_func("/context-resolver/resolve-no-remote", test_resolver_resolve_no_remote);
	g_test_add_func("/context-resolver/resolve-no-remote-no-forge", test_resolver_resolve_no_remote_no_forge);

	return g_test_run();
}
