" Vim compiler file
" Compiler:	HP aCC
" Maintainer:	Matthias Ulrich <matthias-ulrich@web.de>
" URL:		http://www.subhome.de/vim/hp_acc.vim
" Last Change:	2003 May 11
"
"  aCC --version says: "HP ANSI C++ B3910B A.03.13"
"  This compiler has been tested on:
"       hp-ux 10.20, hp-ux 11.0 and hp-ux 11.11 (64bit)

if exists("current_compiler")
  finish
endif
let current_compiler = "hp_acc"


setlocal errorformat=%trror\ %n\:\ \"%f\"\\,\ line\ %l\ \#\ %m\ %#,
	 \%tarning\ %n\:\ \"%f\"\\,\ line\ %l\ \#\ %m\ %#
