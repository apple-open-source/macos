" Test for checking the source code style.

def s:ReportError(fname: string, lnum: number, msg: string)
  if lnum > 0
    assert_report(fname .. ' line ' .. lnum .. ': ' .. msg)
  endif
enddef

def s:PerformCheck(fname: string, pattern: string, msg: string, skip: string)
  var prev_lnum = 1
  var lnum = 1
  while (lnum > 0)
    cursor(lnum, 1)
    lnum = search(pattern, 'W', 0, 0, skip)
    if (prev_lnum == lnum)
      break
    endif
    prev_lnum = lnum
    if (lnum > 0)
      ReportError(fname, lnum, msg)
    endif
  endwhile
enddef

def Test_source_files()
  for fname in glob('../*.[ch]', 0, 1) + ['../xxd/xxd.c']
    bwipe!
    g:ignoreSwapExists = 'e'
    exe 'edit ' .. fname

    # Some files are generated files and may contain space errors.
    if fname =~ 'dlldata.c'
        || fname =~ 'if_ole.h'
        || fname =~ 'iid_ole.c'
      continue
    endif

    PerformCheck(fname, ' \t', 'space before Tab', '')

    PerformCheck(fname, '\s$', 'trailing white space', '')

    # some files don't stick to the Vim style rules
    if fname =~ 'iscygpty.c'
      continue
    endif

    var skip = 'getline(".") =~ "condition) {" || getline(".") =~ "vimglob_func" || getline(".") =~ "{\"" || getline(".") =~ "{\\d" || getline(".") =~ "{{{"'
    PerformCheck(fname, ')\s*{', 'curly after closing paren', skip)

    # Examples in comments use double quotes.
    skip = "getline('.') =~ '\"'"

    PerformCheck(fname, '}\s*else', 'curly before "else"', skip)

    PerformCheck(fname, 'else\s*{', 'curly after "else"', skip)

    PerformCheck(fname, '\<\(if\|while\|for\)(', 'missing white space after "if"/"while"/"for"', skip)
  endfor

  bwipe!
enddef

def Test_test_files()
  for fname in glob('*.vim', 0, 1)
    g:ignoreSwapExists = 'e'
    exe 'edit ' .. fname

    # some files intentionally have misplaced white space
    if fname =~ 'test_cindent.vim' || fname =~ 'test_join.vim'
      continue
    endif

    # skip files that are known to have a space before a tab
    if fname !~ 'test_comments.vim'
        && fname !~ 'test_listchars.vim'
        && fname !~ 'test_visual.vim'
      cursor(1, 1)
      var skip = 'getline(".") =~ "codestyle: ignore"'
      var lnum = search(fname =~ "test_regexp_latin" ? '[^á] \t' : ' \t', 'W', 0, 0, skip)
      ReportError('testdir/' .. fname, lnum, 'space before Tab')
    endif

    # skip files that are known to have trailing white space
    if fname !~ 'test_cmdline.vim'
            && fname !~ 'test_let.vim'
            && fname !~ 'test_tagjump.vim'
            && fname !~ 'test_vim9_cmd.vim'
      cursor(1, 1)
      var lnum = search(
          fname =~ 'test_vim9_assign.vim' ? '[^=]\s$'
          : fname =~ 'test_vim9_class.vim' ? '[^)]\s$'
          : fname =~ 'test_vim9_script.vim' ? '[^,:3]\s$'
          : fname =~ 'test_visual.vim' ? '[^/]\s$'
          : '[^\\]\s$')
      ReportError('testdir/' .. fname, lnum, 'trailing white space')
    endif
  endfor

  bwipe!
enddef

def Test_help_files()
  var lnum: number
  set nowrapscan

  for fpath in glob('../../runtime/doc/*.txt', 0, 1)
    g:ignoreSwapExists = 'e'
    exe 'edit ' .. fpath

    var fname = fnamemodify(fpath, ":t")

    # todo.txt is for developers, it's not need a strictly check
    # version*.txt is a history and large size, so it's not checked
    if fname == 'todo.txt' || fname =~ 'version.*\.txt'
      continue
    endif

    # Check for mixed tabs and spaces
    cursor(1, 1)
    while 1
      lnum = search('[^/] \t')
      if fname == 'visual.txt' && getline(lnum) =~ "STRING  \tjkl"
        || fname == 'usr_27.txt' && getline(lnum) =~ "\[^\? \t\]"
        continue
      endif
      ReportError(fpath, lnum, 'space before tab')
      if lnum == 0
        break
      endif
    endwhile

    # Check for unnecessary whitespace at the end of a line
    cursor(1, 1)
    while 1
      lnum = search('[^/~\\]\s$')
      # skip line that are known to have trailing white space
      if fname == 'map.txt' && getline(lnum) =~ "unmap @@ $"
        || fname == 'usr_12.txt' && getline(lnum) =~ "^\t/ \t$"
        || fname == 'usr_41.txt' && getline(lnum) =~ "map <F4> o#include  $"
        || fname == 'change.txt' && getline(lnum) =~ "foobar bla $"
        continue
      endif
      ReportError('testdir' .. fpath, lnum, 'trailing white space')
      if lnum == 0
        break
      endif
    endwhile

#    # TODO: Check for line over 80 columns
#    cursor(1, 1)
#    while 1
#      lnum = search('\%>80v.*$')
#      ReportError(fpath, lnum, 'line over 80 columns')
#      if lnum == 0
#        break
#      endif
#    endwhile

  endfor

  set wrapscan&vim
  bwipe!
enddef


" vim: shiftwidth=2 sts=2 expandtab nofoldenable
