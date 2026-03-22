# rules.mk - gitctl Build Rules
# Pattern rules and common build recipes
#
# Copyright (C) 2026 Zach Podbielniak
# SPDX-License-Identifier: AGPL-3.0-or-later

# ── Source dependencies on generated headers ─────────────────────────
$(LIB_OBJS): src/gitctl-version.h
$(MAIN_OBJ): src/gitctl-version.h

# ── Object file compilation ──────────────────────────────────────────

$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	@$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJDIR)/core/%.o: src/core/%.c | $(OBJDIR)
	@$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJDIR)/boxed/%.o: src/boxed/%.c | $(OBJDIR)
	@$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJDIR)/interfaces/%.o: src/interfaces/%.c | $(OBJDIR)
	@$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJDIR)/commands/%.o: src/commands/%.c | $(OBJDIR)
	@$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJDIR)/module/%.o: src/module/%.c | $(OBJDIR)
	@$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# ── Test compilation ─────────────────────────────────────────────────

$(OBJDIR)/tests/%.o: tests/%.c | $(OBJDIR)
	@$(MKDIR_P) $(dir $@)
	$(CC) $(TEST_CFLAGS) -MMD -MP -c $< -o $@

.PRECIOUS: $(OBJDIR)/tests/%.o

# ── Dependency compilation: yaml-glib ────────────────────────────────

$(OBJDIR)/deps/yaml-glib/src/%.o: deps/yaml-glib/src/%.c | $(OBJDIR)
	@$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# ── Static library ───────────────────────────────────────────────────

$(OUTDIR)/$(LIB_STATIC): $(LIB_OBJS) $(DEP_OBJS)
	@$(MKDIR_P) $(dir $@)
	$(AR) rcs $@ $^

# ── Shared library ───────────────────────────────────────────────────

$(OUTDIR)/$(LIB_SHARED_FULL): $(LIB_OBJS) $(DEP_OBJS)
	@$(MKDIR_P) $(dir $@)
	$(CC) $(LDFLAGS_SHARED) -o $@ $^ $(LDFLAGS)
	cd $(OUTDIR) && ln -sf $(LIB_SHARED_FULL) $(LIB_SHARED_MAJOR)
	cd $(OUTDIR) && ln -sf $(LIB_SHARED_MAJOR) $(LIB_SHARED)

# ── Executable ───────────────────────────────────────────────────────

$(OUTDIR)/gitctl: $(MAIN_OBJ) $(OUTDIR)/$(LIB_SHARED_FULL)
	$(CC) -o $@ $(MAIN_OBJ) -L$(OUTDIR) -l$(LIB_NAME) $(LDFLAGS) -Wl,-rpath,'$$ORIGIN'

# ── GIR generation ───────────────────────────────────────────────────

$(OUTDIR)/$(GIR_FILE): $(LIB_SRCS) $(LIB_HDRS) | $(OUTDIR)/$(LIB_SHARED_FULL)
	$(GIR_SCANNER) \
		--namespace=$(GIR_NAMESPACE) \
		--nsversion=$(GIR_VERSION) \
		--library=$(LIB_NAME) \
		--library-path=$(OUTDIR) \
		--include=GLib-2.0 \
		--include=GObject-2.0 \
		--include=Gio-2.0 \
		--pkg=glib-2.0 \
		--pkg=gobject-2.0 \
		--pkg=gio-2.0 \
		--output=$@ \
		--warn-all \
		-Isrc \
		$(LIB_HDRS) $(LIB_SRCS)

$(OUTDIR)/$(TYPELIB_FILE): $(OUTDIR)/$(GIR_FILE)
	$(GIR_COMPILER) --output=$@ $<

# ── Generated files ──────────────────────────────────────────────────

src/gitctl-version.h: src/gitctl-version.h.in
	sed \
		-e 's|@GCTL_VERSION_MAJOR@|$(VERSION_MAJOR)|g' \
		-e 's|@GCTL_VERSION_MINOR@|$(VERSION_MINOR)|g' \
		-e 's|@GCTL_VERSION_MICRO@|$(VERSION_MICRO)|g' \
		-e 's|@GCTL_VERSION@|$(VERSION)|g' \
		$< > $@

$(OUTDIR)/gitctl-$(API_VERSION).pc: gitctl-$(API_VERSION).pc.in | $(OUTDIR)
	sed \
		-e 's|@PREFIX@|$(PREFIX)|g' \
		-e 's|@LIBDIR@|$(LIBDIR)|g' \
		-e 's|@INCLUDEDIR@|$(INCLUDEDIR)|g' \
		-e 's|@VERSION@|$(VERSION)|g' \
		-e 's|@API_VERSION@|$(API_VERSION)|g' \
		$< > $@

# ── Directory creation ───────────────────────────────────────────────

$(BUILDDIR):
	@$(MKDIR_P) $(BUILDDIR)

$(OBJDIR): | $(BUILDDIR)
	@$(MKDIR_P) $(OBJDIR)
	@$(MKDIR_P) $(OBJDIR)/core
	@$(MKDIR_P) $(OBJDIR)/boxed
	@$(MKDIR_P) $(OBJDIR)/interfaces
	@$(MKDIR_P) $(OBJDIR)/commands
	@$(MKDIR_P) $(OBJDIR)/module
	@$(MKDIR_P) $(OBJDIR)/tests
	@$(MKDIR_P) $(OBJDIR)/deps/yaml-glib/src

