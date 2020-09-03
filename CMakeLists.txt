cmake_minimum_required(VERSION 3.10)
project("vim")

set(CMAKE_C_STANDARD 99)

set(sources
        src/arabic.c
        src/arglist.c
        src/autocmd.c
        src/beval.c
        src/blob.c
        src/blowfish.c
        src/buffer.c
        src/bufwrite.c
        src/change.c
        src/charset.c
        src/cindent.c
        src/cmdhist.c
        src/cmdexpand.c
        src/crypt.c
        src/crypt_zip.c
        src/debugger.c
        src/dict.c
        src/diff.c
        src/digraph.c
        src/drawline.c
        src/drawscreen.c
        src/edit.c
        src/eval.c
        src/evalbuffer.c
        src/evalfunc.c
        src/evalvars.c
        src/evalwindow.c
        src/ex_cmds.c
        src/ex_cmds2.c
        src/ex_docmd.c
        src/ex_eval.c
        src/ex_getln.c
        src/fileio.c
        src/filepath.c
        src/findfile.c
        src/fold.c
        src/getchar.c
        src/hardcopy.c
        src/hashtab.c
        src/highlight.c
        src/if_cscope.c
        src/indent.c
        src/insexpand.c
        src/json.c
        src/list.c
        src/main.c
        src/mark.c
        src/map.c
        src/mbyte.c
        src/memfile.c
        src/memline.c
        src/menu.c
        src/message.c
        src/misc1.c
        src/misc2.c
        src/mouse.c
        src/move.c
        src/normal.c
        src/ops.c
        src/option.c
        src/optionstr.c
        src/popupmenu.c
        src/popupwin.c
        src/quickfix.c
        src/regexp.c
        src/register.c
        src/screen.c
        src/scriptfile.c
        src/search.c
        src/session.c
        src/sha256.c
        src/sign.c
        src/spell.c
        src/spellfile.c
        src/spellsuggest.c
        src/syntax.c
        src/tag.c
        src/term.c
        src/termlib.c
        src/testing.c
        src/textprop.c
        src/ui.c
        src/undo.c
        src/usercmd.c
        src/userfunc.c
        src/version.c
        src/viminfo.c
        src/window.c
        src/xdiff/xdiffi.c
        src/xdiff/xemit.c
        src/xdiff/xhistogram.c
        src/xdiff/xpatience.c
        src/xdiff/xprepare.c
        src/xdiff/xutils.c
        src/os_amiga.c
        src/gui.c
        src/gui_mui.c
        )

add_executable(vim ${sources})

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    add_definitions("-DAXRT -D__AROS__ -DAMIGA")
    target_link_libraries(vim PUBLIC muimaster intuition dos locale utility mui exec graphics
            /usr/lib/x86_64-linux-gnu/startup.o
            /usr/lib/x86_64-linux-gnu/axrt.ld
            )
    set(CMAKE_EXE_LINKER_FLAGS "-specs=/usr/lib/x86_64-linux-gnu/axrt.specs")
    target_include_directories(vim PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/proto>
            /usr/include/axrt
            )
else()
    target_include_directories(vim PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/proto>
            )
endif()

set(CMAKE_C_FLAGS "-DFEAT_GUI -DFEAT_GUI_MUI -DFEAT_BROWSE -DFEAT_TOOLBAR -DFEAT_HUGE -DNO_ARP -DUSE_TMPNAM -DHAVE_STDARG_H -DHAVE_TGETENT -DHAVE_TERMCAP -DNEW_SHELLSIZE -DOSPEED_EXTERN -DUP_BC_PC_EXTERN")

target_compile_options(vim PRIVATE -Wno-attributes -Wextra)
