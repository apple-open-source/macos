" Vim indent file
" Language:	Php
" Author:	Miles Lott <milos@groupwhere.org>
" URL:		http://groupwhere.org/php.vim
" Last Change:	2003 May 11
" Version:	0.3
" Notes:  Close all switches with default:\nbreak; and it will look better.
"		  Also, open and close brackets should be alone on a line.
"		  This is my preference, and the only way this will look nice.
"		  Try an older version if you care less about the formatting of
"		  switch/case.	It is nearly perfect for anyone regardless of your
"		  stance on brackets.
"
" Options	php_noindent_switch=1 -- do not try to indent switch/case statements (version 0.1 behavior)

" Only load this indent file when no other was loaded.
if exists("b:did_indent")
	finish
endif
let b:did_indent = 1

setlocal indentexpr=GetPhpIndent()
setlocal indentkeys+=0=,0),=EO

" Only define the function once.
if exists("*GetPhpIndent")
	finish
endif

" Handle option(s)
if exists("php_noindent_switch")
	let b:php_noindent_switch=1
endif

function GetPhpIndent()
	" Find a non-blank line above the current line.
	let lnum = prevnonblank(v:lnum - 1)
	" Hit the start of the file, use zero indent.
	if lnum == 0
		return 0
	endif
	let line = getline(lnum)    " last line
	let cline = getline(v:lnum) " current line
	let pline = getline(lnum - 1) " previous to last line
	let ind = indent(lnum)

	" Indent after php open tags
	if line =~ '<?php'
		let ind = ind + &sw
	endif
	if cline =~ '\?\>' " Please fix this...
		let ind = ind - &sw
	endif

	if exists("b:php_noindent_switch") " version 1 behavior, diy switch/case,etc
		" Indent blocks enclosed by {} or ()
		if line =~ '[{(]\s*\(#[^)}]*\)\=$'
			let ind = ind + &sw
		endif
		if cline =~ '^\s*[)}]'
			let ind = ind - &sw
		endif
		return ind
	else " Try to indent switch/case statements as well
		" Indent blocks enclosed by {} or () or case statements, with some anal requirements
		if line =~ 'case.*:\|[{(]\s*\(#[^)}]*\)\=$'
			let ind = ind + &sw
			" return if the current line is not another case statement of the previous line is a bracket open
			if cline !~ '.*case.*:\|default:' || line =~ '[{(]\s*\(#[^)}]*\)\=$'
				return ind
			endif
		endif
		if cline =~ '^\s*case.*:\|^\s*default:\|^\s*[)}]'
			let ind = ind - &sw
			" if the last line is a break or return, or the current line is a close bracket,
			" or if the previous line is a default statement, subtract another
			if line =~ '^\s*break;\|^\s*return\|' && cline =~ '^\s*[)}]' && pline =~ 'default:'
				let ind = ind - &sw
			endif
		endif

		if line =~ 'default:'
			let ind = ind + &sw
		endif
		return ind
	endif
endfunction
" vim: set ts=4 sw=4:
