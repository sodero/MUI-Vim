#------------------------------------------------------------------------------------------
# Host system settings
#------------------------------------------------------------------------------------------
UNM:=$(shell uname)
ARC:=$(shell uname -m | tr -d ower)
VER:=$(shell cat ../src/.ver)
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

#------------------------------------------------------------------------------------------
# Create archive
#------------------------------------------------------------------------------------------
Vim_$(VER)-$(ANM).lha: copy
	lha $(LHO) a $@ $(DST) && \
	lha $(LHO) a $@ $(DST).info && \
	rm -f $(DST).info

$(DST): $(PRG)
	mkdir $@ $(BIN)

#------------------------------------------------------------------------------------------
# Gather all files.
#------------------------------------------------------------------------------------------
.PHONY: copy
copy: $(DST)
	cp $(ICO)/$(UNM)/*.info $< && \
    mv $(DST)/$(DST).info . && mv $(DST)/gVim.info $(BIN)
	for file in $(TXT); do cp $$file $<; done
	cp -r $(TOP)/runtime $<
	cp $(PRG) $(BIN)/gVim
	cp *installer $<

#------------------------------------------------------------------------------------------
# Phony targets
#------------------------------------------------------------------------------------------
.PHONY: clean
clean:
	rm -Rf $(DST) $(DST).info Vim_$(VER)-$(ANM).lha
