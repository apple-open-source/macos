" Vim compiler file
" Compiler:	SGI IRIX 5.3 CC or NCC
" Maintainer:	David Harrison <david_jr@users.sourceforge.net>
" Last Change:	2002 Feb 26

if exists("current_compiler")
  finish
endif
let current_compiler = "irix5_cpp"

setlocal errorformat=%E\"%f\"\\,\ line\ %l:\ error(%n):\ ,
		    \%E\"%f\"\\,\ line\ %l:\ error(%n):\ %m,
		    \%W\"%f\"\\,\ line\ %l:\ warning(%n):\ %m,
		    \%+IC++\ prelinker:\ %m,
		      \%-Z\ \ %p%^,
		      \%+C\ %\\{10}%.%#,
		      \%-G%.%#
