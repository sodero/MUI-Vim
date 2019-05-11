#------------------------------------------------------------------------------------------
# Naming and paths
#------------------------------------------------------------------------------------------
VIM:=Vim
SRC:=src
NST:=installer
UNM:=$(shell uname)
ARC:=$(shell uname -m | tr -d ower)

ifeq ($(UNM),MorphOS)
ANM:=$(ARC)-morphos
LHO:=-3rae
else ifeq ($(UNM),AmigaOS4)
ANM:=$(ARC)-amigaos
else ifeq ($(UNM),AROS)
ANM:=$(ARC)-aros
endif

#------------------------------------------------------------------------------------------
# Phony standard targets
#------------------------------------------------------------------------------------------
.PHONY: all
all: $(SRC)/vim

.PHONY: clean
clean:
	$(MAKE) -C $(SRC) -f Make_ami.mak clean
	rm -Rf *.lha $(SRC)/.pat $(SRC)/.ver .arc

#------------------------------------------------------------------------------------------
# Build Vim
#------------------------------------------------------------------------------------------
$(SRC)/vim: $(SRC)/.ver $(SRC)/.pat
	$(MAKE) -C $(SRC) -f Make_ami.mak PATCHLEVEL=`cat .pat`

#------------------------------------------------------------------------------------------
# Determine version
#------------------------------------------------------------------------------------------
$(SRC)/.ver: $(SRC)/version.h
	grep "#.\{1,\}_SHORT" $< | sed -e "s/.*\"\([0-9]\.[0-9]\)\".*/\1/" > $@

#------------------------------------------------------------------------------------------
# Determine patch number
#------------------------------------------------------------------------------------------
$(SRC)/.pat: $(SRC)/version.c
	grep -m1 "^ \{4\}[0-9]\{1,4\},$$" $< | tr -d "[:space:]," > $@

#------------------------------------------------------------------------------------------
# Create the Lha archives
#------------------------------------------------------------------------------------------
.arc: $(VIM)
	lha $(LHO) a $(SRC).lha $(SRC)
	lha d $(SRC).lha $(SRC)/src/vim $(SRC)/src/*.o $(SRC)/src/xdiff/*.o
	cp $(NST)/$(VIM).info_ $(VIM).info
	lha $(LHO) a $^.lha $^ && lha a $^.lha $(VIM).info
	rm -Rf dist $(VIM).info && mkdir dist
	mv $^.lha dist/$(VIM)_`cat .ver`-$(ANM).lha
	mv $(SRC).lha dist/$(VIM)_`cat .ver`-src.lha
	cp aminet/$(UNM).readme dist/_ 
	sed -i "s/__VER__/`cat .ver`.`cat .pat`/" dist/_
	mv dist/_ dist/$(VIM)_`cat .ver`-$(ANM).readme
	cp aminet/src.readme dist/_ 
	sed -i "s/__VER__/`cat .ver`.`cat .pat`/" dist/_
	mv dist/_ dist/$(VIM)_`cat .ver`-src.readme
	touch $@

#------------------------------------------------------------------------------------------
# Put things that are to be included by the installer in place
#------------------------------------------------------------------------------------------
$(VIM): $(SRC)/src/vim
	mkdir -p $@
	cp $(NST)/README.__OS__.txt $@/README.$(UNM).txt
	cp $(NST)/README.__OS__.txt.info_ $@/README.$(UNM).txt.info_
	cp $(NST)/CHANGELOG.__OS__.txt $@/CHANGELOG.$(UNM).txt
	cp $(NST)/CHANGELOG.__OS__.txt.info_ $@/CHANGELOG.$(UNM).txt.info_
	cp $(NST)/Vim* $(NST)/gvim* $(NST)/README.txt.* $(NST)/INST* $@
	sed -i "s/__OS__/$(UNM)/g" "$@/Vim installer"
	cp $^ $@/vim && cp $(SRC)/*.md $(SRC)/*.txt $@
	cp -R $(SRC)/runtime $@ && touch $@
