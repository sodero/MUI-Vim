/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */

/*
 * os_amiga.c
 *
 * Amiga system-dependent routines.
 */

#include "vim.h"
#include "version.h"

#undef TRUE		/* Will be redefined by exec/types.h */
#undef FALSE

#include <exec/exec.h>
#include <intuition/intuition.h>
#include <devices/conunit.h>

#ifdef __amigaos4__
# define KPrintF DebugPrintF
#endif

#include <workbench/startup.h>
#include <dos/dostags.h>	    // for 2.0 functions
#include <dos/dosasl.h>

// From version 4 of AmigaOS, several system structures must be allocated
// and freed using system functions. "struct AnchorPath" is one.
#ifdef __amigaos4__
# include <dos/anchorpath.h>
# include <dos/obsolete.h>
# define	free_fib(x) FreeDosObject(DOS_FIB, x)
#else
# define	free_fib(x) vim_free(fib)
#endif

/*
 * Set stack size on startup.  1 MiB on NG systems (OS4, AROS, MorphOS)
 * which have plenty of RAM.  256 KiB on classic OS 3 -- enough for syntax
 * highlighting and Vim9 execution but conservative for systems with as
 * little as 2 MiB of Fast RAM.
 */
#if defined(__amigaos4__)
static const char* __attribute__((used)) stackcookie = "$STACK: 1048576";
#else
unsigned long __stack = 1048576;
#endif

/*
 * At this point TRUE and FALSE are defined as 1L and 0L, but we want 1 and 0.
 */
#undef	TRUE
#define TRUE (1)
#undef	FALSE
#define FALSE (0)

#ifdef __amigaos4__
# define	dos_packet(a, b, c)   DoPkt(a, b, c, 0, 0, 0, 0)
#elif !defined(AZTEC_C) && !defined(__AROS__)
static long dos_packet(struct MsgPort *, long, long);
#endif
static void out_num(long n);
static struct FileInfoBlock *get_fib(char_u *);
static int sortcmp(const void *a, const void *b);

static BPTR		raw_in = (BPTR)NULL;
static BPTR		raw_out = (BPTR)NULL;
static int		close_win = FALSE;  // set if Vim opened the window
#if defined(__amigaos3__) || defined(__amigaos4__)
static struct Window	*wb_window = NULL;
#endif
static char_u		*oldwindowtitle = NULL;

static char version[] __attribute__((used)) =
    "\0$VER: Vim "
    VIM_VERSION_MAJOR_STR "."
    VIM_VERSION_MINOR_STR
#ifdef PATCHLEVEL
    "." PATCHLEVEL
#endif
#ifdef BUILDDATE
    " (" BUILDDATE ")"
#endif
    ;

#ifdef __amigaos4__
# define KPrintF DebugPrintF
#endif
#ifdef __AROS__
# ifndef KPrintF
#  define KPrintF kprintf
# endif
#endif
#define kmsg(E) KPrintF((CONST_STRPTR) "%s (%s:%d)\n", E, __func__, __LINE__);
#define HERE \
do { static int c; KPrintF("%s[%ld]:%ld\n",__func__,__LINE__,++c); } while(0)



    void
win_resize_on(void)
{
    OUT_STR_NF("\033[12{");
}

    void
win_resize_off(void)
{
    OUT_STR_NF("\033[12}");
}

    void
mch_write(char_u *p, int len)
{
    Write(raw_out, (char *)p, (long)len);
}

/*
 * mch_inchar(): low level input function.
 * Get a characters from the keyboard.
 * If time == 0 do not wait for characters.
 * If time == n wait a short time for characters.
 * If time == -1 wait forever for characters.
 *
 * Return number of characters read.
 */
    int
mch_inchar(
    char_u  *buf,
    int	    maxlen,
    long    time,		// milliseconds
    int	    tb_change_cnt UNUSED)
{
    int	    len;
    long    utime;

    if (time >= 0)
    {
	if (time == 0)
	    utime = 100L;	    // time = 0 causes problems in DOS 1.2
	else
	    utime = time * 1000L;   // convert from milli to micro secs
	if (WaitForChar(raw_in, utime) == 0)	// no character available
	    return 0;
    }
    else    // time == -1
    {
	/*
	 * If there is no character available within 2 seconds (default)
	 * write the autoscript file to disk.  Or cause the CursorHold event
	 * to be triggered.
	 */
	if (WaitForChar(raw_in, p_ut * 1000L) == 0)
	{
	    if (trigger_cursorhold() && maxlen >= 3)
	    {
		buf[0] = K_SPECIAL;
		buf[1] = KS_EXTRA;
		buf[2] = (int)KE_CURSORHOLD;
		return 3;
	    }

	    before_blocking();

#if defined(__AROS__) || defined(__MORPHOS__)
	    int rows = Rows;
	    int cols = Columns;

	    mch_get_shellsize();

	    if(rows != Rows || cols != Columns)
	    {
		buf[0] = Ctrl_L; // use newsize or whatever instead
		return 1;
	    }
#endif /* __AROS__ || __MORPHOS__ */
	    return 0;
	}
    }

    for (;;)	    // repeat until we got a character
    {
	len = Read(raw_in, (char *)buf, (long)maxlen / input_conv.vc_factor);

	if (len > 0)
	{
	    // Convert from 'termencoding' to 'encoding'.
	    if (input_conv.vc_type != CONV_NONE)
		len = convert_input(buf, len, maxlen);
	    return len;
	}
    }
}

/*
 * return non-zero if a character is available
 */
    int
mch_char_avail(void)
{
    return (WaitForChar(raw_in, 100L) != 0);
}

/*
 * Return amount of memory still available in Kbyte.
 */
    long_u
mch_avail_mem(int special)
{
#if defined(__amigaos4__) || defined(__AROS__) || defined(__MORPHOS__)
    return (long_u)AvailMem(MEMF_ANY) >> 10;
#else
    return (long_u)(AvailMem(special ? (long)MEMF_CHIP : (long)MEMF_ANY)) >> 10;
#endif
}

