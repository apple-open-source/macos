" Vim compiler file
" Compiler:		bcc - Borland C
" Maintainer:	Emile van Raaij (eraaij@xs4all.nl)
" Last Change:	2002 Mar 09

if exists("current_compiler")
  finish
endif
let current_compiler = "bcc"

" A workable errorformat for Borland C
setlocal errorformat=%*[^0-9]%n\ %f\ %l:\ %m

" default make
setlocal makeprg=make
