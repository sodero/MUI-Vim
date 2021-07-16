/*
 * VIM - Vi IMproved    by Bram Moolenaar
 *
 * *****************************************************************
 * *****************************************************************
 * *****************************************************************
 * *****************************************************************
 * *              This is not a part of the official               *
 * *              version of Vim found on www.vim.org              *
 * *****************************************************************
 * *****************************************************************
 * *****************************************************************
 * *****************************************************************
 *
 * MUI support by Ola Söder. AmigaOS4 port by KAS1E.
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */

#include "vim.h"
#include "version.h"

#include <libraries/asl.h>
#include <mui/TheBar_mcc.h>
#include <graphics/rpattr.h>
#include <proto/muimaster.h>
#include <proto/icon.h>
#include <proto/iffparse.h>
#include <devices/rawkeycodes.h>
#include <clib/alib_protos.h>
#include <clib/debug_protos.h>
#include <libraries/gadtools.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <proto/keymap.h>
#ifdef __amigaos4__
# include <proto/utility.h>
# include <dos/obsolete.h>
#else
# include <cybergraphx/cybergraphics.h>
# include <clib/utility_protos.h>
#endif
#ifdef __AROS__
# include <proto/arossupport.h>
#endif

#ifdef __amigaos4__
typedef unsigned long IPTR;
struct Library *MUIMasterBase = NULL;
struct MUIMasterIFace *IMUIMaster = NULL;
struct Library *CyberGfxBase = NULL;
struct CyberGfxIFace *ICyberGfx = NULL;
struct Library *KeymapBase = NULL;
struct KeymapIFace *IKeymap = NULL;
#endif

//------------------------------------------------------------------------------
// Debug
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------
#ifdef __has_builtin
# if __has_builtin (__builtin_expect)
#  define likely(X) __builtin_expect((X), 1)
#  define unlikely(X) __builtin_expect((X), 0)
# endif
#endif
#ifndef likely
# define likely(X) X
#endif
#ifndef unlikely
# define unlikely(X) X
#endif

//------------------------------------------------------------------------------
// Macros - MUI
//------------------------------------------------------------------------------
#ifdef __MORPHOS__
# define DISPATCH_GATE(C) &C ## Gate
# define DISPATCH_ARGS void
# define DISPATCH_HEAD \
  Class *cls = (Class *) REG_A0; \
  Object *obj = (Object *) REG_A2; \
  Msg msg = (Msg) REG_A1