/*
 * Waits a specified amount of time, or until input arrives if
 * flags does not have MCH_DELAY_IGNOREINPUT.
 */
    void
mch_delay(long msec, int flags)
{
    if (msec <= 0)
	return;

    if (flags & MCH_DELAY_IGNOREINPUT)
	Delay(msec / 20L);	    // Delay works with 20 msec intervals
    else
	WaitForChar(raw_in, msec * 1000L);
}

/*
 * We have no job control, fake it by starting a new shell.
 */
    void
mch_suspend(void)
{
    suspend_shell();
}

#ifndef DOS_LIBRARY
# define DOS_LIBRARY	((UBYTE *)"dos.library")
#endif

    void
mch_init(void)
{
#ifdef FEAT_GUI
    if (gui.starting)
    {
        return;
    }
#endif

    void *req_handle = mch_disable_volume_requester();

    /*
     * Set input and output channels, unless we have opened our own window
     */
    if (raw_in == (BPTR) NULL)
    {
	raw_in = Input();
	raw_out = Output();
	/*
	 * If Input() is not interactive, then Output() will be (because of
	 * check in mch_check_win()).  Used for "Vim -".
	 * Also check the other way around, for "Vim -h | more".
	 */
	if (!IsInteractive(raw_in))
	    raw_in = raw_out;
	else if (!IsInteractive(raw_out))
	    raw_out = raw_in;
    }

    out_flush();

    mch_enable_volume_requester(req_handle);
}

static char **cmd_args;

#ifdef FEAT_GUI
/*
 * Check if we're started from Workbench.
 */
static BOOL is_wb_args(char **argv)
{
    return argv == cmd_args;
}
#endif

/*
 * Free Workbench argument list.
 */
static void free_cmd_args(void)
{
    if (!cmd_args)
    {
	return;
    }

    char **arg = cmd_args;

    while(*arg)
    {
	free(*arg);
	arg++;
    }

    free(cmd_args);
    cmd_args = NULL;
}

/*
 * Replace argv with the corresponding list of Workbench arguments
 */
int get_cmd_argsA(int argc, char ***argvp)
{
    struct WBStartup *wb = (struct WBStartup *) *argvp;
    if (argc || !wb->sm_NumArgs)
    {
	return argc;
    }

    free_cmd_args();
    cmd_args = calloc(wb->sm_NumArgs + 1, sizeof(char *));
    if (!cmd_args)
    {
	return 0;
    }

    int i = 0;
    struct WBArg *arg = wb->sm_ArgList;
    BPTR old = CurrentDir(arg->wa_Lock);
    cmd_args[i++] = strdup(arg->wa_Name);

    while(i < wb->sm_NumArgs && cmd_args[i - 1])
    {
	static char path[PATH_MAX + 1];

	// If the argument starts with a '-' and there's no such file, just
	// copy it verbatim. If it's file, copy the absolute path.
	if (arg[i].wa_Name[0] != '-' &&
	    NameFromLock(arg[i].wa_Lock, path, PATH_MAX))
	{
	    AddPart(path, arg[i].wa_Name, PATH_MAX);
	    cmd_args[i] = strdup(path);
	}
	else
	{
	    cmd_args[i] = strdup(arg[i].wa_Name);
	}
	i++;
    }

    *argvp = cmd_args;
    (void) CurrentDir(old);
    return i;
}

/*
 * Check_win checks whether we have an interactive window.
 * If not, a new window is opened with the newcli command.
 * If we would open a window ourselves, the :sh and :! commands would not
 * work properly (Why? probably because we are then running in a background
 * CLI). This also is the best way to assure proper working in a next
 * Workbench release.
 *
 * For the -f option (foreground mode) we open our own window and disable :sh.
 * Otherwise the calling program would never know when editing is finished.
 */
#define BUF2SIZE 320	    // length of buffer for argument with complete path

    int
