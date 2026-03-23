/*
 * test-context-resolver.c - Tests for GctlContextResolver
 *
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define GCTL_COMPILATION
#include <gitctl.h>

#include <gio/gio.h>
#include <string.h>

/* ── Existing tests ───────────────────────────────────────────────── */

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

/* ── Helper: set up a temp git repo with a specific remote URL ──── */

/*
 * setup_temp_repo:
 * @remote_url: the URL to set as the "origin" remote
 * @out_tmpdir: (out) (transfer full): the path to the temp dir
 * @out_git_dir: (out) (transfer full): the path to the .git dir
 *
 * Creates a temporary directory, runs `git init` and
 * `git remote add origin <remote_url>` in it.
 *
 * Returns: %TRUE on success
 */
static gboolean
setup_temp_repo(
	const gchar  *remote_url,
	gchar       **out_tmpdir,
	gchar       **out_git_dir
){
	g_autoptr(GError) err = NULL;
	g_autoptr(GSubprocess) init_proc = NULL;
	g_autoptr(GSubprocess) remote_proc = NULL;
	gchar *tmpdir;

	tmpdir = g_dir_make_tmp("gitctl-test-XXXXXX", &err);
	if (tmpdir == NULL) {
		g_warning("Failed to create temp dir: %s", err->message);
		return FALSE;
	}

	/* git init */
	init_proc = g_subprocess_new(
		G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE,
		&err, "git", "-C", tmpdir, "init", NULL);
	if (init_proc == NULL) {
		g_warning("Failed to spawn git init: %s", err->message);
		g_free(tmpdir);
		return FALSE;
	}
	g_subprocess_wait(init_proc, NULL, NULL);

	/* git remote add origin <url> */
	remote_proc = g_subprocess_new(
		G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE,
		&err, "git", "-C", tmpdir, "remote", "add", "origin",
		remote_url, NULL);
	if (remote_proc == NULL) {
		g_warning("Failed to spawn git remote add: %s", err->message);
		g_free(tmpdir);
		return FALSE;
	}
	g_subprocess_wait(remote_proc, NULL, NULL);

	*out_tmpdir = tmpdir;
	*out_git_dir = g_build_filename(tmpdir, ".git", NULL);

	return TRUE;
}

/*
 * cleanup_temp_repo:
 * @tmpdir: the temporary directory to remove
 *
 * Recursively removes the temp directory used for testing.
 */
static void
cleanup_temp_repo(const gchar *tmpdir)
{
	g_autofree gchar *cmd = NULL;

	cmd = g_strdup_printf("rm -rf '%s'", tmpdir);
	g_spawn_command_line_sync(cmd, NULL, NULL, NULL, NULL);
}

/* ── URL parsing tests via resolve() with temp repos ──────────────── */

/*
 * test_resolver_https_url: verify HTTPS URL with .git suffix
 * parses host, owner, and repo correctly (stripping .git)
 */
static void
test_resolver_https_url(void)
{
	g_autofree gchar *tmpdir = NULL;
	g_autofree gchar *git_dir = NULL;
	g_autoptr(GctlConfig) config = NULL;
	g_autoptr(GctlContextResolver) resolver = NULL;
	g_autoptr(GctlForgeContext) ctx = NULL;
	g_autoptr(GError) error = NULL;

	if (!setup_temp_repo("https://github.com/testowner/testrepo.git",
	                     &tmpdir, &git_dir))
	{
		g_test_skip("Could not create temp git repo");
		return;
	}

	g_setenv("GIT_DIR", git_dir, TRUE);

	config = gctl_config_new();
	resolver = gctl_context_resolver_new(config);
	ctx = gctl_context_resolver_resolve(resolver, "origin", &error);

	g_unsetenv("GIT_DIR");

	g_assert_no_error(error);
	g_assert_nonnull(ctx);
	g_assert_cmpstr(gctl_forge_context_get_host(ctx), ==, "github.com");
	g_assert_cmpstr(gctl_forge_context_get_owner(ctx), ==, "testowner");
	g_assert_cmpstr(gctl_forge_context_get_repo_name(ctx), ==, "testrepo");
	g_assert_cmpint(gctl_forge_context_get_forge_type(ctx),
	                ==, GCTL_FORGE_TYPE_GITHUB);

	cleanup_temp_repo(tmpdir);
}

/*
 * test_resolver_https_no_git_suffix: verify HTTPS URL without .git
 * suffix still resolves correctly
 */
