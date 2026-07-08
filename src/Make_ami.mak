#
# Makefile for AROS, AmigaOS 3.x, AmigaOS 4 and MorphOS.
#
CC := m68k-amigaos-gcc
BIN ?= vim
LD = $(CC)
SHELL = sh

BUILD ?= mui

TARGET := $(shell $(CC) -dumpmachine | cut -d '-' -f1,2)

ENDIAN := \
	$(shell echo | $(CC) -dM -E - | grep \
	"__BYTE_ORDER__.*BIG*" > /dev/null && echo be || echo le)

CONFIG := \
	-DHAVE_GETTIMEOFDAY \
	-DHAVE_SYS_TIME_H \
	-DHAVE_STAT_H \
	-DHAVE_LOCALE_H \
	-DHAVE_STDLIB_H \
	-DHAVE_STRING_H \
	-DHAVE_FCNTL_H \
	-DHAVE_STRICMP \
	-DHAVE_STRNICMP \
	-DHAVE_STRFTIME \
	-DHAVE_SETENV \
	-DHAVE_MEMSET \
	-DHAVE_QSORT \
	-DHAVE_DATE_TIME \
	-DHAVE_STDARG_H \
	-DHAVE_STDINT_H \
	-DHAVE_MATH_H \
	-DHAVE_AVAIL_MEM \
	-DHAVE_ISNAN \
	-DHAVE_ISINF \
	-DHAVE_ERRNO_H

ifeq ($(ENDIAN),be)
CONFIG += -DWORDS_BIGENDIAN
endif

ifeq ($(TARGET),m68k-amigaos)
ifeq ($(BUILD),mui)
override BUILD := huge
endif
endif

CUSTOM := \
	-DMODIFIED_BY=\"'Ola S�der et al.'\"

CFLAGS := \
	-c -O3 \
	-Wall \
	-Wextra \
	-Wno-pointer-sign \
	-Wno-int-conversion \
	-Wno-deprecated-declarations \
	-I proto \
	$(CONFIG) \
	$(CUSTOM)

ifeq ($(TARGET),ppc-amigaos)
LDFLAGS = -lauto
CFLAGS += -DHAVE_FSYNC -D__USE_INLINE__
else
ifeq ($(TARGET),m68k-amigaos)
#CFLAGS += -mcrt=clib2 -std=gnu99
#LDFLAGS = -mcrt=clib2 -lmui -ldebug -lnet -lm

CFLAGS += -noixemul
LDFLAGS = -ldebug -lm -noixemul -lsocket


else
ifeq ($(TARGET),ppc-morphos)
CFLAGS += -noixemul
LDFLAGS = -ldebug -lm -noixemul
else
ifeq ($(TARGET),i386-aros)
CFLAGS += -DHAVE_FSYNC
else
ifeq ($(TARGET),x86_64-aros)
CFLAGS += -DHAVE_FSYNC
endif
endif
endif
endif
endif

# Vim 'huge' build with MUI GUI
ifeq ($(BUILD),mui)
CFLAGS += \
	-DFEAT_GUI \
	-DFEAT_GUI_MUI \
	-DFEAT_BROWSE \
	-DFEAT_TOOLBAR \
	-DFEAT_HUGE
SRC := \
	gui.c \
	gui_mui.c
else

# Vim 'huge' build
ifeq ($(BUILD),huge)
CFLAGS += \
	-DFEAT_HUGE
else

# Vim 'normal' build
ifeq ($(BUILD),normal)
CFLAGS += \
	-DFEAT_NORMAL
else

# Vim 'small' build - now an alias for 'tiny'
ifeq ($(BUILD),small)
CFLAGS += -DFEAT_TINY
else

# Vim 'tiny' build
ifeq ($(BUILD),tiny)
CFLAGS += -DFEAT_TINY
endif
endif
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
	alloc.c \
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
	cmdexpand.c \
	cmdhist.c \
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
	fuzzy.c \
	getchar.c \
	gc.c \
	gui_xim.c \
	hardcopy.c \
	hashtab.c \
	help.c \
	highlight.c \
	if_cscope.c \
	indent.c \
	insexpand.c \
	json.c \
	linematch.c\
	list.c \
	locale.c \
	logfile.c \
	main.c \
	map.c \
	mark.c \
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
	sixel.c \
	kitty.c \
	cairo.c \
	spell.c \
	spellfile.c \
	spellsuggest.c \
	strings.c \
	syntax.c \
	tag.c \
	tabpanel.c \
	term.c \
	termlib.c \
	testing.c \
	textformat.c \
	textobject.c \
	textprop.c \
	time.c \
	tuple.c \
	typval.c \
	ui.c \
	undo.c \
	usercmd.c \
	userfunc.c \
	version.c \
	viminfo.c \
	vim9class.c \
	vim9cmds.c \
	vim9compile.c \
	vim9execute.c \
	vim9expr.c \
	vim9generics.c \
	vim9instr.c \
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
	$(LD) -o $(BIN) $(OBJ) $(LDFLAGS)

# Clean up
.PHONY: clean
clean:
	$(RM) -fv $(shell find . -name '*.o') vi vim gvim evim egvim