mch_check_win(int argc, char **argv)
{
    int		    i;
    BPTR	    nilfh, fh;
    char_u	    buf1[24];
    char_u	    buf2[BUF2SIZE];
    static char_u   *(constrings[3]) = {(char_u *)"con:0/0/662/210/",
					(char_u *)"con:0/0/640/200/",
					(char_u *)"con:0/0/320/200/"};
    static char_u   *winerr = (char_u *)N_("VIM: Can't open window!\n");
    struct WBArg    *argp;
    int		    ac;
    char	    *av;
    char_u	    *device = NULL;
    int		    exitval = 4;
#if !defined(__amigaos4__) && !defined(__amigaos3__) && !defined(__AROS__) && !defined(__MORPHOS__)
    struct Library  *DosBase;
#endif
    int		    usewin = FALSE;

#ifdef FEAT_GUI
    // Enable GUI mode if started from WB.
    if (is_wb_args(argv))
    {
        gui.starting = TRUE;
        return FAIL;
    }
#endif

/*
 * check if we are running under DOS 2.0x or higher
 */
#if !defined(__amigaos4__) && !defined(__amigaos3__) && !defined(__AROS__) && !defined(__MORPHOS__)
    DosBase = OpenLibrary(DOS_LIBRARY, 37L);
    if (DosBase != NULL)
    {
	CloseLibrary(DosBase);
    }
    else	    // without arp functions we NEED 2.0
    {
	mch_errmsg(_("Need Amigados version 2.04 or later\n"));
	exit(3);
    }
#endif	/* __amigaos4__ __AROS__ __MORPHOS__ */

    /*
     * scan argv[] for the "-f" and "-d" arguments
     */
    for (i = 1; i < argc; ++i)
	if (argv[i][0] == '-')
	{
	    switch (argv[i][1])
	    {
	    case 'f':
		usewin = TRUE;
		break;

	    case 'd':
		if (i < argc - 1
#ifdef FEAT_DIFF
			// require using "-dev", "-d" means diff mode
			&& argv[i][2] == 'e' && argv[i][3] == 'v'
#endif
		   )
		    device = (char_u *)argv[i + 1];
		break;
	    }
	}

/*
 * If we were not started from workbench, do not have a "-d" or "-dev"
 * argument and we have been started with an interactive window, use that
 * window.
 */
    if (argc != 0
	    && device == NULL
	    && (IsInteractive(Input()) || IsInteractive(Output())))
	return OK;

/*
 * When given the "-f" argument, we open our own window. We can't use the
 * newcli trick below, because the calling program (mail, rn, etc.) would not
 * know when we are finished.
 */
    if (usewin)
    {
	/*
	 * Try to open a window. First try the specified device.
	 * Then try a 24 line 80 column window.
	 * If that fails, try two smaller ones.
	 */
	for (i = -1; i < 3; ++i)
	{
	    if (i >= 0)
		device = constrings[i];
	    if (device != NULL && (raw_in = Open((UBYTE *)device,
					   (long)MODE_NEWFILE)) != (BPTR)NULL)
		break;
	}
	if (raw_in == (BPTR)NULL)	// all three failed
	{
	    mch_errmsg(_(winerr));
	    goto exit;
	}
	raw_out = raw_in;
	close_win = TRUE;
	return OK;
    }

    if ((nilfh = Open((UBYTE *)"NIL:", (long)MODE_NEWFILE)) == (BPTR)NULL)
    {
	mch_errmsg(_("Cannot open NIL:\n"));
	goto exit;
    }

    /*
     * Make a unique name for the temp file (which we will not delete!).
     * Use a pointer on the stack (nobody else will be using it).
     * Under AmigaOS4, this assumption might change in the future, so
     * we use a pointer to the current task instead. This should be a
     * shared structure and thus globally unique.
     */
#if !defined(__amigaos4__) && !defined(__amigaos3__) && !defined(__AROS__) && !defined(__MORPHOS__)
    sprintf((char *)buf1, "t:nc%p", FindTask(0));
#else
    sprintf((char *)buf1, "t:nc%ld", (long)buf1);
#endif
    if ((fh = Open((UBYTE *)buf1, (long)MODE_NEWFILE)) == (BPTR)NULL)
    {
	mch_errmsg(_("Cannot create "));
	mch_errmsg((char *)buf1);
	mch_errmsg("\n");
	goto exit;
    }
    /*
     * Write the command into the file, put quotes around the arguments that
     * have a space in them.
     */
    if (argc == 0)	// run from workbench
	ac = ((struct WBStartup *)argv)->sm_NumArgs;
    else
	ac = argc;
    for (i = 0; i < ac; ++i)
    {
	if (argc == 0)
	{
	    *buf2 = NUL;
	    argp = &(((struct WBStartup *)argv)->sm_ArgList[i]);

	    if (argp->wa_Lock)
		(void) NameFromLock(argp->wa_Lock, buf2, (long)(BUF2SIZE - 1));

	    AddPart((UBYTE *)buf2, (UBYTE *)argp->wa_Name, (long)(BUF2SIZE - 1));
	    av = (char *)buf2;
	}
	else
	    av = argv[i];

	// skip '-d' or "-dev" option
	if (av[0] == '-' && av[1] == 'd'
#ifdef FEAT_DIFF
		&& av[2] == 'e' && av[3] == 'v'
#endif
		)
	{
	    ++i;
	    continue;
	}
	if (vim_strchr((char_u *)av, ' '))
	    Write(fh, "\"", 1L);
	Write(fh, av, (long)strlen(av));
	if (vim_strchr((char_u *)av, ' '))
	    Write(fh, "\"", 1L);
	Write(fh, " ", 1L);
    }
    Write(fh, "\nendcli\n", 8L);
    Close(fh);

/*
 * Try to open a new cli in a window. If "-d" or "-dev" argument was given try
 * to open the specified device. Then try a 24 line 80 column window.  If that
 * fails, try two smaller ones.
 */
    for (i = -1; i < 3; ++i)
    {
	if (i >= 0)
	    device = constrings[i];
	else if (device == NULL)
	    continue;
	sprintf((char *)buf2, "newcli <nil: >nil: %s from %s", (char *)device, (char *)buf1);

	if (!SystemTags((UBYTE *)buf2, SYS_UserShell, TRUE, TAG_DONE))
	    break;
    }
    if (i == 3)	    // all three failed
    {
	DeleteFile((UBYTE *)buf1);
	mch_errmsg(_(winerr));
	goto exit;
    }
    exitval = 0;    // The Execute succeeded: exit this program

exit:
    if (nilfh)
	Close(nilfh);

    exit(exitval);
    // NOTREACHED
    return FAIL;
}

/*
 * Return TRUE if the input comes from a terminal, FALSE otherwise.
 * We fake there is a window, because we can always open one!
 */
    int
mch_input_isatty(void)
{
    return TRUE;
}

/*
 * In-place conversion of Unix style path to Amiga path
 */
static void mch_normalize_path(char_u *path)
{
    if (!path)
        return;

    for (size_t i = 0; path[i];)
    {
        /* vim:/{1,}file -> vim:file */
        if(path[i] == ':' && path[i + 1] == '/')
        {
            for(size_t j = i + 1; path[j]; ++j)
            {
                path[j] = path[j + 1];
            }
            continue;
        }
        /* ./file -> file */
        /* vim:dir/../file -> vim:dir//file */
        if(path[i] == '.' && (path[i + 1] == '/' ||
           (path[i + 1] == '.' && path[i + 2] == '/')))
        {
            for(size_t j = i; path[j + 1]; ++j)
            {
                path[j] = path[j + 2];
            }
        }
        ++i;
    }
}

/*
 * fname_case(): Set the case of the file name, if it already exists.
 *		 This will cause the file name to remain exactly the same
 *		 if the file system ignores, but preserves case.
 */
