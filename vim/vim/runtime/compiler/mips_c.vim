" Vim compiler file
" Compiler:	SGI IRIX 6.5 MIPS C (cc)
" Maintainer:	David Harrison <david_jr@users.sourceforge.net>
" Last Change:	2002 Feb 26

if exists("current_compiler")
  finish
endif
let current_compiler = "mips_c"

setlocal errorformat=%Ecc\-%n\ %.%#:\ ERROR\ File\ =\ %f\%\\,\ Line\ =\ %l,
		    \%Wcc\-%n\ %.%#:\ WARNING\ File\ =\ %f\%\\,\ Line\ =\ %l,
		    \%Icc\-%n\ %.%#:\ REMARK\ File\ =\ %f\%\\,\ Line\ =\ %l,
		    \%+C\ \ %m.,
		    \%-Z\ \ %p^,
		    \%-G\\s%#,
		    \%-G%.%#
