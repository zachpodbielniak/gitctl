# Makefile - gitctl
# kubectl-like CLI for managing git repositories across forges
#
# Copyright (C) 2026 Zach Podbielniak
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Usage:
#   make             - Build all (lib, gitctl, modules)
#   make lib         - Build static and shared libraries
#   make test        - Run the test suite
#   make install     - Install to PREFIX
#   make clean       - Clean build artifacts
#   make DEBUG=1     - Build with debug symbols
#   make ASAN=1      - Build with AddressSanitizer

.DEFAULT_GOAL := all
.PHONY: all lib test check-deps help gitctl modules gir

# Include configuration
include config.mk

# Check dependencies before anything else
SKIP_DEP_CHECK_TARGETS := install-deps help check-deps show-config clean clean-all
ifeq ($(filter $(SKIP_DEP_CHECK_TARGETS),$(MAKECMDGOALS)),)
$(foreach dep,$(DEPS_REQUIRED),$(call check_dep,$(dep)))
endif

# ── Source files: Library ────────────────────────────────────────────

LIB_SRCS := \
	src/gitctl-types.c \
	src/gitctl-enums.c \
	src/core/gitctl-app.c \
	src/core/gitctl-executor.c \
	src/core/gitctl-context-resolver.c \
	src/core/gitctl-config.c \
	src/core/gitctl-output-formatter.c \
	src/boxed/gitctl-command-result.c \
	src/boxed/gitctl-forge-context.c \
	src/boxed/gitctl-resource.c \
	src/interfaces/gitctl-forge.c \
	src/commands/gitctl-cmd-pr.c \
	src/commands/gitctl-cmd-issue.c \
	src/commands/gitctl-cmd-repo.c \
	src/commands/gitctl-cmd-release.c \
	src/commands/gitctl-cmd-mirror.c \
	src/commands/gitctl-cmd-api.c \
	src/commands/gitctl-cmd-config.c \
	src/commands/gitctl-cmd-completion.c \
	src/module/gitctl-module.c \
	src/module/gitctl-module-manager.c

# ── Header files (for GIR scanner and installation) ──────────────────

LIB_HDRS := $(shell find src/ -name '*.h' ! -name '*-private.h' ! -name 'gitctl-version.h' 2>/dev/null)

# ── Dependency sources: yaml-glib ────────────────────────────────────

YAMLGLIB_SRCS := \
	deps/yaml-glib/src/yaml-builder.c \
	deps/yaml-glib/src/yaml-document.c \
	deps/yaml-glib/src/yaml-generator.c \
	deps/yaml-glib/src/yaml-gobject.c \
	deps/yaml-glib/src/yaml-mapping.c \
	deps/yaml-glib/src/yaml-node.c \
	deps/yaml-glib/src/yaml-parser.c \
	deps/yaml-glib/src/yaml-schema.c \
	deps/yaml-glib/src/yaml-sequence.c \
	deps/yaml-glib/src/yaml-serializable.c

# ── Test sources ─────────────────────────────────────────────────────

TEST_SRCS := $(wildcard tests/test-*.c)

# ── Module directories ───────────────────────────────────────────────