//ARGSUSED
    void
fname_case(
    char_u	*name,
    int		len UNUSED)		// buffer size, ignored here
{
    struct FileInfoBlock    *fib;
    size_t		    flen;

    fib = get_fib(name);
    if (fib == NULL)
	return;

    flen = STRLEN(name);
    // TODO: Check if this fix applies to AmigaOS < 4 too.
#ifdef __amigaos4__
    if (fib->fib_DirEntryType == ST_ROOT)
	strcat(fib->fib_FileName, ":");
#endif
    if (flen == strlen(fib->fib_FileName))	// safety check
	mch_memmove(name, fib->fib_FileName, flen);
    free_fib(fib);
}

/*
 * Get the FileInfoBlock for file "fname"
 * The returned structure has to be free()d.
 * Returns NULL on error.
 */
    static struct FileInfoBlock *
get_fib(char_u *fname)
{
    BPTR		    flock;
    struct FileInfoBlock    *fib;

    if (fname == NULL)	    // safety check
	return NULL;

    mch_normalize_path(fname);

#ifdef __amigaos4__
    fib = AllocDosObject(DOS_FIB,0);
#else
    fib = ALLOC_ONE(struct FileInfoBlock);
#endif
    if (fib == NULL)
	return NULL;

    void *req_handle = mch_disable_volume_requester();

    flock = Lock(fname, ACCESS_READ);

    mch_enable_volume_requester(req_handle);

    if (flock == (BPTR) NULL || !Examine(flock, fib))
    {
	free_fib(fib);  // in case of an error the memory is freed here
	fib = NULL;
    }

    if (flock)
	UnLock(flock);

    return fib;
}

/*
 * set the title of our window
 * icon name is not set
 */
    void
mch_settitle(char_u *title, char_u *icon)
{
    KPrintF("title:%s (window:%p)\n", title, wb_window);
    if (title == NULL)
	return;

#ifdef FEAT_GUI
    if (gui.in_use)
    {
	gui_mch_settitle(title, icon);
	return;
    }
#endif

#if defined(__amigaos3__) || defined(__amigaos4__)
    if (wb_window != NULL)
	SetWindowTitles(wb_window, (UBYTE *)title, (UBYTE *)-1L);
#else
    Write(raw_out, "\033]2;", 4);
    Write(raw_out, title, STRLEN(title));
    Write(raw_out, "\007", 1);
#endif
}

/*
 * Restore the window/icon title.
 * which is one of:
 *  SAVE_RESTORE_TITLE  Just restore title
 *  SAVE_RESTORE_ICON   Just restore icon (which we don't have)
 *  SAVE_RESTORE_BOTH   Restore title and icon (which we don't have)
 */
    void
mch_restore_title(int which)
{
    if (which & SAVE_RESTORE_TITLE)
	mch_settitle(oldwindowtitle, NULL);
}

    int
mch_can_restore_title(void)
{
#if defined(__amigaos3__) || defined(__amigaos4__)
    return (wb_window != NULL);
#else
    return TRUE;
#endif
}

    int
mch_can_restore_icon(void)
{
    return FALSE;
}

    void
mch_setmouse(int on UNUSED)
{
    // TODO: implement
}

/*
 * Insert user name in s[len].
 */
    int
mch_get_user_name(char_u *s, int len)
{
#if !defined(__AROS__) && !defined(__amigaos3__)
    struct passwd *pwd = getpwuid(getuid());

    if (pwd != NULL && pwd->pw_name && len > 0)
    {
	vim_strncpy(s, (char_u *)pwd->pw_name, len - 1);
	return OK;
    }
#else
    vim_strncpy(s, (char_u *)"sysop", len - 1);
#endif
    *s = NUL;
    return FAIL;
}

/*
 * Insert host name is s[len].
 */
    void
mch_get_host_name(char_u *s, int len)
{
#if !defined(__AROS__) && !defined(__amigaos3__)
    gethostname((char *)s, len);
#else
    vim_strncpy(s, (char_u *)"amiga", len - 1);
#endif
}

/*
 * return process ID
 */
    long
mch_get_pid(void)
{
    return (long) getpid();
}

/*
 * Get name of current directory into buffer 'buf' of length 'len' bytes.
 * Return OK for success, FAIL for failure.
 */
    int
mch_dirname(char_u *buf, int len)
{
    BPTR lock = CurrentDir((BPTR) 0);

    int status = NameFromLock(lock, buf, len) ? OK : FAIL;

    CurrentDir(lock);

    return status;
}

/*
 * get absolute file name into buffer 'buf' of length 'len' bytes
 *
 * return FAIL for failure, OK otherwise
 */
    int
mch_FullName(
    char_u	*fname,
    char_u	*buf,
    int		len,
    int		force)
{
    BPTR	l;
    int		retval = FAIL;
    int		i;

    void *req_handle = mch_disable_volume_requester();

    // Lock the file.  If it exists, we can get the exact name.
    if ((l = Lock(fname, ACCESS_READ)) != (BPTR) 0)
    {
	retval = NameFromLock(l, buf, len - 1) ? OK : FAIL;
	UnLock(l);
    }
    else if (force || !mch_isFullName(fname))	    // not a full path yet
    {
	/*
	 * If the file cannot be locked (doesn't exist), try to lock the
	 * current directory and concatenate the file name.
	 */
	if ((l = Lock((UBYTE *)"", (long)ACCESS_READ)) != (BPTR) 0)
	{
	    retval = NameFromLock(l, buf, len) ? OK : FAIL;
	    UnLock(l);
	    if (retval == OK)
	    {
		i = STRLEN(buf);
		// Concatenate the fname to the directory.  Don't add a slash
		// if fname is empty, but do change "" to "/".
		if (i == 0 || *fname != NUL)
		{
		    if (i < len - 1 && (i == 0 || buf[i - 1] != ':'))
			buf[i++] = '/';
		    vim_strncpy(buf + i, fname, len - i - 1);
		}
	    }
	}
    }

    mch_enable_volume_requester(req_handle);

    if (*buf == 0 || *buf == ':')
	retval = FAIL;	// something failed; use the file name
			//
    return retval;
}

