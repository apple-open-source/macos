" Vim indent file
" Language:	Java
" Maintainer:	Bram Moolenaar <Bram@vim.org>
" Last Change:	2002 Oct 04

" Only load this indent file when no other was loaded.
if exists("b:did_indent")
   finish
endif
let b:did_indent = 1

" Indent Java anonymous classes correctly.
setlocal cinoptions& cinoptions+=j1

" The "extends" and "implements" lines start off with the wrong indent.
setlocal indentkeys& indentkeys+=0=extends indentkeys+=0=implements

" Set the function to do the work.
setlocal indentexpr=GetJavaIndent()

" Only define the function once.
if exists("*GetJavaIndent")
    finish
endif

function GetJavaIndent()

  " Java is just like C; use the built-in C indenting and then correct a few
  " specific cases.
  let theIndent = cindent(v:lnum)

  " find start of previous line, in case it was a continuation line
  let prev = prevnonblank(v:lnum - 1)
  while prev > 1
    if getline(prev - 1) !~ ',\s*$'
      break
    endif
    let prev = prev - 1
  endwhile

  " Try to align "throws" lines for methods and "extends" and "implements" for
  " classes.
  if getline(v:lnum) =~ '^\s*\(extends\|implements\)\>'
	\ && getline(prev) !~ '^\s*\(extends\|implements\)\>'
    let theIndent = theIndent + &sw
  endif

  " correct for continuation lines of "throws" and "implements"
  if getline(prev) =~ '^\s*\(throws\|implements\)\>.*,\s*$'
    if getline(prev) =~ '^\s*throws'
      let amount = &sw + 7	" add length of 'throws '
    else
      let amount = 11		" length of 'implements'.
    endif
    if getline(v:lnum - 1) !~ ',\s*$'
      let theIndent = theIndent - amount
      if theIndent < 0
	let theIndent = 0
      endif
    elseif prev == v:lnum - 1
      let theIndent = theIndent + amount
    endif
  elseif getline(prev) =~ '^\s*throws\>'
    let theIndent = theIndent - &sw
  endif

  " When the line starts with a }, try aligning it with the matching {,
  " skipping over "throws", "extends" and "implements" clauses.
  if getline(v:lnum) =~ '^\s*}\s*\(//.*\|/\*.*\)\=$'
    call cursor(v:lnum, 1)
    silent normal %
    let lnum = line('.')
    if lnum < v:lnum
      while lnum > 1
	if getline(lnum) !~ '^\s*\(throws\|extends\|implements\)\>'
	      \ && getline(prevnonblank(lnum - 1)) !~ ',\s*$'
	  break
	endif
	let lnum = prevnonblank(lnum - 1)
      endwhile
      return indent(lnum)
    endif
  endif

  " Below a line starting with "}" never indent more.  Needed for a method
  " below a method with an indented "throws" clause.
  " First ignore comment lines.
  let lnum = v:lnum - 1
  while lnum > 1
    let lnum = prevnonblank(lnum)
    if getline(lnum) =~ '\*/\s*$'
      while getline(lnum) !~ '/\*' && lnum > 1
	let lnum = lnum - 1
      endwhile
      if getline(lnum) =~ '^\s*/\*'
	let lnum = lnum - 1
      else
	break
      endif
    elseif getline(lnum) =~ '^\s*//'
      let lnum = lnum - 1
    else
      break
    endif
  endwhile
  if getline(lnum) =~ '^\s*}\s*\(//.*\|/\*.*\)\=$' && indent(lnum) < theIndent
    let theIndent = indent(lnum)
  endif

  return theIndent
endfunction
