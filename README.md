# gitctl

A kubectl-like CLI tool and library for managing git repositories across forges
(GitHub, GitLab, Forgejo, Gitea).

## Features

- Unified interface across GitHub (`gh`), GitLab (`glab`), Forgejo (`fj`), and Gitea (`tea`)
- Auto-detects forge from git remote URL
- `--dry-run` mode shows commands without executing
- Multiple output formats: table, JSON, YAML, CSV
- Extensible module system for forge backends

## Quick Start

```bash
make install-deps       # Install build dependencies
make                    # Build (release)
make test               # Run tests

# List PRs
./build/release/gitctl pr list

# View an issue
./build/release/gitctl issue get 42

# Dry-run a merge
./build/release/gitctl --dry-run pr merge 123
```

## Building

See [docs/building.md](docs/building.md) for detailed instructions.

## Architecture

See [docs/architecture.md](docs/architecture.md) for design details.

## License

AGPL-3.0-or-later. See [LICENSE](LICENSE) for details.
