" Vim compiler file
" Compiler:	SGI IRIX 5.3 cc
" Maintainer:	David Harrison <david_jr@users.sourceforge.net>
" Last Change:	2002 Feb 26

if exists("current_compiler")
  finish
endif
let current_compiler = "irix5_c"

setlocal errorformat=\%Ecfe:\ Error:\ %f\\,\ line\ %l:\ %m,
		     \%Wcfe:\ Warning:\ %n:\ %f\\,\ line\ %l:\ %m,
		     \%Wcfe:\ Warning\ %n:\ %f\\,\ line\ %l:\ %m,
		     \%W(%l)\ \ Warning\ %n:\ %m,
		     \%-Z\ %p^,
		     \-G\\s%#,
		     \%-G%.%#
