" Description:	InstallShield indenter
" Author:	Johannes Zellner <johannes@zellner.org>
" URL:		http://www.zellner.org/vim/indent/ishd.vim
" Last Change:	Tue, 07 Aug 2001 14:49:42 W. Europe Standard Time

" Only load this indent file when no other was loaded.
if exists("b:did_indent")
    finish
endif
let b:did_indent = 1

setlocal indentexpr=GetIshdIndent(v:lnum)
setlocal indentkeys+==else,=elseif,=endif,=end,=begin
" setlocal indentkeys-=0#

" Only define the function once.
if exists("*GetIshdIndent")
    finish
endif

fun! GetIshdIndent(lnum)
    " labels and preprocessor get zero indent immediately
    let this_line = getline(a:lnum)
    if this_line =~ '^\s*\(\<\k\+\>:\s*$\|#.*\)'
	return 0
    endif

    " Find a non-blank line above the current line.
    " Skip over labels and preprocessor directives.
    let lnum = a:lnum
    while lnum > 0
	let lnum = prevnonblank(lnum - 1)
	let previous_line = getline(lnum)
	if previous_line !~ '^\(\s*\<\k\+\>:\s*$\|#.*\)'
	    break
	endif
    endwhile

    " Hit the start of the file, use zero indent.
    if lnum == 0
	return 0
    endif

    let ind = indent(lnum)

    " Add
    if previous_line =~ '^\s*\<\(function\|begin\|switch\|case\|default\|if.\{-}then\|else\|elseif\|while\|repeat\)\>'
	let ind = ind + &sw
    endif

    " Subtract
    if this_line =~ '^\s*\<endswitch\>'
	let ind = ind - 2 * &sw
    elseif this_line =~ '^\s*\<\(begin\|end\|endif\|endwhile\|else\|elseif\|until\)\>'
	let ind = ind - &sw
    elseif this_line =~ '^\s*\<\(case\|default\)\>'
	if previous_line !~ '^\s*\<switch\>'
	    let ind = ind - &sw
	endif
    endif

    return ind
endfun
