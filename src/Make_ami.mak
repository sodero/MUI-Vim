#
# Makefile for AROS, AmigaOS4 and MorphOS.
#
BIN = vim
CC ?= gcc
LD = $(CC)
UNM ?= $(shell uname)
DEBUG ?= no
BUILD ?= mui
CFLAGS = -c -O3

# Common compiler flags
CFLAGS += \
	-DUSE_TMPNAM \
	-DNEW_SHELLSIZE \
	-I proto \
	-Wall \
	-Wno-pointer-sign \
	-Wno-int-conversion \
	-Wno-deprecated-declarations

# Vim 'huge' build with MUI GUI
ifeq ($(BUILD),mui)
CFLAGS += \
	-DFEAT_GUI \
	-DFEAT_GUI_MUI \
	-DFEAT_BROWSE \
	-DFEAT_TOOLBAR \
	-DFEAT_HUGE \
	-DMODIFIED_BY="Ola\ Söder\ et\ al."
SRC := \
	gui.c \
	gui_mui.c
ifneq ($(UNM),AROS)
CFLAGS += -DMUIVIM_FEAT_SCROLLBAR
endif
else

# Vim 'huge' build
ifeq ($(BUILD),huge)
CFLAGS += \
	-DFEAT_BROWSE \
	-DFEAT_MOUSE \
	-DFEAT_HUGE
else

# Vim 'big' build
ifeq ($(BUILD),big)
CFLAGS += \
	-DFEAT_BROWSE \
	-DFEAT_MOUSE \
	-DFEAT_BIG
else

# Vim 'normal' build
ifeq ($(BUILD),normal)
CFLAGS +=\
	-DFEAT_BROWSE \
	-DFEAT_MOUSE \
	-DFEAT_NORMAL
else

# Vim 'small' build
ifeq ($(BUILD),small)
CFLAGS += -DFEAT_SMALL
else

# Vim 'tiny' build
ifeq ($(BUILD),tiny)
CFLAGS += -DFEAT_TINY
endif
endif
endif
endif
endif
endif

# OS specific compiler flags
ifeq ($(UNM),AmigaOS)
LDFLAGS = -mcrt=clib2 -lauto -lm -lnet
CFLAGS += -DHAVE_FSYNC -D__USE_INLINE__ -mcrt=clib2
else
ifeq ($(UNM),AROS)
LDFLAGS = -DHAVE_FSYNC -ldebug
else
ifeq ($(UNM),MorphOS)
CFLAGS += -noixemul
LDFLAGS = -ldebug -noixemul
else
CFLAGS += -DNO_ARP
endif
endif
endif

# Patch level used for Amiga style version string
ifdef PATCHLEVEL
CFLAGS += -DPATCHLEVEL=\"$(PATCHLEVEL)\"
endif

# Build date used for Amiga style version string
ifdef BUILDDATE
CFLAGS += -DBUILDDATE=\"$(BUILDDATE)\"
endif

# Common sources
SRC += \
	arabic.c \
	arglist.c \
	autocmd.c \
	beval.c \
	blob.c \
	blowfish.c \
	buffer.c \
	bufwrite.c \
	change.c \
	charset.c \
	cindent.c \
	clientserver.c \
	clipboard.c \
	cmdhist.c \
	cmdexpand.c \
	crypt.c \
	crypt_zip.c \
	debugger.c \
	dict.c \
	diff.c \
	digraph.c \
	drawline.c \
	drawscreen.c \
	edit.c \
	eval.c \
	evalbuffer.c \
	evalfunc.c \
	evalvars.c \
	evalwindow.c \
	ex_cmds.c \
	ex_cmds2.c \
	ex_docmd.c \
	ex_eval.c \
	ex_getln.c \
	fileio.c \
	filepath.c \
	findfile.c \
	float.c \
	fold.c \
	getchar.c \
	hardcopy.c \
	hashtab.c \
	help.c \
	highlight.c \
	if_cscope.c \
	indent.c \
	insexpand.c \
	json.c \
	list.c \
	locale.c \
	main.c \
	mark.c \
	map.c \
	match.c \
	mbyte.c \
	memfile.c \
	memline.c \
	menu.c \
	message.c \
	misc1.c \
	misc2.c \
	mouse.c \
	move.c \
	normal.c \
	ops.c \
	option.c \
	optionstr.c \
	os_amiga.c \
	popupmenu.c \
	popupwin.c \
	profiler.c \
	quickfix.c \
	regexp.c \
	register.c \
	screen.c \
	scriptfile.c \
	search.c \
	session.c \
	sha256.c \
	sign.c \
	spell.c \
	spellfile.c \
	spellsuggest.c \
	syntax.c \
	tag.c \
	term.c \
	termlib.c \
	testing.c \
	textformat.c \
	textobject.c \
	textprop.c \
	time.c \
	typval.c \
	ui.c \
	undo.c \
	usercmd.c \
	userfunc.c \
	version.c \
	viminfo.c \
	vim9compile.c \
	vim9execute.c \
	vim9script.c \
	vim9type.c \
	window.c \
	xdiff/xdiffi.c \
	xdiff/xemit.c \
	xdiff/xhistogram.c \
	xdiff/xpatience.c \
	xdiff/xprepare.c \
	xdiff/xutils.c

OBJ = $(SRC:.c=.o)

# Build everything - Ignoring header dependencies.
$(BIN): $(OBJ)
	${LD} -o $(BIN) $(OBJ) $(LDFLAGS)

# Clean up
.PHONY: clean
clean:
	$(RM) -fv $(OBJ) $(BIN)
