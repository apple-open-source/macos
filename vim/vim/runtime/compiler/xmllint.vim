" Vim compiler file
" Compiler:	xmllint
" Maintainer:	Doug Kearns <djkea2@mugca.its.monash.edu.au>
" URL:		http://mugca.its.monash.edu.au/~djkea2/vim/compiler/xmllint.vim
" Last Change:	2002 Oct 23

if exists("current_compiler")
  finish
endif
let current_compiler = "xmllint"

let s:cpo_save = &cpo
set cpo-=C

setlocal makeprg=xmllint\ --valid\ --noout

setlocal errorformat=%E%f:%l:\ error:\ %m,
		    \%W%f:%l:\ warning:\ %m,
		    \%E%f:%l:\ validity\ error:\ %m,
		    \%W%f:%l:\ validity\ warning:\ %m,
		    \%-Z%p^,
		    \%-G%.%#

let &cpo = s:cpo_save
unlet s:cpo_save
