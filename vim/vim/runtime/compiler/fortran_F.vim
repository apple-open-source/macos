" Vim compiler file
" Compiler:	Fortran Company/NAGWare F compiler
" URL:		http://www.unb.ca/chem/ajit/compiler/fortran_F.vim
" Maintainer:	Ajit J. Thakkar (ajit AT unb.ca); <http://www.unb.ca/chem/ajit/>
" Version:	0.2
" Last Change: 2003 Feb. 12

if exists("current_compiler")
  finish
endif
let current_compiler = "fortran_F"

let s:cposet=&cpoptions
set cpoptions-=C

setlocal errorformat=%trror:\ %f\\,\ line\ %l:%m,
      \%tarning:\ %f\\,\ line\ %l:%m,
      \%tatal\ Error:\ %f\\,\ line\ %l:%m,
      \%-G%.%#
setlocal makeprg=F

let &cpoptions=s:cposet
unlet s:cposet
