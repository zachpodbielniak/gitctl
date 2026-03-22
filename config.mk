# config.mk - gitctl Configuration
# kubectl-like CLI for managing git repositories across forges
#
# Copyright (C) 2026 Zach Podbielniak
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# This file contains all configurable build options.
# Override any variable on the command line:
#   make DEBUG=1
#   make PREFIX=/usr/local

# ── Project info ──────────────────────────────────────────────────────
PROJECT_NAME := gitctl
VERSION_MAJOR := 0
VERSION_MINOR := 1
VERSION_MICRO := 0
VERSION := $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_MICRO)
API_VERSION := 1.0

# Git SHA for version traceability
GIT_SHA := $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")
GIT_DIRTY := $(shell git diff --quiet 2>/dev/null || echo "-UNSTAGED")
VERSION_FULL := $(VERSION)-$(GIT_SHA)$(GIT_DIRTY)

# ── Installation directories ─────────────────────────────────────────
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

# Auto-detect lib vs lib64 for 64-bit systems (Fedora, RHEL, SUSE, etc.)
LIBDIR_SUFFIX := $(shell if [ -d /usr/lib64 ]; then echo lib64; else echo lib; fi)
LIBDIR ?= $(PREFIX)/$(LIBDIR_SUFFIX)

INCLUDEDIR ?= $(PREFIX)/include
DATADIR ?= $(PREFIX)/share
SYSCONFDIR ?= /etc
PKGCONFIGDIR ?= $(LIBDIR)/pkgconfig
GIRDIR ?= $(DATADIR)/gir-1.0
TYPELIBDIR ?= $(LIBDIR)/girepository-1.0
MODULEDIR ?= $(LIBDIR)/gitctl/modules

# ── Build directories ────────────────────────────────────────────────
BUILDDIR := build
OBJDIR_DEBUG := $(BUILDDIR)/debug/obj
OBJDIR_RELEASE := $(BUILDDIR)/release/obj
BINDIR_DEBUG := $(BUILDDIR)/debug
BINDIR_RELEASE := $(BUILDDIR)/release

# ── Build options (0 or 1) ───────────────────────────────────────────
DEBUG ?= 0
ASAN ?= 0
UBSAN ?= 0
BUILD_GIR ?= 0
BUILD_TESTS ?= 1
BUILD_MODULES ?= 1

# Select build directories based on DEBUG
ifeq ($(DEBUG),1)
    OBJDIR := $(OBJDIR_DEBUG)
    OUTDIR := $(BINDIR_DEBUG)
    BUILD_TYPE := debug
else
    OBJDIR := $(OBJDIR_RELEASE)
    OUTDIR := $(BINDIR_RELEASE)
    BUILD_TYPE := release
endif

# ── Compiler and tools ───────────────────────────────────────────────
CC := gcc
AR := ar
PKG_CONFIG ?= pkg-config
GIR_SCANNER ?= g-ir-scanner
GIR_COMPILER ?= g-ir-compiler
INSTALL := install
INSTALL_PROGRAM := $(INSTALL) -m 755
INSTALL_DATA := $(INSTALL) -m 644
MKDIR_P := mkdir -p

# ── C standard and warnings ─────────────────────────────────────────
CSTD := -std=gnu89
WARNINGS := -Wall -Wextra -Wno-unused-parameter -Wformat=2 -Wshadow

# ── Base compiler flags ──────────────────────────────────────────────
CFLAGS_BASE := $(CSTD) $(WARNINGS)
CFLAGS_BASE += -fPIC
CFLAGS_BASE += -DGCTL_VERSION=\"$(VERSION)\"
CFLAGS_BASE += -DGCTL_VERSION_FULL=\"$(VERSION_FULL)\"
CFLAGS_BASE += -DGCTL_VERSION_MAJOR=$(VERSION_MAJOR)
CFLAGS_BASE += -DGCTL_VERSION_MINOR=$(VERSION_MINOR)
CFLAGS_BASE += -DGCTL_VERSION_MICRO=$(VERSION_MICRO)
CFLAGS_BASE += -DG_LOG_DOMAIN=\"Gctl\"
CFLAGS_BASE += -DGCTL_MODULEDIR=\"$(MODULEDIR)\"
CFLAGS_BASE += -DGCTL_DEV_MODULE_DIR=\"$(CURDIR)/$(OUTDIR)/modules\"
CFLAGS_BASE += -DGCTL_SYSCONFDIR=\"$(SYSCONFDIR)\"
CFLAGS_BASE += -DGCTL_DATADIR=\"$(DATADIR)\"

