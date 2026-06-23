/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * Amiga Machine-dependent things
 */

#define CASE_INSENSITIVE_FILENAME   // ignore case when comparing file names
#define SPACE_IN_FILENAME
#define USE_FNAME_CASE		    // adjust case of file names
#define USE_TERM_CONSOLE
#define OSPEED_EXTERN
#define UP_BC_PC_EXTERN

#ifndef	DFLT_ERRORFILE
# define DFLT_ERRORFILE		"errors.err"
#endif

#ifndef	DFLT_RUNTIMEPATH
# define DFLT_RUNTIMEPATH "VIM:vimfiles,VIM:,VIM:vimfiles/after"
#endif
#ifndef	CLEAN_RUNTIMEPATH
# define CLEAN_RUNTIMEPATH "VIM:vimfiles,VIM:,VIM:vimfiles/after"
#endif

#ifndef	BASENAMELEN
# define BASENAMELEN	26	// Amiga
#endif

#ifndef	TEMPNAME
# define TEMPNAME	"t:v?XXXXXX"
# define TEMPNAMELEN	12
#endif

#include <exec/types.h>
#include <libraries/dos.h>
#include <libraries/dosextens.h>

#ifdef __MORPHOS__
# include <dos/var.h>
#endif

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>

#define FNAME_ILLEGAL ";*?`#%" // illegal characters in a file name

#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <dirent.h>

// Classic AmigaOS 3.x with GCC/libnix does not provide fchown, fchmod, or
// ftruncate.  Stub them as no-ops.  (OS4 has these via clib2; MorphOS and
// AROS provide them in their respective C libraries.)
#if defined(__GNUC__) && defined(AMIGA) && !defined(__amigaos4__) \
	&& !defined(__AROS__) && !defined(__MORPHOS__)
# define fchown(fd, uid, gid) (0)
# define fchmod(fd, mode) (0)
# define ftruncate(fd, len) (0)
#endif

#include <time.h>	// for strftime() and others

/*
 * This won't be needed if you have a version of Lattice 4.01 without broken
 * break signal handling.
 */
#include <signal.h>

/*
 * Names for the EXRC, HELP and temporary files.
 * Some of these may have been defined in the makefile.
 */
#ifndef SYS_VIMRC_FILE
# define SYS_VIMRC_FILE "VIM:vimrc"
#endif
#ifndef SYS_GVIMRC_FILE
# define SYS_GVIMRC_FILE "VIM:gvimrc"
#endif
#ifndef SYS_MENU_FILE
# define SYS_MENU_FILE	"VIM:menu.vim"
#endif
#ifndef DFLT_HELPFILE
# define DFLT_HELPFILE	"VIM:doc/help.txt"
#endif
#ifndef SYNTAX_FNAME
# define SYNTAX_FNAME	"VIM:syntax/%s.vim"
#endif

#ifndef USR_EXRC_FILE
# define USR_EXRC_FILE	"HOME:.exrc"
#endif
#ifndef USR_EXRC_FILE2
# define USR_EXRC_FILE2	"VIM:.exrc"
#endif

#ifndef USR_VIMRC_FILE
# define USR_VIMRC_FILE	"HOME:.vimrc"
#endif
#ifndef USR_VIMRC_FILE2
# define USR_VIMRC_FILE2 "VIM:.vimrc"
#endif
#ifndef USR_VIMRC_FILE3
# define USR_VIMRC_FILE3 "HOME:vimfiles/vimrc"
#endif
#ifndef USR_VIMRC_FILE4
# define USR_VIMRC_FILE4 "S:.vimrc"
#endif
#ifndef VIM_DEFAULTS_FILE
# define VIM_DEFAULTS_FILE "VIM:defaults.vim"
#endif
#ifndef EVIM_FILE
# define EVIM_FILE	"VIM:evim.vim"
#endif

#ifndef USR_GVIMRC_FILE
# define USR_GVIMRC_FILE "HOME:.gvimrc"
#endif
#ifndef USR_GVIMRC_FILE2
# define USR_GVIMRC_FILE2 "VIM:.gvimrc"
#endif
#ifndef USR_GVIMRC_FILE3
# define USR_GVIMRC_FILE3 "HOME:vimfiles/gvimrc"
#endif
#ifndef USR_GVIMRC_FILE4
# define USR_GVIMRC_FILE4 "S:.gvimrc"
#endif

#ifdef FEAT_VIMINFO
# ifndef VIMINFO_FILE
#  define VIMINFO_FILE	"VIM:.viminfo"
# endif
#endif

#ifndef EXRC_FILE
# define EXRC_FILE	".exrc"
#endif

#ifndef VIMRC_FILE
# define VIMRC_FILE	".vimrc"
#endif

#ifndef GVIMRC_FILE
# define GVIMRC_FILE	".gvimrc"
#endif

#ifndef DFLT_BDIR
# define DFLT_BDIR	"T:"		// default for 'backupdir'
#endif

#ifndef DFLT_DIR
# define DFLT_DIR	"T:"		// default for 'directory'
#endif

#ifndef DFLT_VDIR
# define DFLT_VDIR	"HOME:vimfiles/view"	// default for 'viewdir'
#endif

#ifndef DFLT_MAXMEM
# define DFLT_MAXMEM	(5*1024)	// use up to 5 Mbyte for buffer
#endif

#ifndef DFLT_MAXMEMTOT
# define DFLT_MAXMEMTOT	(10*1024)	// use up to 10 Mbyte for Vim
#endif

// Use OS4 FIB_* macros on MorphOS and AROS as well.
#ifndef FIB_IS_FILE
# define FIB_IS_FILE(FIB)	((FIB)->fib_DirEntryType < 0)
#endif
#ifndef FIB_IS_DRAWER
# define FIB_IS_DRAWER(FIB)	((FIB)->fib_DirEntryType >= 0 && \
				 (FIB)->fib_DirEntryType != ST_SOFTLINK)
#endif

#define mch_remove(X) remove((const char *) (X))
#define mch_rename(X, Y) rename((const char *) (X), (const char *) (Y))
#define mch_chdir(X) chdir((const char *) (X))
#define mch_rmdir(X) rmdir((const char *) (X))
#define vim_mkdir(X, Y) mkdir((const char *) (X), Y)
#ifdef __MORPHOS__
# define mch_setenv(X, Y, Z) SetVar((X), (Y), -1, GVF_GLOBAL_ONLY|LV_VAR)
#else
# define mch_setenv(X, Y, Z) setenv((const char *) (X), (const char *) (Y), Z)
#endif
#define mch_getenv(X) (char_u *) getenv((const char *)(X))
