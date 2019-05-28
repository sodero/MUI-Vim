#
# Makefile for AmigaOS4 build.
#

BIN =		vim
CC =		ppc-amigaos-gcc
LD =		ppc-amigaos-gcc

CFLAGS =	-c				\
			-D__USE_INLINE__\
			-mcrt=clib2		\
			-DNO_ARP		\
			-DUSE_TMPNAM	\
			-DHAVE_FSYNC	\
			-DHAVE_STDARG_H	\
			-DHAVE_TGETENT	\
			-DHAVE_TERMCAP	\
			-DFEAT_GUI		\
			-DFEAT_GUI_MUI	\
			-DFEAT_HUGE		\
			-DFEAT_BROWSE	\
			-DFEAT_TOOLBAR	\
			-I proto		\
			-Wno-attributes \
			-Wextra

LDFLAGS = -mcrt=clib2 -lauto -lm -lnet


ifeq ($(strip $(DEBUG)),1)
CFLAGS +=	-O0 -DLLVL=2
else
CFLAGS +=	-O3
#LDFLAGS += 	-s
LDFLAGS +=
ifeq ($(strip $(LEAK_CHECK)),1)
CFLAGS +=	-DLEAK_CHECK
endif
endif

ifdef PATCHLEVEL
CFLAGS += 	-DPATCHLEVEL=\"$(PATCHLEVEL)\"
endif

SRC =	arabic.c			\
		autocmd.c			\
		blob.c				\
		blowfish.c			\
		buffer.c			\
		charset.c			\
		crypt.c				\
		crypt_zip.c			\
		debugger.c			\
		dict.c				\
		diff.c				\
		digraph.c			\
		edit.c				\
		eval.c				\
		evalfunc.c			\
		ex_cmds.c			\
		ex_cmds2.c			\
		ex_docmd.c			\
		ex_eval.c			\
		ex_getln.c			\
		fileio.c			\
		findfile.c			\
		fold.c				\
		getchar.c			\
		gui.c				\
		gui_mui.c			\
		hardcopy.c			\
		hashtab.c			\
		indent.c			\
		insexpand.c			\
		json.c				\
		list.c				\
		main.c				\
		mark.c				\
		mbyte.c				\
		memfile.c			\
		memline.c			\
		menu.c				\
		message.c			\
		misc1.c				\
		misc2.c				\
		move.c				\
		normal.c			\
		ops.c				\
		option.c			\
		os_amiga.c			\
		popupmnu.c			\
		quickfix.c			\
		regexp.c			\
		screen.c			\
		search.c			\
		sha256.c			\
		sign.c				\
		spell.c				\
		spellfile.c			\
		syntax.c			\
		tag.c				\
		term.c				\
		termlib.c			\
		textprop.c			\
		ui.c				\
		undo.c				\
		usercmd.c			\
		userfunc.c			\
		version.c			\
		window.c			\
		xdiff/xdiffi.c      \
		xdiff/xemit.c       \
		xdiff/xhistogram.c  \
		xdiff/xpatience.c   \
		xdiff/xprepare.c    \
		xdiff/xutils.c

OBJ =	$(SRC:.c=.o)

$(BIN): $(OBJ)
	${LD} -o $(BIN) $(OBJ) $(LDFLAGS)

.PHONY: clean
clean:
	$(RM) -fv $(OBJ) $(BIN) 