static void
test_resolver_https_no_git_suffix(void)
{
	g_autofree gchar *tmpdir = NULL;
	g_autofree gchar *git_dir = NULL;
	g_autoptr(GctlConfig) config = NULL;
	g_autoptr(GctlContextResolver) resolver = NULL;
	g_autoptr(GctlForgeContext) ctx = NULL;
	g_autoptr(GError) error = NULL;

	if (!setup_temp_repo("https://github.com/myowner/myrepo",
	                     &tmpdir, &git_dir))
	{
		g_test_skip("Could not create temp git repo");
		return;
	}

	g_setenv("GIT_DIR", git_dir, TRUE);

	config = gctl_config_new();
	resolver = gctl_context_resolver_new(config);
	ctx = gctl_context_resolver_resolve(resolver, "origin", &error);

	g_unsetenv("GIT_DIR");

	g_assert_no_error(error);
	g_assert_nonnull(ctx);
	g_assert_cmpstr(gctl_forge_context_get_host(ctx), ==, "github.com");
	g_assert_cmpstr(gctl_forge_context_get_owner(ctx), ==, "myowner");
	g_assert_cmpstr(gctl_forge_context_get_repo_name(ctx), ==, "myrepo");

	cleanup_temp_repo(tmpdir);
}

/*
 * test_resolver_ssh_scp_url: verify SCP-style SSH URL
 * (git@host:owner/repo.git)
 */
static void
test_resolver_ssh_scp_url(void)
{
	g_autofree gchar *tmpdir = NULL;
	g_autofree gchar *git_dir = NULL;
	g_autoptr(GctlConfig) config = NULL;
	g_autoptr(GctlContextResolver) resolver = NULL;
	g_autoptr(GctlForgeContext) ctx = NULL;
	g_autoptr(GError) error = NULL;

	if (!setup_temp_repo("git@github.com:sshowner/sshrepo.git",
	                     &tmpdir, &git_dir))
	{
		g_test_skip("Could not create temp git repo");
		return;
	}

	g_setenv("GIT_DIR", git_dir, TRUE);

	config = gctl_config_new();
	resolver = gctl_context_resolver_new(config);
	ctx = gctl_context_resolver_resolve(resolver, "origin", &error);

	g_unsetenv("GIT_DIR");

	g_assert_no_error(error);
	g_assert_nonnull(ctx);
	g_assert_cmpstr(gctl_forge_context_get_host(ctx), ==, "github.com");
	g_assert_cmpstr(gctl_forge_context_get_owner(ctx), ==, "sshowner");
	g_assert_cmpstr(gctl_forge_context_get_repo_name(ctx), ==, "sshrepo");
	g_assert_cmpint(gctl_forge_context_get_forge_type(ctx),
	                ==, GCTL_FORGE_TYPE_GITHUB);

	cleanup_temp_repo(tmpdir);
}

/*
 * test_resolver_ssh_scheme_url: verify ssh:// scheme URL
 * (ssh://git@host/owner/repo.git)
 */
static void
test_resolver_ssh_scheme_url(void)
{
	g_autofree gchar *tmpdir = NULL;
	g_autofree gchar *git_dir = NULL;
	g_autoptr(GctlConfig) config = NULL;
	g_autoptr(GctlContextResolver) resolver = NULL;
	g_autoptr(GctlForgeContext) ctx = NULL;
	g_autoptr(GError) error = NULL;

	if (!setup_temp_repo("ssh://git@gitlab.com/schemeowner/schemerepo.git",
	                     &tmpdir, &git_dir))
	{
		g_test_skip("Could not create temp git repo");
		return;
	}

	g_setenv("GIT_DIR", git_dir, TRUE);

	config = gctl_config_new();
	resolver = gctl_context_resolver_new(config);
	ctx = gctl_context_resolver_resolve(resolver, "origin", &error);

	g_unsetenv("GIT_DIR");

	g_assert_no_error(error);
	g_assert_nonnull(ctx);
	g_assert_cmpstr(gctl_forge_context_get_host(ctx), ==, "gitlab.com");
	g_assert_cmpstr(gctl_forge_context_get_owner(ctx), ==, "schemeowner");
	g_assert_cmpstr(gctl_forge_context_get_repo_name(ctx), ==, "schemerepo");
	g_assert_cmpint(gctl_forge_context_get_forge_type(ctx),
	                ==, GCTL_FORGE_TYPE_GITLAB);

	cleanup_temp_repo(tmpdir);
}

