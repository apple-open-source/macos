"  vim: set sw=4 sts=4:
"  Maintainer	: Nikolai 'pcp' Weibull <da.box@home.se>
"  URL		: http://www.pcppopper.org/
"  Revised on	: Mon, 08 Apr 2002 23:52:55 +0200
"  Language	: Makefile

" Only load this indent file when no other was loaded.
if exists("b:did_indent")
    finish
endif

let b:did_indent = 1

setlocal indentexpr=GetMakeIndent()
setlocal indentkeys=!^F,o,O

" Only define the function once.
if exists("*GetMakeIndent")
    finish
endif

function s:GetStringWidth(line, str)
    let end = matchend(a:line, a:str)
    let width = 0
    let i = 0
    while i < end
	if a:line[i] != "\t"
	    let width = width + 1
	else
	    let width = width + &ts - (width % &ts)
	endif
	let i = i + 1
    endwhile
    return width
endfunction

function GetMakeIndent()
    if v:lnum == 1
	return 0
    endif

    let ind = indent(v:lnum - 1)
    let line = getline(v:lnum - 1)

    if line == ''
	let ind = 0
    elseif line =~ '^[^ \t#:][^#:]*:\{1,2}\([^=:]\|$\)'
	let ind = ind + &ts
    elseif line =~ '^\s*\h\w*\s*=\s*.\+\\$'
	let ind = s:GetStringWidth(line, '=\s*')
    elseif line !~ '\\$'
	let ind = indent(v:lnum)
    endif

    return ind
endfunction
