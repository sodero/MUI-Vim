#------------------------------------------------------------------------------------------
# Host settings
#------------------------------------------------------------------------------------------
UNM:=$(shell uname)
ARC:=$(shell uname -m | tr -d ower)
ifeq ($(UNM),MorphOS)
ANM:=$(ARC)-morphos
LHO:=-3rae
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
ICO:=icons
BIN:=$(DST)/bin
PRG:=$(TOP)/src/vim
TXT:=$(wildcard *.txt)
VER:=$(shell cat $(TOP)/src/.ver)
REV:=$(shell cat $(TOP)/src/.pat)

#------------------------------------------------------------------------------------------
# Aminet preparation.
#------------------------------------------------------------------------------------------
upload: $(DST)_$(VER)-$(ANM).lha
	mkdir -p $@ && cp $< $@ && \
	cat aminet/README.bin | sed -e "s/__VER__/$(VER).$(REV)/" | \
	sed -e "s/__ARC__/$(ANM)/" > $@/$(DST)_$(VER)-$(ANM).readme && \
	cat aminet/README.src | sed -e "s/__VER__/$(VER).$(REV)/" > \
	$@/$(DST)_$(VER)-src.readme

#------------------------------------------------------------------------------------------
# Gather files.
#------------------------------------------------------------------------------------------
$(BIN)/gVim: $(PRG)
	mkdir -p $(DST) $(BIN) && \
	cp $(ICO)/$(UNM)/*.info $(DST) && \
    mv $(DST)/$(DST).info . && mv $(DST)/gVim.info $(BIN) && \
	for file in $(TXT); do cp $$file $(DST); done && \
	cp -r $(TOP)/runtime $(DST) && \
	cp *installer $(DST) && \
	cp $(PRG) $@

#------------------------------------------------------------------------------------------
# Create archive
#------------------------------------------------------------------------------------------
$(DST)_$(VER)-$(ANM).lha: $(BIN)/gVim
	lha $(LHO) a $@ $(DST) && \
	lha $(LHO) a $@ $(DST).info && \
	rm -f $(DST).info

#------------------------------------------------------------------------------------------
# Phony targets
#------------------------------------------------------------------------------------------
.PHONY: clean
clean:
	rm -Rf $(DST) $(DST).info Vim_$(VER)-$(ANM).lha README.bin README.src *.readme upload
