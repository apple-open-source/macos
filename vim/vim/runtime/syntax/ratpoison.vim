" Vim syntax file
" Filename:     ratpoison.vim
" Language:     Ratpoison configuration/commands file ( /etc/ratpoisonrc ~/.ratpoisonrc )
" Maintainer:   Doug Kearns <djkea2@mugca.cc.monash.edu.au>
" URL:		http://mugca.cc.monash.edu.au/~djkea2/vim/syntax/ratpoison.vim
" Last Change:  2003 May 11

" TODO: improve KeySym name support in ratpoisonKeySeqArg group

" For version 5.x: Clear all syntax items
" For version 6.x: Quit when a syntax file was already loaded
if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif

syn match   ratpoisonComment     "^\s*#.*$"			contains=ratpoisonTodo

syn keyword ratpoisonTodo	 TODO NOTE FIXME XXX		contained

syn match   ratpoisonBooleanArg  "\<\(on\|off\)\>"		contained

syn match   ratpoisonCommandArg  "\<\(abort\|banish\|bind\|chdir\|colon\|curframe\)\>" contained
syn match   ratpoisonCommandArg  "\<\(defbarloc\|defbgcolor\|defborder\|deffgcolor\|deffont\|definputwidth\)\>" contained
syn match   ratpoisonCommandArg  "\<\(defmaxsizegravity\|defpadding\|deftransgravity\|defwaitcursor\|defwinfmt\|defwingravity\)\>" contained
syn match   ratpoisonCommandArg  "\<\(defwinname\|delete\|echo\|escape\|exec\|focus\)\>" contained
syn match   ratpoisonCommandArg  "\<\(focusdown\|focusleft\|focusright\|focusup\|gravity\|help\)\>" contained
syn match   ratpoisonCommandArg  "\<\(hsplit\|info\|kill\|lastmsg\|meta\|msgwait\)\>" contained
syn match   ratpoisonCommandArg  "\<\(newwm\|next\|number\|only\|other\|prev\)\>" contained
syn match   ratpoisonCommandArg  "\<\(quit\|redisplay\|remove\|restart\|rudeness\|select\)\>" contained
syn match   ratpoisonCommandArg  "\<\(setenv\|source\|split\|startup_message\|time\|title\)\>" contained
syn match   ratpoisonCommandArg  "\<\(unbind\|unsetenv\|version\|vsplit\|windows\)\>" contained

syn case ignore
syn match   ratpoisonGravityArg  "\<\(n\|north\)\>"		contained
syn match   ratpoisonGravityArg  "\<\(nw\|northwest\)\>"	contained
syn match   ratpoisonGravityArg  "\<\(ne\|northeast\)\>"	contained
syn match   ratpoisonGravityArg  "\<\(w\|west\)\>"		contained
syn match   ratpoisonGravityArg  "\<\(c\|center\)\>"		contained
syn match   ratpoisonGravityArg  "\<\(e\|east\)\>"		contained
syn match   ratpoisonGravityArg  "\<\(s\|south\)\>"		contained
syn match   ratpoisonGravityArg  "\<\(sw\|southwest\)\>"	contained
syn match   ratpoisonGravityArg  "\<\(se\|southeast\)\>"	contained
syn case match

syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=\(F[1-9][0-9]\=\|\(\a\|\d\)\)\>" contained nextgroup=ratpoisonCommandArg skipwhite

syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=\(space\|exclam\|quotedbl\)\>" contained nextgroup=ratpoisonCommandArg skipwhite
syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=\(numbersign\|dollar\|percent\|ampersand\)\>" contained nextgroup=ratpoisonCommandArg skipwhite
syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=\(apostrophe\|quoteright\|parenleft\)\>" contained nextgroup=ratpoisonCommandArg skipwhite
syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=\(parenright\|asterisk\|plus\|comma\)\>" contained nextgroup=ratpoisonCommandArg skipwhite
syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=\(minus\|period\|slash\|colon\|semicolon\)\>" contained nextgroup=ratpoisonCommandArg skipwhite
syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=\(less\|equal\|greater\|question\|at\)\>" contained nextgroup=ratpoisonCommandArg skipwhite
syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=\(bracketleft\|backslash\|bracketright\)\>" contained nextgroup=ratpoisonCommandArg skipwhite
syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=\(asciicircum\|underscore\|grave\)\>" contained nextgroup=ratpoisonCommandArg skipwhite
syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=\(quoteleft\|braceleft\|bar\|braceright\)\>" contained nextgroup=ratpoisonCommandArg skipwhite
syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=\(asciitilde\)\>" contained nextgroup=ratpoisonCommandArg skipwhite

syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=\(BackSpace\|Tab\|Linefeed\|Clear\)\>" contained nextgroup=ratpoisonCommandArg skipwhite
syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=\(Return\|Pause\|Scroll_Lock\)\>" contained nextgroup=ratpoisonCommandArg skipwhite
syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=\(Sys_Req\|Escape\|Delete\)\>" contained nextgroup=ratpoisonCommandArg skipwhite

syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=\(Home\|Left\|Up\|Right\|Down\|Prior\)\>" contained nextgroup=ratpoisonCommandArg skipwhite
syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=\(Page_Up\|Next\|Page_Down\|End\|Begin\)\>" contained nextgroup=ratpoisonCommandArg skipwhite

syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=\(Select\|Print\|Execute\|Insert\|Undo\)\>" contained nextgroup=ratpoisonCommandArg skipwhite
syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=\(Redo\|Menu\|Find\|Cancel\|Help\)\>" contained nextgroup=ratpoisonCommandArg skipwhite
syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=\(Break\|Mode_switch\|script_switch\|Num_Lock\)\>" contained nextgroup=ratpoisonCommandArg skipwhite

syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=KP_\(Space\|Tab\|Enter\|F[1234]\)\>" contained nextgroup=ratpoisonCommandArg skipwhite
syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=KP_\(Home\|Left\|Up\|Right\|Down\)\>" contained nextgroup=ratpoisonCommandArg skipwhite
syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=KP_\(Prior\|Page_Up\|Next\|Page_Down\)\>" contained nextgroup=ratpoisonCommandArg skipwhite
syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=KP_\(End\|Begin\|Insert\|Delete\)\>" contained nextgroup=ratpoisonCommandArg skipwhite
syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=KP_\(Equal\|Multiply\|Add\|Separator\)\>" contained nextgroup=ratpoisonCommandArg skipwhite
syn match   ratpoisonKeySeqArg   "\<\([CMASH]\(-[CMASH]\)\{,4}-\)\=KP_\(Subtract\|Decimal\|Divide\|\d\)\>" contained nextgroup=ratpoisonCommandArg skipwhite

syn match   ratpoisonNumberArg   "\<\d\+\>"			contained nextgroup=ratpoisonNumberArg skipwhite

syn match   ratpoisonWinFmtArg   "%[nstaci]"			contained nextgroup=ratpoisonWinFmtArg skipwhite

syn match   ratpoisonWinNameArg  "\<\(name\|title\|class\)\>"	contained

syn match   ratpoisonStringCommand   "^\s*bind\s*"		nextgroup=ratpoisonKeySeqArg
syn match   ratpoisonStringCommand   "^\s*chdir\s*"
syn match   ratpoisonStringCommand   "^\s*colon\s*"		nextgroup=ratpoisonCommandArg
syn match   ratpoisonStringCommand   "^\s*echo\s*"
syn match   ratpoisonStringCommand   "^\s*escape\s*"		nextgroup=ratpoisonKeySeqArg
syn match   ratpoisonStringCommand   "^\s*exec\s*"
syn match   ratpoisonStringCommand   "^\s*gravity\s*"		nextgroup=ratpoisonGravityArg
syn match   ratpoisonStringCommand   "^\s*newwm\s*"
syn match   ratpoisonStringCommand   "^\s*number\s*"		nextgroup=ratpoisonNumberArg
syn match   ratpoisonStringCommand   "^\s*rudeness\s*"		nextgroup=ratpoisonNumberArg
syn match   ratpoisonStringCommand   "^\s*select\s*"		nextgroup=ratpoisonNumberArg
syn match   ratpoisonStringCommand   "^\s*setenv\s*"
syn match   ratpoisonStringCommand   "^\s*source\s*"
syn match   ratpoisonStringCommand   "^\s*startup_message\s*"	nextgroup=ratpoisonBooleanArg
syn match   ratpoisonStringCommand   "^\s*title\s*"
syn match   ratpoisonStringCommand   "^\s*unbind\s*"		nextgroup=ratpoisonKeySeqArg
syn match   ratpoisonStringCommand   "^\s*unsetenv\s*"