# ── Debug/Release flags ──────────────────────────────────────────────
ifeq ($(DEBUG),1)
    CFLAGS_BUILD := -g -O0 -DDEBUG
else
    CFLAGS_BUILD := -O2 -DNDEBUG
endif

# AddressSanitizer (requires DEBUG=1)
ifeq ($(ASAN),1)
    CFLAGS_BUILD += -fsanitize=address -fno-omit-frame-pointer
    LDFLAGS_ASAN := -fsanitize=address
endif

# UndefinedBehaviorSanitizer
ifeq ($(UBSAN),1)
    CFLAGS_BUILD += -fsanitize=undefined
    LDFLAGS_UBSAN := -fsanitize=undefined
endif

# ── Dependencies (pkg-config) ────────────────────────────────────────
DEPS_REQUIRED := glib-2.0 gobject-2.0 gio-2.0 gmodule-2.0 json-glib-1.0

# Check for required dependencies
define check_dep
$(if $(shell $(PKG_CONFIG) --exists $(1) && echo yes),,$(error Missing dependency: $(1)))
endef

# Get flags from pkg-config
CFLAGS_DEPS := $(shell $(PKG_CONFIG) --cflags $(DEPS_REQUIRED) 2>/dev/null)
LDFLAGS_DEPS := $(shell $(PKG_CONFIG) --libs $(DEPS_REQUIRED) 2>/dev/null)

# ── yaml-glib integration ───────────────────────────────────────────
YAMLGLIB_DIR := deps/yaml-glib
YAMLGLIB_CFLAGS := -I$(YAMLGLIB_DIR)/src

# ── Include paths ────────────────────────────────────────────────────
CFLAGS_INC := -I. -Isrc -I$(OUTDIR)
CFLAGS_INC += $(YAMLGLIB_CFLAGS)

# ── Combine all CFLAGS ───────────────────────────────────────────────
CFLAGS := $(CFLAGS_BASE) $(CFLAGS_BUILD) $(CFLAGS_INC) $(CFLAGS_DEPS)

# ── Linker flags ─────────────────────────────────────────────────────
LDFLAGS := $(LDFLAGS_DEPS) $(LDFLAGS_ASAN) $(LDFLAGS_UBSAN)
LDFLAGS += $(shell $(PKG_CONFIG) --libs yaml-0.1 2>/dev/null)
LDFLAGS_SHARED := -shared -Wl,-soname,libgitctl-$(API_VERSION).so.$(VERSION_MAJOR)

# ── Library names ────────────────────────────────────────────────────
LIB_NAME := gitctl-$(API_VERSION)
LIB_STATIC := lib$(LIB_NAME).a
LIB_SHARED := lib$(LIB_NAME).so
LIB_SHARED_FULL := lib$(LIB_NAME).so.$(VERSION)
LIB_SHARED_MAJOR := lib$(LIB_NAME).so.$(VERSION_MAJOR)

# ── GIR settings ─────────────────────────────────────────────────────
GIR_NAMESPACE := Gitctl
GIR_VERSION := $(API_VERSION)
GIR_FILE := $(GIR_NAMESPACE)-$(GIR_VERSION).gir
TYPELIB_FILE := $(GIR_NAMESPACE)-$(GIR_VERSION).typelib

