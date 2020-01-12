#   ____________________________________________________________________________________
#   |L|_Shell______________________________________________________________________|L|L|
#   |New Shell Process 1                                                             | |
#   |1.Work:> cd vim                                                                 | |
#   |1.Work:vim> make -f Make_ami.mak CC=gcc                                         | |
#   |make: 'vim' is up to date.                                                      | |
#   |1.Work:vim> _                                                                   | |
#   |                                                                                | |
#   |                                                                                | |
#   |                                                                                |_|
#   |                                                                                |^|
#   |                                                                                |v|
#   |________________________________________________________________________________|/|
#
#------------------------------------------------------------------------------------------
# Basic settings
#------------------------------------------------------------------------------------------
BIN =		vim
CC ?=		gcc
LD =		$(CC)
UNM ?= 		$(shell uname)
DEBUG ?=	no
BUILD ?=	mui

#------------------------------------------------------------------------------------------
# Debug or default build
#------------------------------------------------------------------------------------------
ifeq ($(DEBUG),no)
	CFLAGS = -c -O3
else
	CFLAGS = -c -O0
endif

#------------------------------------------------------------------------------------------
# Common compiler flags
#------------------------------------------------------------------------------------------
CFLAGS +=	-DNO_ARP			\
			-DUSE_TMPNAM		\
			-DHAVE_STDARG_H		\
			-DHAVE_TGETENT		\
			-DHAVE_TERMCAP		\
			-I proto			\
			-Wno-attributes 	\
			-Wextra

#------------------------------------------------------------------------------------------
# Vim 'huge' build with MUI GUI
#------------------------------------------------------------------------------------------
ifeq ($(BUILD),mui)
CFLAGS +=	-DFEAT_GUI			\
			-DFEAT_GUI_MUI		\
			-DFEAT_BROWSE		\
			-DFEAT_TOOLBAR		\
			-DFEAT_HUGE
SRC := 		gui.c				\
			gui_mui.c
else

#------------------------------------------------------------------------------------------
# Vim 'huge' build
#------------------------------------------------------------------------------------------
ifeq ($(BUILD),huge)
CFLAGS +=	-DFEAT_BROWSE		\
			-DFEAT_MOUSE		\
			-DFEAT_HUGE
else

#------------------------------------------------------------------------------------------
# Vim 'big' build
#------------------------------------------------------------------------------------------
ifeq ($(BUILD),big)
CFLAGS +=	-DFEAT_BROWSE		\
			-DFEAT_MOUSE		\
			-DFEAT_BIG
else

#------------------------------------------------------------------------------------------
# Vim 'normal' build
#------------------------------------------------------------------------------------------
ifeq ($(BUILD),normal)
CFLAGS +=	-DFEAT_BROWSE		\
			-DFEAT_MOUSE		\
			-DFEAT_NORMAL
else

#------------------------------------------------------------------------------------------
# Vim 'small' build
#------------------------------------------------------------------------------------------
ifeq ($(BUILD),small)
CFLAGS +=	-DFEAT_TERMRESPONSE \
			-DFEAT_SMALL
else

#------------------------------------------------------------------------------------------
# Vim 'tiny' build
#------------------------------------------------------------------------------------------
ifeq ($(BUILD),tiny)
CFLAGS +=	-DFEAT_TERMRESPONSE \
			-DFEAT_TINY
endif
endif
endif
endif
endif
endif

#------------------------------------------------------------------------------------------
# OS specific compiler flags
#------------------------------------------------------------------------------------------
ifeq ($(UNM),AmigaOS)
LDFLAGS = 	-mcrt=clib2 -lauto -lm -lnet
CFLAGS += 	-DHAVE_FSYNC -D__USE_INLINE__ -mcrt=clib2
else
ifeq ($(UNM),AROS)
LDFLAGS = 	-DHAVE_FSYNC -ldebug
else
ifeq ($(UNM),MorphOS)
LDFLAGS = 	-ldebug -noixemul
endif
endif
endif

#------------------------------------------------------------------------------------------
# Patch level used for Amiga style version string
#------------------------------------------------------------------------------------------
ifdef PATCHLEVEL
CFLAGS += 	-DPATCHLEVEL=\"$(PATCHLEVEL)\"
endif

#------------------------------------------------------------------------------------------
# Common sources
#------------------------------------------------------------------------------------------
SRC +=		arabic.c			\
			arglist.c			\
			autocmd.c			\
			beval.c				\
			blob.c				\
			blowfish.c			\
			buffer.c			\
			bufwrite.c			\
			change.c			\
			charset.c			\
			cindent.c			\
			cmdhist.c			\
			cmdexpand.c			\
			crypt.c				\
			crypt_zip.c			\
			debugger.c			\
			dict.c				\
			diff.c				\
			digraph.c			\
			drawline.c			\
			drawscreen.c		\
			edit.c				\
			eval.c				\
			evalbuffer.c		\
			evalfunc.c			\
			evalvars.c			\
			evalwindow.c		\
			ex_cmds.c			\
			ex_cmds2.c			\
			ex_docmd.c			\
			ex_eval.c			\
			ex_getln.c			\
			fileio.c			\
			filepath.c			\
			findfile.c			\
			fold.c				\
			getchar.c			\
			hardcopy.c			\
			hashtab.c			\
			highlight.c			\
			if_cscope.c			\
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
			mouse.c				\
			move.c				\
			normal.c			\
			ops.c				\
			option.c			\
			optionstr.c			\
			os_amiga.c			\
			popupmenu.c			\
			popupwin.c			\
			quickfix.c			\
			regexp.c			\
			register.c			\
			screen.c			\
			scriptfile.c		\
			search.c			\
			session.c			\
			sha256.c			\
			sign.c				\
			spell.c				\
			spellfile.c			\
			spellsuggest.c		\
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

#------------------------------------------------------------------------------------------
# Build everything
#------------------------------------------------------------------------------------------
$(BIN): $(OBJ)
	${LD} -o $(BIN) $(OBJ) $(LDFLAGS)

#------------------------------------------------------------------------------------------
# Clean up
#------------------------------------------------------------------------------------------
.PHONY: clean
clean:
	$(RM) -fv $(OBJ) $(BIN)