# define CLASS_DEF(C) \
  static IPTR C ## Dispatch (void); \
  static struct MUI_CustomClass * C ## Class; \
  const struct EmulLibEntry C ## Gate = \
  { TRAP_LIB, 0, (void (*) (void)) C ## Dispatch }; \
  struct C ## Data
#else
# define DISPATCH_HEAD
# define DISPATCH_ARGS Class *cls, Object *obj, Msg msg
# define DISPATCH_GATE(C) C ## Dispatch
# define CLASS_DEF(C) \
  struct MUI_CustomClass * C ## Class; \
  struct C ## Data
#endif
#ifdef __AROS__
# define DISPATCH(C) BOOPSI_DISPATCHER(IPTR, C ## Dispatch, cls, obj, msg)
# define DISPATCH_END BOOPSI_DISPATCHER_END
#else
# define DISPATCH(C) static IPTR C ## Dispatch (DISPATCH_ARGS)
# define DISPATCH_END
#endif
#ifdef __GNUC__
# define MUIDSP static inline __attribute__((always_inline))
#else
# define MUIDSP static inline
#endif

//------------------------------------------------------------------------------
// MUI Class method definition
//------------------------------------------------------------------------------
#define METHOD(C, F, ...) struct MUIP_ ## C ## _ ## F { STACKED IPTR MethodID, \
__VA_ARGS__;}; enum { MUIM_ ## C ## _ ## F = TAG_USER + __LINE__ }; \
MUIDSP IPTR C ## F(Object *me, struct MUIP_ ## C ## _ ## F *msg, \
struct C ## Data *my)
#define METHOD0(C, F, ...) enum { MUIM_ ## C ## _ ## F = TAG_USER + __LINE__ };\
MUIDSP IPTR C ## F(Object *me, struct C ## Data *my)

//------------------------------------------------------------------------------
// MUI Class method dispatch
//------------------------------------------------------------------------------
#define M_FN(C, F) C ## F(obj, (struct MUIP_ ## C ## _ ## F *) msg, \
(struct C ## Data *) INST_DATA(cls,obj))
#define M_FN0(C, F) C ## F(obj, (struct C ## Data *) INST_DATA(cls,obj))
//------------------------------------------------------------------------------
// MUI Class method ID
//------------------------------------------------------------------------------
#define M_ID(C, F) MUIM_ ## C ## _ ## F

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static Object *App, *Con, *Mnu, *Tlb, *Lsg, *Bsg, *Rsg;

//------------------------------------------------------------------------------
// VimCon - MUI custom class handling everything that the console normally
//          takes care of when running Vim in text mode.
//------------------------------------------------------------------------------
CLASS_DEF(VimCon)
{
    int cursor[3];
    int block, state, blink, xdelta, ydelta, space, xd1, yd1, xd2, yd2, width,
        height, left, right, top, bottom;
    struct BitMap *bm;
    struct RastPort rp;
    struct MUI_EventHandlerNode event;
    struct MUI_InputHandlerNode ticker;
#ifdef MUIVIM_FEAT_TIMEOUT
    struct MUI_InputHandlerNode timeout;
#endif
};
#define MUIV_VimCon_State_Idle       (1 << 0)
#define MUIV_VimCon_State_Yield      (1 << 1)
#ifdef MUIVIM_FEAT_TIMEOUT
# define MUIV_VimCon_State_Timeout    (1 << 2)
#endif
#define MUIV_VimCon_State_Reset      (1 << 3)
#define MUIV_VimCon_State_Unknown    (0)
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// VimConAppMessage - AppMessage notification handler
// Input:             Message
// Return:            0
//------------------------------------------------------------------------------
METHOD(VimCon, AppMessage, Message)
{
    // We assume that all arguments are valid files that we have the permission
    // to read from, the number of files equals the number of arguments.
    struct AppMessage *m = (struct AppMessage *) msg->Message;
    char_u **fnames = calloc(m->am_NumArgs, sizeof(char_u *));

    if(unlikely(!fnames))
    {
        kmsg(_(e_outofmem));
        return 0;
    }

    BPTR owd = CurrentDir(m->am_ArgList->wa_Lock);
    int arg = 0, nfiles = 0;

    // Save the names of everything we have the permission to read from.
    while(arg < m->am_NumArgs)
    {
        CurrentDir(m->am_ArgList[arg].wa_Lock);

        // If we can get a read lock, Vim might be able to use this.
        BPTR f = Lock((STRPTR) m->am_ArgList[arg++].wa_Name, ACCESS_READ);

        if(unlikely(!f))
        {
            continue;
        }

        // Allocate buffer for current filename.
        char_u *fn = calloc(PATH_MAX, sizeof(char_u));

        if(unlikely(!fn))
        {
            UnLock(f);

            while(nfiles--)
            {
                // Free all entries.
                free(fnames[nfiles]);
            }

            kmsg(_(e_outofmem));
            break;
        }

        // Find out what the lock is refering to.
        struct FileInfoBlock *fib = (struct FileInfoBlock *) AllocDosObject
                                    (DOS_FIB, NULL);

        if(likely(fib))
        {
            // If it's a file, save it in the list.
            if(likely(Examine(f, fib) && fib->fib_DirEntryType < 0))
            {
                NameFromLock(f, (STRPTR) fn, PATH_MAX);
                fnames[nfiles++] = fn;
            }

            // Otherwise, skip it.
            FreeDosObject(DOS_FIB, fib);
        }

        UnLock(f);
    }

    // Don't do anything if all we get is garbage.
    if(likely(nfiles > 0))
    {
        // Transpose and cap mouse coordinates.
        int x = m->am_MouseX - my->left;
        int y = m->am_MouseY - my->top;

        x = x > 0 ? x : 0;
        y = y > 0 ? y : 0;
        x = x >= my->width ? my->width - 1 : x;
        y = y >= my->height ? my->height - 1 : y;

        // There was something among the arguments that we could not acquire a
        // read lock for. Shrink list of files before handing it over to Vim.
        if(unlikely(nfiles < m->am_NumArgs))
        {
            char_u **shrunk = calloc(nfiles, sizeof(char_u *));

            if(likely(shrunk))
            {
                // Copy old contents to new list.
                for(arg = 0; arg < nfiles; ++arg)
                {
                    shrunk[arg] = fnames[arg];
                }
            }

            // Replace old list with new list.
            free(fnames);
            fnames = shrunk;
        }

        // The shrunk allocation could have failed.
        if(likely(fnames))
        {
            // Vim will sometimes try to interact with the user when handling
            // the file drop. Activate the window to save one annoying mouse
            // click if that's the case.
            set(_win(me), MUIA_Window_Activate, TRUE);
            gui_handle_drop(x, y, 0, fnames, nfiles);
        }
        else
        {
            // Shrinkage failed.
            kmsg(_(e_outofmem));
        }
    }
    else
    {
        // Nothing to pass over to Vim we need to free this ourselves.
        free(fnames);
    }

    // Go back to where we started and show whatever was read.
    CurrentDir(owd);
    return 0;
}

//------------------------------------------------------------------------------
// VimConStopBlink - Disable cursor blinking
// Input:            -
// Return:           TRUE on state change, FALSE otherwise
//------------------------------------------------------------------------------
METHOD0(VimCon, StopBlink)
{
    if(unlikely(!my->blink || my->block))
    {
        return FALSE;
    }

    // Remove input handler and reset status.
    DoMethod(_app(me), MUIM_Application_RemInputHandler, &my->ticker);
    my->blink = 0;
    return TRUE;
}

//------------------------------------------------------------------------------
// VimConIsBlinking - Get cursor state
// Input:             -
// Return:            TRUE if cursor is blinking, FALSE otherwise
//------------------------------------------------------------------------------
METHOD0(VimCon, IsBlinking)
{
    // Indexing any of the delays?
    return my->blink ? TRUE : FALSE;
}

//------------------------------------------------------------------------------
// VimConAboutMUI - Show MUI about window
// Input:         -
// Return:        0
//------------------------------------------------------------------------------
METHOD0(VimCon, AboutMUI)
{
    // Needed to not mess up the message loop.
    my->state |= MUIV_VimCon_State_Yield;
    DoMethod(_app(me), MUIM_Application_AboutMUI, _win(me));
    return 0;
}

//------------------------------------------------------------------------------
// VimConClear - Clear console
// Input:        -
// Return:       TRUE
//------------------------------------------------------------------------------
METHOD0(VimCon, Clear)
{
    if(unlikely(!my->width || !my->height))
    {
        return FALSE;
    }

    FillPixelArray(&my->rp, 0, 0, my->width, my->height, gui.back_pixel);
    MUI_Redraw(me, MADF_DRAWOBJECT);
    return TRUE;
}

//------------------------------------------------------------------------------
// VimConMUISettings - Open MUI settings
// Input:            -
// Return:           0
//------------------------------------------------------------------------------
METHOD0(VimCon, MUISettings)
{
    // Needed to not mess up the message loop.
    my->state |= MUIV_VimCon_State_Yield;
    DoMethod(_app(me), MUIM_Application_OpenConfigWindow, _win(me));
    return 0;
}

//------------------------------------------------------------------------------
// VimConStartBlink - Enable cursor blinking
// Input:             -
// Return:            TRUE on state change, FALSE otherwise
//------------------------------------------------------------------------------
METHOD0(VimCon, StartBlink)
{
    // If not enabled and none of the delays (wait, on, off) are 0 add input
    // handler and increase status / delay index.
    if(likely(!my->block && !my->blink && my->cursor[0] && my->cursor[1] &&
               my->cursor[2]))
    {
        my->ticker.ihn_Millis = my->cursor[my->blink++];
        DoMethod(_app(me), MUIM_Application_AddInputHandler, &my->ticker);
        return TRUE;
    }

    // Nothing to do.
    return FALSE;
}

//------------------------------------------------------------------------------
// VimConSetBlinking - Set wait, on and off values
// Input:              Wait - initial delay in ms until blinking starts
//                     On - number of ms when the cursor is shown
//                     Off - number of ms when the cursor is hidden
// Return:             TRUE
//------------------------------------------------------------------------------
METHOD(VimCon, SetBlinking, Wait, On, Off)
{
    // Accept anything. Filter later.
    my->cursor[0] = (int) msg->Wait;
    my->cursor[1] = (int) msg->Off;
    my->cursor[2] = (int) msg->On;
    return TRUE;
}

//------------------------------------------------------------------------------
// VimConBrowse - Open file requester
// Input:         Title - Title of the file requester to be created
//                Drawer - FIXME
// Return:        Absolute path of selected file / NULL if cancelled
//------------------------------------------------------------------------------
METHOD(VimCon, Browse, Title, Drawer)
{
    STRPTR res = NULL;
    struct FileRequester *req;

    // Create file requester.
    req = MUI_AllocAslRequestTags(ASL_FileRequest, ASLFR_TitleText, msg->Title,
                                  ASLFR_InitialDrawer, msg->Drawer, TAG_DONE);
    if(unlikely(!req))
    {
        kmsg(_(e_outofmem));
        return (IPTR) NULL;
    }

    // Go to sleep and show requester.
    set(_app(me), MUIA_Application_Sleep, TRUE);

    if(likely(MUI_AslRequestTags(req,TAG_DONE) && req->fr_File))
    {
        // 2 extra bytes for term 0 + AddPart() separator
        size_t s = STRLEN(req->fr_Drawer) + STRLEN(req->fr_File) + 2;

        // Vim will take care of freeing this memory.
        res = calloc(s, sizeof(unsigned char));

        if(likely(res))
        {
            // Prepare the result
            STRCPY(res, req->fr_Drawer);
            AddPart(res, req->fr_File, s);
        }
    }

    // Free memory and wake up.
    set(_app(me), MUIA_Application_Sleep, FALSE);
    MUI_FreeAslRequest(req);
    return (IPTR) res;
}

//------------------------------------------------------------------------------
// VimConGetScreenDim - Get screen dimensions, or rather the dimensions of the
//                      off screen buffer used to render text. This might not
//                      be the same as the screen dimensions, but as far as
//                      Vim is concerned, it is.
// Input:               WidthPtr - IPTR * to contain (output) width
//                      HeightPtr - IPTR * to contain (output) height
// Return:              Buffer area
//------------------------------------------------------------------------------
METHOD(VimCon, GetScreenDim, WidthPtr, HeightPtr)
{
    if(unlikely(!my->bm))
    {
        kmsg(_(e_null));
        return 0;
    }

    IPTR *w = (IPTR*) msg->WidthPtr, *h = (IPTR*) msg->HeightPtr;
    *w = GetBitMapAttr(my->bm, BMA_WIDTH);
    *h = GetBitMapAttr(my->bm, BMA_HEIGHT);
    return (*w) * (*h);
}

//------------------------------------------------------------------------------
// VimConSetTitle - Set window title
// Input:           Title - Window title
// Return:          TRUE on success, FALSE otherwise
//------------------------------------------------------------------------------
METHOD(VimCon, SetTitle, Title)
{
    if(unlikely(!msg->Title))
    {
        return FALSE;
    }

    set(_win(me), MUIA_Window_Title, msg->Title);
    return TRUE;
}

//------------------------------------------------------------------------------
// VimConTicker - Event handler managing cursor state
// Input:         -
// Return:        TRUE if state changed, FALSE otherwise
//------------------------------------------------------------------------------
METHOD0(VimCon, Ticker)
{
    // Only on (2) or off (1) here.
    if(unlikely(my->blink < 1 || my->blink > 2))
    {
        kmsg(_(e_internal));
        return FALSE;
    }

    // Remove current timer
    DoMethod(_app(me), MUIM_Application_RemInputHandler, &my->ticker);

    // Save current state.
    int state = my->blink;

    // Hide or show cursor depending on index
    if(my->blink == 1)
    {
        // Vim might invoke stop blink.
        my->blink = 0;

        // Hide cursor
        gui_undraw_cursor();
    }
    else
    {
        // Vim might invoke stop blink.
        my->blink = 0;

        // Show cursor
        gui_update_cursor(TRUE, FALSE);
    }

    // Restore state.
    my->blink = state;

    // Install new timer. Disallow values lower than 100ms (perf. reasons)
    my->ticker.ihn_Millis = my->cursor[my->blink] >= 100 ?
                            my->cursor[my->blink] : 100;

    DoMethod(_app(me), MUIM_Application_AddInputHandler, &my->ticker);

    // Next index 1,2,1,2,1,2,1..
    my->blink = ~my->blink & 3;

    // Make the results visible
    MUI_Redraw(me, MADF_DRAWUPDATE);
    return TRUE;
}

#ifdef MUIVIM_FEAT_TIMEOUT
//------------------------------------------------------------------------------
// VimConTimeout - Timeout handler
// Input:          -
// Return:         TRUE
//------------------------------------------------------------------------------
METHOD0(VimCon, Timeout)
{
    // Take note and keep on going
    my->state |= MUIV_VimCon_State_Timeout;
    return TRUE;
}

//------------------------------------------------------------------------------
// VimConSetTimeout - Set time (in ms) until next timeout
// Input:             Timeout - Number of ms until the next timeout or 0.
//                              A value of 0 will disable timeouts.
// Return:            TRUE if state changed, FALSE otherwise
//------------------------------------------------------------------------------
METHOD(VimCon, SetTimeout, Timeout)
{
    // Only act if old timeout != new timout
    if(unlikely(my->timeout.ihn_Millis == msg->Timeout))
    {
        return FALSE;
    }

    // If old timeout exists, remove it
    if(unlikely(my->timeout.ihn_Millis))
    {
        DoMethod(_app(me), MUIM_Application_RemInputHandler, &my->timeout);
        my->timeout.ihn_Millis = 0;
    }

    // If new timeout > 0, install new handler
    if(likely(msg->Timeout))
    {
        my->timeout.ihn_Millis = (UWORD) msg->Timeout;
        DoMethod(_app(me), MUIM_Application_AddInputHandler, &my->timeout);
    }

    return TRUE;
}
#endif

//------------------------------------------------------------------------------
// VimConDirty - Tag rectangular part of raster port as dirty
// Input:        x1 - Left
//               y1 - Top
//               x2 - Right
//               y2 - Bottom
// Return:       -
//------------------------------------------------------------------------------
MUIDSP void VimConDirty(struct VimConData *my, int x1, int y1, int x2, int y2)
{
    // Grow dirty region if it doesn't cover the new one. Check bounds.
    if(x1 < my->xd1)
    {
        if(likely(x1 >= 0 && x1 < my->width))
        {
            my->xd1 = x1;
        }
        else
        {
            my->xd1 = 0;
        }
    }

    if(x2 > my->xd2)
    {
        if(likely(x2 >= x1 && x2 < my->width))
        {
            my->xd2 = x2;
        }
        else
        {
            my->xd2 = my->width - 1;
        }
    }

    if(y1 < my->yd1)
    {
        if(likely((y1 >= 0 && y1 < my->height)))
        {
            my->yd1 = y1;
        }
        else
        {
            my->yd1 = 0;
        }
    }

    if(y2 > my->yd2)
    {
        if(likely((y2 >= y1 && y2 < my->height)))
        {
            my->yd2 = y2;
        }
        else
        {
            my->yd2 = my->height - 1;
        }
    }
}

//------------------------------------------------------------------------------
// VimConClean - Consider raster port clean
// Input:        -
// Return:       -
//------------------------------------------------------------------------------
MUIDSP void VimConClean(struct VimConData *my)
{
    // Start with a 'negative' size
    my->xd1 = my->yd1 = INT_MAX;
    my->xd2 = my->yd2 = INT_MIN;
}

//------------------------------------------------------------------------------
// VimConDrawHollowCursor - Draw outline cursor
// Input:                   Row - Y
//                          Col - X
//                          Color - RGB color of cursor
// Return:                  TRUE on success, FALSE otherwise
//------------------------------------------------------------------------------
METHOD(VimCon, DrawHollowCursor, Row, Col, Color)
{
    int x1 = msg->Col * my->xdelta, y1 = msg->Row * my->ydelta,
        x2 = x1 + my->xdelta - my->space, y2 = y1 + my->ydelta;

    if(unlikely(x2 <= x1 || y2 <= y1))
    {
        return FALSE;
    }

    FillPixelArray(&my->rp, x1, y1, my->xdelta, 1, msg->Color);
    FillPixelArray(&my->rp, x1, y1, 1, my->ydelta, msg->Color);
    FillPixelArray(&my->rp, x1, y2, my->xdelta, 1, msg->Color);
    FillPixelArray(&my->rp, x2, y1, 1, my->ydelta, msg->Color);
    VimConDirty(my, x1, y1, x2, y2);
    return TRUE;
}

//------------------------------------------------------------------------------
// VimConDrawPartCursor - Draw part of cursor
// Input:                 Row - Y
//                        Col - X
//                        Width - Width in pixels
//                        Height - Height in pixels
//                        Color - RGB color
// Return:                TRUE on success, FALSE otherwise
//------------------------------------------------------------------------------
METHOD(VimCon, DrawPartCursor, Row, Col, Width, Height, Color)
{
    int x = msg->Col * my->xdelta, xs = msg->Width,
        y = msg->Row * my->ydelta + my->ydelta - msg->Height,
        ys = msg->Height - my->space;

    if(unlikely(xs < 1 || ys < 1))
    {
        return FALSE;
    }

    FillPixelArray(&my->rp, x, y, xs, ys, msg->Color);
    VimConDirty(my, x, y, x + xs, y + ys);
    return TRUE;
}

//------------------------------------------------------------------------------
// VimConInvertRect - Invert colors in rectangular character block
// Input:             Row - Top character row
//                    Col - Left character column
//                    Rows - Number of rows
//                    Cols - Number of columns
// Return:            TRUE on success, FALSE otherwise
//------------------------------------------------------------------------------
METHOD(VimCon, InvertRect, Row, Col, Rows, Cols)
{
    int x = msg->Col * my->xdelta, xs = my->xdelta * msg->Cols,
        y = msg->Row * my->ydelta, ys = my->ydelta * msg->Rows;

    if(unlikely(xs < 1 || ys < 1))
    {
        return FALSE;
    }

    InvertPixelArray(&my->rp, x, y, xs, ys);
    VimConDirty(my, x, y, x + xs, y + ys);
    return TRUE;
}

//------------------------------------------------------------------------------
// VimConFillBlock - Fill character block with color
// Input:            Row1 - Top character row
//                   Col1 - Left character column
//                   Row2 - Bottom character row
//                   Col2 - Right character column
//                   Color - RGB color
// Return:           TRUE on success, FALSE otherwise
//------------------------------------------------------------------------------
METHOD(VimCon, FillBlock, Row1, Col1, Row2, Col2, Color)
{
    int x = msg->Col1 * my->xdelta, xs = my->xdelta * (msg->Col2 + 1) - x,
        y = msg->Row1 * my->ydelta, ys = my->ydelta * (msg->Row2 + 1) - y;

    if(unlikely(xs < 1 || ys < 1))
    {
        return FALSE;
    }

    // We might be dealing with incomplete characters
    if(xs + x > my->width)
    {
        xs = my->width - x;
    }

    if(ys + y > my->height)
    {
        ys = my->height - y;
    }

    FillPixelArray(&my->rp, x, y, xs, ys, msg->Color);
    VimConDirty(my, x, y, x + xs, y + ys);
    return TRUE;
}

//------------------------------------------------------------------------------
// VimConDeleteLines - Delete line(s) of text / insert empty line(s) within
//                     rectangular text region
// Input:              Row - (1st) row to delete / where to insert empty line(s)
//                     Lines - Number of lines to delete / insert
//                     RegLeft - Left side of text region
//                     RegRight - Right side of text region
//                     RegBottom - Bottom of text region
//                     Color - RGB color used to fill empty lines
// Return:             TRUE if state changed, FALSE otherwise
//------------------------------------------------------------------------------
METHOD(VimCon, DeleteLines, Row, Lines, RegLeft, RegRight, RegBottom, Color)
{
    int n = (int) msg->Lines;

    if(unlikely(!n))
    {
        return FALSE;
    }

    int yctop, ycsiz, ydst, ysrc, xsrcdst = my->xdelta * msg->RegLeft,
        xsize = my->xdelta * (msg->RegRight + 1) - xsrcdst,
        ysize = my->ydelta * (msg->RegBottom + 1) - msg->Row * my->ydelta;

    if(n > 0)
    {
        // Deletion
        ysize -= n * my->ydelta;
        ydst = msg->Row * my->ydelta;
        ysrc = ydst + n * my->ydelta;
        yctop = ydst + ysize;
        ycsiz = n * my->ydelta;
        VimConDirty(my, xsrcdst, ydst,  xsrcdst + xsize, yctop + ycsiz);
    }
    else
    {
        // Insertion
        ysize += n * my->ydelta;
        yctop = ysrc = msg->Row * my->ydelta;
        ydst = ysrc - n * my->ydelta;
        ycsiz = - (n * my->ydelta);
        VimConDirty(my, xsrcdst, yctop,  xsrcdst + xsize, ydst + ysize);
    }

    // Blit and fill the abandoned area with color
    MovePixelArray(xsrcdst, ysrc, &my->rp, xsrcdst, ydst , xsize, ysize);
    FillPixelArray(&my->rp, xsrcdst, yctop, xsize, ycsiz, msg->Color);
    return TRUE;
}

#ifdef __amigaos4__
# define RPTAG_FgColor RPTAG_APenColor
# define RPTAG_BgColor RPTAG_BPenColor
# define ALPHA_MASK 0xFF000000
#else
# define ALPHA_MASK 0x00000000
#endif
//------------------------------------------------------------------------------
// VimConSetFgColor - Set foreground color
// Input:             Color - RGB color
// Return:            TRUE
//------------------------------------------------------------------------------
METHOD(VimCon, SetFgColor, Color)
{
    static struct TagItem tags[] =
    {
        { .ti_Tag = RPTAG_FgColor, .ti_Data = 0},
        { .ti_Tag = TAG_END, .ti_Data = 0 }
    };

    if(likely((msg->Color|ALPHA_MASK) == tags[0].ti_Data))
    {
        return FALSE;
    }

    tags[0].ti_Data = msg->Color|ALPHA_MASK;
    SetRPAttrsA(&my->rp, tags);
    return TRUE;
}

//------------------------------------------------------------------------------
// VimConSetBgColor - Set background color
// Input:             Color - RGB color
// Return:            TRUE
//------------------------------------------------------------------------------
METHOD(VimCon, SetBgColor, Color)
{
    static struct TagItem tags[] =
    {
        { .ti_Tag = RPTAG_BgColor, .ti_Data = 0},
        { .ti_Tag = TAG_END, .ti_Data = 0 }
    };

    if(unlikely((msg->Color|ALPHA_MASK) == tags[0].ti_Data))
    {
        return FALSE;
    }

    tags[0].ti_Data = msg->Color|ALPHA_MASK;
    SetRPAttrsA(&my->rp, tags);
    return TRUE;
}

//------------------------------------------------------------------------------
// VimConDrawString - Render string of text
// Input:             Row - Character row
//                    Col - Character column
//                    Str - String
//                    Len - Number of characters to render
//                    Flags - DRAW_UNDERL | DRAW_BOLD | DRAW_TRANSP
// Return:            TRUE if anything was rendered, FALSE otherwise
//------------------------------------------------------------------------------
METHOD(VimCon, DrawString, Row, Col, Str, Len, Flags)
{
    static IPTR flags;

    if(unlikely(flags != msg->Flags))
    {
        // Store until next invocation.
        flags = msg->Flags;

        IPTR style = FS_NORMAL;

        // Translate Vim bold flag.
        if(likely(msg->Flags & DRAW_BOLD))
        {
            style |= FSF_BOLD;
        }

        // Translate Vim underline flag.
        if(unlikely(msg->Flags & DRAW_UNDERL))
        {
            style |= FSF_UNDERLINED;
        }

        IPTR mode = JAM2;

        // Translate Vim transparency flag.
        if(unlikely(msg->Flags & DRAW_TRANSP))
        {
            mode = JAM1;
        }

#ifdef RPTAG_SoftStyle
        SetRPAttrs(&my->rp, RPTAG_DrMd, mode, RPTAG_SoftStyle, style, TAG_END);
#else
        SetSoftStyle(&my->rp, style, FS_NORMAL|FSF_BOLD|FSF_UNDERLINED);
        SetRPAttrs(&my->rp, RPTAG_DrMd, mode, TAG_END);
#endif
    }

    int y = msg->Row * my->ydelta, x = msg->Col * my->xdelta;

    // Tag area as dirty and move into position.
    VimConDirty(my, x, y, x + msg->Len * my->xdelta, y + my->ydelta);
    Move(&my->rp, x, y + my->rp.TxBaseline);
    Text(&my->rp, (CONST_STRPTR) msg->Str, msg->Len);

    return TRUE;
}

//------------------------------------------------------------------------------
// VimConNew - Overloading OM_NEW
// Input:      See BOOPSI docs
// Return:     See BOOPSI docs
//------------------------------------------------------------------------------
MUIDSP IPTR VimConNew(Class *cls, Object *obj, struct opSet *msg)
{
    struct TagItem tags[] =
    {
        { .ti_Tag = MUIA_Frame, .ti_Data = MUIV_Frame_None },
        { .ti_Tag = MUIA_InputMode, .ti_Data = MUIV_InputMode_None },
        { .ti_Tag = MUIA_FillArea, .ti_Data = TRUE },
        { .ti_Tag = MUIA_Font, .ti_Data =  MUIV_Font_Fixed },
        { .ti_Tag = TAG_MORE, .ti_Data = (IPTR) msg->ops_AttrList }
    };

    obj = (Object *) DoSuperMethod(cls, obj, OM_NEW, tags, NULL);

    if(unlikely(!obj))
    {
        kmsg(_(e_outofmem));
        return (IPTR) NULL;
    }

    struct Screen *s = LockPubScreen(NULL);

    if(unlikely(!s))
    {
        kmsg(_(e_null));
        CoerceMethod(cls, obj, OM_DISPOSE);
        return (IPTR) NULL;
    }

    struct VimConData *my = INST_DATA(cls,obj);

    my->bm = (struct BitMap *) AllocBitMap
             (GetBitMapAttr(s->RastPort.BitMap, BMA_WIDTH),
              GetBitMapAttr(s->RastPort.BitMap, BMA_HEIGHT),
              GetBitMapAttr(s->RastPort.BitMap, BMA_DEPTH),
#ifndef __MORPHOS__
              BMF_CLEAR,
              s->RastPort.BitMap
#else
              BMF_CLEAR | BMF_REQUESTVMEM,
              NULL
#endif
              );

    UnlockPubScreen(NULL, s);

    if(unlikely(!my->bm))
    {
        kmsg(_(e_outofmem));
        CoerceMethod(cls, obj, OM_DISPOSE);
        return (IPTR) NULL;
    }

    // Initial RP settings
    InitRastPort(&my->rp);
    my->rp.BitMap = my->bm;
    SetRPAttrs(&my->rp, RPTAG_DrMd, JAM2,
#ifndef __amigaos4__
               RPTAG_PenMode, FALSE,
#endif
               RPTAG_FgColor, 0,
               RPTAG_BgColor, 0,
               TAG_DONE);

    // Static settings
    my->cursor[0] = 700;
    my->cursor[1] = 250;
    my->cursor[2] = 400;
    my->xdelta = my->ydelta = 1;
    my->xd1 = my->yd1 = INT_MAX;
    my->xd2 = my->yd2 = INT_MIN;
    my->state = MUIV_VimCon_State_Idle;
    my->block = FALSE;
    my->blink = my->space = my->width =
    my->height = my->left = my->right =
    my->top = my->bottom = 0;
#ifdef MUIVIM_FEAT_TIMEOUT
    my->timeout.ihn_Object = obj;
    my->timeout.ihn_Millis = 0;
    my->timeout.ihn_Flags = MUIIHNF_TIMER;
    my->timeout.ihn_Method = M_ID(VimCon, Timeout);
#endif
    my->ticker.ihn_Object = obj;
    my->ticker.ihn_Flags = MUIIHNF_TIMER;
    my->ticker.ihn_Method = M_ID(VimCon, Ticker);
    my->event.ehn_Priority = 1;
    my->event.ehn_Flags = 0;
    my->event.ehn_Object = obj;
    my->event.ehn_Class = NULL;
    my->event.ehn_Events = IDCMP_RAWKEY |
#ifdef __amigaos4__
                           IDCMP_EXTENDEDMOUSE |
#endif
                           IDCMP_MOUSEBUTTONS;
    return (IPTR) obj;
}

//------------------------------------------------------------------------------
// VimConSetup - Overloading MUIM_Setup
// Input:        See MUI docs
// Return:       See MUI docs
//------------------------------------------------------------------------------
MUIDSP IPTR VimConSetup(Class *cls, Object *obj, struct MUI_RenderInfo *msg)
{
    // Setup parent class
    if(unlikely(!DoSuperMethodA(cls, obj, (Msg) msg)))
    {
        kmsg(_(e_internal));
        return FALSE;
    }

    struct VimConData *my = INST_DATA(cls,obj);

    // Font might have changed
    SetFont(&my->rp, _font(obj));

    // Make room for smearing, no overlap allowed
    my->rp.TxSpacing = my->rp.Font->tf_BoldSmear;

    // If we aren't using a bitmap font we need to do more work to determine the
    // right amount of spacing necessary to avoid overlap
    if(unlikely(!(my->rp.Font->tf_Flags & FPF_DESIGNED)))
    {
        struct TextExtent te;
        TextExtent(&my->rp, (STRPTR) "VI", 2, &te);
        int xs = te.te_Extent.MaxX - te.te_Extent.MinX;
        xs -= my->rp.TxWidth * 2;
        my->rp.TxSpacing += xs;
    }

    my->xdelta = my->rp.TxWidth + my->rp.TxSpacing;
    my->ydelta = my->rp.TxHeight;
    my->space = my->rp.TxSpacing;
    gui.char_width = my->xdelta;
    gui.char_height = my->ydelta;
    gui.scrollbar_width = gui.scrollbar_height = 0;
    gui.toolbar_height = 0;
    gui.menu_height = 0;

    // Install the main event handler
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, &my->event);

#ifdef MUIVIM_FEAT_TIMEOUT
    // Install timeout timer if previously present
    if(unlikely(my->timeout.ihn_Millis))
    {
        DoMethod(_app(obj), MUIM_Application_AddInputHandler, &my->timeout);
    }
#endif

    // Clear Vim communication blocker.
    my->block = FALSE;

    // Install blink handler if previously present
    if(likely(my->blink))
    {
        DoMethod(_app(obj), MUIM_Application_AddInputHandler, &my->ticker);
    }

    return TRUE;
}

//------------------------------------------------------------------------------
// VimConDispose - Overloading OM_DISPOSE
// Input:          See BOOPSI docs
// Return:         See BOOPSI docs
//------------------------------------------------------------------------------
MUIDSP IPTR VimConDispose(Class *cls, Object *obj, Msg msg)
{
    struct VimConData *my = INST_DATA(cls,obj);

    if(likely(my->bm))
    {
        WaitBlit();
        FreeBitMap(my->bm);
        my->bm = NULL;
    }

    return DoSuperMethodA(cls, obj, msg);
}

//------------------------------------------------------------------------------
// VimConCleanup - Overloading MUIM_Cleanup
// Input:          See MUI docs
// Return:         See MUI docs
//------------------------------------------------------------------------------
MUIDSP IPTR VimConCleanup(Class *cls, Object *obj, Msg msg)
{
    struct VimConData *my = INST_DATA(cls,obj);

#ifdef MUIVIM_FEAT_TIMEOUT
    // Remove timeout timer if present
    if(unlikely(my->timeout.ihn_Millis))
    {
        DoMethod(_app(obj), MUIM_Application_RemInputHandler, &my->timeout);
    }
#endif
    // Remove cursor blink timer if present
    if(likely(my->blink))
    {
        DoMethod(_app(obj), MUIM_Application_RemInputHandler, &my->ticker);
    }

    // Input handler for keys and mouse is always present
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, &my->event);

    // Let the superclass do its part
    return (IPTR) DoSuperMethodA(cls, obj, (Msg) msg);
}