syn match   ratpoisonVoidCommand     "^\s*abort\s*$"
syn match   ratpoisonVoidCommand     "^\s*banish\s*$"
syn match   ratpoisonVoidCommand     "^\s*curframe\s*$"
syn match   ratpoisonVoidCommand     "^\s*delete"
syn match   ratpoisonVoidCommand     "^\s*focusdown\s*$"
syn match   ratpoisonVoidCommand     "^\s*focusleft\s*$"
syn match   ratpoisonVoidCommand     "^\s*focusright\s*$"
syn match   ratpoisonVoidCommand     "^\s*focus\s*$"
syn match   ratpoisonVoidCommand     "^\s*focusup\s*$"
syn match   ratpoisonVoidCommand     "^\s*help\s*$"
syn match   ratpoisonVoidCommand     "^\s*hsplit\s*$"
syn match   ratpoisonVoidCommand     "^\s*info\s*$"
syn match   ratpoisonVoidCommand     "^\s*kill\s*$"
syn match   ratpoisonVoidCommand     "^\s*lastmsg\s*$"
syn match   ratpoisonVoidCommand     "^\s*meta\s*$"
syn match   ratpoisonVoidCommand     "^\s*next\s*$"
syn match   ratpoisonVoidCommand     "^\s*only\s*$"
syn match   ratpoisonVoidCommand     "^\s*other\s*$"
syn match   ratpoisonVoidCommand     "^\s*prev\s*$"
syn match   ratpoisonVoidCommand     "^\s*quit\s*$"
syn match   ratpoisonVoidCommand     "^\s*redisplay\s*$"
syn match   ratpoisonVoidCommand     "^\s*remove\s*$"
syn match   ratpoisonVoidCommand     "^\s*restart\s*$"
syn match   ratpoisonVoidCommand     "^\s*split\s*$"
syn match   ratpoisonVoidCommand     "^\s*time\s*$"
syn match   ratpoisonVoidCommand     "^\s*version\s*$"
syn match   ratpoisonVoidCommand     "^\s*vsplit\s*$"
syn match   ratpoisonVoidCommand     "^\s*windows\s*$"

syn match   ratpoisonDefCommand      "^\s*defbarloc\s*"		nextgroup=ratpoisonNumberArg
syn match   ratpoisonDefCommand      "^\s*defbgcolor\s*"
syn match   ratpoisonDefCommand      "^\s*defborder\s*"		nextgroup=ratpoisonNumberArg
syn match   ratpoisonDefCommand      "^\s*deffgcolor\s*"
syn match   ratpoisonDefCommand      "^\s*deffont\s*"
syn match   ratpoisonDefCommand      "^\s*definputwidth\s*"	nextgroup=ratpoisonNumberArg
syn match   ratpoisonDefCommand      "^\s*defmaxsizegravity\s*"	nextgroup=ratpoisonGravityArg
syn match   ratpoisonDefCommand      "^\s*defpadding\s*"	nextgroup=ratpoisonNumberArg
syn match   ratpoisonDefCommand      "^\s*deftransgravity\s*"	nextgroup=ratpoisonGravityArg
syn match   ratpoisonDefCommand      "^\s*defwaitcursor\s*"	nextgroup=ratpoisonNumberArg
syn match   ratpoisonDefCommand      "^\s*defwinfmt\s*"		nextgroup=ratpoisonWinFmtArg
syn match   ratpoisonDefCommand      "^\s*defwingravity\s*"	nextgroup=ratpoisonGravityArg
syn match   ratpoisonDefCommand      "^\s*defwinname\s*"	nextgroup=ratpoisonWinNameArg
syn match   ratpoisonDefCommand      "^\s*msgwait\s*"		nextgroup=ratpoisonNumberArg

" Define the default highlighting.
" For version 5.7 and earlier: only when not done already
" For version 5.8 and later: only when an item doesn't have highlighting yet
if version >= 508 || !exists("did_ratpoison_syn_inits")
  if version < 508
    let did_ratpoison_syn_inits = 1
    command -nargs=+ HiLink hi link <args>
  else
    command -nargs=+ HiLink hi def link <args>
  endif

  HiLink ratpoisonArg		Constant
  HiLink ratpoisonBooleanArg	Boolean
  HiLink ratpoisonCommandArg	Constant
  HiLink ratpoisonComment	Comment
  HiLink ratpoisonDefCommand	Identifier
  HiLink ratpoisonGravityArg	Constant
  HiLink ratpoisonKeySeqArg	Special
  HiLink ratpoisonNumberArg	Number
  HiLink ratpoisonStringCommand Identifier
  HiLink ratpoisonTodo		Todo
  HiLink ratpoisonVoidCommand	 Identifier
  HiLink ratpoisonWinFmtArg	Special
  HiLink ratpoisonWinNameArg	Constant

  delcommand HiLink
endif

let b:current_syntax = "ratpoison"

" vim: ts=8 sw=2
