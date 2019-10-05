#------------------------------------------------------------------------------------------
# Host settings
#------------------------------------------------------------------------------------------
UNM?=$(shell uname)
ARC?=$(shell uname -m | tr -d ower)
ANM:=$(ARC)-$(shell echo $(UNM) | tr A-Z a-z)
ifeq ($(UNM),AmigaOS4)
ANM:=$(ARC)-amigaos
else ifeq ($(UNM),AROS)
ANM:=$(ARC)-aros
else ifeq ($(UNM),MorphOS)
ANM:=$(ARC)-morphos
LHO:=-3rae
#-This is just for testing purposes--------------------------------------------------------
else ifeq ($(UNM),Darwin)
ANM:=$(ARC)-darwin
PRO:=@echo
SED:=gsed
#-This is just for testing purposes--------------------------------------------------------
else ifeq ($(UNM),Linux)
ANM:=$(ARC)-linux
PRO:=@echo
else
#-This is just for testing purposes--------------------------------------------------------
ANM:=$(ARC)-unknown
UNM:="Unknown"
PRO:=@echo
endif

#------------------------------------------------------------------------------------------
# General settings
#------------------------------------------------------------------------------------------
TOP:=..
SRC:=$(TOP)/src
VIM:=$(SRC)/vim
PRO?=protect
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

$(DST)_$(VER)-$(ANM).lha: $(DST)/vim
	lha $(LHO) a $@ $(DST) $(DST).info

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
	cp *.txt $(DST)
	cp gVim $(DST)
	cp vi $(DST)

$(DST).info: | $(DST)/$(DST).info
	mv $(DST)/$(DST).info .
	$(PRO) $(DST)/gVim +s
	$(PRO) $(DST)/vi +s

$(DST)/vim: | $(DST).info
	cp $(VIM) $@

$(DST):
	mkdir $@

#------------------------------------------------------------------------------------------
# Clean up
#------------------------------------------------------------------------------------------
.PHONY: clean
clean:
	rm -Rf tmp Vim Vim.info *.lha
