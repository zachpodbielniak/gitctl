# Makefile - Gitea forge module
#
# Copyright (C) 2026 Zach Podbielniak
# SPDX-License-Identifier: AGPL-3.0-or-later

MODULE_NAME := gitea
MODULE_SRC  := gitctl-gitea-forge.c

CC      ?= gcc

.PHONY: all
all: $(OUTDIR)/$(MODULE_NAME).so

$(OUTDIR)/$(MODULE_NAME).so: $(MODULE_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
