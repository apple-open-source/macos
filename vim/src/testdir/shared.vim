" Functions shared by several tests.

" Only load this script once.
if exists('*PythonProg')
  finish
endif

source view_util.vim

" When 'term' is changed some status requests may be sent.  The responses may
" interfere with what is being tested.  A short sleep is used to process any of
" those responses first.
func WaitForResponses()
  sleep 50m
endfunc

" Get the name of the Python executable.
" Also keeps it in s:python.
func PythonProg()
  " This test requires the Python command to run the test server.
  " This most likely only works on Unix and Windows.
  if has('unix')
    " We also need the job feature or the pkill command to make sure the server
    " can be stopped.
    if !(has('job') || executable('pkill'))
      return ''
    endif
    if executable('python')
      let s:python = 'python'
    elseif executable('python3')
      let s:python = 'python3'
    else
      return ''
    end
  elseif has('win32')
    " Use Python Launcher for Windows (py.exe) if available.
    " NOTE: if you get a "Python was not found" error, disable the Python
    " shortcuts in "Windows menu / Settings / Manage App Execution Aliases".
    if executable('py.exe')
      let s:python = 'py.exe'
    elseif executable('python.exe')
      let s:python = 'python.exe'
    else
      return ''
    endif
  else
    return ''
  endif
  return s:python
endfunc

" Run "cmd".  Returns the job if using a job.
func RunCommand(cmd)
  " Running an external command can occasionally be slow or fail.
  let g:test_is_flaky = 1

  let job = 0
  if has('job')
    let job = job_start(a:cmd, {"stoponexit": "hup"})
    call job_setoptions(job, {"stoponexit": "kill"})
  elseif has('win32')
    exe 'silent !start cmd /c start "test_channel" ' . a:cmd
  else
    exe 'silent !' . a:cmd . '&'
  endif
  return job
endfunc

" Read the port number from the Xportnr file.
func GetPort()
  let l = []
  " with 200 it sometimes failed, with 400 is rarily failed
  for i in range(600)
    try
      let l = readfile("Xportnr")
    catch
    endtry
    if len(l) >= 1
      break
    endif
    sleep 10m
  endfor
  call delete("Xportnr")

  if len(l) == 0
    " Can't make the connection, give up.
    return 0
  endif
  return l[0]
endfunc

" Run a Python server for "cmd" and call "testfunc".
" Always kills the server before returning.
func RunServer(cmd, testfunc, args)
  " The Python program writes the port number in Xportnr.
  call delete("Xportnr")

  if len(a:args) == 1
    let arg = ' ' . a:args[0]
  else
    let arg = ''
  endif
  let pycmd = s:python . " " . a:cmd . arg

  try
    let g:currentJob = RunCommand(pycmd)

    " Wait for some time for the port number to be there.
    let port = GetPort()
    if port == 0
      call assert_report(strftime("%H:%M:%S") .. " Can't start " .. a:cmd)
      return
    endif

    call call(function(a:testfunc), [port])
  catch /E901.*Address family for hostname not supported/
    throw 'Skipped: Invalid network setup ("' .. v:exception .. '" in ' .. v:throwpoint .. ')'
  catch
    call assert_report('Caught exception: "' . v:exception . '" in ' . v:throwpoint)
  finally
    call s:kill_server(a:cmd)
  endtry
endfunc

func s:kill_server(cmd)
  if has('job')
    if exists('g:currentJob')
      call job_stop(g:currentJob)
      unlet g:currentJob
    endif
  elseif has('win32')
    let cmd = substitute(a:cmd, ".py", '', '')
    call system('taskkill /IM ' . s:python . ' /T /F /FI "WINDOWTITLE eq ' . cmd . '"')
  else
    call system("pkill -f " . a:cmd)
  endif
endfunc

" Wait for up to five seconds for "expr" to become true.  "expr" can be a
" stringified expression to evaluate, or a funcref without arguments.
" Using a lambda works best.  Example:
"	call WaitFor({-> status == "ok"})
"
" A second argument can be used to specify a different timeout in msec.
"
" When successful the time slept is returned.
" When running into the timeout an exception is thrown, thus the function does
" not return.
func WaitFor(expr, ...)
  let timeout = get(a:000, 0, 5000)
  let slept = s:WaitForCommon(a:expr, v:null, timeout)
  if slept < 0
    throw 'WaitFor() timed out after ' . timeout . ' msec'
  endif
  return slept
