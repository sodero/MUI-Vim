#------------------------------------------------------------------------------------------
# Host settings
#------------------------------------------------------------------------------------------
UNM?=$(shell uname)
ARC?=$(shell uname -m | tr -d ower)
ifeq ($(UNM),MorphOS)
ANM:=$(ARC)-morphos
LHO:=-rae
else ifeq ($(UNM),AmigaOS4)
ANM:=$(ARC)-amigaos
else ifeq ($(UNM),AROS)
ANM:=$(ARC)-aros
else
ANM:=$(ARC)-unknown
UNM:="Unknown"
endif

#------------------------------------------------------------------------------------------
# Paths and files
#------------------------------------------------------------------------------------------
TOP:=..
DST:=Vim
EXT:=extras
PRO:=protect
SRC:=$(TOP)/src
PRG:=$(SRC)/vim
ALL:=$(wildcard *.txt) vi gVim eVim
VER:=$(shell cat $(SRC)/.ver)
REV:=$(shell cat $(SRC)/.pat)

#------------------------------------------------------------------------------------------
# Create Lha archive and Aminet readme.
#------------------------------------------------------------------------------------------
tmp: $(DST)_$(VER)-$(ANM).lha
	mkdir -p $@ && mv $< $@ && \
	cat aminet/README.bin | sed -e "s/__VER__/$(VER).$(REV)/" | \
	sed -e "s/__ARC__/$(ANM)/" > $@/$(DST)_$(VER)-$(ANM).readme

#------------------------------------------------------------------------------------------
# Gather files.
#------------------------------------------------------------------------------------------
$(DST)/vim: $(PRG)
	$(MAKE) -C $(SRC) -f Make_ami.mak && mkdir -p $(DST) && \
	cp $(EXT)/$(UNM)/* $(DST) && mv $(DST)/$(DST).info . && \
	$(PRO) $(DST)/gVim +s && $(PRO) $(DST)/eVim +s && $(PRO) $(DST)/vi +s && \
	for file in $(ALL); do cp $$file $(DST); done && \
	cp -r $(TOP)/runtime $(DST) && \
	cp $(PRG) $@

#------------------------------------------------------------------------------------------
# Create archive
#------------------------------------------------------------------------------------------
$(DST)_$(VER)-$(ANM).lha: $(DST)/vim
	lha $(LHO) a $@ $(DST) && \
	lha $(LHO) a $@ $(DST).info && \
	rm -Rf $(DST) $(DST).info

#------------------------------------------------------------------------------------------
# Phony targets
#------------------------------------------------------------------------------------------
.PHONY: clean
clean:
	rm -Rf tmp