# ── Test framework ───────────────────────────────────────────────────
TEST_CFLAGS := $(CFLAGS) $(shell $(PKG_CONFIG) --cflags glib-2.0)
TEST_LDFLAGS := $(LDFLAGS) -L$(OUTDIR) -l$(LIB_NAME) -Wl,-rpath,$(OUTDIR)

# ── Module flags (absolute include paths for out-of-tree compilation) ─
MODULE_CFLAGS_INC := -I$(CURDIR) -I$(CURDIR)/src
MODULE_CFLAGS_INC += -I$(CURDIR)/$(YAMLGLIB_DIR)/src
MODULE_CFLAGS := $(CFLAGS_BASE) $(CFLAGS_BUILD) $(MODULE_CFLAGS_INC) $(CFLAGS_DEPS)
MODULE_LDFLAGS := -shared -fPIC

# ── Print configuration ──────────────────────────────────────────────
.PHONY: show-config
show-config:
	@echo "gitctl Build Configuration"
	@echo "========================"
	@echo "Version:        $(VERSION_FULL)"
	@echo "API Version:    $(API_VERSION)"
	@echo "Build type:     $(BUILD_TYPE)"
	@echo "Compiler:       $(CC)"
	@echo "CFLAGS:         $(CFLAGS)"
	@echo "LDFLAGS:        $(LDFLAGS)"
	@echo "PREFIX:         $(PREFIX)"
	@echo "LIBDIR:         $(LIBDIR)"
	@echo "MODULEDIR:      $(MODULEDIR)"
	@echo "DEBUG:          $(DEBUG)"
	@echo "ASAN:           $(ASAN)"
	@echo "UBSAN:          $(UBSAN)"
	@echo "BUILD_GIR:      $(BUILD_GIR)"
	@echo "BUILD_TESTS:    $(BUILD_TESTS)"
	@echo "BUILD_MODULES:  $(BUILD_MODULES)"

# ── Package names for install-deps ───────────────────────────────────

FEDORA_DEPS_TOOLS := gcc make pkgconf-pkg-config
FEDORA_DEPS_REQUIRED := glib2-devel json-glib-devel libyaml-devel
FEDORA_DEPS_GIR := gobject-introspection-devel

UBUNTU_DEPS_TOOLS := gcc make pkg-config
UBUNTU_DEPS_REQUIRED := libglib2.0-dev libjson-glib-dev libyaml-dev
UBUNTU_DEPS_GIR := gobject-introspection libgirepository1.0-dev

ARCH_DEPS_TOOLS := gcc make pkgconf
ARCH_DEPS_REQUIRED := glib2 json-glib libyaml
ARCH_DEPS_GIR := gobject-introspection

.PHONY: install-deps
install-deps:
	@if command -v dnf >/dev/null 2>&1; then \
		echo "Detected Fedora/RHEL (dnf)"; \
		sudo dnf install -y $(FEDORA_DEPS_TOOLS) $(FEDORA_DEPS_REQUIRED) \
			$(if $(filter 1,$(BUILD_GIR)),$(FEDORA_DEPS_GIR)); \
	elif command -v apt-get >/dev/null 2>&1; then \
		echo "Detected Ubuntu/Debian (apt)"; \
		sudo apt-get install -y $(UBUNTU_DEPS_TOOLS) $(UBUNTU_DEPS_REQUIRED) \
			$(if $(filter 1,$(BUILD_GIR)),$(UBUNTU_DEPS_GIR)); \
	elif command -v pacman >/dev/null 2>&1; then \
		echo "Detected Arch Linux (pacman)"; \
		sudo pacman -S --needed $(ARCH_DEPS_TOOLS) $(ARCH_DEPS_REQUIRED) \
			$(if $(filter 1,$(BUILD_GIR)),$(ARCH_DEPS_GIR)); \
	else \
		echo "Unknown distro. Required packages:"; \
		echo "  glib-2.0 gobject-2.0 gio-2.0 gmodule-2.0 json-glib-1.0 yaml-0.1"; \
		exit 1; \
	fi
