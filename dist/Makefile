#------------------------------------------------------------------------------------------
# Host settings
#------------------------------------------------------------------------------------------
LHA?=lha
UNM?=$(shell uname)
ARC?=$(shell uname -m | tr -d ower)
ANM:=$(ARC)-$(shell echo $(UNM) | tr A-Z a-z)
#- AmigaOS --------------------------------------------------------------------------------
ifeq ($(UNM),AmigaOS)
ANM:=$(ARC)-amigaos
ifeq ($(shell uname),AmigaOS)
# Native compilation (use default compression)
LHAFLAGS?=-r a
endif
#- AROS -----------------------------------------------------------------------------------
else ifeq ($(UNM),AROS)
# Native or cross.
ANM:=$(ARC)-aros
#- MorphOS --------------------------------------------------------------------------------
else ifeq ($(UNM),MorphOS)
ANM:=$(ARC)-morphos
ifeq ($(shell uname),MorphOS)
# Native compilation (use default compression)
LHAFLAGS?=-r a
endif
#-This is just for testing purposes--------------------------------------------------------
else ifeq ($(UNM),Darwin)
ANM:=$(ARC)-darwin
SED:=gsed
#-This is just for testing purposes--------------------------------------------------------
else ifeq ($(UNM),Linux)
ANM:=$(ARC)-linux
else
#-This is just for testing purposes--------------------------------------------------------
ANM:=$(ARC)-unknown
UNM:="Unknown"
endif
#- Fallback LHA flags (use lha5 for maximum compatibility) --------------------------------
LHAFLAGS?=ao5

#------------------------------------------------------------------------------------------
# General settings
#------------------------------------------------------------------------------------------
TOP:=..
SRC:=$(TOP)/src
VIM:=$(SRC)/vim
VI:=$(SRC)/vi
EXT:=extras
DST:=Vim
SED?=sed
VER?=0
REV?=0

#------------------------------------------------------------------------------------------
# Create archive and Aminet readme
#------------------------------------------------------------------------------------------
.PHONY: all
all: tmp/$(DST)_$(VER)-$(ANM).lha tmp/$(DST)_$(VER)-$(ANM).readme

$(DST)_$(VER)-$(ANM).lha: $(DST)/gVim $(DST)/vi
	$(LHA) $(LHAFLAGS) $@ $(DST)
	$(LHA) $(LHAFLAGS) $@ $(DST).info
ifeq ($(UNM),AROS)
	# This is a weird workaround for a problem that I don't understand;
	# for some reason these two directories are left out on AROS unless
	# they are explicitly included.
	$(LHA) $(LHAFLAGS) $@ $(DST)/runtime/tutor
	$(LHA) $(LHAFLAGS) $@ $(DST)/runtime/tools
endif

#------------------------------------------------------------------------------------------
# Populate temp
#------------------------------------------------------------------------------------------
tmp/$(DST)_$(VER)-$(ANM).readme: | tmp
	cat aminet/README.bin | $(SED) -e "s/__VER__/$(VER).$(REV)/" | \
	$(SED) -e "s/__ARC__/$(ANM)/" > $@

tmp/$(DST)_$(VER)-$(ANM).lha: $(DST)_$(VER)-$(ANM).lha | tmp
	mv $< $@

tmp:
	mkdir -p $@

#------------------------------------------------------------------------------------------
# Gather files
#------------------------------------------------------------------------------------------
$(DST)/$(DST).info: | $(DST)
	cp -r $(TOP)/runtime $(DST)
	cp $(EXT)/$(UNM)/* $(DST)
	cp "Vim installer" $(DST)
	cp *.guide $(DST)

$(DST).info: | $(DST)/$(DST).info
	mv $(DST)/$(DST).info .

$(DST)/gVim: | $(DST).info
	cp $(VIM) $@

$(DST)/vi: | $(DST).info
	cp $(VI) $@

$(DST):
	mkdir $@

#------------------------------------------------------------------------------------------
# Clean up
#------------------------------------------------------------------------------------------
.PHONY: clean
clean:
	rm -Rf tmp gVim vi *.info *.lha
