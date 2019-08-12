" Vim syntax file
" Language:     Amiga Installer
" Maintainer:   Ola Söder (rolfkopman@gmail.com)
" First Author: Ola Söder (rolfkopman@gmail.com)
" Last Change:  2019 Aug 12

" quit when a syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

syn case ignore

" strings
syn region InstallerString	start=+"+ end=+"+ oneline contains=@Spell
syn region InstallerString	start="'" end="'" oneline contains=@Spell

" numbers
syn match InstallerNumber	"\<\d\+\>"

" reserved
syn match InstallerReserved	"@\<[a-zA-Z0-9\-]\+"

" commands
syn keyword	InstallerKey abort add and askbool askchoice askdir askdisk askfile asknumber askoptions
syn keyword	InstallerKey askstring bitand bitnot bitor cat closemedia complete copyfiles copylib database debug
syn keyword	InstallerKey delete div earlier effect eq execute exists exit expandpath fileonly fmt foreach
syn keyword	InstallerKey getassign getdevice getdiskspace getenv getsize getsum getversion gt gte
syn keyword	InstallerKey iconinfo if in lt lte makeassign makedir message mul neq not onerror or
syn keyword	InstallerKey pathonly patmatch procedure protect rename retrace rexx run select set setmedia shiftleft
syn keyword	InstallerKey shiftright showmedia startup strlen sub substr symbolset symbolval sys tackon textfile
syn keyword	InstallerKey tooltype transcript trap trace until user welcome while working xor
syn match      InstallerKey "[=+-/*><]"

" options
syn keyword	InstallerOption all append assigns back choices command compression confirm default delopts
syn keyword	InstallerOption dest disk files fonts getdefaulttool getposition getstack gettooltype help
syn keyword	InstallerOption include infos newname newpath nogauge noposition noreq optional override
syn keyword	InstallerOption pattern prompt quiet range safe setdefaulttool setposition setstack source
syn keyword	InstallerOption settooltype swapcolors resident

" scope
syn match InstallerScope "[()]"

" comments
syn case match
syn match	InstallerComment	";.*$" contains=InstallerCommentGroup

" sync
syn sync lines=50

" Define the default highlighting.
if !exists("skip_installer_syntax_inits")

  hi def link InstallerComment	Comment
  hi def link InstallerKey		Statement
  hi def link InstallerNumber		Number
  hi def link InstallerString		String
  hi def link InstallerOption		Define
  hi def link InstallerScope		Special
  hi def link InstallerReserved	Error

endif
let b:current_syntax = "installer"

" vim:ts=15