/*
 * Return TRUE if "fname" does not depend on the current directory.
 */
    int
mch_isFullName(char_u *fname)
{
    return (vim_strchr(fname, ':') != NULL && *fname != ':');
}

/*
 * get file permissions for 'name'
 * Returns -1 when it doesn't exist.
 */
    long
mch_getperm(char_u *name)
{
    struct FileInfoBlock    *fib;
    long		    retval = -1;

    fib = get_fib(name);
    if (fib == NULL)
	return -1;

    retval = fib->fib_Protection;
    free_fib(fib);
    return retval;
}

/*
 * set file permission for 'name' to 'perm'
 *
 * return FAIL for failure, OK otherwise
 */
    int
mch_setperm(char_u *name, long perm)
{
    perm &= ~FIBF_ARCHIVE;		// reset archived bit
    return (SetProtection((UBYTE *)name, (long)perm) ? OK : FAIL);
}

/*
 * Set hidden flag for "name".
 */
    void
mch_hide(char_u *name UNUSED)
{
    // can't hide a file
}

/*
 * return FALSE if "name" is not a directory
 * return TRUE if "name" is a directory.
 * return FALSE for error.
 */
    int
mch_isdir(char_u *name)
{
    struct FileInfoBlock    *fib;
    int			    retval = FALSE;

    fib = get_fib(name);
    if (fib == NULL)
	return FALSE;

#ifdef __amigaos4__
    retval = (FIB_IS_DRAWER(fib)) ? TRUE : FALSE;
#else
    retval = ((fib->fib_DirEntryType >= 0) ? TRUE : FALSE);
#endif
    free_fib(fib);
    return retval;
}

/*
 * Create directory "name".
 */
    int
mch_mkdir(char_u *name)
{
    BPTR	lock;

    lock = CreateDir(name);
    if (lock == NULL)
	return -1;

    UnLock(lock);
    return 0;
}

    const void *
mch_disable_volume_requester(void)
{
    struct Process *proc = (struct Process *) FindTask(0L);
    void *req_handle = proc->pr_WindowPtr;

    proc->pr_WindowPtr = (APTR) -1L;

    return req_handle;
}

    void
mch_enable_volume_requester(const void *req_handle)
{
    struct Process *proc = (struct Process *) FindTask(0L);
    proc->pr_WindowPtr = (APTR) req_handle;
}

/*
 * Return 1 if "name" can be executed, 0 if not.
 * If "use_path" is FALSE only check if "name" is executable.
 * Return -1 if unknown.
 */
    int
mch_can_exe(char_u *name, char_u **path UNUSED, int use_path)
{
    int exe = -1;
#ifdef __amigaos4__
    // Load file sections using elf.library or hunk.library.
    BPTR seg = LoadSeg(name);

    if (seg && GetSegListInfoTags(seg, GSLI_Native, NULL, TAG_DONE) !=
	GetSegListInfoTags(seg, GSLI_68KHUNK, NULL, TAG_DONE))
    {
	// Test if file permissions allow execution.
	struct ExamineData *exd = ExamineObjectTags(EX_StringNameInput, name);

	exe = (exd && !(exd->Protection & EXDF_NO_EXECUTE)) ? 1 : 0;
	FreeDosObject(DOS_EXAMINEDATA, exd);
    }
    else
    {
	exe = 0;
    }

    UnLoadSeg(seg);

    // Search for executable in path if applicable.
    if (!exe && use_path)
    {
	// Save current working dir.
	BPTR cwd = GetCurrentDir();
	struct PathNode *head = DupCmdPathList(NULL);

	// For each entry, recur to check for executable.
	for (struct PathNode *tail = head; !exe && tail;
	     tail = (struct PathNode *) BADDR(tail->pn_Next))
	{
	    SetCurrentDir(tail->pn_Lock);
	    exe = mch_can_exe(name, path, 0);
	}

	// Go back to where we were.
	FreeCmdPathList(head);
	SetCurrentDir(cwd);
    }
#endif
    return exe;
}

/*
 * Check what "name" is:
 * NODE_NORMAL: file or directory (or doesn't exist)
 * NODE_WRITABLE: writable device, socket, fifo, etc.
 * NODE_OTHER: non-writable things
 */
    int
mch_nodetype(char_u *name UNUSED)
{
    // TODO
    return NODE_NORMAL;
}

    void
mch_early_init(void)
{
}

/*
 * Careful: mch_exit() may be called before mch_init()!
 */
    void
mch_exit(int r)
{
    free_cmd_args();
    exiting = TRUE;

#ifdef FEAT_GUI
    if (gui.in_use)
    {
       ml_close_all(TRUE);	    /* remove all memfiles */
       gui_exit(r);
       exit(r);
    }
#endif
    if (raw_in)			    /* put terminal in 'normal' mode */
    {
	settmode(TMODE_COOK);
	stoptermcap();
    }

    out_char('\n');

    if (raw_out && term_console)
    {
	win_resize_off();	    // window resize events de-activated
	out_flush();
    }

    mch_restore_title(SAVE_RESTORE_BOTH);    // restore window title

    ml_close_all(TRUE);		    // remove all memfiles

    if (close_win)
	Close(raw_in);

    if (r)
	printf(_("Vim exiting with %d\n"), r); // somehow this makes :cq work!?
					       //
    exit(r);
}

/*
 * Function mch_settmode() - Convert the specified file pointer to 'raw' or
 * 'cooked' mode. This only works on TTY's.
 *
 * Raw: keeps DOS from translating keys for you, also (BIG WIN) it means
 *	getch() will return immediately rather than wait for a return. You
 *	lose editing features though.
 *
 * Cooked: This function returns the designate file pointer to its normal,
 *	wait for a <CR> mode. This is exactly like raw() except that
 *	it sends a 0 to the console to make it back into a CON: from a RAW:
 */
    void
