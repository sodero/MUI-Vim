#------------------------------------------------------------------------------------------
# Host settings
#------------------------------------------------------------------------------------------
LHA?=lha
CPU?=$(shell uname -m | tr -d ower)
UNM?=$(shell uname)
ANM:=$(CPU)-$(shell echo $(UNM) | tr A-Z a-z)
ifeq ($(UNM),AmigaOS)
LHAFLAGS?=-r a
else ifeq ($(UNM),MorphOS)
LHAFLAGS?=-r a
else
LHAFLAGS?=ao5
endif
ifeq ($(UNM),Darwin)
SED?=gsed
else
SED?=sed
endif

#------------------------------------------------------------------------------------------
# General settings
#------------------------------------------------------------------------------------------
TOP:=..
SRC:=$(TOP)/src
VIM:=$(SRC)/vim
VI:=$(SRC)/vi
EXT:=extras
DST:=Vim
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
	cp *.png $(DST)

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
	rm -Rf $(DST) tmp gVim vi *.info *.lha
