"  vim: set sw=4 sts=4:
"  Maintainer	: Nikolai 'pcp' Weibull <da.box@home.se>
"  Revised on	: Wed, 25 Jul 2001 20:46:22 CEST
"  Language	: Tcl

" Only load this indent file when no other was loaded.
if exists("b:did_indent")
    finish
endif

let b:did_indent = 1

setlocal indentexpr=GetTclIndent()
setlocal indentkeys-=:,0#

" Only define the function once.
if exists("*GetTclIndent")
    finish
endif

function GetTclIndent()
    let lnum = v:lnum
    let line = getline(lnum)
    let pline = getline(lnum - 1)

    while lnum > 0 && (line =~ '^\s*#' || line =~ '^\s*$' || pline =~ '\\\s*$')
	let lnum = lnum - 1
	let line = getline(lnum)
	let pline = getline(lnum - 1)
    endwhile

    if lnum == 0
	return 0
    endif

    let ind = indent(lnum)

    " Check for opening brace on previous line
    if line =~ '{\(.*}\)\@!'
	let ind = ind + &sw
    endif

    let line = getline(v:lnum)

    " Check for closing brace on current line
    if line =~ '^\s*}'
	let ind	= ind - &sw
    endif

    return ind
endfunction
