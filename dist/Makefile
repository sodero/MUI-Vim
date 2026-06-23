#------------------------------------------------------------------------------------------
# Settings
#------------------------------------------------------------------------------------------

LHA?=lha
SED?=$(shell which gsed || which sed)
TARGET:=$(shell $(CC) -dumpmachine | cut -d '-' -f1,2)
ifeq ($(shell $(LHA) | head -1 | grep 'LHa for UNIX' | wc -l),1)
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
tmp/$(DEST)-$(TARGET).readme: | tmp
	cat aminet/README.bin | $(SED) -e "s/__VER__/$(VER).$(REV)/" | \
	$(SED) -e "s/__ARC__/$(TARGET)/" > $@

tmp/$(DEST)-$(TARGET).lha: $(DEST)-$(TARGET).lha | tmp
	mv $< $@

tmp:
	mkdir -p $@

#------------------------------------------------------------------------------------------
# Gather files
#------------------------------------------------------------------------------------------
$(DEST)/$(DEST).info: | $(DEST)
	cp -r $(TOP)/runtime $(DEST)
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
