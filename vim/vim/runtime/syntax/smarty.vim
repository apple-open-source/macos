" Vim syntax file
" Language:	Smarty Templates (http://www.phpinsider.com/php/code/Smarty/)
" Maintainer:	Manfred Stienstra manfred.stienstra@dwerg.net
" Last Change:  Fri Apr 12 10:33:51 CEST 2002
" Filenames:    *.tpl
" URL:		http://www.dwerg.net/download/vim/smarty.vim

" For version 5.x: Clear all syntax items
" For version 6.x: Quit when a syntax file was already loaded
if !exists("main_syntax")
  if version < 600
    syntax clear
  elseif exists("b:current_syntax")
  finish
endif
  let main_syntax = 'smarty'
endif

syn case ignore

runtime! syntax/html.vim
"syn cluster htmlPreproc add=smartyUnZone

syn match smartyBlock contained "[\[\]]"

syn keyword smartyTagName capture config_load include include_php
syn keyword smartyTagName insert if elseif else ldelim rdelim literal
syn keyword smartyTagName php section sectionelse foreach foreachelse
syn keyword smartyTagName strip

syn keyword smartyInFunc ne eq

syn keyword smartyProperty contained "file="
syn keyword smartyProperty contained "loop="
syn keyword smartyProperty contained "name="
syn keyword smartyProperty contained "include="
syn keyword smartyProperty contained "skip="
syn keyword smartyProperty contained "section="

syn keyword smartyConstant "\$smarty"

syn keyword smartyDot .

syn region smartyZone matchgroup=Delimiter start="{" end="}" contains=smartyProperty, smartyString, smartyBlock, smartyTagName, smartyConstant, smartyInFunc

syn region  htmlString   contained start=+"+ end=+"+ contains=htmlSpecialChar,javaScriptExpression,@htmlPreproc,smartyZone
syn region  htmlString   contained start=+'+ end=+'+ contains=htmlSpecialChar,javaScriptExpression,@htmlPreproc,smartyZone
  syn region htmlLink start="<a\>\_[^>]*\<href\>" end="</a>"me=e-4 contains=@Spell,htmlTag,htmlEndTag,htmlSpecialChar,htmlPreProc,htmlComment,javaScript,@htmlPreproc,smartyZone


if version >= 508 || !exists("did_smarty_syn_inits")
  if version < 508
    let did_smarty_syn_inits = 1
    command -nargs=+ HiLink hi link <args>
  else
    command -nargs=+ HiLink hi def link <args>
  endif

  HiLink smartyTagName Identifier
  HiLink smartyProperty Constant
  " if you want the text inside the braces to be colored, then
  " remove the comment in from of the next statement
  "HiLink smartyZone Include
  HiLink smartyInFunc Function
  HiLink smartyBlock Constant
  HiLink smartyDot SpecialChar
  delcommand HiLink
endif

let b:current_syntax = "smarty"

if main_syntax == 'smarty'
  unlet main_syntax
endif

" vim: ts=8
