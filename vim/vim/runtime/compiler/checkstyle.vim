" Vim compiler file
" Compiler:	Checkstyle
" Maintainer:	Doug Kearns <djkea2@mugca.its.monash.edu.au>
" URL:		http://mugca.its.monash.edu.au/~djkea2/vim/compiler/checkstyle.vim
" Last Change:	2003 May 11

if exists("current_compiler")
  finish
endif
let current_compiler = "checkstyle"

setlocal makeprg=java\ com.puppycrawl.tools.checkstyle.Main\ -f\ plain

" sample error: WebTable.java:282: '+=' is not preceeded with whitespace.
"		WebTable.java:201:1: '{' should be on the previous line.
setlocal errorformat=%f:%l:\ %m,%f:%l:%v:\ %m,%-G%.%#
