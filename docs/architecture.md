# gitctl Architecture

## Overview

gitctl is a kubectl-like CLI tool and GObject library for managing git repositories across multiple forges (GitHub, GitLab, Forgejo, Gitea). It provides a unified noun-verb command interface that delegates to forge-specific CLI tools (`gh`, `glab`, `fj`, `tea`).

## Directory Structure

| Path | Description |
|------|-------------|
| `src/` | Library and application source |
| `src/gitctl.h` | Umbrella header (include only this) |
| `src/gitctl-types.h` | Forward declarations for all types |
| `src/gitctl-enums.h` | Enum types (GctlForgeType, GctlVerb, etc.) |
| `src/gitctl-error.h` | Error domain and codes |
| `src/gitctl-version.h.in` | Version template (processed at build time) |
| `src/core/` | Core GObject types (App, Executor, Config, etc.) |
| `src/boxed/` | Boxed types (CommandResult, ForgeContext, Resource) |
| `src/interfaces/` | GInterface definitions (GctlForge) |
| `src/commands/` | Command handlers (pr, issue, repo, release, api, config) |
| `src/module/` | Module system (GctlModule base class, GctlModuleManager) |
| `src/main.c` | Entry point and global option parsing |
| `modules/` | Forge backend module implementations (.so) |
| `modules/github/` | GitHub forge backend (uses `gh`) |
| `modules/gitlab/` | GitLab forge backend (uses `glab`) |
| `modules/forgejo/` | Forgejo forge backend (uses `fj`) |
| `modules/gitea/` | Gitea forge backend (uses `tea`) |
| `tests/` | GLib GTest test files |
| `docs/` | Documentation |
| `data/` | Default configuration and examples |
| `deps/` | Vendored dependencies (yaml-glib) |
| `config.mk` | Build configuration variables |
| `rules.mk` | Build rules and pattern recipes |
| `Makefile` | Top-level build orchestration |

## GObject Type Hierarchy

```
GObject
├── GctlApp              (derivable)     -- Main application object
├── GctlExecutor         (final)         -- Subprocess execution with dry-run
├── GctlContextResolver  (final)         -- Forge detection from git remotes
├── GctlConfig           (final)         -- YAML configuration loader
├── GctlOutputFormatter  (final)         -- Multi-format resource rendering
├── GctlModuleManager    (final)         -- Module loader and registry
└── GctlModule           (derivable)     -- Base class for forge modules
    ├── GctlGithubForge  (final, implements GctlForge)
    ├── GctlGitlabForge  (final, implements GctlForge)
    ├── GctlForgejoForge (final, implements GctlForge)
    └── GctlGiteaForge   (final, implements GctlForge)

GInterface
└── GctlForge            -- Interface for forge backend operations

Boxed Types (GType-registered, not GObject)
├── GctlCommandResult    -- Subprocess result (exit code, stdout, stderr)
├── GctlForgeContext     -- Resolved forge info (type, owner, repo, URL)
└── GctlResource         -- Normalized forge resource (PR, issue, repo, release)
```

## Forge Abstraction Layer

The `GctlForge` GInterface defines the contract that all forge backends must implement:

- **Identity**: `get_name()`, `get_cli_tool()`, `get_forge_type()`
- **Detection**: `can_handle_url()` -- returns TRUE if the forge recognizes a remote URL
- **Availability**: `is_available()` -- checks if the CLI tool is in PATH
- **Command building**: `build_argv()` -- constructs a subprocess argv for a given resource/verb combination
- **Output parsing**: `parse_list_output()`, `parse_get_output()` -- parses CLI JSON into normalized `GctlResource` objects
- **API passthrough**: `build_api_argv()` -- constructs argv for raw REST API calls

Each forge backend is a `GctlModule` subclass that also implements `GctlForge`. This allows backends to be loaded dynamically from shared libraries at runtime.

## Module System

Modules are loaded in two phases:

1. **Discovery**: `GctlModuleManager` scans a directory for `.so` files
2. **Registration**: Each `.so` must export a `gctl_module_register()` symbol that returns a `GType`
3. **Activation**: The manager calls `gctl_module_activate()` on each module

The module search order is:
1. Development module directory (`build/<type>/modules/`)
2. System module directory (`$LIBDIR/gitctl/modules/`)

## Command Dispatch

gitctl uses a noun-verb command pattern:

```
gitctl [global-options] <noun> <verb> [args] [flags]
```

### Execution Flow

```
main()
  │
  ├── Parse global options (GOptionContext)
  │     --dry-run, --verbose, --output, --forge, --remote, --config
  │
  ├── Create GctlApp
  │     ├── GctlConfig (load YAML)
  │     ├── GctlExecutor (subprocess runner)
  │     ├── GctlContextResolver (forge detection)
  │     ├── GctlOutputFormatter (table/json/yaml/csv)
  │     └── GctlModuleManager (load .so modules)
  │
  ├── Look up noun in dispatch table
  │     pr → gctl_cmd_pr()
  │     issue → gctl_cmd_issue()
  │     repo → gctl_cmd_repo()
  │     release → gctl_cmd_release()
  │     api → gctl_cmd_api()
  │     config → gctl_cmd_config()
  │
  └── Noun handler:
        ├── Parse verb from argv[0]
        ├── Resolve forge context (GctlContextResolver)
        ├── Find matching forge module (GctlModuleManager)
        ├── Build argv (GctlForge.build_argv)
        ├── Execute (GctlExecutor.run) -- or print in dry-run mode
        ├── Parse output (GctlForge.parse_*_output)
        └── Format and print (GctlOutputFormatter)
```

## Configuration System

Configuration is loaded from YAML with the following precedence:

1. Command-line flags (highest priority)
2. `$XDG_CONFIG_HOME/gitctl/config.yaml` (or `~/.config/gitctl/config.yaml`)
3. Built-in defaults (lowest priority)

The configuration controls:
- Default output format (table, json, yaml, csv)
- Default git remote name (default: `origin`)
- Default forge type (default: `github`)
- Forge-to-host mappings (e.g. `github.com` -> GitHub)
- CLI tool paths per forge (e.g. GitHub -> `gh`)
- Command aliases (e.g. `prl` -> `pr list`)
