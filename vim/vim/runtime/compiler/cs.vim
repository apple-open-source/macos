" Vim compiler file
" Compiler:	ms C#
" Maintainer:	Joseph H. Yao (hyao@sina.com)
" Last Change:	2003 Apr 25

if exists("current_compiler")
  finish
endif
let current_compiler = "cs"

" default errorformat
setlocal errorformat&

" default make
setlocal makeprg=csc\ %
