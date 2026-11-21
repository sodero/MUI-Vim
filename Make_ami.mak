#------------------------------------------------------------------------------------------
# General settings
#------------------------------------------------------------------------------------------
SRC:=src
# The default is AmigaOS 3, use "make CC=ppc-amigaos-gcc" etc. for the others.
ifeq ($(origin CC),default)
CC:=m68k-amigaos-gcc
endif
VER=$(shell cat $(SRC)/.ver)
PAT=$(shell cat $(SRC)/.pat)
BLD:=$(shell date +%Y-%m-%d || echo 1985-07-23)

#------------------------------------------------------------------------------------------
# Build Vim - Both MUI and Huge.
#------------------------------------------------------------------------------------------
.PHONY: vim
vim: $(SRC)/.ver $(SRC)/.pat
	$(MAKE) -C $(SRC) -f Make_ami.mak CC=$(CC) clean
	$(MAKE) -C $(SRC) -f Make_ami.mak CC=$(CC) PATCHLEVEL=$(PAT) BUILDDATE=$(BLD) BUILD=mui
	mv $(SRC)/vim $@
	$(MAKE) -C $(SRC) -f Make_ami.mak CC=$(CC) clean
	$(MAKE) -C $(SRC) -f Make_ami.mak CC=$(CC) PATCHLEVEL=$(PAT) BUILDDATE=$(BLD) BUILD=huge
	mv $(SRC)/vim $(SRC)/vi
	mv $@ $(SRC)/vim

#------------------------------------------------------------------------------------------
# Create archive
#------------------------------------------------------------------------------------------
.PHONY: dist
dist: vim
	$(MAKE) -C $@ CC=$(CC) VER=$(VER) REV=$(PAT)

#------------------------------------------------------------------------------------------
# Determine version
#------------------------------------------------------------------------------------------
$(SRC)/.ver: $(SRC)/version.h
	awk '/^#define VIM_VERSION_MAJOR[ \t]/ { maj = $$3 } \
	     /^#define VIM_VERSION_MINOR[ \t]/ { print maj "." $$3; exit }' $< > $@

#------------------------------------------------------------------------------------------
# Determine patch number
#------------------------------------------------------------------------------------------
$(SRC)/.pat: $(SRC)/version.c
	echo $(shell grep -E -m1 "^ \{4\}[0-9]\{1,4\}," $< || \
	grep -m1 "^ \{4\}[0-9]\{1,4\}," $<) | tr -d "[:space:]," > $@

#------------------------------------------------------------------------------------------
# Clean up
#------------------------------------------------------------------------------------------
.PHONY: clean
clean:
	$(MAKE) -C dist $@
	$(MAKE) -C $(SRC) -f Make_ami.mak $@
	rm -f $(SRC)/.pat $(SRC)/.ver $(SRC)/vi $(SRC)/vim
