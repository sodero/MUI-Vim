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
PRG:=$(TOP)/src/vim
TXT:=$(wildcard *.txt)
VER:=$(shell cat $(TOP)/src/.ver)
REV:=$(shell cat $(TOP)/src/.pat)

#------------------------------------------------------------------------------------------
# Create Lha archive and Aminet readme.
#------------------------------------------------------------------------------------------
archive: $(DST)_$(VER)-$(ANM).lha
	mkdir -p $@ && mv $< $@ && \
	cat aminet/README.bin | sed -e "s/__VER__/$(VER).$(REV)/" | \
	sed -e "s/__ARC__/$(ANM)/" > $@/$(DST)_$(VER)-$(ANM).readme && \
	cat aminet/README.src | sed -e "s/__VER__/$(VER).$(REV)/" > \
	$@/$(DST)_$(VER)-src.readme

#------------------------------------------------------------------------------------------
# Gather files.
#------------------------------------------------------------------------------------------
$(DST)/vim: $(PRG)
	$(MAKE) -C $(TOP) && mkdir -p $(DST) && \
	cp $(EXT)/$(UNM)/* $(DST) && mv $(DST)/$(DST).info . && \
	protect $(DST)/gVim +s && protect $(DST)/eVim +s &&protect $(DST)/vi +s && \
	for file in $(TXT); do cp $$file $(DST); done && \
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
	rm -Rf archive
