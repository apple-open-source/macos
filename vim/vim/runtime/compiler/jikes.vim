" Vim Compiler File
" Compiler:	Jikes
" Maintainer:	Dan Sharp <dwsharp at hotmail dot com>
" Last Change:	2002 Oct 25
" URL:		http://mywebpage.netscape.com/sharppeople/vim/compiler

if exists("current_compiler")
  finish
endif
let current_compiler = "jikes"

" Jikes defaults to printing output on stderr
setlocal makeprg=jikes\ -Xstdout\ +E\ \"%\"
setlocal errorformat=%f:%l:%v:%*\\d:%*\\d:%*\\s%m