//------------------------------------------------------------------------------
// VimConAddEvent - Add event subscription to event handler
// Input:           IDCMP event to add
// Return:          -
//------------------------------------------------------------------------------
static void VimConAddEvent(Class *cls, Object *obj, IPTR event)
{
    struct VimConData *my = INST_DATA(cls,obj);

    // Don't add events more than once.
    if(unlikely(event == (my->event.ehn_Events & event)))
    {
        return;
    }

    // Can't modify event handlers, we need to remove and add them again.
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, &my->event);
    my->event.ehn_Events |= event;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, &my->event);
}

//------------------------------------------------------------------------------
// VimConRemEvent - Remove event subscription from event handler
// Input:           IDCMP event to remove
// Return:          -
//------------------------------------------------------------------------------
static void VimConRemEvent(Class *cls, Object *obj, ULONG event)
{
    struct VimConData *my = INST_DATA(cls,obj);

    // Don't remove what's not there.
    if(unlikely(!(my->event.ehn_Events & event)))
    {
        return;
    }

    // Can't modify event handlers, we need to remove and add them again.
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, &my->event);
    my->event.ehn_Events &= ~event;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, &my->event);
}

//------------------------------------------------------------------------------
// VimConMouseScrollEvent - Determine in which direction(s) we need to scroll.
// Input:                   IDCMP MOUSEMOVE event messaage
// Return:                  Vim mouse scroll event
//------------------------------------------------------------------------------
MUIDSP int VimConMouseScrollEvent(Class *cls, Object *obj,
                                  struct MUIP_HandleEvent *msg)
{
    struct VimConData *my = INST_DATA(cls,obj);
    int x = msg->imsg->MouseX, y = msg->imsg->MouseY,
        event = (y - my->top < 0 ? MOUSE_4 : 0) |
                (y - my->top >= my->height ? MOUSE_5 : 0) |
                (x - my->left < 0 ? MOUSE_7 : 0) |
                (x - my->left >= my->width ? MOUSE_6 : 0);
    return event;
}

//------------------------------------------------------------------------------
// VimConMouseMove - Handle mouse dragging
// Input:            IDCMP MOUSEMOVE or INTUITICKS
// Return:           TRUE
//------------------------------------------------------------------------------
MUIDSP int VimConMouseMove(Class *cls, Object *obj,
                           struct MUIP_HandleEvent *msg)
{
    struct VimConData *my = INST_DATA(cls,obj);
    static int x = 0, y = 0, tick = FALSE;
    int out = msg->imsg->MouseX <= my->left || msg->imsg->MouseX >= my->right ||
              msg->imsg->MouseY <= my->top || msg->imsg->MouseY >= my->bottom;

    // Replace MOUSEMOVE with INTUITICKS when we're outside
    if(unlikely(out))
    {
        int event = VimConMouseScrollEvent(cls, obj, msg);
        gui_send_mouse_event(event, x, y, FALSE, 0);

        // But only do it once
        if(unlikely(!tick))
        {
            VimConRemEvent(cls, obj, IDCMP_MOUSEMOVE);
            VimConAddEvent(cls, obj, IDCMP_INTUITICKS);
            tick = TRUE;
        }
    }
    // Replace INTUITICKS with MOUSEMOVE when we're inside
    else
    {
        x = msg->imsg->MouseX - my->left;
        y = msg->imsg->MouseY - my->top;

        // But only do it once
        if(unlikely(tick))
        {
            VimConRemEvent(cls, obj, IDCMP_INTUITICKS);
            VimConAddEvent(cls, obj, IDCMP_MOUSEMOVE);
            tick = FALSE;
        }
    }

    gui_send_mouse_event(MOUSE_DRAG, x, y, FALSE, 0);
    return TRUE;
}

//------------------------------------------------------------------------------
// VimConMouseHandleButton - Handle mouse click and release
// Input:                    IDCMP MOUSEBUTTON event messaage
// Return:                   TRUE if handled, FALSE otherwise.
//------------------------------------------------------------------------------
MUIDSP int VimConMouseHandleButton(Class *cls, Object *obj,
                                   struct MUIP_HandleEvent *msg)
{
    struct VimConData *my = INST_DATA(cls,obj);
    static int drag;
    int out = msg->imsg->MouseX <= my->left ||
              msg->imsg->MouseX >= my->right ||
              msg->imsg->MouseY <= my->top ||
              msg->imsg->MouseY >= my->bottom;

    // Left button within region -> start dragging
    if(msg->imsg->Code == SELECTDOWN && !out)
    {
        drag = TRUE;
        VimConAddEvent(cls, obj, IDCMP_MOUSEMOVE);
        gui_send_mouse_event(MOUSE_LEFT, msg->imsg->MouseX - my->left,
                             msg->imsg->MouseY - my->top, FALSE, 0);
        return TRUE;
    }

    // Left button released -> stop dragging if we're doing so.
    if(drag && msg->imsg->Code == SELECTUP)
    {
        drag = FALSE;
        VimConRemEvent(cls, obj, IDCMP_MOUSEMOVE|IDCMP_INTUITICKS);
        gui_send_mouse_event(MOUSE_RELEASE, msg->imsg->MouseX - my->left,
                             msg->imsg->MouseY - my->top, FALSE, 0);
        return TRUE;
    }

    return FALSE;
}

//------------------------------------------------------------------------------
// VimConHandleRaw - Handle raw key event
// Input:            IDCMP RAW event message
// Return:           TRUE
//------------------------------------------------------------------------------
MUIDSP int VimConHandleRaw(Class *cls, Object *obj,
                           struct MUIP_HandleEvent *msg)
{
    static TEXT b[4];
    static char_u s[6];
    static struct InputEvent ie = { .ie_Class = IECLASS_RAWKEY };

    ie.ie_Code = msg->imsg->Code;
    ie.ie_Qualifier = msg->imsg->Qualifier;

    // Are we dealing with a vanilla key?
    WORD w = MapRawKey(&ie, (STRPTR) b, 4, 0);

    if(w == 1)
    {
        add_to_input_buf(b, w);
        return TRUE;
    }

    int c;

    // No, something else....
    switch(msg->imsg->Code)
    {
        case RAWKEY_UP: c = TO_SPECIAL('k', 'u'); break;
        case RAWKEY_DOWN: c = TO_SPECIAL('k', 'd'); break;
        case RAWKEY_LEFT: c = TO_SPECIAL('k', 'l'); break;
        case RAWKEY_RIGHT: c = TO_SPECIAL('k', 'r'); break;
        case RAWKEY_F1: c = TO_SPECIAL('k', '1'); break;
        case RAWKEY_F2: c = TO_SPECIAL('k', '2'); break;
        case RAWKEY_F3: c = TO_SPECIAL('k', '3'); break;
        case RAWKEY_F4: c = TO_SPECIAL('k', '4'); break;
        case RAWKEY_F5: c = TO_SPECIAL('k', '5'); break;
        case RAWKEY_F6: c = TO_SPECIAL('k', '6'); break;
        case RAWKEY_F7: c = TO_SPECIAL('k', '7'); break;
        case RAWKEY_F8: c = TO_SPECIAL('k', '8'); break;
        case RAWKEY_F9: c = TO_SPECIAL('k', '9'); break;
        case RAWKEY_F10: c = TO_SPECIAL('k', ';'); break;
        case RAWKEY_F11: c = TO_SPECIAL('F', '1'); break;
        case RAWKEY_F12: c = TO_SPECIAL('F', '2'); break;
        case RAWKEY_HELP: c = TO_SPECIAL('%', '1'); break;
        case RAWKEY_INSERT: c = TO_SPECIAL('k', 'I'); break;
        case RAWKEY_HOME: c = TO_SPECIAL('k', 'h'); break;
        case RAWKEY_END: c = TO_SPECIAL('@', '7'); break;
        case RAWKEY_PAGEUP: c = TO_SPECIAL('k', 'P'); break;
        case RAWKEY_PAGEDOWN: c = TO_SPECIAL('k', 'N'); break;
        case RAWKEY_NM_WHEEL_DOWN:
            gui_send_mouse_event(MOUSE_5, msg->imsg->MouseX, msg->imsg->MouseY,
                                 FALSE, 0);
            return TRUE;

        case RAWKEY_NM_WHEEL_UP:
            gui_send_mouse_event(MOUSE_4, msg->imsg->MouseX, msg->imsg->MouseY,
                                 FALSE, 0);
            return TRUE;

        case RAWKEY_NM_WHEEL_LEFT:
            gui_send_mouse_event(MOUSE_7, msg->imsg->MouseX, msg->imsg->MouseY,
                                 FALSE, 0);
            return TRUE;

        case RAWKEY_NM_WHEEL_RIGHT:
            gui_send_mouse_event(MOUSE_6, msg->imsg->MouseX, msg->imsg->MouseY,
                                 FALSE, 0);
            return TRUE;

        default: return FALSE;
    }

    if(unlikely(!c))
    {
        return FALSE;
    }

    int m = (msg->imsg->Qualifier & IEQUALIFIER_CONTROL) ? MOD_MASK_CTRL : 0;

    m |= (msg->imsg->Qualifier &
         (IEQUALIFIER_LALT|IEQUALIFIER_RALT)) ? MOD_MASK_ALT : 0;
    m |= (msg->imsg->Qualifier &
         (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT)) ? MOD_MASK_SHIFT : 0;
    m |= (msg->imsg->Qualifier &
         (IEQUALIFIER_LCOMMAND|IEQUALIFIER_RCOMMAND)) ? MOD_MASK_META : 0;

    c = simplify_key(c, &m);

    int l = 0;

    if(unlikely(m))
    {
        s[l++] = CSI;
        s[l++] = KS_MODIFIER;
        s[l++] = (char_u) m;
    }

    if(likely(IS_SPECIAL(c)))
    {
        s[l++] = CSI;
        s[l++] = K_SECOND(c);
        s[l++] = K_THIRD(c);
    }
    else
    {
        s[l++] = (char_u) c;
    }

    add_to_input_buf(s, l);
    return TRUE;
}

//------------------------------------------------------------------------------
// VimConHandleEvent - Top-level IDCMP event handler
// Input:              IDCMP event
// Return:             See MUI docs
//------------------------------------------------------------------------------
MUIDSP IPTR VimConHandleEvent(Class *cls, Object *obj,
                              struct MUIP_HandleEvent *msg)
{
    struct VimConData *my = INST_DATA(cls,obj);

    if(likely(msg->imsg->Class == IDCMP_RAWKEY))
    {
        if(VimConHandleRaw(cls, obj, msg))
        {
            my->state |= MUIV_VimCon_State_Yield;
        }
    }
    else
    if(msg->imsg->Class == IDCMP_MOUSEBUTTONS)
    {
        if(VimConMouseHandleButton(cls, obj, msg))
        {
            my->state |= MUIV_VimCon_State_Yield;
        }
    }
    else
    if(msg->imsg->Class == IDCMP_MOUSEMOVE ||
       msg->imsg->Class == IDCMP_INTUITICKS)
    {
        if(VimConMouseMove(cls, obj, msg))
        {
            my->state |= MUIV_VimCon_State_Yield;
        }
    }
    else
#ifdef __amigaos4__
    if(msg->imsg->Class == IDCMP_EXTENDEDMOUSE &&
       msg->imsg->Code == IMSGCODE_INTUIWHEELDATA)
    {
        struct IntuiWheelData *iwd = (struct IntuiWheelData *)
               msg->imsg->IAddress;
        msg->imsg->Code = iwd->WheelY < 0 ? RAWKEY_NM_WHEEL_UP :
                          iwd->WheelY > 0 ? RAWKEY_NM_WHEEL_DOWN :
                          iwd->WheelX < 0 ? RAWKEY_NM_WHEEL_LEFT :
                          iwd->WheelX > 0 ? RAWKEY_NM_WHEEL_RIGHT : 0;

        if(VimConHandleRaw(cls, obj, msg))
        {
            my->state |= MUIV_VimCon_State_Yield;
        }
    }
#endif

    // Leave unhandeled events to our parent class
    return my->state & MUIV_VimCon_State_Yield ? MUI_EventHandlerRC_Eat :
           DoSuperMethodA(cls, obj, (Msg) msg);
}

