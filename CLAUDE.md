# gitctl -- Project Instructions

## Build

```bash
make                    # Release build
make DEBUG=1            # Debug build
make DEBUG=1 ASAN=1     # AddressSanitizer
make test               # Run tests
make show-config        # Show build configuration
make check-deps         # Verify dependencies
make install-deps       # Auto-install dependencies
```

## Things to NEVER do
- run `local_postgres drop` . never do this, instead prompt me to run `psql`. ALWAYS ask for permission to run psql commands.

## Code Style

- C standard: `-std=gnu89` (K&R-compatible)
- Comments: `/* */` only -- never `//`
- Indentation: TAB (4-space width)
- Naming: `Gctl` PascalCase for types, `gctl_snake_case()` for functions, `GCTL_UPPER_CASE` for macros
- Memory: use `g_autoptr()`, `g_steal_pointer()` -- avoid manual ref/unref where possible
- Headers: include only `<gitctl.h>` -- individual headers have `GCTL_INSIDE` guards
- GObject Introspection: all public API must have GTK-Doc comments with transfer/nullable annotations

## Architecture

- Noun-verb command pattern: `gitctl <noun> <verb> [args] [flags]`
- Forge backends are loadable modules (.so) in modules/
- Each module extends GctlModule and implements GctlForge interface
- GctlExecutor handles all subprocess execution with dry-run support
- GctlContextResolver auto-detects forge from git remote URL

## Key Files

- `src/gitctl.h` -- Umbrella header (include only this)
- `src/gitctl-types.h` -- Forward declarations
- `src/gitctl-enums.h` -- Enum types (GctlForgeType, GctlVerb, etc.)
- `src/gitctl-error.h` -- Error domain and codes
- `src/main.c` -- Entry point with noun dispatch table
- `src/interfaces/gitctl-forge.h` -- Core forge interface
- `src/core/gitctl-executor.h` -- Subprocess executor with dry-run
- `src/core/gitctl-context-resolver.h` -- Forge auto-detection
- `config.mk` -- Build configuration
- `rules.mk` -- Build rules
- `Makefile` -- Build orchestration

## Testing

- Framework: GLib GTest (`g_test_*`)
- Test files: `tests/test-*.c`
- Run: `make test` or `LD_LIBRARY_PATH=build/debug ./build/debug/test-app`
