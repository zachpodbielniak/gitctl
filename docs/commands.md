# gitctl Command Reference

## Global Options

These options apply to all commands:

| Option | Short | Description |
|--------|-------|-------------|
| `--help` | `-h` | Show help |
| `--version` | `-v` | Show version |
| `--license` | | Show AGPLv3 license text |
| `--dry-run` | `-n` | Print commands without executing |
| `--verbose` | | Enable verbose output |
| `--output FORMAT` | `-o` | Output format: `table`, `json`, `yaml`, `csv` |
| `--forge TYPE` | `-f` | Force forge: `github`, `gitlab`, `forgejo`, `gitea` |
| `--remote NAME` | `-r` | Git remote to use (default: `origin`) |
| `--config PATH` | `-c` | Configuration file path |

## pr -- Pull Requests

### Verbs

| Verb | Description |
|------|-------------|
| `list` | List open pull requests |
| `get <number>` | View a single pull request |
| `create` | Create a new pull request |
| `edit <number>` | Edit an existing pull request |
| `close <number>` | Close a pull request |
| `reopen <number>` | Reopen a closed pull request |
| `merge <number>` | Merge a pull request |
| `checkout <number>` | Check out a PR branch locally |
| `comment <number>` | Comment on a pull request |
| `review <number>` | Review a pull request |
| `browse <number>` | Open pull request in web browser |

### Create Options

| Option | Description |
|--------|-------------|
| `--title TEXT` | PR title |
| `--body TEXT` | PR body / description |
| `--base BRANCH` | Target branch (default: repo default) |
| `--head BRANCH` | Source branch (default: current branch) |
| `--draft` | Create as draft |

### List Options

| Option | Description |
|--------|-------------|
| `--state STATE` | Filter: `open`, `closed`, `merged`, `all` |
| `--limit N` | Maximum number of results |
| `--author USER` | Filter by author |

### Examples

```bash
gitctl pr list                              # List open PRs
gitctl pr list --state all --limit 50       # List all PRs (max 50)
gitctl pr get 123                           # View PR #123
gitctl pr create --title "Add feature"      # Create a PR
gitctl pr create --title "WIP" --draft      # Create a draft PR
gitctl --dry-run pr merge 123               # Show merge command
gitctl -o json pr list                      # List PRs as JSON
gitctl pr checkout 42                       # Check out PR #42 locally
gitctl pr comment 123 --body "LGTM"        # Comment on PR #123
gitctl pr browse 123                        # Open PR #123 in browser
```

## issue -- Issues

### Verbs

| Verb | Description |
|------|-------------|
| `list` | List issues |
| `get <number>` | View a single issue |
| `create` | Create a new issue |
| `edit <number>` | Edit an existing issue |
| `close <number>` | Close an issue |
| `reopen <number>` | Reopen a closed issue |
| `comment <number>` | Comment on an issue |
| `browse <number>` | Open issue in web browser |

### Create Options

| Option | Description |
|--------|-------------|
| `--title TEXT` | Issue title |
| `--body TEXT` | Issue body / description |
| `--labels LABELS` | Comma-separated labels |
| `--assignee USER` | Assign to user |

### List Options

| Option | Description |
|--------|-------------|
| `--state STATE` | Filter: `open`, `closed`, `all` |
| `--limit N` | Maximum number of results |
| `--labels LABELS` | Filter by labels |
| `--author USER` | Filter by author |

### Examples

```bash
gitctl issue list                           # List open issues
gitctl issue list --state closed            # List closed issues
gitctl issue get 42                         # View issue #42
gitctl issue create --title "Bug report"    # Create an issue
gitctl issue close 42                       # Close issue #42
gitctl issue comment 42 --body "Fixed"      # Comment on issue #42
```

## repo -- Repositories

### Verbs

| Verb | Description |
|------|-------------|
| `list` | List repositories |
| `get [name]` | View repository details |
| `create <name>` | Create a new repository |
| `fork [owner/repo]` | Fork a repository |
| `clone <owner/repo>` | Clone a repository |
| `delete <name>` | Delete a repository |
| `browse` | Open repository in web browser |

### Create Options

| Option | Description |
|--------|-------------|
| `--private` | Create as private |
| `--description TEXT` | Repository description |

### List Options

| Option | Description |
|--------|-------------|
| `--limit N` | Maximum number of results |
| `--visibility VIS` | Filter: `public`, `private`, `all` |

### Examples

```bash
gitctl repo list                            # List your repositories
gitctl repo get                             # View current repo details
gitctl repo create myproject --private      # Create a private repo
gitctl repo fork user/project               # Fork a repository
gitctl repo clone user/project              # Clone a repository
gitctl repo browse                          # Open repo in browser
```

## release -- Releases

### Verbs

| Verb | Description |
|------|-------------|
| `list` | List releases |
| `get <tag>` | View a single release |
| `create <tag>` | Create a new release |
| `delete <tag>` | Delete a release |

### Create Options

| Option | Description |
|--------|-------------|
| `--title TEXT` | Release title |
| `--body TEXT` | Release notes |
| `--draft` | Create as draft |
| `--prerelease` | Mark as pre-release |

### Examples

```bash
gitctl release list                         # List releases
gitctl release get v1.0.0                   # View release v1.0.0
gitctl release create v1.0.0 --title "1.0"  # Create a release
gitctl release delete v0.9.0               # Delete a release
```

## api -- Raw API Requests

The `api` command does not use the verb dispatch pattern. Instead it passes a raw HTTP method and endpoint directly to the forge CLI's API interface.

### Usage

```
gitctl api <METHOD> <endpoint> [--body JSON]
```

### Examples

```bash
gitctl api GET /repos/{owner}/{repo}                  # Get repo info
gitctl api GET /repos/{owner}/{repo}/pulls             # List PRs via API
gitctl api POST /repos/{owner}/{repo}/issues \
    --body '{"title":"Bug","body":"Details"}'           # Create issue via API
gitctl -o json api GET /user                           # Get authenticated user
```

## config -- Configuration

The `config` command operates on the local gitctl configuration file (`~/.config/gitctl/config.yaml`).

### Verbs

| Verb | Description |
|------|-------------|
| `list` | Show all configuration values |
| `get <key>` | Get a single configuration value |
| `set <key> <value>` | Set a configuration value |

### Examples

```bash
gitctl config list                          # Show all config
gitctl config get output                    # Get default output format
gitctl config set output json               # Set default output to JSON
gitctl config set default_forge gitlab      # Set default forge to GitLab
```