//------------------------------------------------------------------------------
// VimConMinMax - Overloading MUIM_AskMinMax
// Input:         See MUI docs
// Return:        See MUI docs
//------------------------------------------------------------------------------
MUIDSP IPTR VimConMinMax(Class *cls, Object *obj, struct MUIP_AskMinMax *msg)
{
    IPTR r = (IPTR) DoSuperMethodA(cls, obj, (Msg) msg);
    struct VimConData *my = INST_DATA(cls,obj);

    if(unlikely(!my->bm))
    {
        kmsg(_(e_null));
        return r;
    }

    // Maybe something less ad hoc?
    msg->MinMaxInfo->MaxWidth = GetBitMapAttr(my->bm, BMA_WIDTH);
    msg->MinMaxInfo->MaxHeight = GetBitMapAttr(my->bm, BMA_HEIGHT);
    msg->MinMaxInfo->MinWidth += msg->MinMaxInfo->MaxWidth >> 3L;
    msg->MinMaxInfo->MinHeight += msg->MinMaxInfo->MaxHeight >> 3L;
    msg->MinMaxInfo->DefWidth += (msg->MinMaxInfo->MaxWidth >> 1L) +
                                  msg->MinMaxInfo->MinWidth;
    msg->MinMaxInfo->DefHeight += (msg->MinMaxInfo->MaxHeight >> 1L) +
                                   msg->MinMaxInfo->MinHeight;
    return r;
}

//------------------------------------------------------------------------------
// VimConDraw - Overloading MUIM_Draw
// Input:       See MUI docs
// Return:      See MUI docs
//------------------------------------------------------------------------------
MUIDSP IPTR VimConDraw(Class *cls, Object *obj, struct MUIP_Draw *msg)
{
    IPTR r = (IPTR) DoSuperMethodA(cls, obj, (Msg) msg);
    struct VimConData *my = INST_DATA(cls,obj);

    // Update dirty parts.
    if(likely(msg->flags & MADF_DRAWUPDATE))
    {
        if(unlikely(my->xd1 == INT_MAX))
        {
            // No dirt.
            return r;
        }

        // We have atleast one pixel of dirt.
        ClipBlit(&my->rp, my->xd1, my->yd1, _rp(obj), my->left + my->xd1,
                 my->top + my->yd1, my->xd2 - my->xd1 + 1, my->yd2 -
                 my->yd1 + 1, 0xc0);

        // We're clean.
        VimConClean(my);
    }
    // Update everything.
    else
    if(unlikely(msg->flags & MADF_DRAWOBJECT))
    {
        if(likely(my->width < _mwidth(obj)))
        {
            // We're wider. Clear new bitmap area before showing it.
            FillPixelArray(&my->rp, my->width, 0,  _mwidth(obj) - my->width,
                           _mheight(obj), gui.back_pixel);
        }

        if(likely(my->height < _mheight(obj)))
        {
            // We're taller. Clear new bitmap area before showing it.
            FillPixelArray(&my->rp, 0, my->height, _mwidth(obj), _mheight(obj) -
                           my->height, gui.back_pixel);
        }

        // Save sizes for use outside MUIM_Draw.
        my->bottom = _mbottom(obj);
        my->height = _mheight(obj);
        my->width = _mwidth(obj);
        my->right = _mright(obj);
        my->left = _mleft(obj);
        my->top = _mtop(obj);

        // Blit off screen bitmap to window rastport.
        ClipBlit(&my->rp, 0, 0, _rp(obj),  my->left, my->top, my->width,
                 my->height, 0xc0);

        // We're clean.
        VimConClean(my);
    }

    return r;
}

//------------------------------------------------------------------------------
// VimConCallback - Vim menu / button type callback method
// Input:           VimMenuPtr - Vim menuitem pointer
// Return:          TRUE on successful invocation of callback, FALSE otherwise
//------------------------------------------------------------------------------
METHOD(VimCon, Callback, VimMenuPtr)
{
    vimmenu_T *mp = (vimmenu_T *) msg->VimMenuPtr;

    if(unlikely(!mp || !mp->cb))
    {
        return FALSE;
    }

    // Treat menu / buttons like keyboard input
    my->state |= MUIV_VimCon_State_Yield;
    mp->cb(mp);
    return TRUE;
}

//------------------------------------------------------------------------------
// VimConYield - FIXME
// Input:        -
// Return:       TRUE
//------------------------------------------------------------------------------
METHOD0(VimCon, Yield)
{
    my->state |= MUIV_VimCon_State_Yield;
    return TRUE;
}

//------------------------------------------------------------------------------
// VimConGetState - Why did we wake up?
// Input:           -
// Return:          MUIV_VimCon_GetState_(Idle|Input|Timeout|Unknown)
//------------------------------------------------------------------------------
METHOD0(VimCon, GetState)
{
    // Can't idle and X at the same time
    if(likely(my->state == MUIV_VimCon_State_Idle))
    {
        return MUIV_VimCon_State_Idle;
    }

    // Reset terminal and let Vim know our size.
    if(unlikely(my->state & MUIV_VimCon_State_Reset))
    {
        my->state = MUIV_VimCon_State_Idle;
        gui_resize_shell(my->width, my->height);
        add_to_input_buf("\f", 1);
        return MUIV_VimCon_State_Yield;
    }

    // Yields take precendence over timeouts
    if(unlikely(my->state & MUIV_VimCon_State_Yield))
    {
        my->state = MUIV_VimCon_State_Idle;
        return MUIV_VimCon_State_Yield;
    }

#ifdef MUIVIM_FEAT_TIMEOUT
    // Timeout takes precedence over nothing
    if(unlikely(my->state & MUIV_VimCon_State_Timeout))
    {
        my->state = MUIV_VimCon_State_Idle;
        return MUIV_VimCon_State_Timeout;
    }
#endif

    kmsg(_(e_internal));
    return MUIV_VimCon_State_Unknown;
}

//------------------------------------------------------------------------------
// VimConShow - Overloading MUIM_Show
// Input:       See MUI docs
// Return:      See MUI docs
//------------------------------------------------------------------------------
MUIDSP IPTR VimConShow(Class *cls, Object *obj, Msg msg)
{
    IPTR r = (IPTR) DoSuperMethodA(cls, obj, msg);
    struct VimConData *my = INST_DATA(cls,obj);

    // Let Vim know the console size.
    my->state |= MUIV_VimCon_State_Reset;
    return r;
}

//------------------------------------------------------------------------------
// VimConBeep - Visual bell
// Input:       -
// Return:      TRUE
//------------------------------------------------------------------------------
METHOD0(VimCon, Beep)
{
    InvertPixelArray(&my->rp, 0, 0, my->width, my->height);
    MUI_Redraw(me, MADF_DRAWOBJECT);

    // Postpone terminal reset.
    my->state |= MUIV_VimCon_State_Reset;
    return TRUE;
}

//------------------------------------------------------------------------------
// VimConCopy - Copy data from Vim clipboard to clipboard.device
// Input:       Clipboard
// Return:      0
//------------------------------------------------------------------------------
METHOD(VimCon, Copy, Clipboard)
{
    Clipboard_T *cbd = (Clipboard_T *) msg->Clipboard;

    if(unlikely(!cbd->owned))
    {
        return 0;
    }

    clip_get_selection(cbd);
    cbd->owned = FALSE;

    long_u size;
    char_u *data;
    int type = clip_convert_selection(&data, &size, cbd);

    if(unlikely(type == -1))
    {
        // Do nothing if conversion fails
        vim_free(data);
        return 0;
    }

    struct IFFHandle *iffh = AllocIFF();

    if(unlikely(!iffh))
    {
        kmsg(_(e_outofmem));
        vim_free(data);
        return 0;
    }

    iffh->iff_Stream = (IPTR) OpenClipboard(PRIMARY_CLIP);

    if(likely(iffh->iff_Stream))
    {
        LONG ftxt = MAKE_ID('F','T','X','T'),
             chrs = MAKE_ID('C','H','R','S');

        InitIFFasClip(iffh);

        if(likely(!OpenIFF(iffh, IFFF_WRITE)))
        {
            if(likely(!PushChunk(iffh, ftxt, ID_FORM, IFFSIZE_UNKNOWN)))
            {
                if(likely(!PushChunk(iffh, 0, chrs, IFFSIZE_UNKNOWN)))
                {
                    WriteChunkBytes(iffh, data, size);
                    PopChunk(iffh);
                }

                PopChunk(iffh);
            }

            CloseIFF(iffh);
        }

        CloseClipboard((struct ClipboardHandle *) iffh->iff_Stream);
    }

    FreeIFF(iffh);
    vim_free(data);
    return 0;
}

//------------------------------------------------------------------------------
// VimConIconState - Listener for iconification events
// Input:            Iconified - Application state
// Return:           Input pass through
//------------------------------------------------------------------------------
METHOD(VimCon, IconState, Iconified)
{
    my->block = msg->Iconified;
    return msg->Iconified;
}

//------------------------------------------------------------------------------
// VimConPaste - Paste data from clipboard.device
// Input:        Clipboard
// Return:       0
//------------------------------------------------------------------------------
METHOD(VimCon, Paste, Clipboard)
{
    Clipboard_T *cbd = (Clipboard_T *) msg->Clipboard;
    struct IFFHandle *iffh = AllocIFF();

    if(unlikely(!iffh))
    {
        kmsg(_(e_outofmem));
        return 0;
    }

    iffh->iff_Stream = (IPTR) OpenClipboard(PRIMARY_CLIP);

    if(unlikely(!iffh->iff_Stream))
    {
        FreeIFF(iffh);
        return 0;
    }

    LONG ftxt = MAKE_ID('F','T','X','T'), chrs = MAKE_ID('C','H','R','S'),
         cset = MAKE_ID('C','S','E','T');

    InitIFFasClip(iffh);

    // Open and set stop points
    if(likely(!OpenIFF(iffh,IFFF_READ) && !StopChunk(iffh, ftxt, chrs) &&
       !StopChunk(iffh, ftxt, cset)))
    {
        LONG stat;

        for(stat = IFFERR_EOC; stat == IFFERR_EOC; )
        {
            for(stat = ParseIFF(iffh, IFFPARSE_SCAN); !stat;
                stat = ParseIFF(iffh, IFFPARSE_SCAN))
            {
                struct ContextNode *c = CurrentChunk(iffh);

                if(unlikely(!c || c->cn_Type != ftxt || c->cn_ID != chrs))
                {
                    continue;
                }

                // Start with a 1k buffer
                LONG size = 1 << 10;
                char_u *data = calloc(size, sizeof(char_u));

                if(unlikely(!data))
                {
                    kmsg(_(e_outofmem));
                    break;
                }

                LONG read = ReadChunkBytes(iffh, data, size);

                // If 1k isn't enough, ROL size until it is
                while(read == size)
                {
                    char_u *next = calloc(size << 1, sizeof(char_u));

                    if(unlikely(!next))
                    {
                        kmsg(_(e_outofmem));
                        stat = IFFERR_NOMEM;
                        break;
                    }

                    // Read some more and do some stitching
                    read += ReadChunkBytes(iffh, next + size, size);
                    memcpy(next, data, size);
                    free(data);
                    data = next;
                    size <<= 1;
                }

                // Yank unless we ran out of memory
                if(likely(!stat))
                {
                    clip_yank_selection(MCHAR, data, read, cbd);
                }

                free(data);
            }
        }

        CloseIFF(iffh);
    }

    CloseClipboard((struct ClipboardHandle *) iffh->iff_Stream);
    FreeIFF(iffh);
    return 0;
}


//------------------------------------------------------------------------------
// VimConDispatch - MUI custom class dispatcher
// Input:           See dispatched method
// Return:          See dispatched method
//------------------------------------------------------------------------------
DISPATCH(VimCon)
{
    DISPATCH_HEAD;
    switch(msg->MethodID)
    {
        //----------------------------------------------------------------------
        // BOOPSI
        //----------------------------------------------------------------------
        case OM_NEW: return VimConNew(cls, obj, (struct opSet *) msg);
        case OM_DISPOSE: return VimConDispose(cls, obj, msg);
        //----------------------------------------------------------------------
        // MUI
        //----------------------------------------------------------------------
        case MUIM_AskMinMax:
            return VimConMinMax(cls, obj, (struct MUIP_AskMinMax *) msg);
        case MUIM_Cleanup: return VimConCleanup(cls, obj, msg);
        case MUIM_Draw: return VimConDraw(cls, obj, (struct MUIP_Draw *) msg);
        case MUIM_HandleEvent:
            return VimConHandleEvent(cls, obj, (struct MUIP_HandleEvent *) msg);
        case MUIM_Setup:
            return VimConSetup(cls, obj, (struct MUI_RenderInfo *) msg);
        case MUIM_Show: return VimConShow(cls, obj, msg);
        //----------------------------------------------------------------------
        // Custom
        //----------------------------------------------------------------------
        case M_ID(VimCon, AboutMUI): return M_FN0(VimCon, AboutMUI);
        case M_ID(VimCon, AppMessage): return M_FN(VimCon, AppMessage);
        case M_ID(VimCon, Beep): return M_FN0(VimCon, Beep);
        case M_ID(VimCon, Browse): return M_FN(VimCon, Browse);
        case M_ID(VimCon, Callback): return M_FN(VimCon, Callback);
        case M_ID(VimCon, Copy): return M_FN(VimCon, Copy);
        case M_ID(VimCon, Clear): return M_FN0(VimCon, Clear);
        case M_ID(VimCon, DeleteLines): return M_FN(VimCon, DeleteLines);
        case M_ID(VimCon, DrawHollowCursor):
            return M_FN(VimCon, DrawHollowCursor);
        case M_ID(VimCon, DrawPartCursor): return M_FN(VimCon, DrawPartCursor);
        case M_ID(VimCon, DrawString): return M_FN(VimCon, DrawString);
        case M_ID(VimCon, FillBlock): return M_FN(VimCon, FillBlock);
        case M_ID(VimCon, GetScreenDim): return M_FN(VimCon, GetScreenDim);
        case M_ID(VimCon, Yield): return M_FN0(VimCon, Yield);
        case M_ID(VimCon, GetState): return M_FN0(VimCon, GetState);
        case M_ID(VimCon, IconState): return M_FN(VimCon, IconState);
        case M_ID(VimCon, InvertRect): return M_FN(VimCon, InvertRect);
        case M_ID(VimCon, IsBlinking): return M_FN0(VimCon, IsBlinking);
        case M_ID(VimCon, MUISettings): return M_FN0(VimCon, MUISettings);
        case M_ID(VimCon, Paste): return M_FN(VimCon, Paste);
        case M_ID(VimCon, SetFgColor): return M_FN(VimCon, SetFgColor);
        case M_ID(VimCon, SetBgColor): return M_FN(VimCon, SetBgColor);
        case M_ID(VimCon, SetBlinking): return M_FN(VimCon, SetBlinking);
#ifdef MUIVIM_FEAT_TIMEOUT
        case M_ID(VimCon, SetTimeout): return M_FN(VimCon, SetTimeout);
#endif
        case M_ID(VimCon, SetTitle): return M_FN(VimCon, SetTitle);
        case M_ID(VimCon, StartBlink): return M_FN0(VimCon, StartBlink);
        case M_ID(VimCon, StopBlink): return M_FN0(VimCon, StopBlink);
        case M_ID(VimCon, Ticker): return M_FN0(VimCon, Ticker);
#ifdef MUIVIM_FEAT_TIMEOUT
        case M_ID(VimCon, Timeout): return M_FN0(VimCon, Timeout);
#endif
        //----------------------------------------------------------------------
        // Fallthrough
        //----------------------------------------------------------------------
        default: return DoSuperMethodA(cls, obj, msg);
    }
}
DISPATCH_END

//------------------------------------------------------------------------------
// VimToolbar - MUI custom class handling the toolbar. Currently this class is
//              rather primitve, pretty much everything is hardcoded and slow,
//              but it does the job for now.
//------------------------------------------------------------------------------
CLASS_DEF(VimToolbar)
{
    struct MUIS_TheBar_Button *btn;
};

//------------------------------------------------------------------------------
// VimToolbarAddButton - Add button to toolbar
// Input:                ID - Vim menu item pointer
//                       Label - Button text (not shown)
//                       Help - Help text shown when hovering over button
// Return:               TRUE on success, FALSE otherwise
//------------------------------------------------------------------------------
METHOD(VimToolbar, AddButton, ID, Label, Help)
{
    struct MUIS_TheBar_Button *b = my->btn;

    // Traverse our static toolbar and set up a notification if we find a match.
    while(b->img != (IPTR) MUIV_TheBar_End)
    {
        if(unlikely(b->help && !strcmp((const char *) msg->Label, b->help)))
        {
            DoMethod(me, MUIM_TheBar_Notify, (IPTR) b->ID, MUIA_Pressed, FALSE,
                     Con, 2, M_ID(VimCon, Callback), (IPTR) msg->ID);

            // Save the Vim menu item pointer as the parent class of the button.
            // Used to translate from menu item to MUI button ID.
            b->_class = (struct IClass *) msg->ID;
            return TRUE;
        }

        b++;
    }

    // Could not find a match.
    kmsg(_(e_internal));
    return FALSE;
}

