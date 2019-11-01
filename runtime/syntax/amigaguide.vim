" AmigaGuide syntax file
" Language:	    AmigaGuide
" Maintainer:	Ola Söder <rolfkopman@gmail.com>
" First Author:	Ola Söder <rolfkopman@gmail.com>
" Last Change:	2019 Nov 1
" Version:      1.0
" URL:	        -

" quit when a syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

" version
syn region AmigaGuideDocVer start="@$VER:" end="$" oneline

" strings
syn region AmigaGuideString	start=+"+ end=+"+ oneline

" keywords
syn match AmigaGuideKey "\v(\@ENDNODE|\@NODE|\@TOC|\@DATABASE|LINK)"

" blocks
syn region AmigaGuideBlock start="@{" end="}" contains=ALL

" sync
syn sync lines=50

" Define the default highlighting.
if !exists("skip_installer_syntax_inits")
  hi def link AmigaGuideDocVer		Underlined
  hi def link AmigaGuideKey         Label
  hi def link AmigaGuideString      String
  hi def link AmigaGuideBlock       PreProc
endif

let b:current_syntax = "installer"

" vim:ts=15