/*
 * test_resolver_ssh_scheme_with_port: verify ssh:// scheme URL with
 * a custom port (ssh://git@host:2222/owner/repo.git).
 *
 * GUri parses the port separately, so host extraction should still
 * work correctly.
 */
static void
test_resolver_ssh_scheme_with_port(void)
{
	g_autofree gchar *tmpdir = NULL;
	g_autofree gchar *git_dir = NULL;
	g_autoptr(GctlConfig) config = NULL;
	g_autoptr(GctlContextResolver) resolver = NULL;
	g_autoptr(GctlForgeContext) ctx = NULL;
	g_autoptr(GError) error = NULL;

	if (!setup_temp_repo(
	        "ssh://git@codeberg.org:2222/portowner/portrepo.git",
	        &tmpdir, &git_dir))
	{
		g_test_skip("Could not create temp git repo");
		return;
	}

	g_setenv("GIT_DIR", git_dir, TRUE);

	config = gctl_config_new();
	resolver = gctl_context_resolver_new(config);
	ctx = gctl_context_resolver_resolve(resolver, "origin", &error);

	g_unsetenv("GIT_DIR");

	g_assert_no_error(error);
	g_assert_nonnull(ctx);
	g_assert_cmpstr(gctl_forge_context_get_host(ctx), ==, "codeberg.org");
	g_assert_cmpstr(gctl_forge_context_get_owner(ctx), ==, "portowner");
	g_assert_cmpstr(gctl_forge_context_get_repo_name(ctx), ==, "portrepo");
	g_assert_cmpint(gctl_forge_context_get_forge_type(ctx),
	                ==, GCTL_FORGE_TYPE_FORGEJO);

	cleanup_temp_repo(tmpdir);
}

/*
 * test_resolver_https_trailing_slash: verify HTTPS URL with trailing
 * slash (https://github.com/owner/repo/).  The trailing slash produces
 * an empty third segment which should not break parsing.
 */
static void
test_resolver_https_trailing_slash(void)
{
	g_autofree gchar *tmpdir = NULL;
	g_autofree gchar *git_dir = NULL;
	g_autoptr(GctlConfig) config = NULL;
	g_autoptr(GctlContextResolver) resolver = NULL;
	g_autoptr(GctlForgeContext) ctx = NULL;
	g_autoptr(GError) error = NULL;

	if (!setup_temp_repo("https://github.com/slashowner/slashrepo/",
	                     &tmpdir, &git_dir))
	{
		g_test_skip("Could not create temp git repo");
		return;
	}

	g_setenv("GIT_DIR", git_dir, TRUE);

	config = gctl_config_new();
	resolver = gctl_context_resolver_new(config);
	ctx = gctl_context_resolver_resolve(resolver, "origin", &error);

	g_unsetenv("GIT_DIR");

	g_assert_no_error(error);
	g_assert_nonnull(ctx);
	g_assert_cmpstr(gctl_forge_context_get_host(ctx), ==, "github.com");
	g_assert_cmpstr(gctl_forge_context_get_owner(ctx), ==, "slashowner");
	g_assert_cmpstr(gctl_forge_context_get_repo_name(ctx), ==, "slashrepo");

	cleanup_temp_repo(tmpdir);
}

/*
 * test_resolver_https_gitlab_url: verify HTTPS URL for a GitLab host
 * resolves to GCTL_FORGE_TYPE_GITLAB
 */
static void
test_resolver_https_gitlab_url(void)
{
	g_autofree gchar *tmpdir = NULL;
	g_autofree gchar *git_dir = NULL;
	g_autoptr(GctlConfig) config = NULL;
	g_autoptr(GctlContextResolver) resolver = NULL;
	g_autoptr(GctlForgeContext) ctx = NULL;
	g_autoptr(GError) error = NULL;

	if (!setup_temp_repo("https://gitlab.com/glowner/glrepo.git",
	                     &tmpdir, &git_dir))
	{
		g_test_skip("Could not create temp git repo");
		return;
	}

	g_setenv("GIT_DIR", git_dir, TRUE);

	config = gctl_config_new();
	resolver = gctl_context_resolver_new(config);
	ctx = gctl_context_resolver_resolve(resolver, "origin", &error);

	g_unsetenv("GIT_DIR");

	g_assert_no_error(error);
	g_assert_nonnull(ctx);
	g_assert_cmpstr(gctl_forge_context_get_host(ctx), ==, "gitlab.com");
	g_assert_cmpstr(gctl_forge_context_get_owner(ctx), ==, "glowner");
	g_assert_cmpstr(gctl_forge_context_get_repo_name(ctx), ==, "glrepo");
	g_assert_cmpint(gctl_forge_context_get_forge_type(ctx),
	                ==, GCTL_FORGE_TYPE_GITLAB);

	cleanup_temp_repo(tmpdir);
}