//------------------------------------------------------------------------------
// VimToolbarDisableButton - Disable button
// Input:                    ID - Vim menu item pointer
//                           Grey - TRUE to disable item, FALSE to enable
// Return:                   TRUE on success, FALSE otherwise
//------------------------------------------------------------------------------
METHOD(VimToolbar, DisableButton, ID, Grey)
{
    struct MUIS_TheBar_Button *b = my->btn;

    // Traverse our static toolbar and look for a Vim menu item match. If we
    // find one, we can translate this into a MUI ID and use the TheBar method.
    while(b->img != MUIV_TheBar_End)
    {
        if(unlikely(msg->ID == (IPTR) b->_class))
        {
            // We found the MUI ID. Use the proper TheBar method to disable /
            // enable the button.
            DoMethod(me, MUIM_TheBar_SetAttr, b->ID, MUIV_TheBar_Attr_Disabled,
                     msg->Grey);
            return TRUE;
        }

        b++;
    }

    // This is expected since we ignore some of the menu items.
    return FALSE;
}

//------------------------------------------------------------------------------
// VimMessage - Show message
// Input:       char *title - Window title
//              char *msg   - Message
//              char *fmt   - Gadget format
// Return:      -
//------------------------------------------------------------------------------
static void VimMessage(const char *title, const char *msg, const char *fmt)
{
#ifdef __amigaos4__
    struct TagItem tags[] = {{ .ti_Tag = ESA_Position,
                               .ti_Data = REQPOS_CENTERSCREEN },
                             { .ti_Tag = TAG_DONE }};
#endif
    struct EasyStruct req =
    {
        .es_StructSize   = sizeof(struct EasyStruct),
        .es_Flags        = 0,
        .es_Title        = (UBYTE *) title,
        .es_TextFormat   = (UBYTE *) msg,
        .es_GadgetFormat = (UBYTE *) fmt,
#ifdef __amigaos4__
        .es_Screen       = ((struct IntuitionBase *)IntuitionBase)->ActiveScreen,
        .es_TagList      = tags
#endif
    };

    EasyRequest(NULL, &req, NULL, NULL);
}

//------------------------------------------------------------------------------
// VimToolbarNew - Overloading OM_NEW
// Input:          See BOOPSI docs
// Return:         See BOOPSI docs
//------------------------------------------------------------------------------
MUIDSP IPTR VimToolbarNew(Class *cls, Object *obj, struct opSet *msg)
{
    static struct MUIS_TheBar_Button b[] =
    {
        { .ID = 1, .img = 0, .help = "Open"        },
        { .ID = 2, .img = 1, .help = "Save"        },
        { .ID = 3, .img = 2, .help = "SaveAll"     },
        { .img = MUIV_TheBar_BarSpacer             },
        { .ID = 5, .img = 3, .help = "Undo"        },
        { .ID = 6, .img = 4, .help = "Redo"        },
        { .img = MUIV_TheBar_BarSpacer             },
        { .ID = 8, .img = 5, .help = "Cut"         },
        { .ID = 9, .img = 6, .help = "Copy"        },
        { .ID = 10, .img = 7, .help = "Paste"      },
        { .img = MUIV_TheBar_BarSpacer             },
        { .ID = 12, .img = 8, .help = "Replace"    },
        { .ID = 13, .img = 9,  .help = "FindNext"  },
        { .ID = 14, .img = 10, .help = "FindPrev"  },
        { .img = MUIV_TheBar_BarSpacer             },
        { .ID = 16, .img = 11, .help = "LoadSesn"  },
        { .ID = 17, .img = 12, .help = "SaveSesn"  },
        { .ID = 18, .img = 13, .help = "RunScript" },
        { .img = MUIV_TheBar_BarSpacer             },
        { .ID = 20, .img = 14, .help = "Make"      },
        { .ID = 21, .img = 15, .help = "RunCtags"  },
        { .ID = 22, .img = 16, .help = "TagJump"   },
        { .img = MUIV_TheBar_BarSpacer             },
        { .ID = 24, .img = 17, .help = "Help"      },
        { .ID = 25, .img = 18, .help = "FindHelp"  },
        { .img = MUIV_TheBar_End                   },
    };

    // User settings are ignored to achieve something which looks like the
    // toolbar on other platforms, refer to gui_mch_init() for details.
    struct TagItem tags[] =
    {
        { .ti_Tag = MUIA_Group_Horiz, .ti_Data = TRUE },
        { .ti_Tag = MUIA_TheBar_Buttons, .ti_Data = (IPTR) b },
        { .ti_Tag = MUIA_TheBar_IgnoreAppearance, .ti_Data = TRUE },
        { .ti_Tag = MUIA_TheBar_Borderless, .ti_Data = TRUE },
        { .ti_Tag = MUIA_TheBar_ViewMode, .ti_Data = MUIV_TheBar_ViewMode_Gfx },
        { .ti_Tag = MUIA_TheBar_PicsDrawer, .ti_Data = (IPTR) "VIM:icons" },
        { .ti_Tag = MUIA_TheBar_Strip, .ti_Data = (IPTR) "tb_strip.png" },
        { .ti_Tag = MUIA_TheBar_DisStrip, .ti_Data = (IPTR)"tb_dis_strip.png" },
        { .ti_Tag = MUIA_TheBar_SelStrip, .ti_Data = (IPTR)"tb_sel_strip.png" },
        { .ti_Tag = MUIA_TheBar_StripCols, .ti_Data = 19 },
        { .ti_Tag = MUIA_TheBar_StripRows, .ti_Data = TRUE },
        { .ti_Tag = MUIA_TheBar_StripHSpace, .ti_Data = FALSE },
        { .ti_Tag = MUIA_TheBar_StripVSpace, .ti_Data = FALSE },
        { .ti_Tag = TAG_MORE, .ti_Data = (IPTR) msg->ops_AttrList}
    };

    obj = (Object *) DoSuperMethod(cls, obj, OM_NEW, tags, NULL);

    if(likely(obj))
    {
        struct VimToolbarData *my = INST_DATA(cls,obj);
        my->btn = b;
    }

    return (IPTR) obj;
}

//------------------------------------------------------------------------------
// VimToolbarDispatch - MUI custom class dispatcher
// Input:               See dispatched method
// Return:              See dispatched method
//------------------------------------------------------------------------------
DISPATCH(VimToolbar)
{
    DISPATCH_HEAD;
    switch(msg->MethodID)
    {
        //----------------------------------------------------------------------
        // BOOPSI
        //----------------------------------------------------------------------
        case OM_NEW: return VimToolbarNew(cls, obj, (struct opSet *) msg);
        //----------------------------------------------------------------------
        // Custom
        //----------------------------------------------------------------------
        case M_ID(VimToolbar, AddButton): return M_FN(VimToolbar, AddButton);
        case M_ID(VimToolbar, DisableButton):
             return M_FN(VimToolbar, DisableButton);
        //----------------------------------------------------------------------
        // Fallthrough
        //----------------------------------------------------------------------
        default: return DoSuperMethodA(cls, obj, msg);
    }
}
DISPATCH_END

//------------------------------------------------------------------------------
// VimMenu - MUI custom class handling the menu. Currently it's not possible
//           to hide the menu. This must be fixed in order to support all
//           gui options in Vim. This class does not handle the fact that Vim
//           treats menu items and buttons in the same way, there's glue for
//           that in gui_mch_add_menu and gui_mch_add_menu_item.
//------------------------------------------------------------------------------
CLASS_DEF(VimMenu)
{
    int state;
};

//------------------------------------------------------------------------------
// VimMenu constants
//------------------------------------------------------------------------------
#define MUIV_VimMenu_AddMenu_AlwaysLast 303

//------------------------------------------------------------------------------
// VimMenuGrey - Enable/disable menu item
// Input:        ID - Menu item ID
//               Grey - TRUE to disable item, FALSE to enable
// Return:       TRUE on success, FALSE otherwise
//------------------------------------------------------------------------------
METHOD(VimMenu, Grey, ID, Grey)
{
    // ID:s must be valid vim menu pointers
    if(unlikely(!msg || !msg->ID))
    {
        return FALSE;
    }

    vimmenu_T *menu = (vimmenu_T *) msg->ID;

    // Ignore popup menus and separators
    if(unlikely(menu_is_popup(menu->name) || menu_is_separator(menu->name) ||
      (menu->parent && menu_is_popup(menu->parent->name))))
    {
        return FALSE;
    }

    // Some of our menu items are in fact toolbar buttons
    if(unlikely(menu_is_toolbar(menu->name) ||
      (menu->parent && menu_is_toolbar(menu->parent->name))))
    {
        DoMethod(Tlb, MUIM_VimToolbar_DisableButton, menu, msg->Grey);
        return TRUE;
    }

    // Vim menu pointers are used as MUI user data / ID:s
    Object *m = (Object *) DoMethod(me, MUIM_FindUData, msg->ID);

    if(unlikely(!m))
    {
        return FALSE;
    }

    IPTR currentSetting;
    GetAttr(MUIA_Menuitem_Enabled, m, &currentSetting);

    // Zune handles menu item updates very inefficiently, so only update if
    // value is changed. Please note that msg->Grey is true if we're going to
    // disable the menuitem, and that currentSetting is true if the item is
    // enabled, therefore update when msg->Grey == currentSetting.
    if(currentSetting == msg->Grey)
    {
        set(m, MUIA_Menuitem_Enabled, (BOOL) msg->Grey ? FALSE : TRUE);
    }

    return TRUE;
}

//------------------------------------------------------------------------------
// VimMenuRemoveMenu - Remove menu / menu item
// Input:              ID - Menu item ID
// Return:             TRUE on success, FALSE otherwise
//------------------------------------------------------------------------------
METHOD(VimMenu, RemoveMenu, ID)
{
    // ID:s must be valid vim menu pointers
    if(unlikely(!msg || !msg->ID))
    {
        return FALSE;
    }

    // Vim menu pointers are used as MUI user data / ID:s
    Object *m = (Object *) DoMethod(me, MUIM_FindUData, msg->ID);

    if(unlikely(!m))
    {
        return FALSE;
    }

    // FIXME: Are we leaking m?
    DoMethod(me, MUIM_Family_Remove, m);
    return TRUE;
}

//------------------------------------------------------------------------------
// VimMenuAddSpacer - Add menu spacer
// Input:             ParentID - ID of parent menu
// Return:            The created spacer on success, NULL otherwise
//------------------------------------------------------------------------------
METHOD(VimMenu, AddSpacer, ParentID)
{
    // All spacers have parents (they can't be top level menus)
    Object *m = (Object *) DoMethod(me, MUIM_FindUData, msg->ParentID);

    if(unlikely(!m))
    {
        return (IPTR) NULL;
    }

    // No MUI user data needed, spacers have no callback
    Object *i = MUI_NewObject(MUIC_Menuitem, MUIA_Menuitem_Title, NM_BARLABEL,
                              TAG_END);
    if(unlikely(!i))
    {
        kmsg(_(e_outofmem));
        return (IPTR) NULL;
    }

    // Add spacer to parent menu
    DoMethod(m, MUIM_Family_AddTail, i);
    return (IPTR) i;
}

//------------------------------------------------------------------------------
// VimMenuAddMenu - Add menu to menu strip
// Input:           ParentID - ID of parent. Either a menu strip or a menu
//                  ID - ID of menu to add
//                  Label - Text label of menu
// Return:          The created menu on success, NULL otherwise
//------------------------------------------------------------------------------
METHOD(VimMenu, AddMenu, ParentID, ID, Label)
{
    // Sub menus have their own parents. Top level menus belong to us.
    Object *m = msg->ParentID ? (Object *) DoMethod(me, MUIM_FindUData,
                                                    msg->ParentID) : me;
    if(unlikely(!m))
    {
        // Parent not found.
        return (IPTR) NULL;
    }

    // Vim menu type pointers used as MUI user data
    Object *i = MUI_NewObject(MUIC_Menu, MUIA_Menu_Title, msg->Label,
                              MUIA_UserData, msg->ID, TAG_END);
    if(unlikely(!i))
    {
        kmsg(_(e_outofmem));
        return (IPTR) NULL;
    }

    // Add menu to menu strip.
    DoMethod(m, MUIM_Family_AddTail, i);

    // Make sure that the AlwaysLast menu really is last.
    Object *l = (Object *) DoMethod(me, MUIM_FindUData,
                                    MUIV_VimMenu_AddMenu_AlwaysLast);
    if(likely(l))
    {
        DoMethod(me, MUIM_Family_Remove, l);
        DoMethod(me, MUIM_Family_AddTail, l);
    }

    return (IPTR) i;
}

//------------------------------------------------------------------------------
// VimMenuAddMenuItem - Add menu item to menu
// Input:               ParentID - ID of menu. Always a menu, never a strip
//                      ID - ID of menu item to add
//                      Label - Text label of item
// Return:              The created item on success, NULL otherwise
//------------------------------------------------------------------------------
METHOD(VimMenu, AddMenuItem, ParentID, ID, Label)
{
    // Menu items must have a parent menu
    Object *m = (Object *) DoMethod(me, MUIM_FindUData, msg->ParentID);

    if(unlikely(!m))
    {
        // Parent not found.
        return (IPTR) NULL;
    }

    // Vim menu type pointers used as MUI user data
    Object *i = MUI_NewObject(MUIC_Menuitem, MUIA_Menuitem_Title, msg->Label,
                              MUIA_UserData, msg->ID, TAG_END);
    if(unlikely(!i))
    {
        kmsg(_(e_outofmem));
        return (IPTR) NULL;
    }

    // Add item to menu and set callback
    DoMethod(m, MUIM_Family_AddTail, i);
    DoMethod(i, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime, Con, 2,
             M_ID(VimCon, Callback), msg->ID);

    return (IPTR) i;
}

//------------------------------------------------------------------------------
// VimMenuDispatch - MUI custom class dispatcher
// Input:            See dispatched method
// Return:           See dispatched method
//------------------------------------------------------------------------------
DISPATCH(VimMenu)
{
    DISPATCH_HEAD;
    switch(msg->MethodID)
    {
        //----------------------------------------------------------------------
        // Custom
        //----------------------------------------------------------------------
        case M_ID(VimMenu, AddSpacer): return M_FN(VimMenu, AddSpacer);
        case M_ID(VimMenu, AddMenu): return M_FN(VimMenu, AddMenu);
        case M_ID(VimMenu, AddMenuItem): return M_FN(VimMenu, AddMenuItem);
        case M_ID(VimMenu, RemoveMenu): return M_FN(VimMenu, RemoveMenu);
        case M_ID(VimMenu, Grey): return M_FN(VimMenu, Grey);
        //----------------------------------------------------------------------
        // Fallthrough
        //----------------------------------------------------------------------
        default: return DoSuperMethodA(cls, obj, msg);
    }
}
DISPATCH_END

#ifdef MUIVIM_FEAT_SCROLLBAR
//------------------------------------------------------------------------------
// VimScrollbar - MUI custom class handling Vim scrollbars.
//------------------------------------------------------------------------------
CLASS_DEF(VimScrollbar)
{
    scrollbar_T *sb;
    IPTR top, visible, weight;
    Object *grp;
};
#define MUIA_VimScrollbar_Sb 808
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// VimScrollbarTop - Get top of scrollbar
// Input:            -
// Return:           -
//------------------------------------------------------------------------------
METHOD0(VimScrollbar, Top)
{
    return my->top;
}

//------------------------------------------------------------------------------
// VimScrollbarTop - Get scrollbar visibility
// Input:            -
// Return:           -
//------------------------------------------------------------------------------
METHOD0(VimScrollbar, Visible)
{
    return my->visible;
}

//------------------------------------------------------------------------------
// VimScrollbarDrag - Drag scrollbar
// Input:             Value - Top line
// Return:            TRUE
//------------------------------------------------------------------------------
METHOD(VimScrollbar, Drag, Value)
{
    if(unlikely(!my->sb))
    {
        return FALSE;
    }

	gui_drag_scrollbar(my->sb, (int) msg->Value, FALSE);
    (void) DoMethod(Con, M_ID(VimCon, Yield));
    return TRUE;
}

