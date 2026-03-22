# Makefile - GitHub forge module
#
# Copyright (C) 2026 Zach Podbielniak
# SPDX-License-Identifier: AGPL-3.0-or-later

MODULE_NAME := github
MODULE_SRC  := gitctl-github-forge.c

CC      ?= gcc

.PHONY: all
all: $(OUTDIR)/$(MODULE_NAME).so

$(OUTDIR)/$(MODULE_NAME).so: $(MODULE_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