MODULE_DIRS := $(wildcard modules/*)

# ── Object file mappings ─────────────────────────────────────────────

LIB_OBJS := $(patsubst src/%.c,$(OBJDIR)/%.o,$(LIB_SRCS))

YAMLGLIB_OBJS := $(patsubst deps/%.c,$(OBJDIR)/deps/%.o,$(YAMLGLIB_SRCS))
DEP_OBJS := $(YAMLGLIB_OBJS)

MAIN_OBJ := $(OBJDIR)/main.o

TEST_OBJS := $(patsubst tests/%.c,$(OBJDIR)/tests/%.o,$(TEST_SRCS))
TEST_BINS := $(patsubst tests/%.c,$(OUTDIR)/%,$(TEST_SRCS))

# Include build rules
include rules.mk

# ── Default target ───────────────────────────────────────────────────

all: src/gitctl-version.h lib gitctl
ifeq ($(BUILD_MODULES),1)
all: modules
endif
ifeq ($(BUILD_GIR),1)
all: gir
endif

# ── Build the library ────────────────────────────────────────────────

lib: src/gitctl-version.h \
     $(OUTDIR)/$(LIB_STATIC) $(OUTDIR)/$(LIB_SHARED_FULL) \
     $(OUTDIR)/gitctl-$(API_VERSION).pc

# ── Build the executable ─────────────────────────────────────────────

gitctl: lib $(OUTDIR)/gitctl

# ── Build GIR/typelib ────────────────────────────────────────────────

gir: $(OUTDIR)/$(GIR_FILE) $(OUTDIR)/$(TYPELIB_FILE)

# ── Build all modules ────────────────────────────────────────────────

modules: lib $(OUTDIR)/modules
	@for dir in $(MODULE_DIRS); do \
		if [ -d "$$dir" ] && [ -f "$$dir/Makefile" ]; then \
			echo "Building module: $$(basename $$dir)"; \
			$(MAKE) -C "$$dir" \
				OUTDIR=$(abspath $(OUTDIR)/modules) \
				LIBDIR=$(abspath $(OUTDIR)) \
				CFLAGS='$(MODULE_CFLAGS)' \
				LDFLAGS='$(MODULE_LDFLAGS)'; \
		fi; \
	done

# ── Build and run tests ──────────────────────────────────────────────

$(OUTDIR)/test-%: $(OBJDIR)/tests/test-%.o $(OUTDIR)/$(LIB_SHARED_FULL)
	$(CC) -o $@ $< $(TEST_LDFLAGS)

test: lib $(TEST_BINS)
	@echo "Running tests..."
	@failed=0; \
	total=0; \
	passed=0; \
	for test in $(TEST_BINS); do \
		total=$$((total + 1)); \
		echo "  Running $$(basename $$test)..."; \
		if LD_LIBRARY_PATH=$(OUTDIR) $$test; then \
			echo "    PASS"; \
			passed=$$((passed + 1)); \
		else \
			echo "    FAIL"; \
			failed=$$((failed + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "Results: $$passed/$$total passed"; \
	if [ $$failed -gt 0 ]; then \
		echo "$$failed test(s) failed"; \
		exit 1; \
	else \
		echo "All tests passed"; \
	fi

# ── Check dependencies ───────────────────────────────────────────────

check-deps:
	@echo "Checking dependencies..."
	@for dep in $(DEPS_REQUIRED) yaml-0.1; do \
		if $(PKG_CONFIG) --exists $$dep; then \
			ver=$$($(PKG_CONFIG) --modversion $$dep 2>/dev/null); \
			echo "  $$dep: OK ($$ver)"; \
		else \
			echo "  $$dep: MISSING"; \
		fi; \
	done
	@echo ""
	@echo "Checking forge CLI tools..."
	@for tool in gh glab fj tea; do \
		if command -v $$tool >/dev/null 2>&1; then \
			echo "  $$tool: OK ($$(command -v $$tool))"; \
		else \
			echo "  $$tool: not found (optional)"; \
		fi; \
	done

# ── Help ─────────────────────────────────────────────────────────────

help:
	@echo "gitctl - kubectl-like CLI for managing git repositories across forges"
	@echo ""
	@echo "Build targets:"
	@echo "  all          - Build everything (default)"
	@echo "  lib          - Build static and shared libraries"
	@echo "  gitctl       - Build the gitctl executable"
	@echo "  modules      - Build forge backend modules"
	@echo "  test         - Build and run the test suite"
	@echo "  install      - Install to PREFIX ($(PREFIX))"
	@echo "  uninstall    - Remove installed files"
	@echo "  clean        - Remove build artifacts for current build type"
	@echo "  clean-all    - Remove all build directories"
	@echo ""
	@echo "Build options (set on command line):"
	@echo "  DEBUG=1         - Enable debug build (-g -O0)"
	@echo "  ASAN=1          - Enable AddressSanitizer"
	@echo "  UBSAN=1         - Enable UndefinedBehaviorSanitizer"
	@echo "  BUILD_GIR=1     - Enable GObject Introspection generation"
	@echo "  BUILD_TESTS=0   - Disable test building"
	@echo "  BUILD_MODULES=0 - Disable module building"
	@echo "  PREFIX=path     - Set installation prefix (default: /usr/local)"
	@echo ""
	@echo "Utility targets:"
	@echo "  install-deps  - Install build dependencies (auto-detects distro)"
	@echo "  check-deps    - Check for required pkg-config dependencies"
	@echo "  show-config   - Show current build configuration"
	@echo "  help          - Show this help message"

# ── Dependency tracking (incremental builds) ─────────────────────────

ALL_DEPS := $(LIB_OBJS:.o=.d) $(TEST_OBJS:.o=.d) $(YAMLGLIB_OBJS:.o=.d)
ifneq ($(MAIN_OBJ),)
ALL_DEPS += $(MAIN_OBJ:.o=.d)
endif

ifeq ($(filter clean clean-all,$(MAKECMDGOALS)),)
-include $(ALL_DEPS)
endif
