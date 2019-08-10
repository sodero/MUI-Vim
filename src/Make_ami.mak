#
# Makefile for Amiga-like systems
#

BIN =		vim
CC ?=		gcc
LD =		$(CC)

CFLAGS =	-c				\
			-O3				\
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

UNM := $(shell uname)
ifeq ($(UNM),AmigaOS)
LDFLAGS = 	-mcrt=clib2 -lauto -lm -lnet
CFLAGS += 	-D__USE_INLINE__ -mcrt=clib2
else
ifeq ($(UNM),AROS)
LDFLAGS = 	-ldebug
else
ifeq ($(UNM),MorphOS)
LDFLAGS = 	-ldebug -noixemul
endif
endif
endif

ifdef PATCHLEVEL
CFLAGS += 	-DPATCHLEVEL=\"$(PATCHLEVEL)\"
endif

SRC =		arabic.c			\
			autocmd.c			\
			beval.c				\
			blob.c				\
			blowfish.c			\
			buffer.c			\
			change.c			\
			charset.c			\
			cmdhist.c			\
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
			highlight.c			\
			indent.c			\
			insexpand.c			\
			json.c				\
			list.c				\
			main.c				\
			mark.c				\
			map.c				\
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
			popupwin.c			\
			quickfix.c			\
			regexp.c			\
			screen.c			\
			search.c			\
			session.c			\
			sha256.c			\
			sign.c				\
			spell.c				\
			spellfile.c			\
			syntax.c			\
			tag.c				\
			term.c				\
			termlib.c			\
			testing.c			\
			textprop.c			\
			ui.c				\
			undo.c				\
			usercmd.c			\
			userfunc.c			\
			version.c			\
			viminfo.c			\
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