mch_settmode(tmode_T tmode)
{
    if (!SetMode(raw_in, tmode == TMODE_RAW ? 1 : 0))
	mch_errmsg(_("Cannot change console mode\n"));
}

/*
 * Get console size on AROS and MorphOS.
 * Return FAIL for failure, OK otherwise
 */
    int
mch_get_shellsize_fallback(void)
{
#ifdef FEAT_GUI
    if (gui.in_use || gui.starting)
    {
        return FAIL;
    }
#endif

    if (!term_console)
	return FAIL;

    if (raw_in && raw_out)
    {
	// Save current console mode.
	int old_tmode = cur_tmode;
	char ctrl[] = "\x9b""0 q";

	// Set RAW mode.
	mch_settmode(TMODE_RAW);

	// Write control sequence to console.
	if (Write(raw_out, ctrl, sizeof(ctrl)) == sizeof(ctrl))
	{
	    char scan[] = "\x9b""1;1;%d;%d r",
		 answ[sizeof(scan) + 8] = { '\0' };

	    // Read return sequence from input.
	    if (Read(raw_in, answ, sizeof(answ) - 1) > 0)
	    {
		// Parse result and set Vim globals.
		if (sscanf(answ, scan, &Rows, &Columns) == 2)
		{
		    // Restore console mode.
		    mch_settmode(old_tmode);
		    return OK;
		}
	    }
	}

	// Restore console mode.
	mch_settmode(old_tmode);
    }

    // I/O error. Default size fallback.
    term_console = FALSE;
    Columns = 80;
    Rows = 24;

    return FAIL;
}

    static struct InfoData *
get_infodata(void)
{
#ifdef __amigaos4__
    return AllocDosObject(DOS_INFODATA, 0);
#else
    return ALLOC_ONE(struct InfoData);
#endif
}

    static void
free_infodata(struct InfoData *id)
{
#ifdef __amigaos4__
    FreeDosObject(DOS_INFODATA, id);
#else
    vim_free(id);
#endif
}

    static struct Window *
get_window(struct InfoData *id)
{
    if (!id)
	return NULL;

    return (struct Window *) id->id_VolumeNode;
}

    static struct InfoData *
get_window_infodata(BPTR handle)
{
    if (!handle)
	return NULL;

    struct MsgPort *port = (struct MsgPort *)
	((struct FileHandle *) (BADDR(handle)))->fh_Type;

    if (!port)
	return NULL;

    struct InfoData *id = get_infodata();

    if (!id)
	return NULL;

    if (!DoPkt(port, ACTION_DISK_INFO, MKBADDR(id), 0L, 0L, 0L, 0L))
    {
	free_infodata(id);
	return NULL;
    }

    return id;
}

    static void
free_window_infodata(struct InfoData *id)
{
    free_infodata(id);
}

    static struct ConUnit *
get_console(struct InfoData *id)
{
    if (!id)
	return NULL;

    struct IOStdReq *req = (struct IOStdReq *) id->id_InUse;

    if (!req)
	return NULL;

    return (struct ConUnit *) req->io_Unit;
}

/*
 * Get console size on AmigaOS
 * return FAIL for failure, OK otherwise
 */
    int
mch_get_shellsize(void)
{
#ifdef FEAT_GUI
    if (gui.in_use || gui.starting)
    {
        return FAIL;
    }
#endif

    KPrintF("term_console:%d\n", term_console);

    if (!term_console)
    {
        return FAIL;
    }

    term_console = FALSE;

    struct InfoData *id = get_window_infodata(raw_out);

    KPrintF("id:%p\n", id);

    if (!id)
	return FAIL;

    wb_window = get_window(id);

    KPrintF("wb_window:%p\n", wb_window);

    if (wb_window)
    {
	if (!oldwindowtitle)
	    oldwindowtitle = (char_u *) wb_window->Title;

	struct ConUnit *con = get_console(id);

	KPrintF("con:%p\n", con);

	if (con)
	{
	    term_console = TRUE;

	    // Get window size
	    Rows = con->cu_YMax + 1;
	    Columns = con->cu_XMax + 1;
	}
    }

    free_window_infodata(id);

    KPrintF("term_console:%d\n", term_console);

    if (!term_console)
    {
	// Not a window, maybe aux device terminal type should be set
	Rows = 24;
	Columns = 80;
	return FAIL;
    }

    KPrintF("rows:%d\n", Rows);
    KPrintF("cols:%d\n", Columns);

    return OK;
}

/*
 * Try to set the real window size to Rows and Columns.
 */
    void
mch_set_shellsize(void)
{
    if (!term_console)
	return;

    struct InfoData *id = get_window_infodata(raw_out);

    if (!id)
	return;

    struct Window *win = get_window(id);
    struct ConUnit *con = get_console(id);

    if (win && con)
    {
	int dx = (Columns - con->cu_XMax - 1) * con->cu_XRSize;
	int dy = (Rows - con->cu_YMax - 1) * con->cu_YRSize;

	SizeWindow(win, dx, dy);
    }

    free_window_infodata(id);
}

/*
 * Rows and/or Columns has changed.
 */
    void
mch_new_shellsize(void)
{
    // Reset console
    out_str((char_u *)"\033c");
    out_flush();
}

/*
 * out_num - output a (big) number fast
 */
    static void
out_num(long n)
{
    OUT_STR_NF(tltoa((unsigned long)n));
}

#if !defined(AZTEC_C) && !defined(__AROS__) && !defined(__amigaos4__)
/*
 * Sendpacket.c
 *
 * An invaluable addition to your Amiga.lib file. This code sends a packet to
 * the given message port. This makes working around DOS lots easier.
 *
 * Note, I didn't write this, those wonderful folks at CBM did. I do suggest
 * however that you may wish to add it to Amiga.Lib, to do so, compile it and
 * say 'oml lib:amiga.lib -r sendpacket.o'
 */