endfunc

" Wait for up to five seconds for "assert" to return zero.  "assert" must be a
" (lambda) function containing one assert function.  Example:
"	call WaitForAssert({-> assert_equal("dead", job_status(job)})
"
" A second argument can be used to specify a different timeout in msec.
"
" Return zero for success, one for failure (like the assert function).
func WaitForAssert(assert, ...)
  let timeout = get(a:000, 0, 5000)
  if s:WaitForCommon(v:null, a:assert, timeout) < 0
    return 1
  endif
  return 0
endfunc

" Common implementation of WaitFor() and WaitForAssert().
" Either "expr" or "assert" is not v:null
" Return the waiting time for success, -1 for failure.
func s:WaitForCommon(expr, assert, timeout)
  " using reltime() is more accurate, but not always available
  let slept = 0
  if exists('*reltimefloat')
    let start = reltime()
  endif

  while 1
    if type(a:expr) == v:t_func
      let success = a:expr()
    elseif type(a:assert) == v:t_func
      let success = a:assert() == 0
    else
      let success = eval(a:expr)
    endif
    if success
      return slept
    endif

    if slept >= a:timeout
      break
    endif
    if type(a:assert) == v:t_func
      " Remove the error added by the assert function.
      call remove(v:errors, -1)
    endif

    sleep 10m
    if exists('*reltimefloat')
      let slept = float2nr(reltimefloat(reltime(start)) * 1000)
    else
      let slept += 10
    endif
  endwhile

  return -1  " timed out
endfunc


" Wait for up to a given milliseconds.
" With the +timers feature this waits for key-input by getchar(), Resume()
" feeds key-input and resumes process. Return time waited in milliseconds.
" Without +timers it uses simply :sleep.
func Standby(msec)
  if has('timers') && exists('*reltimefloat')
    let start = reltime()
    let g:_standby_timer = timer_start(a:msec, function('s:feedkeys'))
    call getchar()
    return float2nr(reltimefloat(reltime(start)) * 1000)
  else
    execute 'sleep ' a:msec . 'm'
    return a:msec
  endif
endfunc

func Resume()
  if exists('g:_standby_timer')
    call timer_stop(g:_standby_timer)
    call s:feedkeys(0)
    unlet g:_standby_timer
  endif
endfunc

func s:feedkeys(timer)
  call feedkeys('x', 'nt')
endfunc

" Get the name of the Vim executable that we expect has been build in the src
" directory.
func s:GetJustBuildVimExe()
  if has("win32")
    if !filereadable('..\vim.exe') && filereadable('..\vimd.exe')
      " looks like the debug executable was intentionally build, so use it
      return '..\vimd.exe'
    endif
    return '..\vim.exe'
  endif
  return '../vim'
endfunc

" Get $VIMPROG to run the Vim executable.
" The Makefile writes it as the first line in the "vimcmd" file.
" Falls back to the Vim executable in the src directory.
func GetVimProg()
  if filereadable('vimcmd')
    return readfile('vimcmd')[0]
  endif
  echo 'Cannot read the "vimcmd" file, falling back to ../vim.'

  " Probably the script was sourced instead of running "make".
  " We assume Vim was just build in the src directory then.
  return s:GetJustBuildVimExe()
endfunc

let g:valgrind_cnt = 1