$(OUTDIR):
	@$(MKDIR_P) $(OUTDIR)

$(OUTDIR)/modules:
	@$(MKDIR_P) $(OUTDIR)/modules

# ── Clean rules ──────────────────────────────────────────────────────

.PHONY: clean clean-all

clean:
	rm -rf $(BUILDDIR)/$(BUILD_TYPE)
	rm -f src/gitctl-version.h

clean-all:
	rm -rf $(BUILDDIR)
	rm -f src/gitctl-version.h

# ── Installation rules ───────────────────────────────────────────────

.PHONY: install uninstall

install: install-lib install-headers install-pc install-bin
ifeq ($(BUILD_GIR),1)
install: install-gir
endif
ifeq ($(BUILD_MODULES),1)
install: install-modules
endif

install-lib: $(OUTDIR)/$(LIB_STATIC) $(OUTDIR)/$(LIB_SHARED_FULL)
	$(MKDIR_P) $(DESTDIR)$(LIBDIR)
	$(INSTALL_DATA) $(OUTDIR)/$(LIB_STATIC) $(DESTDIR)$(LIBDIR)/
	$(INSTALL_PROGRAM) $(OUTDIR)/$(LIB_SHARED_FULL) $(DESTDIR)$(LIBDIR)/
	cd $(DESTDIR)$(LIBDIR) && ln -sf $(LIB_SHARED_FULL) $(LIB_SHARED_MAJOR)
	cd $(DESTDIR)$(LIBDIR) && ln -sf $(LIB_SHARED_MAJOR) $(LIB_SHARED)
	@if [ -z "$(DESTDIR)" ] && command -v ldconfig >/dev/null 2>&1; then \
		echo "Updating shared library cache..."; \
		ldconfig; \
	fi

install-bin: $(OUTDIR)/gitctl
	$(MKDIR_P) $(DESTDIR)$(BINDIR)
	$(INSTALL_PROGRAM) $(OUTDIR)/gitctl $(DESTDIR)$(BINDIR)/

install-headers:
	$(MKDIR_P) $(DESTDIR)$(INCLUDEDIR)/gitctl
	$(INSTALL_DATA) src/gitctl-version.h $(DESTDIR)$(INCLUDEDIR)/gitctl/
	@for dir in core boxed interfaces commands module; do \
		if ls src/$$dir/*.h >/dev/null 2>&1; then \
			$(MKDIR_P) $(DESTDIR)$(INCLUDEDIR)/gitctl/$$dir; \
			$(INSTALL_DATA) src/$$dir/*.h $(DESTDIR)$(INCLUDEDIR)/gitctl/$$dir/; \
		fi; \
	done
	@if ls src/*.h >/dev/null 2>&1; then \
		$(INSTALL_DATA) src/*.h $(DESTDIR)$(INCLUDEDIR)/gitctl/; \
	fi

install-pc: $(OUTDIR)/gitctl-$(API_VERSION).pc
	$(MKDIR_P) $(DESTDIR)$(PKGCONFIGDIR)
	$(INSTALL_DATA) $(OUTDIR)/gitctl-$(API_VERSION).pc $(DESTDIR)$(PKGCONFIGDIR)/

install-gir: $(OUTDIR)/$(GIR_FILE) $(OUTDIR)/$(TYPELIB_FILE)
	$(MKDIR_P) $(DESTDIR)$(GIRDIR)
	$(MKDIR_P) $(DESTDIR)$(TYPELIBDIR)
	$(INSTALL_DATA) $(OUTDIR)/$(GIR_FILE) $(DESTDIR)$(GIRDIR)/
	$(INSTALL_DATA) $(OUTDIR)/$(TYPELIB_FILE) $(DESTDIR)$(TYPELIBDIR)/

install-modules:
	$(MKDIR_P) $(DESTDIR)$(MODULEDIR)
	@for mod in $(OUTDIR)/modules/*.so; do \
		if [ -f "$$mod" ]; then \
			$(INSTALL_DATA) "$$mod" $(DESTDIR)$(MODULEDIR)/; \
		fi; \
	done

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/gitctl
	rm -f $(DESTDIR)$(LIBDIR)/$(LIB_STATIC)
	rm -f $(DESTDIR)$(LIBDIR)/$(LIB_SHARED_FULL)
	rm -f $(DESTDIR)$(LIBDIR)/$(LIB_SHARED_MAJOR)
	rm -f $(DESTDIR)$(LIBDIR)/$(LIB_SHARED)
	rm -rf $(DESTDIR)$(INCLUDEDIR)/gitctl
	rm -f $(DESTDIR)$(PKGCONFIGDIR)/gitctl-$(API_VERSION).pc
	rm -f $(DESTDIR)$(GIRDIR)/$(GIR_FILE)
	rm -f $(DESTDIR)$(TYPELIBDIR)/$(TYPELIB_FILE)
	rm -rf $(DESTDIR)$(MODULEDIR)