//#include <proto/exec.h>
//#include <proto/dos.h>
# include <exec/memory.h>

/*
 * Function - dos_packet written by Phil Lindsay, Carolyn Scheppner, and Andy
 * Finkel. This function will send a packet of the given type to the Message
 * Port supplied.
 */

    static long
dos_packet(
    struct MsgPort *pid,    // process identifier ... (handlers message port)
    long	    action, // packet type ... (what you want handler to do)
    long	    arg)    // single argument
{
    return DoPkt(pid, action, arg, 0L, 0L, 0L, 0L);	// use 2.0 function
}
#endif // !defined(AZTEC_C) && !defined(__AROS__)

/*
 * Call shell.
 * Return error number for failure, 0 otherwise
 */
    int
mch_call_shell(
    char_u	*cmd,
    int		options)	// SHELL_*, see vim.h
{
    BPTR	mydir;
    int		x;
    int		tmode = cur_tmode;
    int		retval = 0;

    if (close_win)
    {
	// if Vim opened a window: Executing a shell may cause crashes
	emsg(_(e_cannot_execute_shell_with_f_option));
	return -1;
    }

    if (term_console)
	win_resize_off();	    // window resize events de-activated
    out_flush();

    if (options & SHELL_COOKED)
	settmode(TMODE_COOK);	    // set to normal mode
				    //
    mydir = Lock((UBYTE *)"", (long)ACCESS_READ);   // remember current dir

    if ((mydir = CurrentDir(mydir)) != 0) // make sure we stay in the same directory
	UnLock(mydir);

    if (tmode == TMODE_RAW)
    {
	// The shell may have messed with the mode, always set it.
	cur_tmode = TMODE_UNKNOWN;
	settmode(TMODE_RAW);		// set to raw mode
    }

    resettitle();

    if (term_console)
	win_resize_on();		// window resize events activated
					//
    return retval;
}

/*
 * check for an "interrupt signal"
 * We only react to a CTRL-C, but also clear the other break signals to avoid
 * trouble with lattice-c programs.
 */
    void
mch_breakcheck(int force UNUSED)
{
   if (SetSignal(0L, (long)(SIGBREAKF_CTRL_C|SIGBREAKF_CTRL_D|SIGBREAKF_CTRL_E|SIGBREAKF_CTRL_F)) & SIGBREAKF_CTRL_C)
	got_int = TRUE;
}

/*
 * mch_expandpath() - this code does wild-card pattern matching using the arp
 *		      routines.
 *
 * "pat" has backslashes before chars that are not to be expanded.
 * Returns the number of matches found.
 *
 * This is based on WildDemo2.c (found in arp1.1 distribution).
 * That code's copyright follows:
 *	Copyright (c) 1987, Scott Ballantyne
 *	Use and abuse as you please.
 */

#ifdef __amigaos4__
# define ANCHOR_BUF_SIZE 1024
#else
# define ANCHOR_BUF_SIZE (512)
# define ANCHOR_SIZE (sizeof(struct AnchorPath) + ANCHOR_BUF_SIZE)
#endif

    int
mch_expandpath(
    garray_T	*gap,
    char_u	*pat,
    int		flags)		// EW_* flags
{
    struct AnchorPath	*Anchor;
    LONG		Result;
    char_u		*starbuf, *sp, *dp;
    int			start_len;
    int			matches;
#ifdef __amigaos4__
    struct TagItem	AnchorTags[] = {
	{ADO_Strlen, ANCHOR_BUF_SIZE},
	{ADO_Flags, APF_DODOT|APF_DOWILD|APF_MultiAssigns},
	{TAG_DONE, 0L}
    };
#endif

    mch_normalize_path(pat);

    start_len = gap->ga_len;

    // Get our AnchorBase
#ifdef __amigaos4__
    Anchor = AllocDosObject(DOS_ANCHORPATH, AnchorTags);
#else
    Anchor = alloc_clear(ANCHOR_SIZE);
#endif
    if (Anchor == NULL)
	return 0;

#ifndef __amigaos4__
    Anchor->ap_Strlen = ANCHOR_BUF_SIZE;  // ap_Length not supported anymore
# ifdef APF_DODOT
    Anchor->ap_Flags = APF_DODOT | APF_DOWILD;	// allow '.' for current dir
# else
    Anchor->ap_Flags = APF_DoDot | APF_DoWild;	// allow '.' for current dir
# endif
#endif

    // hack to replace '*' by '#?'
    starbuf = alloc(2 * STRLEN(pat) + 1);

    if (starbuf == NULL)
	goto Return;

    for (sp = pat, dp = starbuf; *sp; ++sp)
    {
	if (*sp == '*')
	{
	    *dp++ = '#';
	    *dp++ = '?';
	}
	else
	    *dp++ = *sp;
    }

    *dp = NUL;
    Result = MatchFirst((UBYTE *)starbuf, Anchor);
    vim_free(starbuf);

    /*
     * Loop to get all matches.
     */
    while (Result == 0)
    {
#ifdef __amigaos4__
	addfile(gap, (char_u *)Anchor->ap_Buffer, flags);
#else
	addfile(gap, (char_u *)Anchor->ap_Buf, flags);
#endif
	Result = MatchNext(Anchor);
    }

    matches = gap->ga_len - start_len;

    if (Result == ERROR_BUFFER_OVERFLOW)
	emsg(_("ANCHOR_BUF_SIZE too small."));
    else if (matches == 0 && Result != ERROR_OBJECT_NOT_FOUND
			  && Result != ERROR_DEVICE_NOT_MOUNTED
			  && Result != ERROR_NO_MORE_ENTRIES)
	emsg(_("I/O ERROR"));

    /*
     * Sort the files for this pattern.
     */
    if (matches)
	qsort((void *)(((char_u **)gap->ga_data) + start_len),
				  (size_t)matches, sizeof(char_u *), sortcmp);

    // Free the wildcard stuff
    MatchEnd(Anchor);

Return:
#ifdef __amigaos4__
    FreeDosObject(DOS_ANCHORPATH, Anchor);
#else
    vim_free(Anchor);
#endif

    return matches;
}

    static int