//------------------------------------------------------------------------------
// VimScrollbarShow - Show / hide scrollbar.
// Input:             Show - Show or hide scrollbar.
// Return:            TRUE on state change, FALSE otherwise.
//------------------------------------------------------------------------------
METHOD(VimScrollbar, Show, Show)
{
    // Vim likes to update scrollbars even though nothing changed. Bail out if
    // nothing changed since the last invocation.
    if(likely(my->visible == msg->Show || !my->grp))
    {
        return FALSE;
    }

    // Save new state.
    my->visible = msg->Show;

    // The bottom scrollbar belongs to the same group as the console. Don't
    // do anything about that one. Just enable / disable the scrollbar.
    if(unlikely(my->sb->type == SBAR_BOTTOM))
    {
        set(me, MUIA_ShowMe, msg->Show);
        return TRUE;
    }

    if(msg->Show)
    {
        // Show scrollbar.
        set(me, MUIA_ShowMe, TRUE);

        // Show group unless it's already shown.
        IPTR shown;
        get(my->grp, MUIA_ShowMe, &shown);

        if(!shown)
        {
            set(my->grp, MUIA_ShowMe, TRUE);
        }

        return TRUE;
    }

    struct List *lst = NULL;
    get(my->grp, MUIA_Group_ChildList, &lst);

    IPTR enable = FALSE;
    struct Node *cur = lst->lh_Head;
    Object *chl;

    // The group shall be hidden if all scrollbars are hidden.
    for(chl = NextObject(&cur); chl && !enable; chl = NextObject(&cur))
    {
        enable = DoMethod(chl, M_ID(VimScrollbar, Visible));
    }

    if(!enable)
    {
        // Hide group.
        set(my->grp, MUIA_ShowMe, FALSE);
    }

    // Hide scrollbar.
    set(me, MUIA_ShowMe, FALSE);
    return TRUE;
}

//------------------------------------------------------------------------------
// VimScrollbarSortNeeded - Check if scrollbar group needs sorting
// Input:                   Object *grp - Scrollbar group
// Return:                  TRUE if group needs to be sorted, FALSE otherwise
//------------------------------------------------------------------------------
MUIDSP IPTR VimScrollbarSortNeeded(Object *grp)
{
    struct List *lst = NULL;
    get(grp, MUIA_Group_ChildList, &lst);
    struct Node *cur = lst->lh_Head;
    IPTR top = 0;
    Object *chl;

    for(chl = NextObject(&cur); chl; chl = NextObject(&cur))
    {
        IPTR stop = DoMethod(chl, M_ID(VimScrollbar, Top));

        // Ascending order.
        if(unlikely(top > stop))
        {
            return TRUE;
        }

        top = stop;
    }

    return FALSE;
}

//------------------------------------------------------------------------------
// VimScrollbarCount - Get number of objects in group
// Input:              Object *grp - Group of objects
// Return:             Number of objects in group
//------------------------------------------------------------------------------
MUIDSP size_t VimScrollbarCount(Object *grp)
{
    struct List *lst = NULL;
    get(grp, MUIA_Group_ChildList, &lst);

    struct Node *cur = lst->lh_Head;
    IPTR cnt = 0;
    Object *chl;

    for(chl = NextObject(&cur); chl; chl = NextObject(&cur))
    {
        cnt++;
    }

    return cnt;
}

//------------------------------------------------------------------------------
// VimScrollbarOrderAll - Add objects to group in order and remove duplicates
// Input:                 Object **obj - Array of ordered objects
//                        Object *grp - Group of objects
// Return:                -
//------------------------------------------------------------------------------
MUIDSP void VimScrollbarOrderAll(Object **obj, Object *grp)
{
    // Make sure that array and group order are the same.
    size_t cur;

    // Note that MUIM_Group_InitChange is already called.
    for(cur = 0; obj[cur]; cur++)
    {
        DoMethod(grp, OM_REMMEMBER, obj[cur]);
        DoMethod(grp, OM_ADDMEMBER, obj[cur]);
    }
}

//------------------------------------------------------------------------------
// VimScrollbarGroupCopy - Create a shallow copy of objects in a group
// Input:                  Object *grp - Group of objects
//                         size_t cnt - Number of objects to copy
// Return:                 Object * array if success full, NULL otherwisee
//------------------------------------------------------------------------------
MUIDSP Object **VimScrollbarGroupCopy(Object *grp, size_t cnt)
{
    // The array must be NULL terminated.
    Object **scs = calloc(cnt + 1, sizeof(scrollbar_T *));

    if(unlikely(!scs))
    {
        kmsg(_(e_outofmem));
        return NULL;
    }

    struct List *lst = NULL;
    get(grp, MUIA_Group_ChildList, &lst);

    struct Node *cur = lst->lh_Head;
    size_t ndx = 0;
    Object *chl;

    for(chl = NextObject(&cur); chl && ndx < cnt; chl = NextObject(&cur))
    {
        scs[ndx++] = chl;
    }

    return scs;
}

//------------------------------------------------------------------------------
// VimScrollbarSort - Sort scrollbars in group in ascending order depending on
//                    the top attribute
// Input:             Object *grp - Group of scrollbars
// Return:            -
//------------------------------------------------------------------------------
MUIDSP void VimScrollbarSort(Object *grp)
{
    // Get number of objects in group.
    size_t cnt = VimScrollbarCount(grp);

    if(unlikely(cnt < 2))
    {
        return;
    }

    // Create an array of object pointers.
    Object **scs = VimScrollbarGroupCopy(grp, cnt);

    if(unlikely(!scs))
    {
        kmsg(_(e_outofmem));
        return;
    }

    // Assume that the array needs sorting.
    BOOL flip = TRUE;

    while(flip)
    {
        // Assume that the array is sorted.
        flip = FALSE;
        size_t end;

        // Iterate over all object pairs.
        for(end = cnt - 1; end-- && !flip;)
        {
            // Get top value of scrollbars.
            IPTR alfa = DoMethod(scs[end], M_ID(VimScrollbar, Top)),
                 beta = DoMethod(scs[end + 1], M_ID(VimScrollbar, Top));

            if(unlikely(beta < alfa))
            {
                // Flip non sorted pairs.
                Object *tmp = scs[end];
                scs[end] = scs[end + 1];
                scs[end + 1] = tmp;

                // We changed something.
                flip = TRUE;
            }
        }
    }

    // Descending to ascending order.
    VimScrollbarOrderAll(scs, grp);

    // Free temporary array of object pointers.
    free(scs);
}

//------------------------------------------------------------------------------
// VimScrollbarPos - Update weight, position and order of scrollbars in group
// Input:            Top - Top of scrollbar
//                   Height - Height of scrollbar
// Return:           TRUE on success, FALSE otherwise
//------------------------------------------------------------------------------
METHOD(VimScrollbar, Pos, Top, Height)
{
    // Vim likes to update scrollbars even though nothing changed. Bail out if
    // nothing changed since the last invocation.
    if(likely((my->top == msg->Top && my->weight == msg->Height) ||
              !my->grp || !my->sb))
    {
        return FALSE;
    }

    // The bottom scrollbar needs no weight and no position.
    if(unlikely(my->sb->type == SBAR_BOTTOM))
    {
        return TRUE;
    }

    // Prepare to update weight and position.
    my->top = msg->Top;
    my->weight = msg->Height;
    DoMethod(my->grp, MUIM_Group_InitChange);

    // Test if the scrollbar is properly located.
    if(unlikely(VimScrollbarSortNeeded(my->grp)))
    {
        // It's not, sort parent group.
        VimScrollbarSort(my->grp);
    }

    // Give scrollbar the right proportions.
    set(me, MUIA_VertWeight, msg->Height);
    DoMethod(my->grp, MUIM_Group_ExitChange);
    return TRUE;
}

//------------------------------------------------------------------------------
// VimScrollbarInstall - Insert scrollbar in the appropriate group depending
//                       on type
// Input:                -
// Return:               TRUE on success, FALSE otherwise
//------------------------------------------------------------------------------
METHOD0(VimScrollbar, Install)
{
    if(unlikely(!my->sb))
    {
        return FALSE;
    }

    // Determine where to put the scrollbar.
    my->grp = my->sb->type == SBAR_LEFT ? Lsg :
             (my->sb->type == SBAR_RIGHT ? Rsg : Bsg);

    DoMethod(my->grp, MUIM_Group_InitChange);
    DoMethod(my->grp, OM_ADDMEMBER, me);
    DoMethod(my->grp, MUIM_Group_ExitChange);
    return TRUE;
}

//------------------------------------------------------------------------------
// VimScrollbarUninstall - Remove scrollbar from parent group
// Input:                  -
// Return:                 TRUE on success, FALSE otherwise
//------------------------------------------------------------------------------
METHOD0(VimScrollbar, Uninstall)
{
    if(unlikely(!my->sb || !my->grp))
    {
        return FALSE;
    }

    // Hide scrollbar before removing it.
    DoMethod(me, MUIM_VimScrollbar_Show, FALSE);
    DoMethod(my->grp, MUIM_Group_InitChange);
    DoMethod(my->grp, OM_REMMEMBER, me);
    DoMethod(my->grp, MUIM_Group_ExitChange);
    my->grp  = NULL;
    return TRUE;
}

//------------------------------------------------------------------------------
// VimScrollbarNew - Overloading OM_NEW
// Input:            See BOOPSI docs
// Return:           See BOOPSI docs
//------------------------------------------------------------------------------
MUIDSP IPTR VimScrollbarNew(Class *cls, Object *obj, struct opSet *msg)
{
    // Get Vim scrollbar reference.
    scrollbar_T *sb = (scrollbar_T *) GetTagData(MUIA_VimScrollbar_Sb, 0,
                      ((struct opSet *) msg)->ops_AttrList);

    struct TagItem tags[] =
    {
        { .ti_Tag = MUIA_ShowMe, .ti_Data = FALSE },
        { .ti_Tag = MUIA_Group_Horiz,
          .ti_Data = sb->type == SBAR_BOTTOM ? TRUE : FALSE },
        { .ti_Tag = TAG_MORE, .ti_Data = (IPTR) msg->ops_AttrList }
    };

    obj = (Object *) DoSuperMethod(cls, obj, OM_NEW, tags, NULL);

    if(unlikely(!obj))
    {
        kmsg(_(e_outofmem));
        return (IPTR) NULL;
    }

    struct VimScrollbarData *my = INST_DATA(cls,obj);

    my->sb = sb;
    sb->id = obj;
    my->visible = FALSE;
    my->weight = my->top = 0;

    // We notify ourselves when a dragging event occurs.
    DoMethod(obj, MUIM_Notify, MUIA_Prop_First, MUIV_EveryTime, (IPTR) obj, 2,
             MUIM_VimScrollbar_Drag, MUIV_TriggerValue);
    return (IPTR) obj;
}

//------------------------------------------------------------------------------
// VimScrollbarDispatch - MUI custom class dispatcher
// Input:                 See dispatched method
// Return:                See dispatched method
//------------------------------------------------------------------------------
DISPATCH(VimScrollbar)
{
    DISPATCH_HEAD;
    switch(msg->MethodID)
    {
        //----------------------------------------------------------------------
        // BOOPSI
        //----------------------------------------------------------------------
        case OM_NEW: return VimScrollbarNew(cls, obj, (struct opSet *) msg);
        //----------------------------------------------------------------------
        // Custom
        //----------------------------------------------------------------------
        case M_ID(VimScrollbar, Top): return M_FN0(VimScrollbar, Top);
        case M_ID(VimScrollbar, Visible): return M_FN0(VimScrollbar, Visible);
        case M_ID(VimScrollbar, Drag): return M_FN(VimScrollbar, Drag);
        case M_ID(VimScrollbar, Install): return M_FN0(VimScrollbar, Install);
        case M_ID(VimScrollbar, Uninstall): return M_FN0(VimScrollbar, Uninstall);
        case M_ID(VimScrollbar, Show): return M_FN(VimScrollbar, Show);
        case M_ID(VimScrollbar, Pos): return M_FN(VimScrollbar, Pos);
        //----------------------------------------------------------------------
        // Fallthrough
        //----------------------------------------------------------------------
        default:
            return DoSuperMethodA(cls, obj, msg);
    }
}
DISPATCH_END
#endif // MUIVIM_FEAT_SCROLLBAR

//------------------------------------------------------------------------------
// Vim interface - The functions below, all prefixed with (gui|clip)_mch, are
//                 the interface to Vim. Most of them contain almost no code,
//                 they merely act as a proxy between the rest of Vim and
//                 the MUI classes above. Some of them contain some glue code
//                 that I for some reason did not want in any of the classes
//                 above. Some of them do more, but those are the exceptions.
//                 Quite a few of them are currently empty, some of them will
//                 be implemented in the future and some of them don't make
//                 sense in combination with MUI and will therefore remain
//                 empty until the end of time.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// gui_mch_set_foreground -
//------------------------------------------------------------------------------
void gui_mch_set_foreground()
{
    (void) DoMethod(_win(Con), MUIM_Window_ToFront);
}

//------------------------------------------------------------------------------
// gui_mch_get_font - Not supported (let MUI handle this)
//------------------------------------------------------------------------------
GuiFont gui_mch_get_font(char_u *vim_font_name, int report_error)
{
    (void) vim_font_name;
    (void) report_error;
    return (GuiFont) NULL;
}

//------------------------------------------------------------------------------
// gui_mch_get_fontname - Not supported (let MUI handle this)
//------------------------------------------------------------------------------
char_u *gui_mch_get_fontname(GuiFont font, char_u *name)
{
    (void) font;
    (void) name;
    return NULL;
}

//------------------------------------------------------------------------------
// gui_mch_free_font - Not supported (let MUI handle this)
//------------------------------------------------------------------------------
void gui_mch_free_font(GuiFont font)
{
    (void) font;
}

//------------------------------------------------------------------------------
// gui_mch_get_winpos
//------------------------------------------------------------------------------
int gui_mch_get_winpos(int *x, int *y)
{
    if(likely(GetAttr(MUIA_Window_TopEdge, _win(Con), (IPTR *) x) &&
              GetAttr(MUIA_Window_LeftEdge, _win(Con), (IPTR *) y)))
    {
        return OK;
    }

    return FAIL;
}

//------------------------------------------------------------------------------
// gui_mch_set_winpos - Not supported
//------------------------------------------------------------------------------
void gui_mch_set_winpos(int x, int y)
{
    (void) x;
    (void) y;
}

//------------------------------------------------------------------------------
// gui_mch_enable_scrollbar
//------------------------------------------------------------------------------
void gui_mch_enable_scrollbar(scrollbar_T *sb, int flag)
{
#ifdef MUIVIM_FEAT_SCROLLBAR
    if(unlikely(!sb || !sb->id))
    {
        return;
    }

    (void) DoMethod(sb->id, M_ID(VimScrollbar, Show), flag ? TRUE : FALSE);
#else
    (void) sb;
    (void) flag;
#endif
}

//------------------------------------------------------------------------------
// gui_mch_create_scrollbar
//------------------------------------------------------------------------------
void gui_mch_create_scrollbar(scrollbar_T *sb, int orient)
{
#ifdef MUIVIM_FEAT_SCROLLBAR
    Object *obj = NewObject(VimScrollbarClass->mcc_Class, NULL,
                            MUIA_VimScrollbar_Sb, (IPTR) sb, TAG_END);
    if(unlikely(!obj))
    {
        kmsg(_(e_outofmem));
        return;
    }

    (void) DoMethod(obj, M_ID(VimScrollbar, Install));
#else
    (void) sb;
    (void) orient;
#endif
}

//------------------------------------------------------------------------------
// gui_mch_set_scrollbar_thumb
//------------------------------------------------------------------------------
void gui_mch_set_scrollbar_thumb(scrollbar_T *sb, int val, int size, int max)
{
#ifdef MUIVIM_FEAT_SCROLLBAR
    if(unlikely(!sb->id))
    {
        return;
    }

    SetAttrs(sb->id, MUIA_Prop_Entries, max, MUIA_Prop_Visible, size,
             MUIA_Prop_First, val, TAG_DONE);
#else
    (void) sb;
    (void) val;
    (void) size;
    (void) max;
#endif
}

