"  vim: set sw=4 sts=4:
"  Maintainer	: Nikolai 'pcp' Weibull <da.box@home.se>
"  Revised on	: Tue, 24 Jul 2001 18:45:00 CEST
"  Language	: Eterm configuration file

" Only load this indent file when no other was loaded.
if exists("b:did_indent")
    finish
endif

let b:did_indent = 1

setlocal indentexpr=GetEtermIndent()
setlocal indentkeys=!^F,o,O,=end

" Only define the function once.
if exists("*GetEtermIndent")
    finish
endif

function GetEtermIndent()
    " Find a non-blank line above the current line.
    let lnum = prevnonblank(v:lnum - 1)

    " Hit the start of the file, use zero indent.
    if lnum == 0
       return 0
    endif

    let line	= getline(lnum)
    let ind	= indent(lnum)

    if line =~ '^\s*begin\>'
	let ind	= ind + &sw
    endif

    let line	= getline(v:lnum)

    " Check for closing brace on current line
    if line =~ '^\s*end\>'
	let ind	= ind - &sw
    endif

    return ind
endfunction