sortcmp(const void *a, const void *b)
{
    char *s = *(char **)a;
    char *t = *(char **)b;

    return pathcmp(s, t, -1);
}

/*
 * Return TRUE if "p" has wildcards that can be expanded by mch_expandpath().
 */
    int
mch_has_exp_wildcard(char_u *p)
{
    for ( ; *p; MB_PTR_ADV(p))
    {
	if (*p == '\\' && p[1] != NUL)
	    ++p;
	else if (vim_strchr((char_u *)"*?[(#", *p) != NULL)
	    return TRUE;
    }
    return FALSE;
}

    int
mch_has_wildcard(char_u *p)
{
    for ( ; *p; MB_PTR_ADV(p))
    {
	if (*p == '\\' && p[1] != NUL)
	    ++p;
	else
	    if (vim_strchr((char_u *)"*?[(#$`", *p) != NULL
		    || (*p == '~' && p[1] != NUL))
		return TRUE;
    }
    return FALSE;
}

/*
 * Return TRUE if "name" is a valid console
 */
    int
mch_is_console(char_u *name)
{
#ifdef FEAT_GUI
    if(gui.in_use || gui.starting)
    {
	return FALSE;
    }
#endif

    if(STRCMP(name, "amiga") == 0)
    {
	return TRUE;
    }

    if(STRCMP(name, "morphos") == 0)
    {
	return TRUE;
    }

    return FALSE;
}

/*
 * With AmigaDOS 2.0 support for reading local environment variables
 *
 * Two buffers are allocated:
 * - A big one to do the expansion into.  It is freed before returning.
 * - A small one to hold the return value.  It is kept until the next call.
 */
    char_u *
mch_getenv(char_u *var)
{
    int		    len;
    UBYTE	    *buf;		// buffer to expand in
    char_u	    *retval;		// return value
    static char_u   *alloced = NULL;	// allocated memory

    VIM_CLEAR(alloced);
    retval = NULL;

    buf = alloc(IOSIZE);
    if (buf == NULL)
	return NULL;

    len = GetVar((UBYTE *)var, buf, (long)(IOSIZE - 1), (long)0);
    if (len >= 0)
    {
	retval = vim_strsave((char_u *)buf);
	alloced = retval;
    }

    vim_free(buf);

    // if $VIM is not defined, use "vim:" instead
    if (retval == NULL && STRCMP(var, "VIM") == 0)
	retval = (char_u *)"vim:";

    return retval;
}

/*
 * Amiga version of setenv() with AmigaDOS 2.0 support.
 */
// ARGSUSED
    int
mch_setenv(char *var, char *value, int x UNUSED)
{
#ifdef FEAT_ARP
    if (!dos2)
	return setenv(var, value);
#endif

    if (SetVar((UBYTE *)var, (UBYTE *)value, (LONG)-1, (ULONG)GVF_LOCAL_ONLY))
	return 0;   // success
    return -1;	    // failure
}



/*
 * Fill the buffer 'buf' with 'len' random bytes.
 * Returns FAIL if RANDOM: is not available or something went wrong.
 */
    int
mch_get_random(char_u *buf, int len)
{
    // Don't show requester if RANDOM: doesn't exist
    void *req_handle = mch_disable_volume_requester();

    BPTR fh = Open("RANDOM:", MODE_OLDFILE);

    mch_enable_volume_requester(req_handle);

    int status;

    if (!fh || Read(fh, buf, len) != len)
	status = FAIL;
    else
	status = OK;

    Close(fh);
    return status;
}

/*
 * Execute "argv" directly without the shell and return the output.
 * Used by system() and systemlist() when the command is a List.
 * "infile" is an optional temp file for stdin input.
 * "flags" is SHELL_SILENT etc.
 * When "ret_len" is not NULL, set it to the length of the output.
 * Returns the output in allocated memory (or NULL on error).
 * Sets v:shell_error to the exit status.
 */
    char_u *
mch_get_cmd_output_direct(
    char	**argv,
    char_u	*infile,
    int		flags UNUSED,
    int		*ret_len)
{
    garray_T ga;
    ga_init2(&ga, sizeof(char_u *), 4);

    // Merge strings into one command
    for (size_t i = 0; argv[i]; ++i)
    {
	ga_copy_string(&ga, (char_u *) argv[i] );
    }

    char_u *buf = NULL;
    char_u *cmd = ga_concat_strings(&ga, " ");

    ga_clear_strings(&ga);

    if (cmd)
    {
	// Use temp file instead of proc + pipe.
	char_u *tmp = vim_tempname('c', FALSE);

	if (tmp)
	{
	    BPTR output = Open(tmp, MODE_NEWFILE);

	    if (output)
	    {
		// Use input if required.
		BPTR input = infile ? Open(infile, MODE_OLDFILE) : (BPTR) NULL;

		SystemTags(cmd, SYS_Output, output, SYS_Input, input, TAG_DONE);

		if (input)
		    Close(input);

		// Get file size
		Seek(output, 0, OFFSET_END);
		LONG size = Seek(output, 0, OFFSET_BEGINNING);

		buf = alloc(size + 1);

		if (buf)
		{
		    buf[size] = NUL;

		    Read(output, buf, size);

		    if (ret_len)
			*ret_len = size;
		}

		Close(output);
		DeleteFile(tmp);
	    }
	    else
	    {
		semsg(_(e_cant_open_file_str), tmp);
	    }

	    vim_free(tmp);
	}

	vim_free(cmd);
    }

    return buf;
}