//------------------------------------------------------------------------------
// gui_mch_set_scrollbar_pos
//------------------------------------------------------------------------------
void gui_mch_set_scrollbar_pos(scrollbar_T *sb, int x, int y, int w, int h)
{
#ifdef MUIVIM_FEAT_SCROLLBAR
    if(unlikely(!sb->id || !h))
    {
        return;
    }

    (void) DoMethod(sb->id, M_ID(VimScrollbar, Pos), (IPTR) y, (IPTR) h);
#else
    (void) sb;
    (void) x;
    (void) y;
    (void) w;
    (void) h;
#endif
}

//------------------------------------------------------------------------------
// gui_mch_get_scrollbar_xpadding
//------------------------------------------------------------------------------
int gui_mch_get_scrollbar_xpadding(void)
{
    return 0;
}

//------------------------------------------------------------------------------
// gui_mch_get_scrollbar_ypadding
//------------------------------------------------------------------------------
int gui_mch_get_scrollbar_ypadding(void)
{
    return 0;
}

//------------------------------------------------------------------------------
// gui_mch_get_rgb
//------------------------------------------------------------------------------
guicolor_T gui_mch_get_rgb(guicolor_T pixel)
{
    return pixel;
}

//------------------------------------------------------------------------------
// gui_mch_get_color
//------------------------------------------------------------------------------
guicolor_T gui_mch_get_color(char_u *name)
{
    static struct { char *n; guicolor_T c; } t[] =
    {
        {"Black",             0x00000000},
        {"DarkGray",          0x00A9A9A9},
        {"DarkGrey",          0x00A9A9A9},
        {"Gray",              0x00C0C0C0},
        {"Grey",              0x00C0C0C0},
        {"LightGray",         0x00D3D3D3},
        {"LightGrey",         0x00D3D3D3},
        {"Gray10",            0x001A1A1A},
        {"Grey10",            0x001A1A1A},
        {"Gray20",            0x00333333},
        {"Grey20",            0x00333333},
        {"Gray30",            0x004D4D4D},
        {"Grey30",            0x004D4D4D},
        {"Gray40",            0x00666666},
        {"Grey40",            0x00666666},
        {"Gray50",            0x007F7F7F},
        {"Grey50",            0x007F7F7F},
        {"Gray60",            0x00999999},
        {"Grey60",            0x00999999},
        {"Gray70",            0x00B3B3B3},
        {"Grey70",            0x00B3B3B3},
        {"Gray80",            0x00CCCCCC},
        {"Grey80",            0x00CCCCCC},
        {"Gray90",            0x00E5E5E5},
        {"Grey90",            0x00E5E5E5},
        {"White",             0x00FFFFFF},
        {"DarkRed",           0x00800000},
        {"Red",               0x00FF0000},
        {"LightRed",          0x00FFA0A0},
        {"DarkBlue",          0x00000080},
        {"Blue",              0x000000FF},
        {"LightBlue",         0x00ADD8E6},
        {"DarkGreen",         0x00008000},
        {"Green",             0x0000FF00},
        {"LightGreen",        0x0090EE90},
        {"DarkCyan",          0x00008080},
        {"Cyan",              0x0000FFFF},
        {"LightCyan",         0x00E0FFFF},
        {"DarkMagenta",       0x00800080},
        {"Magenta",           0x00FF00FF},
        {"LightMagenta",      0x00FFA0FF},
        {"Brown",             0x00804040},
        {"Yellow",            0x00FFFF00},
        {"LightYellow",       0x00FFFFE0},
        {"DarkYellow",        0x00BBBB00},
        {"SeaGreen",          0x002E8B57},
        {"Orange",            0x00FFA500},
        {"Purple",            0x00A020F0},
        {"SlateBlue",         0x006A5ACD},
        {"Violet",            0x00EE82EE},
        {"olivedrab",         0x006B8E23},
        {"coral",             0x00FF7F50},
        {"gold",              0x00FFD700},
        {"red2",              0x00EE0000},
        {"green3",            0x0000CD00},
        {"cyan4",             0x00008B8B},
        {"magenta3",          0x00CD00CD},
        {"deeppink",          0x00FF1493},
        {"khaki",             0x00F0E68C},
        {"slategrey",         0x00708090},
        {"tan",               0x00D2B48C},
        {"goldenrod",         0x00DAA520},
        {"springgreen",       0x0000FF7F},
        {"peru",              0x00CD853F},
        {"wheat",             0x00F5DEB3},
        {"yellowgreen",       0x009ACB32},
        {"indianred",         0x00CD5C5C},
        {"salmon",            0x00FA8072},
        {"SkyBlue",           0x0087CEEB},
        {"palegreen",         0x0098FB98},
        {"darkkhaki",         0x00BDB76B},
        {"navajowhite",       0x00FFDEAD},
        {"orangered",         0x00FF4500},
        {"yellow2",           0x00EEEE00},
        {"grey5",             0x000D0D0D},
        {"grey95",            0x00F2F2F2},
        {"Orchid",            0x00DA70D6},
        {"Pink",              0x00FFC0CB},
        {"PeachPuff",         0x00FFDAB9},
        {"Gold2",             0x00EEC900},
        {"Red3",              0x00CD0000},
        {"Gray45",            0x00737373},
        {"DeepPink3",         0x00CD1076},
        {"Pink2",             0x00EEA9B8},
        {"steelblue",         0x004682B4},
        {"DarkOrange",        0x00FF8C00},
        {"grey15",            0x00262626},
        {"RoyalBlue",         0x004169e1},
        {"CornflowerBlue",    0x006495ED},
    };

    if(name[0] == '#' && strlen((char *) name) == 7)
    {
        return strtol((char *) name + 1, NULL, 16);
    }

    size_t i;

    for(i = sizeof(t) / sizeof(t[0]); i--;)
    {
        if(STRICMP(name, t[i].n) == 0)
        {
            return t[i].c;
        }
    }

    return INVALCOLOR;
}

//------------------------------------------------------------------------------
// gui_mch_getmouse - Not supported
//------------------------------------------------------------------------------
void gui_mch_getmouse(int *x, int *y)
{
    *x = 0;
    *y = 0;
}

//------------------------------------------------------------------------------
// gui_mch_setmouse - Not supported
//------------------------------------------------------------------------------
void gui_mch_setmouse(int x, int y)
{
    (void) x;
    (void) y;
}

//------------------------------------------------------------------------------
// gui_mch_update - Not supported (not needed)
//------------------------------------------------------------------------------
void gui_mch_update(void)
{
}

//------------------------------------------------------------------------------
// gui_mch_wait_for_chars - Main loop, here control is passed back and forth
//                          between Vim and our MUI classes
//
// GUI input routine called by gui_wait_for_chars().  Waits for a character
// from the keyboard.
//
//  wtime == -1       Wait forever.
//  wtime == 0        This should never happen.
//  wtime > 0         Wait wtime milliseconds for a character.
//
// Returns OK if a character was found to be available within the given time,
// or FAIL otherwise.
//------------------------------------------------------------------------------
int gui_mch_wait_for_chars(int wtime)
{
#ifdef MUIVIM_FEAT_TIMEOUT
    // Don't enable timeouts for now, it might cause problems in the MUI message
    // loop. Passing the control over to Vim at any time is not safe.
    #warning Timeout support will cause MUI message loop problems
    (void) DoMethod(Con, M_ID(VimCon, SetTimeout), wtime > 0 ? wtime : 0);
#endif
    // Assume that we're idling.
    int state = MUIV_VimCon_State_Idle;

    // Wait until something happens.
    for(; state == MUIV_VimCon_State_Idle ;
        state = DoMethod(Con, M_ID(VimCon, GetState)))
    {
        IPTR sig;

        // Pass control over to MUI.
        if(unlikely(DoMethod(_app(Con), MUIM_Application_NewInput, &sig) ==
          (IPTR) MUIV_Application_ReturnID_Quit))
        {
            // Quit.
            gui_shell_closed();
            break;
        }

        // For some reason MUI returns 0 when jumping to the same screen that
        // we're currently on. If that happens, just pass control over to Vim.
        if(unlikely(!sig))
        {
            break;
        }

        sig = Wait(sig | SIGBREAKF_CTRL_C);

        if(unlikely(sig & SIGBREAKF_CTRL_C))
        {
            // SIGKILL.
            getout_preserve_modified(0);
        }
    }

    // Timeout or yield.
    return state == MUIV_VimCon_State_Yield ? OK : FAIL;
}

//------------------------------------------------------------------------------
// gui_mch_set_fg_color
//------------------------------------------------------------------------------
void gui_mch_set_fg_color(guicolor_T fg)
{
    (void) DoMethod(Con, M_ID(VimCon, SetFgColor), fg);
}

//------------------------------------------------------------------------------
// gui_mch_set_bg_color
//------------------------------------------------------------------------------
void gui_mch_set_bg_color(guicolor_T bg)
{
    (void) DoMethod(Con, M_ID(VimCon, SetBgColor), bg);
}

//------------------------------------------------------------------------------
// gui_mch_set_sp_color
//------------------------------------------------------------------------------
void gui_mch_set_sp_color(guicolor_T sp)
{
    (void) DoMethod(Con, M_ID(VimCon, SetFgColor), sp);
}

//------------------------------------------------------------------------------
// gui_mch_draw_string
//------------------------------------------------------------------------------
void gui_mch_draw_string(int row, int col, char_u *s, int len, int flags)
{
    (void) DoMethod(Con, M_ID(VimCon, DrawString), row, col, s, len,
                    flags & (DRAW_UNDERL|DRAW_BOLD|DRAW_TRANSP));
}

//------------------------------------------------------------------------------
// gui_mch_enable_menu - Not supported
//------------------------------------------------------------------------------
void gui_mch_enable_menu(int flag)
{
    (void) flag;
}

//------------------------------------------------------------------------------
// gui_mch_toggle_tearoff - Not supported
//------------------------------------------------------------------------------
void gui_mch_toggle_tearoffs(int enable)
{
    (void) enable;
}

//------------------------------------------------------------------------------
// gui_mch_flush - Not supported (not needed)
//------------------------------------------------------------------------------
void gui_mch_flush(void)
{
    MUI_Redraw(Con, MADF_DRAWUPDATE);
}

//------------------------------------------------------------------------------
// gui_mch_beep
//------------------------------------------------------------------------------
void gui_mch_beep(void)
{
    (void) DoMethod(Con, M_ID(VimCon, Beep));
}

//------------------------------------------------------------------------------
// gui_mch_set_shellsize - Not supported
//------------------------------------------------------------------------------
void gui_mch_set_shellsize(int width, int height, int min_width, int min_height,
                           int base_width, int base_height, int direction)
{
    (void) width;
    (void) height;
    (void) min_width;
    (void) min_height;
    (void) base_width;
    (void) base_height;
    (void) direction;
}

//------------------------------------------------------------------------------
// gui_mch_clear_block
//------------------------------------------------------------------------------
void gui_mch_clear_block(int row1, int col1, int row2, int col2)
{
    (void) DoMethod(Con, M_ID(VimCon, FillBlock), row1, col1, row2, col2,
                    gui.back_pixel);
}

//------------------------------------------------------------------------------
// gui_mch_delete_lines
//------------------------------------------------------------------------------
void gui_mch_delete_lines(int row, int num_lines)
{
    (void) DoMethod(Con, M_ID(VimCon, DeleteLines), row, num_lines,
                    gui.scroll_region_left, gui.scroll_region_right,
                    gui.scroll_region_bot, gui.back_pixel);
}

//------------------------------------------------------------------------------
// gui_mch_insert_lines
//------------------------------------------------------------------------------
void gui_mch_insert_lines(int row, int num_lines)
{
    (void) DoMethod(Con, M_ID(VimCon, DeleteLines), row, -num_lines,
                    gui.scroll_region_left, gui.scroll_region_right,
                    gui.scroll_region_bot, gui.back_pixel);
}

//------------------------------------------------------------------------------
// gui_mch_set_font - Not supported (let MUI handle this)
//------------------------------------------------------------------------------
void gui_mch_set_font(GuiFont font)
{
    (void) font;
}

//------------------------------------------------------------------------------
// gui_mch_clear_all
//------------------------------------------------------------------------------
void gui_mch_clear_all()
{
    (void) DoMethod(Con, M_ID(VimCon, FillBlock), 0, 0, gui.num_rows,
                    gui.num_cols, gui.back_pixel);
}

//------------------------------------------------------------------------------
// gui_mch_flash
//------------------------------------------------------------------------------
void gui_mch_flash(int msec)
{
    (void) msec;
    (void) DoMethod(Con, M_ID(VimCon, Beep));
}

//------------------------------------------------------------------------------
// gui_mch_set_menu_pos - Not supported (let MUI handle this)
//------------------------------------------------------------------------------
void gui_mch_set_menu_pos(int x, int y, int w, int h)
{
    (void) x;
    (void) y;
    (void) w;
    (void) h;
}

#ifdef __amigaos4__
static int gui_os4_init(void)
{
    if(unlikely(!(MUIMasterBase = OpenLibrary("muimaster.library", 19))))
    {
        fprintf(stderr, "Failed to open muimaster.library.\n");
        return FALSE;
    }

    IMUIMaster = (struct MUIMasterIFace *)
                 GetInterface(MUIMasterBase, "main", 1, NULL);

    if(unlikely(!(CyberGfxBase=OpenLibrary("cybergraphics.library",40))))
    {
        fprintf(stderr, "Failed to open cybergraphics.library.\n");
        return FALSE;
    }

    ICyberGfx= (struct CyberGfxIFace*) GetInterface(CyberGfxBase,"main",1,NULL);

    if(unlikely(!(KeymapBase =OpenLibrary("keymap.library", 50))))
    {
        fprintf(stderr, "Failed to open keymap.library.\n");
        return FALSE;
    }

    IKeymap = (struct KeymapIFace*) GetInterface(KeymapBase, "main", 1, NULL);
    return (IMUIMaster && ICyberGfx && IKeymap) ? OK : FAIL;
}
#endif

