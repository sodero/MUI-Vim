#------------------------------------------------------------------------------------------
# Settings
#------------------------------------------------------------------------------------------

LHA?=lha
SED?=$(shell which gsed || which sed)
ifeq ($(origin CC),default)
CC:=m68k-amigaos-gcc
endif
TARGET:=$(shell $(CC) -dumpmachine | cut -d '-' -f1,2)
ifeq ($(shell $(LHA) 2>&1 | head -1 | grep -c 'LHa for UNIX'),1)
LHAFLAGS?=ao5
else
LHAFLAGS?=-r a
endif
TOP:=$(patsubst %/,%,$(dir $(patsubst %/,%,$(CURDIR))))
SRC:=$(TOP)/src
VIM:=$(SRC)/vim
VI:=$(SRC)/vi
EXT:=extras
DEST:=MUI-Vim
VER?=0
REV?=0

#------------------------------------------------------------------------------------------
# Create archive and Aminet readme
#------------------------------------------------------------------------------------------
.PHONY: all
all: tmp/$(DEST)-$(TARGET).lha tmp/$(DEST)-$(TARGET).readme

$(DEST)-$(TARGET).lha: $(DEST)/gVim $(DEST)/vi
	lha $(LHAFLAGS) $@ $(DEST)
	lha $(LHAFLAGS) $@ $(DEST).info
ifeq ($(LHAFLAGS),ao5)
	# Workaround for a problem I don't understand; for some reason these two directories
	# are left out when using LHa for Unix unless they are explicitly included.
	lha $(LHAFLAGS) $@ $(DEST)/runtime/tutor
	lha $(LHAFLAGS) $@ $(DEST)/runtime/tools
endif

#------------------------------------------------------------------------------------------
# Populate temp
#------------------------------------------------------------------------------------------
# The CPU requirement is only stated for AmigaOS 3, elsewhere the paragraph
# is removed.
ifeq ($(TARGET),m68k-amigaos)
README_CPU = s/__CPU__/This version requires a 68020 CPU or better./
else
README_CPU = /__CPU__/{N;d;}
endif

tmp/$(DEST)-$(TARGET).readme: | tmp
	cat aminet/README.bin | $(SED) -e "s/__VER__/$(VER).$(REV)/" | \
	$(SED) -e "s/__ARC__/$(TARGET)/" | $(SED) -e "$(README_CPU)" > $@

tmp/$(DEST)-$(TARGET).lha: $(DEST)-$(TARGET).lha | tmp
	mv $< $@

tmp:
	mkdir -p $@

#------------------------------------------------------------------------------------------
# Gather files
#------------------------------------------------------------------------------------------
$(DEST)/$(DEST).info: | $(DEST)
	cp -r $(TOP)/runtime $(DEST)
	# The syntax and indent tests are not needed at runtime.
	rm -Rf $(DEST)/runtime/syntax/testdir $(DEST)/runtime/indent/testdir
ifeq ($(TARGET),m68k-amigaos)
	# FFS on AmigaOS 3 limits file names to 30 characters, longer names make
	# the installer fail.  What remains are MS-Windows locale aliases for the
	# menu translations and one keymap.
	find $(DEST)/runtime -type f -name '???????????????????????????????*' \
		-print -exec rm -f {} +
	# Remapping the true colour toolbar strips to the pens of a palette based
	# screen takes more than ten seconds on a 68020, use palette versions
	# instead (made with mkpalette.py).
	cp palette/*.png $(DEST)/runtime/icons
endif
	cp $(EXT)/$(TARGET)/* $(DEST)
	cat "Install" | $(SED) -e "s/__VER__/$(VER).$(REV)/" > $(DEST)/Install
	cat $(DEST).guide | $(SED) -e "s/__VER__/$(VER).$(REV)/" > $(DEST)/$(DEST).guide
	cp *.png $(DEST)

$(DEST).info: | $(DEST)/$(DEST).info
	mv $(DEST)/$(DEST).info .

$(DEST)/gVim: | $(DEST).info
	cp $(VIM) $@

$(DEST)/vi: | $(DEST).info
	cp $(VI) $@

$(DEST):
	mkdir $@

#------------------------------------------------------------------------------------------
# Clean up
#------------------------------------------------------------------------------------------
.PHONY: clean
clean:
	rm -Rf $(DEST) tmp gVim vi *.info *.lha
