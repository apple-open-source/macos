"Dorai Sitaram
"Mar 4, 2000
"
"See http://www.ccs.neu.edu/~dorai/scmindent/scmindent.html
"for details

if exists("b:did_indent")
  finish
endif
let b:did_indent = 1

setlocal indentexpr=GetSchemeIndent()

func! Scheme_word_p(s)
  let x = "," . a:s . ","
  let y = match("," . &lispwords . ",", x)
  return (y != -1)
endfunc

"Some scheme-y stuff

func! Cons(a, d)
  return a:a . ',' . a:d
endfunc

func! Null(s)
  if a:s =~ '^ *$'
    return 1
  else
    return 0
  endif
endfunc

func! Car(s)
  return matchstr(a:s, '^[^,]\+')
endfunc

func! Cdr(s)
  return substitute(a:s, '^[^,]\+,', '', '')
endfunc

func! Cadr(s)
  return Car(Cdr(a:s))
endfunc

func! Cddr(s)
  return Cdr(Cdr(a:s))
endfunc

"String procs

func! String_ref(s, i)
  return strpart(a:s, a:i, 1)
endfunc

func! String_trim_blanks(s)
  return substitute(a:s, '\(^\s\+\|\s\+$\)', '', '')
endfunc

func! Num_leading_spaces(s)
  if a:s =~ '^\s*$'
    return -1
  else
    return strlen(matchstr(a:s, '^\s*'))
  endif
endfunc

"Aux fns used by indenter

func! Calc_subindent(s, i, n)
  let j = Past_next_token(a:s, a:i, a:n)
  if j == a:i
    return 1
  elseif Scheme_word_p(strpart(a:s, a:i, j-a:i))
    return 2
  elseif j == a:n
    return 1
  else
    return j - a:i + 2
  endif
endfunc

func! Past_next_token(s, i, n)
  let j = a:i
  while 1
    if j >= a:n
      return j
    else
      let c = String_ref(a:s, j)
      if c =~ "[ ()'`,;]"
        return j
      endif
    endif
    let j = j + 1
  endwhile
endfunc

"return number of indents for current line

func! GetSchemeIndent()
  normal mx
  let last_line = v:lnum
  let first_line = search("^(", 'bW') || 1

  normal `x

  let left_i = 0
  let paren_stack = ''
  let verbp = 'nil'

  if first_line == last_line
    return -1
  endif

  let curr_line = first_line

  while curr_line <= last_line
    let str = getline(curr_line)
    let leading_spaces = Num_leading_spaces(str)

    if verbp != 'nil'
      let curr_left_i = leading_spaces
    elseif Null(paren_stack)
      if left_i == 0 && leading_spaces >= 0
        let left_i = leading_spaces
      endif
      let curr_left_i = left_i
    else
      let curr_left_i = Car(paren_stack) + Cadr(paren_stack)
    endif

    if curr_line == last_line
      return curr_left_i
    endif

    let s = String_trim_blanks(str)
    let n = strlen(s)

    let i = 0
    let j = curr_left_i
    let escp = 0

    while i < n
      let c = String_ref(s, i)
      if verbp == 'comment'
      elseif escp
        let escp = 0
      elseif c == '\'
        let escp = 1
      elseif verbp == 'string'
        if c == '"'
          let verbp = 'nil'
        endif
      elseif c == ';'
        let verbp = 'comment'
      elseif c == '"'
        let verbp = 'string'
      elseif c == '('
        let paren_stack = Cons(Calc_subindent(s, i+1, n), Cons(j, paren_stack))
      elseif c == ')'
        if ! Null(paren_stack)
          let paren_stack = Cddr(paren_stack)
        else
          let left_i = 0
        endif
      endif
      let i = i + 1
      let j = j + 1
    endwhile
    let curr_line = curr_line + 1
    if verbp == 'comment'
      let verbp = 'nil'
    endif
  endwhile
endfunc