//------------------------------------------------------------------------------
// gui_mch_init - Initialise the GUI
//------------------------------------------------------------------------------
int gui_mch_init(void)
{
    Object *Win, *Set = NULL, *Abo = NULL;
#ifdef __amigaos4__
    if(unlikely(!gui_os4_init()))
    {
        getout(1);
    }
#endif
    static const CONST_STRPTR classes[] = {"TheBar.mcc", NULL};
    VimToolbarClass = VimConClass = VimMenuClass = NULL;

    // Create custom classes
    VimToolbarClass = MUI_CreateCustomClass(NULL, (ClassID) MUIC_TheBar, NULL,
                                            sizeof(struct VimToolbarData),
                                            (APTR) DISPATCH_GATE(VimToolbar));
    if(unlikely(!VimToolbarClass))
    {
        VimMessage("Error", "MCC_TheBar required", "OK");
        getout(1);
    }

    VimConClass = MUI_CreateCustomClass(NULL, (ClassID) MUIC_Area, NULL,
                                        sizeof(struct VimConData),
                                        (APTR) DISPATCH_GATE(VimCon));
    VimMenuClass = MUI_CreateCustomClass(NULL, (ClassID) MUIC_Menustrip, NULL,
                                         sizeof(struct VimMenuData),
                                         (APTR) DISPATCH_GATE(VimMenu));

#ifdef MUIVIM_FEAT_SCROLLBAR
    VimScrollbarClass = MUI_CreateCustomClass(NULL, (ClassID) MUIC_Scrollbar, NULL,
                                         sizeof(struct VimScrollbarData),
                                         (APTR) DISPATCH_GATE(VimScrollbar));

    if(unlikely(!VimConClass || !VimMenuClass || !VimScrollbarClass))
#else
    if(unlikely(!VimConClass || !VimMenuClass))
#endif
    {
        kmsg(_(e_outofmem));
        getout(1);
    }

    // Generate full version string
    static char vs[64];
    snprintf(vs, sizeof(vs),
#ifdef __MORPHOS__
            " "
#endif
            "Vim %d.%d.%d"
#ifdef BUILDDATE
            " (%s)"
#endif
            , VIM_VERSION_MAJOR, VIM_VERSION_MINOR, highest_patch()
#ifdef BUILDDATE
            , BUILDDATE
#endif
            );

    // Set up the class hierachy
    App = MUI_NewObject(MUIC_Application,
        MUIA_Application_UsedClasses, classes,
        MUIA_Application_Menustrip, Mnu =
            NewObject(VimMenuClass->mcc_Class, NULL,
            MUIA_Menustrip_Enabled, TRUE,
            MUIA_Family_Child, MUI_NewObject(MUIC_Menu,
                MUIA_Menu_Title, "MUI",
                    MUIA_UserData, MUIV_VimMenu_AddMenu_AlwaysLast,
                    MUIA_Family_Child, Set = MUI_NewObject(MUIC_Menuitem,
                        MUIA_Menuitem_Title, "MUI Settings...",
                    TAG_END),
                    MUIA_Family_Child, MUI_NewObject(MUIC_Menuitem,
                        MUIA_Menuitem_Title, NM_BARLABEL,
                    TAG_END),
                    MUIA_Family_Child, Abo = MUI_NewObject(MUIC_Menuitem,
                        MUIA_Menuitem_Title, "About MUI...",
                    TAG_END),
                TAG_END),
        TAG_END),
        MUIA_Application_Base, "Vim",
        MUIA_Application_Description, "The ubiquitous editor",
        MUIA_Application_Title, "Vim",
        MUIA_Application_Version, vs,
        MUIA_Application_DiskObject,
#ifdef __amigaos4__
            GetDiskObject((STRPTR) "VIM:icons/Vim_LodsaColorsMason"),
#else
            GetDiskObject((STRPTR) "VIM:icons/Vim_LodsaColors"),
#endif
        MUIA_Application_Window, Win =
            MUI_NewObject(MUIC_Window,
            MUIA_Window_Title, vs,
            MUIA_Window_ScreenTitle, vs,
            MUIA_Window_ID, MAKE_ID('W','D','L','A'),
            MUIA_Window_AppWindow, TRUE,
            MUIA_Window_DisableKeys, 0xffffffff,
            MUIA_Window_RootObject, MUI_NewObject(MUIC_Group,
                MUIA_Group_Horiz, FALSE,
                MUIA_Group_Child, Tlb =
                    NewObject(VimToolbarClass->mcc_Class, NULL,
                    TAG_END),
                MUIA_Group_Child, MUI_NewObject(MUIC_Group,
                    MUIA_Group_Horiz, TRUE,
                    MUIA_Group_Child, Lsg = MUI_NewObject(MUIC_Group,
                        MUIA_Group_Horiz, FALSE,
                        MUIA_ShowMe, FALSE,
                        TAG_END),
                    MUIA_Group_Child, Bsg = MUI_NewObject(MUIC_Group,
                        MUIA_Group_Horiz, FALSE,
                        MUIA_ShowMe, TRUE,
                        MUIA_Group_Child, Con =
                            NewObject(VimConClass->mcc_Class, NULL,
                            TAG_END),
                        TAG_END),
                    MUIA_Group_Child, Rsg = MUI_NewObject(MUIC_Group,
                        MUIA_Group_Horiz, FALSE,
                        MUIA_ShowMe, FALSE,
                        TAG_END),
                    TAG_END),
                TAG_END),
            TAG_END),
        TAG_END);

    if(unlikely(!App))
    {
        kmsg(_(e_outofmem));
        getout(1);
    }

    // Open the window to finish setup, cheat (see gui_mch_open).
    set(Win, MUIA_Window_Open, TRUE);

    // We want keyboard input by default
    set(Win, MUIA_Window_DefaultObject, Con);

    // Exit application upon close request (trap this later on)
    (void) DoMethod(Win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
                   (IPTR) App, 2, MUIM_Application_ReturnID,
                    MUIV_Application_ReturnID_Quit);

    // Set up drag and drop notifications
    (void) DoMethod(Win, MUIM_Notify, MUIA_AppMessage, MUIV_EveryTime,
                   (IPTR) Con, 2, M_ID(VimCon, AppMessage), MUIV_TriggerValue);

    // Let us know if the application gets iconified.
    (void) DoMethod(App, MUIM_Notify, MUIA_Application_Iconified,
                    MUIV_EveryTime, (IPTR) Con, 2, M_ID(VimCon, IconState),
                    MUIV_TriggerValue);

    // MUI specific menu parts
    if(likely(Abo && Set))
    {
        (void) DoMethod(Abo, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime,
                        Con, 1, M_ID(VimCon, AboutMUI));
        (void) DoMethod(Set, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime,
                        Con, 1, M_ID(VimCon, MUISettings));
    }

    return OK;
}

//------------------------------------------------------------------------------
// gui_mch_prepare
//------------------------------------------------------------------------------
void gui_mch_prepare(int *argc, char **argv)
{
}

//------------------------------------------------------------------------------
// gui_mch_init_check
//------------------------------------------------------------------------------
int gui_mch_init_check(void)
{
    BPTR icons = Lock("VIM:icons", ACCESS_READ);

    if(unlikely(!icons))
    {
        VimMessage("Warning", "Invalid VIM assign", "OK");
    }

    UnLock(icons);
    return OK;
}

//------------------------------------------------------------------------------
// gui_mch_open
//------------------------------------------------------------------------------
int gui_mch_open(void)
{
    return OK;
}

//------------------------------------------------------------------------------
// gui_mch_init_font - Not supported (let MUI handle this)
//------------------------------------------------------------------------------
int gui_mch_init_font(char_u *vim_font_name, int fontset)
{
    (void) vim_font_name;
    (void) fontset;
    return OK;
}

//------------------------------------------------------------------------------
// gui_mch_exit
//------------------------------------------------------------------------------
void gui_mch_exit(int rc)
{
    (void) rc;

    if(likely(App))
    {
        // Save icon pointer
        IPTR icon = 0;
        get(App, MUIA_Application_DiskObject, &icon);

        // Close window and destroy app
        set(_win(App), MUIA_Window_Open, FALSE);
        MUI_DisposeObject(App);

        // Free icon resources, MUI won't do this
        if(likely(icon))
        {
            FreeDiskObject((struct DiskObject *) icon);
        }
    }

    // Destroy custom classes. Must check for NULL.
    if(likely(VimMenuClass))
    {
        MUI_DeleteCustomClass(VimMenuClass);
    }

    if(likely(VimConClass))
    {
        MUI_DeleteCustomClass(VimConClass);
    }

    if(likely(VimToolbarClass))
    {
        MUI_DeleteCustomClass(VimToolbarClass);
    }

#ifdef MUIVIM_FEAT_SCROLLBAR
    if(likely(VimScrollbarClass))
    {
        MUI_DeleteCustomClass(VimScrollbarClass);
    }
#endif

#ifdef __amigaos4__
    if(likely(IMUIMaster))
    {
        DropInterface((struct Interface *) IMUIMaster);
        CloseLibrary((struct Library *) MUIMasterBase);
    }

    if(likely(ICyberGfx))
    {
        DropInterface((struct Interface *) ICyberGfx);
        CloseLibrary((struct Library *) CyberGfxBase);
    }

    if(likely(IKeymap))
    {
        DropInterface((struct Interface*) IKeymap);
        CloseLibrary((struct Library *) KeymapBase);
    }
#endif
}

//------------------------------------------------------------------------------
// gui_mch_draw_hollow_cursor
//------------------------------------------------------------------------------
void gui_mch_draw_hollow_cursor(guicolor_T color)
{
    (void) color;
    (void) DoMethod(Con, M_ID(VimCon, DrawHollowCursor), gui.row, gui.col,
                    gui.norm_pixel);
}

//------------------------------------------------------------------------------
// gui_mch_draw_part_cursor
//------------------------------------------------------------------------------
void gui_mch_draw_part_cursor(int w, int h, guicolor_T color)
{
    (void) color;
    (void) DoMethod(Con, M_ID(VimCon, DrawPartCursor), gui.row, gui.col, w, h,
                    gui.norm_pixel);
}

//------------------------------------------------------------------------------
// gui_mch_set_text_area_pos - Not supported (let MUI handle this)
//------------------------------------------------------------------------------
void gui_mch_set_text_area_pos(int x, int y, int w, int h)
{
    (void) x;
    (void) y;
    (void) w;
    (void) h;
}

//------------------------------------------------------------------------------
// gui_mch_get_screen_dimensions
//------------------------------------------------------------------------------
void gui_mch_get_screen_dimensions(int *screen_w, int *screen_h)
{
    (void) DoMethod(Con, M_ID(VimCon, GetScreenDim), screen_w, screen_h);
}

//------------------------------------------------------------------------------
// gui_mch_add_menu
//------------------------------------------------------------------------------
void gui_mch_add_menu(vimmenu_T *menu, int index)
{
    (void) index;

    if(unlikely(menu_is_popup(menu->name) || menu_is_toolbar(menu->name)))
    {
        return;
    }

    (void) DoMethod(Mnu, M_ID(VimMenu, AddMenu), menu->parent, menu,
                    menu->dname);
}

//------------------------------------------------------------------------------
// gui_mch_add_menu_item - Since Vim treats menu items and toolbar buttons
//                         in the same way we need some code for demuxing
//                         and discarding.
//------------------------------------------------------------------------------
void gui_mch_add_menu_item(vimmenu_T *menu, int index)
{
    (void) index;
    vimmenu_T *p = menu->parent;

    // Menu items must have parents
    if(unlikely(!p))
    {
        kmsg(_(e_null));
        return;
    }

    // Ignore popups for now
    if(unlikely(menu_is_popup(p->name)))
    {
        return;
    }

    // Menu items can be proper menu items or toolbar buttons
    if(unlikely(menu_is_toolbar(p->name) && !menu_is_separator(menu->name)))
    {
        (void) DoMethod(Tlb, M_ID(VimToolbar, AddButton), menu, menu->dname,
                        menu->dname);
        return;
    }

    // A menu spacer?
    if(unlikely(menu_is_separator(menu->name)))
    {
        (void) DoMethod(Mnu, M_ID(VimMenu, AddSpacer), menu->parent);
        return;
    }

    // A normal menu item.
    (void) DoMethod(Mnu, M_ID(VimMenu, AddMenuItem), menu->parent, menu,
                    menu->dname);
}

//------------------------------------------------------------------------------
// gui_mch_show_toolbar
//------------------------------------------------------------------------------
void gui_mch_show_toolbar(int showit)
{
    (void) DoMethod(Tlb, MUIM_Set, MUIA_ShowMe, showit);
}

//------------------------------------------------------------------------------
// gui_mch_destroy_menu
//------------------------------------------------------------------------------
void gui_mch_destroy_menu(vimmenu_T *menu)
{
    (void) DoMethod(Mnu, M_ID(VimMenu, RemoveMenu), menu);
}

//------------------------------------------------------------------------------
// gui_mch_menu_grey
//------------------------------------------------------------------------------
void gui_mch_menu_grey(vimmenu_T *menu, int grey)
{
    (void) DoMethod(Mnu, M_ID(VimMenu, Grey), menu, grey);
}

//------------------------------------------------------------------------------
// gui_mch_menu_hidden - Not supported
//------------------------------------------------------------------------------
void gui_mch_menu_hidden(vimmenu_T *menu, int hidden)
{
    (void) menu;
    (void) hidden;
}

//------------------------------------------------------------------------------
// gui_mch_draw_menubar - Not supported (not needed)
//------------------------------------------------------------------------------
void gui_mch_draw_menubar(void)
{
}

//------------------------------------------------------------------------------
// gui_mch_show_popupmenu - Not supported
//------------------------------------------------------------------------------
void gui_mch_show_popupmenu(vimmenu_T *menu)
{
    (void) menu;
}

//------------------------------------------------------------------------------
// gui_mch_mousehide - Not supported
//------------------------------------------------------------------------------
void gui_mch_mousehide(int hide)
{
    (void) hide;
}

//------------------------------------------------------------------------------
// gui_mch_adjust_charheight - Not supported
//------------------------------------------------------------------------------
int gui_mch_adjust_charheight(void)
{
    return OK;
}

//------------------------------------------------------------------------------
// gui_mch_new_colors - Not supported (not needed)
//------------------------------------------------------------------------------
void gui_mch_new_colors(void)
{
    (void) DoMethod(Con, M_ID(VimCon, Clear));
}

//------------------------------------------------------------------------------
// gui_mch_haskey - Not supported
//------------------------------------------------------------------------------
int gui_mch_haskey(char_u *name)
{
    (void) name;
    return OK;
}

//------------------------------------------------------------------------------
// gui_mch_iconify - Not supported
//------------------------------------------------------------------------------
void gui_mch_iconify(void)
{
    // For some reason this causes a crash on OS4. Disable for now.
#ifndef __amigaos4__
    set(App, MUIA_Application_Iconified, TRUE);
#endif
}

//------------------------------------------------------------------------------
// gui_mch_invert_rectangle
//------------------------------------------------------------------------------
void gui_mch_invert_rectangle(int row, int col, int nr, int nc)
{
    (void) DoMethod(Con, M_ID(VimCon, InvertRect), row, col, nr, nc);
}

//------------------------------------------------------------------------------
// clip_mch_own_selection - Not supported
//------------------------------------------------------------------------------
int clip_mch_own_selection(Clipboard_T *cbd)
{
    (void) cbd;
    return OK;
}

//------------------------------------------------------------------------------
// clip_mch_lose_selection - Not supported
//------------------------------------------------------------------------------
void clip_mch_lose_selection(Clipboard_T *cbd)
{
    (void) cbd;
}

//------------------------------------------------------------------------------
// clip_mch_request_selection
//------------------------------------------------------------------------------
void clip_mch_request_selection(Clipboard_T *cbd)
{
    (void) cbd;
    (void) DoMethod(Con, M_ID(VimCon, Paste), cbd);
}

//------------------------------------------------------------------------------
// clip_mch_set_selection
//------------------------------------------------------------------------------
void clip_mch_set_selection(Clipboard_T *cbd)
{
    (void) cbd;
    (void) DoMethod(Con, M_ID(VimCon, Copy), cbd);
}

//------------------------------------------------------------------------------
// gui_mch_destroy_scrollbar - Not supported
//------------------------------------------------------------------------------
void gui_mch_destroy_scrollbar(scrollbar_T *sb)
{
#ifdef MUIVIM_FEAT_SCROLLBAR
    if(unlikely(!sb || !sb->id || !DoMethod(sb->id, M_ID(VimScrollbar, Uninstall))))
    {
        return;
    }

    // Free uninstalled object.
    MUI_DisposeObject(sb->id);

    // Safety measure.
    sb->id = NULL;
#else
    (void) sb;
#endif
}

//------------------------------------------------------------------------------
// gui_mch_browse - Put up a file requester.
// Returns the selected name in allocated memory, or NULL for Cancel.
// saving           select file to write
// title            title for the window
// dflt             default name
// ext              not used (extension added)
// initdir          initial directory, NULL for current dir
// filter           not used (file name filter)
//------------------------------------------------------------------------------
char_u *gui_mch_browse(int saving, char_u *title, char_u *dflt, char_u *ext,
                       char_u *initdir, char_u *filter )
{
    (void) saving;
    (void) filter;
    (void) dflt;
    (void) ext;
    return (char_u *) DoMethod(Con, M_ID(VimCon, Browse), title, initdir);
}

//------------------------------------------------------------------------------
// gui_mch_set_blinking
//------------------------------------------------------------------------------
void gui_mch_set_blinking(long wait, long on, long off)
{
    (void) DoMethod(Con, M_ID(VimCon, SetBlinking), wait, on, off);
}

//------------------------------------------------------------------------------
// gui_mch_start_blink
//------------------------------------------------------------------------------
void gui_mch_start_blink(void)
{
    (void) DoMethod(Con, M_ID(VimCon, StartBlink));
}

//------------------------------------------------------------------------------
// gui_mch_stop_blink
//------------------------------------------------------------------------------
void gui_mch_stop_blink(int FIXME)
{
    (void) FIXME;
    (void) DoMethod(Con, M_ID(VimCon, StopBlink));
}

//------------------------------------------------------------------------------
// gui_mch_is_blinking
//------------------------------------------------------------------------------
int gui_mch_is_blinking(void)
{
    return DoMethod(Con, M_ID(VimCon, IsBlinking));
}

//------------------------------------------------------------------------------
// gui_mch_is_blink_off
//------------------------------------------------------------------------------
int gui_mch_is_blink_off(void)
{
    return !DoMethod(Con, M_ID(VimCon, IsBlinking));
}

//------------------------------------------------------------------------------
// gui_mch_settitle
//------------------------------------------------------------------------------
void gui_mch_settitle(char_u *title, char_u *icon)
{
    (void) icon;
    (void) DoMethod(Con, M_ID(VimCon, SetTitle), title);
}

