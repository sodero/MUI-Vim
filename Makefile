#------------------------------------------------------------------------------------------
# Naming and paths 
#------------------------------------------------------------------------------------------
VIM:=Vim
DWN:=download
DST:=unpacked
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
all: .arc

.PHONY: clean
clean:
	rm -Rf $(DWN) $(DST) $(VIM) vim-master dist *.lha .pat .ver .arc 

#------------------------------------------------------------------------------------------
# Build Vim
#------------------------------------------------------------------------------------------
$(DST)/src/vim: $(DST)/src/.pat
	$(MAKE) -C $(DST)/src -f Make_ami.mak PATCHLEVEL=`cat .pat`

#------------------------------------------------------------------------------------------
# Put MUI code and runtime files in place and apply patches
#------------------------------------------------------------------------------------------
$(DST)/src/.pat: $(DST)
	cp -R files/* $(DST)
	for f in `find $(DST) -name ".*"`; do rm -Rf $$f; done
	set -e; for f in patches/*.patch; do patch -p1 -d $(DST)/src -N < $$f; done && touch $@

#------------------------------------------------------------------------------------------
# Unpack sources and determine version and patch number
#------------------------------------------------------------------------------------------
$(DST): $(DWN)/$(VIM).tar.gz
	tar xvfzm $^ --no-same-owner && cp -R vim-master $@
	grep -m1 "^ \{4\}[0-9]\{1,4\},$$" $@/src/version.c | tr -d "[:space:]," > .pat
	grep "#.\{1,\}_SHORT" $@/src/version.h | sed -e "s/.*\"\([0-9]\.[0-9]\)\".*/\1/" >.ver

#------------------------------------------------------------------------------------------
# Fetch upstream sources
#------------------------------------------------------------------------------------------
$(DWN)/$(VIM).tar.gz:
	mkdir -p $(DWN)
	curl -k -L -o $@ https://github.com/vim/vim/archive/master.tar.gz

#------------------------------------------------------------------------------------------
# Create the Lha archives
#------------------------------------------------------------------------------------------
.arc: $(VIM)
	lha $(LHO) a $(DST).lha $(DST)
	lha d $(DST).lha $(DST)/src/vim $(DST)/src/*.o $(DST)/src/xdiff/*.o
	cp $(NST)/$(VIM).info_ $(VIM).info
	lha $(LHO) a $^.lha $^ && lha a $^.lha $(VIM).info
	rm -Rf dist $(VIM).info && mkdir dist
	mv $^.lha dist/$(VIM)_`cat .ver`-$(ANM).lha
	mv $(DST).lha dist/$(VIM)_`cat .ver`-src.lha
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
$(VIM): $(DST)/src/vim
	mkdir -p $@
	cp $(NST)/README.__OS__.txt $@/README.$(UNM).txt
	cp $(NST)/README.__OS__.txt.info_ $@/README.$(UNM).txt.info_
	cp $(NST)/CHANGELOG.__OS__.txt $@/CHANGELOG.$(UNM).txt
	cp $(NST)/CHANGELOG.__OS__.txt.info_ $@/CHANGELOG.$(UNM).txt.info_
	cp $(NST)/Vim* $(NST)/gvim* $(NST)/README.txt.* $(NST)/INST* $@
	sed -i "s/__OS__/$(UNM)/g" "$@/Vim installer"
	cp $^ $@/vim && cp $(DST)/*.md $(DST)/*.txt $@
	cp -R $(DST)/runtime $@ && touch $@