" Get the command to run Vim, with -u NONE and --not-a-term arguments.
" If there is an argument use it instead of "NONE".
func GetVimCommand(...)
  if filereadable('vimcmd')
    let lines = readfile('vimcmd')
  else
    echo 'Cannot read the "vimcmd" file, falling back to ../vim.'
    let lines = [s:GetJustBuildVimExe()]
  endif

  if a:0 == 0
    let name = 'NONE'
  else
    let name = a:1
  endif
  " For Unix Makefile writes the command to use in the second line of the
  " "vimcmd" file, including environment options.
  " Other Makefiles just write the executable in the first line, so fall back
  " to that if there is no second line or it is empty.
  if len(lines) > 1 && lines[1] != ''
    let cmd = lines[1]
  else
    let cmd = lines[0]
  endif

  let cmd = substitute(cmd, '-u \f\+', '-u ' . name, '')
  if cmd !~ '-u '. name
    let cmd = cmd . ' -u ' . name
  endif
  let cmd .= ' --not-a-term'
  let cmd .= ' --gui-dialog-file guidialogfile'
  " remove any environment variables
  let cmd = substitute(cmd, '[A-Z_]\+=\S\+ *', '', 'g')

  " If using valgrind, make sure every run uses a different log file.
  if cmd =~ 'valgrind.*--log-file='
    let cmd = substitute(cmd, '--log-file=\(\S*\)', '--log-file=\1.' . g:valgrind_cnt, '')
    let g:valgrind_cnt += 1
  endif

  return cmd
endfunc

" Return one when it looks like the tests are run with valgrind, which means
" that everything is much slower.
func RunningWithValgrind()
  return GetVimCommand() =~ '\<valgrind\>'
endfunc

" Get the command to run Vim, with --clean instead of "-u NONE".
func GetVimCommandClean()
  let cmd = GetVimCommand()
  let cmd = substitute(cmd, '-u NONE', '--clean', '')
  let cmd = substitute(cmd, '--not-a-term', '', '')

  " Force using utf-8, Vim may pick up something else from the environment.
  let cmd ..= ' --cmd "set enc=utf8" '

  " Optionally run Vim under valgrind
  " let cmd = 'valgrind --tool=memcheck --leak-check=yes --num-callers=25 --log-file=valgrind ' . cmd

  return cmd
endfunc

" Get the command to run Vim, with --clean, and force to run in terminal so it
" won't start a new GUI.
func GetVimCommandCleanTerm()
  " Add -v to have gvim run in the terminal (if possible)
  return GetVimCommandClean() .. ' -v '
endfunc

" Run Vim, using the "vimcmd" file and "-u NORC".
" "before" is a list of Vim commands to be executed before loading plugins.
" "after" is a list of Vim commands to be executed after loading plugins.
" Plugins are not loaded, unless 'loadplugins' is set in "before".
" Return 1 if Vim could be executed.
func RunVim(before, after, arguments)
  return RunVimPiped(a:before, a:after, a:arguments, '')
endfunc

func RunVimPiped(before, after, arguments, pipecmd)
  let cmd = GetVimCommand()
  let args = ''
  if len(a:before) > 0
    call writefile(a:before, 'Xbefore.vim')
    let args .= ' --cmd "so Xbefore.vim"'
  endif
  if len(a:after) > 0
    call writefile(a:after, 'Xafter.vim')
    let args .= ' -S Xafter.vim'
  endif

  " Optionally run Vim under valgrind
  " let cmd = 'valgrind --tool=memcheck --leak-check=yes --num-callers=25 --log-file=valgrind ' . cmd

  exe "silent !" .. a:pipecmd .. ' ' ..  cmd .. args .. ' ' .. a:arguments

  if len(a:before) > 0
    call delete('Xbefore.vim')
  endif
  if len(a:after) > 0
    call delete('Xafter.vim')
  endif
  return 1
endfunc

func IsRoot()
  if !has('unix')
    return v:false
  elseif $USER == 'root' || system('id -un') =~ '\<root\>'
    return v:true
  endif
  return v:false
endfunc

" Get all messages but drop the maintainer entry.
func GetMessages()
  redir => result
  redraw | messages
  redir END
  let msg_list = split(result, "\n")
  if msg_list->len() > 0 && msg_list[0] =~ 'Messages maintainer:'
    return msg_list[1:]
  endif
  return msg_list
endfunc

" Run the list of commands in 'cmds' and look for 'errstr' in exception.
" Note that assert_fails() cannot be used in some places and this function
" can be used.
func AssertException(cmds, errstr)
  let save_exception = ''
  try
    for cmd in a:cmds
      exe cmd
    endfor
  catch
    let save_exception = v:exception
  endtry
  call assert_match(a:errstr, save_exception)
endfunc

" vim: shiftwidth=2 sts=2 expandtab
