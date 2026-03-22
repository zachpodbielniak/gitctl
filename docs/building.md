# Building gitctl

## Prerequisites

### Fedora / RHEL

```bash
sudo dnf install gcc make pkgconf-pkg-config \
    glib2-devel json-glib-devel libyaml-devel
```

For GObject Introspection (optional):

```bash
sudo dnf install gobject-introspection-devel
```

### Ubuntu / Debian

```bash
sudo apt-get install gcc make pkg-config \
    libglib2.0-dev libjson-glib-dev libyaml-dev
```

For GObject Introspection (optional):

```bash
sudo apt-get install gobject-introspection libgirepository1.0-dev
```

### Arch Linux

```bash
sudo pacman -S gcc make pkgconf \
    glib2 json-glib libyaml
```

For GObject Introspection (optional):

```bash
sudo pacman -S gobject-introspection
```

### Automatic Dependency Installation

The build system can auto-detect your distribution and install dependencies:

```bash
make install-deps
```

## Quick Start

```bash
make                    # Release build
make test               # Run tests
sudo make install       # Install to /usr/local
```

## Build Options

All options are passed on the command line:

| Option | Default | Description |
|--------|---------|-------------|
| `DEBUG=1` | 0 | Debug build (`-g -O0 -DDEBUG`) |
| `ASAN=1` | 0 | Enable AddressSanitizer (requires `DEBUG=1`) |
| `UBSAN=1` | 0 | Enable UndefinedBehaviorSanitizer |
| `BUILD_GIR=1` | 0 | Generate GObject Introspection data |
| `BUILD_TESTS=0` | 1 | Disable test building |
| `BUILD_MODULES=0` | 1 | Disable module building |
| `PREFIX=/path` | `/usr/local` | Installation prefix |

## Development Workflow

### Debug Build with AddressSanitizer

```bash
make DEBUG=1 ASAN=1
```

This enables `-g -O0` and AddressSanitizer for catching memory errors. The output goes to `build/debug/`.

### Release Build

```bash
make
```

Compiles with `-O2 -DNDEBUG`. Output goes to `build/release/`.

### Running Tests

```bash
make test
```

This builds the library and all test binaries, then runs each test. Individual tests can be run directly:

```bash
LD_LIBRARY_PATH=build/debug ./build/debug/test-app
LD_LIBRARY_PATH=build/debug ./build/debug/test-executor
LD_LIBRARY_PATH=build/debug ./build/debug/test-resource
LD_LIBRARY_PATH=build/debug ./build/debug/test-enums
```

### Checking Build Configuration

```bash
make show-config
```

Displays the current version, compiler, flags, and all build options.

### Verifying Dependencies

```bash
make check-deps
```

Reports the status of each required pkg-config dependency and optional forge CLI tools.

### Cleaning

```bash
make clean              # Clean current build type (debug or release)
make clean-all          # Remove all build directories
```

## Module Development

Forge backend modules are shared libraries that:

1. Subclass `GctlModule`
2. Implement the `GctlForge` interface
3. Export a `gctl_module_register()` function returning the module's `GType`

Each module lives in `modules/<name>/` with its own `Makefile`. The top-level build compiles modules using the flags defined in `config.mk` (`MODULE_CFLAGS`, `MODULE_LDFLAGS`).

Built modules are placed in `build/<type>/modules/` and the executable searches that directory at runtime (in addition to the system module directory).

## Installation

```bash
sudo make install                   # Install to /usr/local
sudo make PREFIX=/usr install       # Install to /usr
make DESTDIR=/tmp/pkg install       # Staged install for packaging
```

Installs:
- `$BINDIR/gitctl` -- the executable
- `$LIBDIR/libgitctl-1.0.{a,so}` -- static and shared libraries
- `$INCLUDEDIR/gitctl/` -- header files
- `$PKGCONFIGDIR/gitctl-1.0.pc` -- pkg-config file
- `$MODULEDIR/*.so` -- forge backend modules (if `BUILD_MODULES=1`)
- `$GIRDIR/`, `$TYPELIBDIR/` -- GIR data (if `BUILD_GIR=1`)