/*
 * test_resolver_invalid_remote: resolve with a valid temp repo but
 * a non-existent remote name, and no forced forge.  Should fail.
 */
static void
test_resolver_invalid_remote(void)
{
	g_autofree gchar *tmpdir = NULL;
	g_autofree gchar *git_dir = NULL;
	g_autoptr(GctlConfig) config = NULL;
	g_autoptr(GctlContextResolver) resolver = NULL;
	g_autoptr(GctlForgeContext) ctx = NULL;
	g_autoptr(GError) error = NULL;

	if (!setup_temp_repo("https://github.com/owner/repo.git",
	                     &tmpdir, &git_dir))
	{
		g_test_skip("Could not create temp git repo");
		return;
	}

	g_setenv("GIT_DIR", git_dir, TRUE);

	config = gctl_config_new();
	resolver = gctl_context_resolver_new(config);

	/* Ask for a remote that does not exist in the temp repo */
	ctx = gctl_context_resolver_resolve(resolver, "upstream", &error);

	g_unsetenv("GIT_DIR");

	g_assert_null(ctx);
	g_assert_nonnull(error);

	cleanup_temp_repo(tmpdir);
}

/*
 * test_resolver_unknown_host: resolve with a host that is not in
 * the default forge-host mapping.  Should fail with forge-detect error.
 */
static void
test_resolver_unknown_host(void)
{
	g_autofree gchar *tmpdir = NULL;
	g_autofree gchar *git_dir = NULL;
	g_autoptr(GctlConfig) config = NULL;
	g_autoptr(GctlContextResolver) resolver = NULL;
	g_autoptr(GctlForgeContext) ctx = NULL;
	g_autoptr(GError) error = NULL;

	if (!setup_temp_repo("https://unknown-forge.example.com/owner/repo.git",
	                     &tmpdir, &git_dir))
	{
		g_test_skip("Could not create temp git repo");
		return;
	}

	g_setenv("GIT_DIR", git_dir, TRUE);

	config = gctl_config_new();
	resolver = gctl_context_resolver_new(config);
	ctx = gctl_context_resolver_resolve(resolver, "origin", &error);

	g_unsetenv("GIT_DIR");

	g_assert_null(ctx);
	g_assert_nonnull(error);

	cleanup_temp_repo(tmpdir);
}

int
main(
	int     argc,
	char  **argv
){
	g_test_init(&argc, &argv, NULL);

	/* Original tests */
	g_test_add_func("/context-resolver/new", test_resolver_new);
	g_test_add_func("/context-resolver/forced-forge", test_resolver_forced_forge);
	g_test_add_func("/context-resolver/forced-repo", test_resolver_forced_repo);
	g_test_add_func("/context-resolver/resolve-no-remote", test_resolver_resolve_no_remote);
	g_test_add_func("/context-resolver/resolve-no-remote-no-forge", test_resolver_resolve_no_remote_no_forge);

	/* URL parsing through resolve() with temp repos */
	g_test_add_func("/context-resolver/https-url", test_resolver_https_url);
	g_test_add_func("/context-resolver/https-no-git-suffix", test_resolver_https_no_git_suffix);
	g_test_add_func("/context-resolver/ssh-scp-url", test_resolver_ssh_scp_url);
	g_test_add_func("/context-resolver/ssh-scheme-url", test_resolver_ssh_scheme_url);
	g_test_add_func("/context-resolver/ssh-scheme-with-port", test_resolver_ssh_scheme_with_port);
	g_test_add_func("/context-resolver/https-trailing-slash", test_resolver_https_trailing_slash);
	g_test_add_func("/context-resolver/https-gitlab-url", test_resolver_https_gitlab_url);
	g_test_add_func("/context-resolver/invalid-remote", test_resolver_invalid_remote);
	g_test_add_func("/context-resolver/unknown-host", test_resolver_unknown_host);

	return g_test_run();
}
