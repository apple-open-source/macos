"  vim: set sw=4 sts=4:
"  Maintainer	: Nikolai 'pcp' Weibull <da.box@home.se>
"  Revised on	: Wed, 25 Jul 2001 21:19:41 CEST
"  Language	: Cascading Style Sheets (CSS)

" Only load this indent file when no other was loaded.
if exists("b:did_indent")
    finish
endif

let b:did_indent = 1

setlocal indentexpr=GetCSSIndent()
setlocal indentkeys-=:,0# indentkeys-=e

" Only define the function once.
if exists("*GetCSSIndent")
    finish
endif

function! s:LookupLine(lnum)
    " find a non-blank line above the current line
    let lnum = prevnonblank(a:lnum - 1)

    if lnum == 0
       return 0
    endif

    let line = getline(lnum)

    " if the line has an end comment sequence we need to find a line
    " that isn't affected by the comment.
    if line =~ '\*/'
	while line !~ '/\*'
	    let lnum = lnum - 1
	    let line = getline(lnum)
	endwhile
    endif

    " if the line we found only contained the comment and whitespace
    " we need to find another line to use...
    if line =~ '^\s*/\*'
	return s:LookupLine(lnum)
    else
	return lnum
    endif
endfunction

function GetCSSIndent()
    let lnum = s:LookupLine(v:lnum)

    if lnum == 0
	return 0
    endif

    " remove commented stuff from line
    let line = substitute(getline(lnum), '/\*.\*/', '', 'eg')

    let ind = indent(lnum)

    " check for opening brace on the previous line
    " skip if it also contains a closing brace...
    if line =~ '{\(.*}\)\@!'
       let ind = ind + &sw
    endif

    let line = getline(v:lnum)

    " check for closing brace first on current line
    if line =~ '^\s*}'
	let ind	= ind - &sw
    endif

    return ind
endfunction
