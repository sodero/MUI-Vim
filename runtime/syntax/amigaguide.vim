" Vim syntax file
" Language:    AmigaGuide
" Maintainer:  Ola Söder <rolfkopman@gmail.com>
" Last Change: 2026 Mar 25
" Version:     2

if exists("b:current_syntax")
  finish
endif

syn case ignore

" Comments
syn match agComment "^@REM\>.*$"
syn match agComment "^@REMARK\>.*$"

" Version
syn region agVersion start="^@\$VER:" end="$" oneline

" Copyright
syn region agVersion start="^@(C)" end="$" oneline

" Strings
syn region agString start=+"+ end=+"+ oneline

" Escape sequences
syn match agEscape "\\@"
syn match agEscape "\\\\"

" Global and node commands
syn match agCommand "^@DATABASE\>"
syn match agCommand "^@NODE\>"
syn match agCommand "^@ENDNODE\>"
syn match agCommand "^@DNODE\>"
syn match agCommand "^@FONT\>"
syn match agCommand "^@AUTHOR\>"
syn match agCommand "^@MASTER\>"
syn match agCommand "^@HELP\>"
syn match agCommand "^@INDEX\>"
syn match agCommand "^@MACRO\>"
syn match agCommand "^@WIDTH\>"
syn match agCommand "^@HEIGHT\>"
syn match agCommand "^@WORDWRAP\>"
syn match agCommand "^@SMARTWRAP\>"
syn match agCommand "^@TAB\>"
syn match agCommand "^@ONOPEN\>"
syn match agCommand "^@ONCLOSE\>"
syn match agCommand "^@TITLE\>"
syn match agCommand "^@TOC\>"
syn match agCommand "^@NEXT\>"
syn match agCommand "^@PREV\>"
syn match agCommand "^@KEYWORDS\>"

" Attribute blocks
syn region agBlock start="@{" end="}" oneline contains=agLink,agFormat,agColor,agLayout,agSpecial,agString

" Link attributes
syn keyword agLink contained LINK ALINK CLOSE QUIT SYSTEM RX RXS

" Formatting
syn keyword agFormat contained B UB I UI U UU PLAIN BODY CODE

" Colors
syn keyword agColor contained APEN BPEN FG BG
syn keyword agColor contained Text Shine Shadow Fill FillText Background Highlight

" Layout
syn keyword agLayout contained JLEFT JCENTER JRIGHT
syn keyword agLayout contained LINDENT PARI PAR PARD LINE TAB
syn keyword agLayout contained SETTABS CLEARTABS

" Special attributes
syn keyword agSpecial contained AMIGAGUIDE

" Highlighting
hi def link agBlock PreProc
hi def link agColor Constant
hi def link agCommand Statement
hi def link agComment Comment
hi def link agEscape SpecialChar
hi def link agFormat Type
hi def link agLayout Identifier
hi def link agLink Special
hi def link agSpecial PreProc
hi def link agString String
hi def link agVersion Underlined

let b:current_syntax = "amigaguide"
