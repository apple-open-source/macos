" netrw.vim: (global plugin) Handles file transfer across a network
" Last Change:	May 02, 2003
" Maintainer:	Charles E. Campbell, Jr. PhD   <cec@NgrOyphSon.gPsfAc.nMasa.gov>
" Version:	31

" Credits:
"  Vim editor   by Bram Moolenaar (Thanks, Bram!)
"  rcp, ftp support by C Campbell <cec@NgrOyphSon.gPsfAc.nMasa.gov>
"  fetch    support by Bram Moolenaar and C Campbell
"  scp	    support by raf	      <raf@comdyn.com.au>
"  http     support by Bram Moolenaar <bram@moolenaar.net>
"  dav	    support by C Campbell
"  rsync    support by C Campbell (suggested by Erik Warendorph)
"  inputsecret(), BufReadCmd, BufWriteCmd contributed by C Campbell
"
"     Jérôme Augé	 -- also using new buffer method with ftp+.netrc
"     Bram Moolenaar	 -- obviously vim itself, plus :e and v:cmdarg use
"     Yasuhiro Matsumoto -- pointing out undo+0r problem and a solution
"     Erik Warendorph	 -- for several suggestions (g:netrw_..._cmd
"			    variables, rsync etc)

" Debugging:
"	If you'd like to try the built-in debugging commands...
""		:g/DBG/s/^"//		to activate	debugging
""		:g/DBG/s/^/"/		to de-activate	debugging
""	You'll need to get <Decho.vim> and put it into your <.vim/plugin>
""	(or <vimfiles\plugin> for Windows).  Its available at
""	http://www.erols.com/astronaut/vim/vimscript/Decho.vim

" Options:
"	let g:netrw_ftp =0 use ftp (default)		     (uid password)
"			=1 use alternate ftp method	(user uid password)
"	  If you're having trouble with ftp, try changing the value
"	  of this variable in your <.vimrc> to change methods
"
"	let g:netrw_ignorenetrc= 1
"	  If you have a <.netrc> file but it doesn't work and you
"	  want it ignored, then set this variable as shown.  Its mere
"	  existence is enough to cause <.netrc> to be ignored.
"
"	User Function NetReadFixup(method, line1, line2)
"	  If your ftp has an obnoxious habit of prepending/appending
"	  lines to stuff it reads (for example, one chap had a misconfigured
"	  ftp with kerberos which kept complaining with AUTH and KERBEROS
"	  messages) you may write your own function NetReadFixup to fix
"	  up the file.	To help with writing a NetReadFixup function,
"	  some information has been provided:
"
"	     line1 = first new line in current file
"	     line2 = last  new line in current file
"	     method= 1 = rcp
"		     2 = ftp+.netrc
"		     3 = ftp
"		     4 = scp
"		     5 = wget
"		     6 = cadaver
"		     7 = rsync
"
"
"	Controlling External Applications
"
"	 Protocol  Variable	       Default Value
"	 --------  ----------------    -------------
"	   rcp:    g:netrw_rcp_cmd   = "rcp"
"	   ftp:    g:netrw_ftp_cmd   = "ftp"
"	   scp:    g:netrw_scp_cmd   = "scp -q"
"	   http:   g:netrw_http_cmd  = "wget -O"
"	   dav:    g:netrw_dav_cmd   = "cadaver"
"	   rsync:  g:netrw_rsync_cmd = "rsync -a"
"	   ftp:    g:netrw_fetch_cmd = ""   (if its not "" it will be used
"	   http:			     to read files via ftp and http:)


" Reading:
" :Nread ?					give help
" :Nread "machine:file"				uses rcp
" :Nread "machine file"				uses ftp   with <.netrc>
" :Nread "machine id password file"		uses ftp
" :Nread "ftp://[user@]machine[[:#]port]/file"	uses ftp   autodetects <.netrc>
" :Nread "http://[user@]machine/file"		uses http  uses wget
" :Nread "rcp://[user@]machine/file"		uses rcp
" :Nread "scp://[user@]machine/file"		uses scp
" :Nread "dav://machine[:port]/file"		uses cadaver
" :Nread "rsync://[user@]machine[:port]/file"	uses rsync

" Writing:
" :Nwrite ?					give help
" :Nwrite "machine:file"			uses rcp
" :Nwrite "machine file"			uses ftp   with <.netrc>
" :Nwrite "machine id password file"		uses ftp
" :Nwrite "ftp://[user@]machine[[:#]port]/file"	uses ftp   autodetects <.netrc>
" :Nwrite "rcp://[user@]machine/file"		uses rcp
" :Nwrite "scp://[user@]machine/file"		uses scp
" :Nwrite "dav://machine[:port]/file"		uses cadaver
" :Nwrite "rsync://[user@]machine[:port]/file"	uses rsync
" http: not supported!

" User And Password Changing:
"  Attempts to use ftp will prompt you for a user-id and a password.
"  These will be saved in g:netrw_uid and g:netrw_passwd
"  Subsequent uses of ftp will re-use those.  If you need to use
"  a different user id and/or password, you'll want to
"  call NetUserPass() first.

"	:NetUserPass [uid [password]]		-- prompts as needed
"	:call NetUserPass()			-- prompts for uid and password
"	:call NetUserPass("uid")		-- prompts for password
"	:call NetUserPass("uid","password")	-- sets global uid and password

" Variables:
"	b:netrw_lastfile last file Network-read/written retained on
"			  a per-buffer basis		(supports plain :Nw )
"	b:netrw_line	  during Nw/NetWrite, holds current line   number
"	b:netrw_col	  during Nw/NetWrite, holds current column number
"			  b:netrw_line and b:netrw_col are used to
"			  restore the cursor position on writes
"	g:netrw_ftp	  if it doesn't exist, use default ftp
"			  =0 use default ftp		       (uid password)
"			  =1 use alternate ftp method	  (user uid password)
"	g:netrw_ftpmode   ="binary"				    (default)
"			  ="ascii"			     (or your choice)
"	g:netrw_uid	  (ftp) user-id,      retained on a per-session basis
"	g:netrw_passwd	  (ftp) password,     retained on a per-session basis
"	g:netrw_win95ftp  =0 use unix-style ftp even if win95/win98/winME
"			  =1 use default method to do ftp
"	g:netrw_cygwin	  =1 assume scp under windows is from cygwin
"							 (default if windows)
"			  =0 assume scp under windows accepts
"			    windows-style paths		 (default otherwise)
"	g:netrw_use_nt_rcp=0 don't use the rcp of WinNT, Win2000 and WinXP (default)
"			  =1 use the rcp of WinNT,... in binary mode
"
"  But be doers of the word, and not only hearers, deluding your own selves
"  (James 1:22 RSV)
" =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

" Exit quickly when already loaded or when 'compatible' is set.
if exists("loaded_netrw") || &cp
  finish
endif
let loaded_netrw = "v25"
let s:save_cpo = &cpo
set cpo&vim

" Default values for global netrw variables
if !exists("g:netrw_ftpmode")
 let g:netrw_ftpmode= "binary"
endif
if !exists("g:netrw_win95ftp")
 let g:netrw_win95ftp= 1
endif
if !exists("g:netrw_cygwin")
 if has("win32")
  let g:netrw_cygwin= 1
 else
  let g:netrw_cygwin= 0
 endif
endif

" Default commands(+options) for associated
" protocols.  Users may override these in
" their <.vimrc> by simply defining them
" to be whatever they wish.
if !exists("g:netrw_rcp_cmd")
  let g:netrw_rcp_cmd   = "rcp"
endif
if !exists("g:netrw_ftp_cmd")
  let g:netrw_ftp_cmd   = "ftp"
endif
if !exists("g:netrw_scp_cmd")
  let g:netrw_scp_cmd   = "scp -q"
endif
if !exists("g:netrw_http_cmd")
  let g:netrw_http_cmd  = "wget -O"
endif
if !exists("g:netrw_dav_cmd")
  let g:netrw_dav_cmd   = "cadaver"
endif
if !exists("g:netrw_rsync_cmd")
  let g:netrw_rsync_cmd = "rsync -a"
endif
if !exists("g:netrw_fetch_cmd")
  if executable("fetch") > 0
    let g:netrw_fetch_cmd = "fetch -o"
  else
    let g:netrw_fetch_cmd = ""
  endif
endif

if has("win32")
  \ && exists("g:netrw_use_nt_rcp")
  \ && g:netrw_use_nt_rcp
  \ && executable( $SystemRoot .'/system32/rcp.exe')
 let s:netrw_has_nt_rcp = 1
 let s:netrw_rcpmode = '-b'
else
 let s:netrw_has_nt_rcp = 0
 let s:netrw_rcpmode = ''
endif

" Transparency Support:
" Auto-detection for ftp://*, rcp://*, scp://*, http://*, dav://*, and rsync://*
" Should make file transfers across networks transparent.  Currently I haven't
" supported appends.  Hey, gotta leave something for <netrw.vim> version 3!
if version >= 600
 augroup Network
  au!
  au BufReadCmd  ftp://*,rcp://*,scp://*,http://*,dav://*,rsync://* exe "Nread 0r ".expand("<afile>")|exe "doau BufReadPost ".expand("<afile>")
  au FileReadCmd ftp://*,rcp://*,scp://*,http://*,dav://*,rsync://* exe "Nread "   .expand("<afile>")|exe "doau BufReadPost ".expand("<afile>")
  au BufWriteCmd ftp://*,rcp://*,scp://*,dav://*,rsync://*	   exe "Nwrite "  .expand("<afile>")
 augroup END
endif

" ------------------------------------------------------------------------

" Commands: :Nread, :Nwrite, and :NetUserPass
:com -nargs=* Nread	      call s:NetRead(<f-args>)
:com -range=% -nargs=* Nwrite silent <line1>,<line2>call s:NetWrite(<f-args>)
:com -nargs=* NetUserPass     call NetUserPass(<f-args>)

" ------------------------------------------------------------------------

" NetSavePosn: saves position of cursor on screen so NetWrite can restore it
function! s:NetSavePosn()
  " Save current line and column
  let b:netrw_line = line(".")
  let b:netrw_col  = col(".") - 1

  " Save top-of-screen line
  norm! H
  let b:netrw_hline= line(".")

  call s:NetRestorePosn()
endfunction

" ------------------------------------------------------------------------

" NetRestorePosn:
fu! s:NetRestorePosn()

  " restore top-of-screen line
  exe "norm! ".b:netrw_hline."G0z\<CR>"

  " restore position
  if b:netrw_col == 0
   exe "norm! ".b:netrw_line."G0"
  else
   exe "norm! ".b:netrw_line."G0".b:netrw_col."l"
  endif
endfunction

" ------------------------------------------------------------------------

" NetRead: responsible for reading a file over the net
function! s:NetRead(...)
" call Decho("DBG: NetRead(a:1<".a:1.">) {")

 " save options
 call s:NetOptionSave()

 " get name of a temporary file
 let tmpfile= tempname()

 " Special Exception: if a file is named "0r", then
 "		      "0r" will be used to read the
 "		      following files instead of "r"
 if	a:0 == 0
  let readcmd= "r"
  let ichoice= 0
 elseif a:1 == "0r"
  let readcmd = "0r"
  let ichoice = 2
 else
  let readcmd = "r"
  let ichoice = 1
 endif

 while ichoice <= a:0

  " attempt to repeat with previous host-file-etc
  if exists("b:netrw_lastfile") && a:0 == 0
"   call Decho("DBG: using b:netrw_lastfile<" . b:netrw_lastfile . ">")
   let choice = b:netrw_lastfile
   let ichoice= ichoice + 1

  else
   exe "let choice= a:" . ichoice
"   call Decho("DBG: NetRead1: choice<" . choice . ">")

   " Reconstruct Choice if choice starts with '"'
   if match(choice,"?") == 0
    echo 'NetRead Usage:'
    echo ':Nread machine:path                      uses rcp'
    echo ':Nread "machine path"                    uses ftp   with <.netrc>'
    echo ':Nread "machine id password path"        uses ftp'
    echo ':Nread ftp://[user@]machine[:port]/path  uses ftp   autodetects <.netrc>'
    echo ':Nread http://[user@]machine/path        uses http  wget'
    echo ':Nread rcp://[user@]machine/path         uses rcp'
    echo ':Nread scp://[user@]machine/path         uses scp'
    echo ':Nread dav://machine[:port]/path         uses cadaver'
    echo ':Nread rsync://machine[:port]/path       uses rsync'
    break
   elseif match(choice,"^\"") != -1
"    call Decho("DBG: reconstructing choice")
    if match(choice,"\"$") != -1
     " case "..."
     let choice=strpart(choice,1,strlen(choice)-2)
    else
      "  case "... ... ..."
     let choice      = strpart(choice,1,strlen(choice)-1)
     let wholechoice = ""

     while match(choice,"\"$") == -1
      let wholechoice = wholechoice . " " . choice
      let ichoice     = ichoice + 1
      if ichoice > a:0
       echoerr "Unbalanced string in filename '". wholechoice ."'"
       return
      endif
      let choice= a:{ichoice}
     endwhile
     let choice= strpart(wholechoice,1,strlen(wholechoice)-1) . " " . strpart(choice,0,strlen(choice)-1)
    endif
   endif
  endif
"  call Decho("DBG: NetRead2: choice<" . choice . ">")
  let ichoice= ichoice + 1

  " fix up windows urls
  if has("win32")
   let choice = substitute(choice,'\\','/','ge')
"   call Decho("DBG: fixing up windows url to <".choice.">")
   exe 'lcd ' . fnamemodify(tmpfile,':h')
   let tmpfile = fnamemodify(tmpfile,':t')
  endif

  " Determine method of read (ftp, rcp, etc)
  call s:NetMethod(choice)

  " ============
  " Perform Read
  " ============

  ".........................................
  " rcp:  Method #1
  if  b:netrw_method == 1 " read with rcp
"   call Decho("DBG:read via rcp (method #1)")
  " ER: noting done with g:netrw_uid yet?
  " ER: on Win2K" rcp machine[.user]:file tmpfile
  " ER: if machine contains '.' adding .user is required (use $USERNAME)
  " ER: the tmpfile is full path: rcp sees C:\... as host C
  if s:netrw_has_nt_rcp == 1
   if exists("g:netrw_uid") &&  ( g:netrw_uid != "" )
    let uid_machine = g:netrw_machine .'.'. g:netrw_uid
   else
    " Any way needed it machine contains a '.'
    let uid_machine = g:netrw_machine .'.'. $USERNAME
   endif
  else
   if exists("g:netrw_uid") &&  ( g:netrw_uid != "" )
    let uid_machine = g:netrw_uid .'@'. g:netrw_machine
   else
    let uid_machine = g:netrw_machine
   endif
  endif
  exe "!".g:netrw_rcp_cmd." ".s:netrw_rcpmode." ".uid_machine.":".escape(b:netrw_fname,' ?&')." ".tmpfile
   let result		= s:NetGetFile(readcmd, tmpfile, b:netrw_method)
   let b:netrw_lastfile = choice

  ".........................................
  " ftp + <.netrc>:  Method #2
  elseif b:netrw_method  == 2		" read with ftp + <.netrc>
   if g:netrw_fetch_cmd != ""
"    call Decho("DBG: read via fetch for ftp+.netrc (method #2)")
    exe "!".g:netrw_fetch_cmd." ".tmpfile." ftp://".g:netrw_machine."/".escape(b:netrw_fname,' ?&')
   else
"    call Decho("DBG: read via ftp+.netrc (method #2)")
    let netrw_fname= b:netrw_fname
    new
    set ff=unix
    exe "put ='".g:netrw_ftpmode."'"
    exe "put ='get ".netrw_fname." ".tmpfile."'"
    if exists("g:netrw_port") && g:netrw_port != ""
     exe "%!".g:netrw_ftp_cmd." -i ".g:netrw_machine." ".g:netrw_port
    else
     exe "%!".g:netrw_ftp_cmd." -i ".g:netrw_machine
    endif
    bd!
   endif
   let result = s:NetGetFile(readcmd, tmpfile, b:netrw_method)
   let b:netrw_lastfile = choice

  ".........................................
  " ftp + machine,id,passwd,filename:  Method #3
  elseif b:netrw_method == 3		" read with ftp + machine, id, passwd, and fname
   if g:netrw_fetch_cmd != ""
"   call Decho("DBG: read via fetch for ftp+mipf (method #3)")
    exe "!".g:netrw_fetch_cmd." ".tmpfile." ftp://".g:netrw_machine."/".escape(b:netrw_fname,' ?&')
   else
    " Construct execution string (four lines) which will be passed through filter
"   call Decho("DBG: read via ftp+mipf (method #3)")
    let netrw_fname= b:netrw_fname
    new
    set ff=unix
    if exists("g:netrw_port") && g:netrw_port != ""
     put ='open '.g:netrw_machine.' '.g:netrw_port
    else
     put ='open '.g:netrw_machine
    endif

    if exists("g:netrw_ftp") && g:netrw_ftp == 1
     put =g:netrw_uid
     put =g:netrw_passwd
    else
     put ='user '.g:netrw_uid.' '.g:netrw_passwd
    endif

    if exists("g:netrw_ftpmode") && g:netrw_ftpmode != ""
     put =g:netrw_ftpmode
    endif
    put ='get '.netrw_fname.' '.tmpfile

    " perform ftp:
    " -i       : turns off interactive prompting from ftp
    " -n  unix : DON'T use <.netrc>, even though it exists
    " -n  win32: quit being obnoxious about password
"    call Decho('DBG: performing ftp -i -n')
    norm 1Gdd
    silent exe "%!".g:netrw_ftp_cmd." -i -n"
    bd!
   endif
   let result		= s:NetGetFile(readcmd, tmpfile, b:netrw_method)
   let b:netrw_lastfile = choice

  ".........................................
  " scp: Method #4
  elseif     b:netrw_method  == 4	" read with scp
"   call Decho("DBG: read via scp (method #4)")
   if g:netrw_cygwin == 1
    let cygtmpfile=substitute(tmpfile,'^\(\a\):','/cygdrive/\1/','e')
    exe "!".g:netrw_scp_cmd." ".g:netrw_machine.":".escape(b:netrw_fname,' ?&')." ".cygtmpfile
   else
    exe "!".g:netrw_scp_cmd." ".g:netrw_machine.":".escape(b:netrw_fname,' ?&')." ".tmpfile
   endif
   let result		= s:NetGetFile(readcmd, tmpfile, b:netrw_method)
   let b:netrw_lastfile = choice

  ".........................................
  elseif     b:netrw_method  == 5	" read with http (wget)
"   call Decho("DBG: read via http (method #5)")
   if g:netrw_fetch_cmd != ""
    exe "!".g:netrw_fetch_cmd." ".tmpfile." http://".g:netrw_machine.":".escape(b:netrw_fname,' ?&')
    let result = s:NetGetFile(readcmd, tmpfile, b:netrw_method)

   elseif match(b:netrw_fname,"#") == -1
    exe "!".g:netrw_http_cmd." ".tmpfile." http://".g:netrw_machine.escape(b:netrw_fname,' ?&')
    let result = s:NetGetFile(readcmd, tmpfile, b:netrw_method)

   else
    let netrw_html= substitute(b:netrw_fname,"#.*$","","")
    let netrw_tag = substitute(b:netrw_fname,"^.*#","","")
"	call Decho("DBG: netrw_html<".netrw_html.">")
"	call Decho("DBG: netrw_tag <".netrw_tag.">")
    exe "!".g:netrw_http_cmd." ".tmpfile." http://".g:netrw_machine.netrw_html
    let result = s:NetGetFile(readcmd, tmpfile, b:netrw_method)
"    call Decho('DBG: <\s*a\s*name=\s*"'.netrw_tag.'"/')
    exe 'norm! 1G/<\s*a\s*name=\s*"'.netrw_tag.'"/'."\<CR>"
   endif
   let b:netrw_lastfile = choice

  ".........................................
  " cadaver: Method #6
  elseif     b:netrw_method  == 6	" read with cadaver
"   call Decho("DBG: read via cadaver (method #6)")

   " Construct execution string (four lines) which will be passed through filter
   let netrw_fname= b:netrw_fname
   new
   set ff=unix
   if exists("g:netrw_port") && g:netrw_port != ""
    put ='open '.g:netrw_machine.' '.g:netrw_port
   else
    put ='open '.g:netrw_machine
   endif
   put ='user '.g:netrw_uid.' '.g:netrw_passwd

   if g:netrw_cygwin == 1
    let cygtmpfile=substitute(tmpfile,'^\(\a\):','/cygdrive/\1/','e')
    put ='get '.netrw_fname.' '.cygtmpfile
   else
    put ='get '.netrw_fname.' '.tmpfile
   endif

   " perform cadaver operation:
   norm 1Gdd
   silent exe "%!".g:netrw_dav_cmd
   bd!
   let result		= s:NetGetFile(readcmd, tmpfile, b:netrw_method)
   let b:netrw_lastfile = choice

  ".........................................
  " rsync: Method #7
  elseif     b:netrw_method  == 7	" read with rsync
"   call Decho("DBG: read via rsync (method #7)")
   if g:netrw_cygwin == 1
    let cygtmpfile=substitute(tmpfile,'^\(\a\):','/cygdrive/\1/','e')
    exe "!".g:netrw_rsync_cmd." ".g:netrw_machine.":".escape(b:netrw_fname,' ?&')." ".cygtmpfile
   else
    exe "!".g:netrw_rsync_cmd." ".g:netrw_machine.":".escape(b:netrw_fname,' ?&')." ".tmpfile
   endif
   let result		= s:NetGetFile(readcmd,tmpfile, b:netrw_method)
   let b:netrw_lastfile = choice

  ".........................................
  else " Complain
   echo "***warning*** unable to comply with your request<" . choice . ">"
  endif
 endwhile

 " cleanup
" call Decho("DBG NetRead: cleanup")
 if exists("b:netrw_method")
  unlet b:netrw_method
  unlet g:netrw_machine
  unlet b:netrw_fname
 endif
 call s:NetOptionRestore()

" call Decho("DBG: return NetRead }")
endfunction
" end of NetRead

" ------------------------------------------------------------------------

" NetGetFile: Function to read file "fname" with command "readcmd".
function! s:NetGetFile(readcmd, fname, method)
"  call Decho("NetGetFile(readcmd<".a:readcmd.">,fname<".a:fname."> method<".a:method.">)")

 if exists("*NetReadFixup")
  " for the use of NetReadFixup (not otherwise used internally)
  let line2= line("$")
 endif

 if &term == "win32"
  if &shell == "bash"
   let fname=a:fname
"  call Decho("(win32 && bash) fname<".fname.">")
  else
   let fname=substitute(a:fname,'/','\\\\','ge')
"  call Decho("(win32 && !bash) fname<".fname.">")
  endif
 else
  let fname= a:fname
"  call Decho("(copied) fname<".fname.">")
 endif

 " get the file, but disable undo when reading a new buffer
 if a:readcmd[0] == '0'
  let use_e_cmd = 0		" 1 when using ':edit'
  let delline = 0		" 1 when have to delete empty last line
  if line("$") == 1 && getline(1) == ""
   " Now being asked to 0r a file into an empty file.  Safe to :e it instead,
   " unless there is another window on the same buffer.
   let curbufnr = bufnr("%")
   let use_e_cmd = 1
   let delline = 1
   " Loop over all windows, reset use_e_cmd when another one is editing the
   " current buffer.
   let i = 1
   while 1
     if i != winnr() && winbufnr(i) == curbufnr
       let use_e_cmd = 0
       break
     endif
     let i = i + 1
     if winbufnr(i) < 0
       break
     endif
   endwhile
  endif
  if use_e_cmd > 0
   " ':edit' the temp file, wipe out the old buffer and rename the buffer
   let curfilename = expand("%")
   exe "e!".v:cmdarg." ".fname
   exe curbufnr . "bwipe"
   exe "f ".curfilename
  else
   let oldul= &ul
   set ul=-1
   exe a:readcmd.v:cmdarg . " " . fname
   if delline > 0
     $del
   endif
   let &ul= oldul
  endif
 else
  exe a:readcmd.v:cmdarg . " " . fname
 endif

 " User-provided (ie. optional) fix-it-up command
 if exists("*NetReadFixup")
  let line1= line(".")
  if a:readcmd == "r"
   let line2= line("$") - line2 + line1
  else
   let line2= line("$") - line2
  endif
"  call Decho("calling NetReadFixup(method<".a:method."> line1=".line1." line2=".line2.")")
  call NetReadFixup(a:method, line1, line2)
 endif
" call Decho("DBG: NetGetFile readcmd<".a:readcmd."> cmdarg<".v:cmdarg."> fname<".a:fname."> readable=".filereadable(a:fname))
 redraw!
 return
endfunction

" ------------------------------------------------------------------------

" NetWrite: responsible for writing a file over the net
function! s:NetWrite(...) range
" call Decho("DBG: NetWrite(a:0=".a:0.") {")

 call s:NetSavePosn()

 " option handling
 call s:NetOptionSave()

 " Get Temporary Filename
 let tmpfile= tempname()

 if a:0 == 0
  let ichoice = 0
 else
  let ichoice = 1
 endif

 " write (selected portion of) file to temporary
 exe a:firstline . "," . a:lastline . "w!" . v:cmdarg . " " . tmpfile

 while ichoice <= a:0

  " attempt to repeat with previous host-file-etc
  if exists("b:netrw_lastfile") && a:0 == 0
"   call Decho("DBG: using b:netrw_lastfile<" . b:netrw_lastfile . ">")
   let choice = b:netrw_lastfile
   let ichoice= ichoice + 1
  else
   exe "let choice= a:" . ichoice

   " Reconstruct Choice if choice starts with '"'
   if match(choice,"?") == 0
    echo 'NetWrite Usage:"'
    echo ':Nwrite machine:path                uses rcp'
    echo ':Nwrite "machine path"              uses ftp with <.netrc>'
    echo ':Nwrite "machine id password path"  uses ftp'
    echo ':Nwrite ftp://machine[#port]/path   uses ftp  (autodetects <.netrc>)'
    echo ':Nwrite rcp://machine/path          uses rcp'
    echo ':Nwrite scp://[user@]machine/path   uses scp'
    echo ':Nwrite dav://[user@]machine/path   uses cadaver'
    echo ':Nwrite rsync://[user@]machine/path uses cadaver'
    break

   elseif match(choice,"^\"") != -1
    if match(choice,"\"$") != -1
      " case "..."
     let choice=strpart(choice,1,strlen(choice)-2)
    else
     "  case "... ... ..."
     let choice      = strpart(choice,1,strlen(choice)-1)
     let wholechoice = ""

     while match(choice,"\"$") == -1
      let wholechoice= wholechoice . " " . choice
      let ichoice    = ichoice + 1
      if choice > a:0
       echoerr "Unbalanced string in filename '". wholechoice ."'"
       return
      endif
      let choice= a:{ichoice}
     endwhile
     let choice= strpart(wholechoice,1,strlen(wholechoice)-1) . " " . strpart(choice,0,strlen(choice)-1)
    endif
   endif
  endif
"  call Decho("DBG: choice<" . choice . ">")
  let ichoice= ichoice + 1

  " fix up windows urls
  if has("win32")
   let choice= substitute(choice,'\\','/','ge')
   "ER: see NetRead()
   exe 'lcd ' . fnamemodify(tmpfile,':h')
   let tmpfile = fnamemodify(tmpfile,':t')
  endif

  " Determine method of read (ftp, rcp, etc)
  call s:NetMethod(choice)

  " =============
  " Perform Write
  " =============

  ".........................................
  " rcp: Method #1
  if  b:netrw_method == 1	" write with rcp
"	Decho "DBG:write via rcp (method #1)"
   if s:netrw_has_nt_rcp == 1
    if exists("g:netrw_uid") &&  ( g:netrw_uid != "" )
     let uid_machine = g:netrw_machine .'.'. g:netrw_uid
    else
     " Any way needed it machine contains a '.'
     let uid_machine = g:netrw_machine .'.'. $USERNAME
    endif
   else
    if exists("g:netrw_uid") &&  ( g:netrw_uid != "" )
     let uid_machine = g:netrw_uid .'@'. g:netrw_machine
    else
     let uid_machine = g:netrw_machine
    endif
   endif
   exe "!".g:netrw_rcp_cmd." ".s:netrw_rcpmode." ".tmpfile." ".uid_machine.":".escape(b:netrw_fname,' ?&')
   let b:netrw_lastfile = choice

  ".........................................
  " ftp + <.netrc>: Method #2
  elseif b:netrw_method == 2	" write with ftp + <.netrc>
   let netrw_fname= b:netrw_fname
   new
   set ff=unix
   exe "put ='".g:netrw_ftpmode."'"
"   call Decho("DBG: NetWrite: put ='".g:netrw_ftpmode."'")
   exe "put ='put ".tmpfile." ".netrw_fname."'"
"   call Decho("DBG: NetWrite: put ='put ".tmpfile." ".netrw_fname."'")
   if exists("g:netrw_port") && g:netrw_port != ""
    exe "%!".g:netrw_ftp_cmd." -i ".g:netrw_machine." ".g:netrw_port
"    call Decho("DBG: NetWrite: %!".g:netrw_ftp_cmd." -i ".g:netrw_machine." ".g:netrw_port)
   else
    exe "%!".g:netrw_ftp_cmd." -i ".g:netrw_machine
"    call Decho("DBG: NetWrite: %!".g:netrw_ftp_cmd." -i ".g:netrw_machine)
   endif
   bd!
   let b:netrw_lastfile = choice

  ".........................................
  " ftp + machine, id, passwd, filename: Method #3
  elseif b:netrw_method == 3	" write with ftp + machine, id, passwd, and fname
   let netrw_fname= b:netrw_fname
   new
   set ff=unix
   if exists("g:netrw_port") && g:netrw_port != ""
    put ='open '.g:netrw_machine.' '.g:netrw_port
   else
    put ='open '.g:netrw_machine
   endif
   if exists("g:netrw_ftp") && g:netrw_ftp == 1
    put =g:netrw_uid
    put =g:netrw_passwd
   else
    put ='user '.g:netrw_uid.' '.g:netrw_passwd
   endif
   put ='put '.tmpfile.' '.netrw_fname
   " save choice/id/password for future use
   let b:netrw_lastfile = choice

   " perform ftp:
   " -i       : turns off interactive prompting from ftp
   " -n  unix : DON'T use <.netrc>, even though it exists
   " -n  win32: quit being obnoxious about password
"   call Decho('DBG: performing ftp -i -n')
   norm 1Gdd
   silent exe "%!".g:netrw_ftp_cmd." -i -n"
   bd!

  ".........................................
  " scp: Method #4
  elseif     b:netrw_method == 4	" write with scp
   if g:netrw_cygwin == 1
    let cygtmpfile=substitute(tmpfile,'^\(\a\):','/cygdrive/\1/','e')
    exe "!".g:netrw_scp_cmd." ".cygtmpfile." ".g:netrw_machine.":".escape(b:netrw_fname,' ?&')
   else
    exe "!".g:netrw_scp_cmd." ".tmpfile." ".g:netrw_machine.":".escape(b:netrw_fname,' ?&')
   endif
   let b:netrw_lastfile = choice

  ".........................................
  " http: Method #5
  elseif     b:netrw_method == 5
   echoerr "***warning*** currently <netrw.vim> does not support writing using http:"

  ".........................................
  " dav: Method #6
  elseif     b:netrw_method == 6	" write with cadaver
"   call Decho("DBG: write via cadaver (method #6)")

   " Construct execution string (four lines) which will be passed through filter
   let netrw_fname= b:netrw_fname
   new
   set ff=unix
   if exists("g:netrw_port") && g:netrw_port != ""
    put ='open '.g:netrw_machine.' '.g:netrw_port
   else
    put ='open '.g:netrw_machine
   endif
   put ='user '.g:netrw_uid.' '.g:netrw_passwd

   if g:netrw_cygwin == 1
    let cygtmpfile=substitute(tmpfile,'^\(\a\):','/cygdrive/\1/','e')
    put ='put '.cygtmpfile.' '.netrw_fname
   else
    put ='put '.tmpfile.' '.netrw_fname
   endif

   " perform cadaver operation:
   norm 1Gdd
   silent exe "%!".g:netrw_dav_cmd
   bd!
   let b:netrw_lastfile = choice

  ".........................................
  " rsync: Method #7
  elseif     b:netrw_method == 7	" write with rsync
   if g:netrw_cygwin == 1
    let cygtmpfile=substitute(tmpfile,'^\(\a\):','/cygdrive/\1/','e')
    exe "!".g:netrw_rsync_cmd." ".cygtmpfile." ".g:netrw_machine.":".escape(b:netrw_fname,' ?&')
   else
    exe "!".g:netrw_rsync_cmd." ".tmpfile." ".g:netrw_machine.":".escape(b:netrw_fname,' ?&')
   endif
   let b:netrw_lastfile = choice

  ".........................................
  else " Complain
   echo "***warning*** unable to comply with your request<" . choice . ">"
  endif
 endwhile

 " cleanup
" call Decho("DBG: NetWrite: cleanup")
 let result=delete(tmpfile)
 call s:NetOptionRestore()

 if a:firstline == 1 && a:lastline == line("$")
  set nomod
 endif

 " restore position -- goto original top-of-screen line,
 " make it the current top-of-screen.  Then goto the
 " original line and column.
 exe "norm! ".b:netrw_hline."Gzt"
 if b:netrw_col == 0
  exe "norm! ".b:netrw_line."G0"
 else
  exe "norm! ".b:netrw_line."G0".b:netrw_col."l"
 endif

" call Decho("DBG: return NetWrite }")
endfunction
" end of NetWrite

" ------------------------------------------------------------------------

" NetMethod:  determine method of transfer
"  method == 1: rcp
"	     2: ftp + <.netrc>
"	     3: ftp + machine, id, password, and [path]filename
"	     4: scp
"	     5: http (wget)
"	     6: cadaver
"	     7: rsync
function! s:NetMethod(choice)  " globals: method machine id passwd fname
" call Decho("DBG: NetMethod(a:choice<".a:choice.">) {")

 " initialization
 let b:netrw_method  = 0
 let g:netrw_machine = ""
 let b:netrw_fname   = ""
 let g:netrw_port    = ""

 " Patterns:
 " mipf     : a:machine a:id password filename	    Use ftp
 " mf	    : a:machine filename		    Use ftp + <.netrc> or g:netrw_uid g:netrw_passwd
 " ftpurm   : ftp://[user@]host[[#:]port]/filename  Use ftp + <.netrc> or g:netrw_uid g:netrw_passwd
 " rcpurm   : rcp://[user@]host/filename	    Use rcp
 " rcphf    : [user@]host:filename		    Use rcp
 " scpurm   : scp://[user@]host/filename	    Use scp
 " httpurm  : http://[user@]host/filename	    Use wget
 " davurm   : dav://host[:port]/path		    Use cadaver
 " rsyncurm : rsync://host[:port]/path		    Use rsync
 let mipf     = '\(\S\+\)\s\+\(\S\+\)\s\+\(\S\+\)\s\+\(\S\+\)'
 let mf       = '\(\S\+\)\s\+\(\S\+\)'
 let ftpurm   = 'ftp://\([^/@]@\)\=\([^/#:]\{-}\)\([#:]\d\+\)\=/\(.*\)$'
 let rcpurm   = 'rcp://\([^/@]@\)\=\([^/]\{-}\)/\(.*\)$'
 let rcphf    = '\([^@]\{-}@\)\=\(\I\i*\):\(\S\+\)'
 let scpurm   = 'scp://\([^/]\{-}\)/\(.*\)$'
 let httpurm  = 'http://\([^/]\{-}\)\(/.*\)\=$'
 let davurm   = 'dav://\([^/]\{-}\)/\(.*\)\=$'
 let rsyncurm = 'rsync://\([^/]\{-}\)/\(.*\)\=$'

 " Determine Method
 " rcp://user@hostname/...path-to-file
 if match(a:choice,rcpurm) == 0
"  call Decho("DBG: NetMethod: rcp://...")
  let b:netrw_method = 1
  let userid	     = substitute(a:choice,rcpurm,'\1',"")
  let g:netrw_machine= substitute(a:choice,rcpurm,'\2',"")
  let b:netrw_fname  = substitute(a:choice,rcpurm,'\3',"")
  if userid != ""
   let g:netrw_uid= userid
  endif

 " scp://user@hostname/...path-to-file
 elseif match(a:choice,scpurm) == 0
"  call Decho("DBG: NetMethod: scp://...")
  let b:netrw_method = 4
  let g:netrw_machine= substitute(a:choice,scpurm,'\1',"")
  let b:netrw_fname  = substitute(a:choice,scpurm,'\2',"")

 " http://user@hostname/...path-to-file
 elseif match(a:choice,httpurm) == 0
"  call Decho("DBG: NetMethod: http://...")
  let b:netrw_method = 5
  let g:netrw_machine= substitute(a:choice,httpurm,'\1',"")
  let b:netrw_fname  = substitute(a:choice,httpurm,'\2',"")

 " dav://hostname[:port]/..path-to-file..
 elseif match(a:choice,davurm) == 0
  let b:netrw_method= 6
  let g:netrw_machine= substitute(a:choice,davurm,'\1',"")
  let b:netrw_fname  = substitute(a:choice,davurm,'\2',"")

 " rsync://user@hostname/...path-to-file
 elseif match(a:choice,rsyncurm) == 0
"  call Decho("DBG: NetMethod: rsync://...")
  let b:netrw_method = 7
  let g:netrw_machine= substitute(a:choice,rsyncurm,'\1',"")
  let b:netrw_fname  = substitute(a:choice,rsyncurm,'\2',"")

 " ftp://[user@]hostname[[:#]port]/...path-to-file
 elseif match(a:choice,ftpurm) == 0
"  call Decho("DBG: NetMethod: ftp://...")
  let userid	     = substitute(a:choice,ftpurm,'\1',"")
  let g:netrw_machine= substitute(a:choice,ftpurm,'\2',"")
  let g:netrw_port   = substitute(a:choice,ftpurm,'\3',"")
  let b:netrw_fname  = substitute(a:choice,ftpurm,'\4',"")
  if g:netrw_port != ""
    let g:netrw_port = substitute(g:netrw_port,"[#:]","","")
  endif
  if userid != ""
   let g:netrw_uid= userid
  endif
  if exists("g:netrw_uid") && exists("g:netrw_passwd")
   let b:netrw_method = 3
  else
   if filereadable(expand("$HOME/.netrc")) && !exists("g:netrw_ignorenetrc")
    let b:netrw_method= 2
   else
    if !exists("g:netrw_uid") || g:netrw_uid == ""
     call NetUserPass()
    elseif !exists("g:netrw_passwd") || g:netrw_passwd == ""
     call NetUserPass(g:netrw_uid)
   " else just use current g:netrw_uid and g:netrw_passwd
    endif
    let b:netrw_method= 3
   endif
  endif

 " Issue an rcp: hostname:filename"
 elseif match(a:choice,rcphf) == 0
"  call Decho("DBG: NetMethod: (rcp) host:file")
  let b:netrw_method = 1
  let userid	     = substitute(a:choice,rcphf,'\1',"")
  let g:netrw_machine= substitute(a:choice,rcphf,'\2',"")
  let b:netrw_fname  = substitute(a:choice,rcphf,'\3',"")
  if userid != ""
   let g:netrw_uid= userid
  endif
  if has("win32")
   " don't let PCs try <.netrc>
   let b:netrw_method = 3
  endif

 " Issue an ftp : "machine id password [path/]filename"
 elseif match(a:choice,mipf) == 0
"  call Decho("DBG: NetMethod: (ftp) host id pass file")
  let b:netrw_method  = 3
  let g:netrw_machine = substitute(a:choice,mipf,'\1',"")
  let g:netrw_uid     = substitute(a:choice,mipf,'\2',"")
  let g:netrw_passwd  = substitute(a:choice,mipf,'\3',"")
  let b:netrw_fname   = substitute(a:choice,mipf,'\4',"")

 " Issue an ftp: "hostname [path/]filename"
 elseif match(a:choice,mf) == 0
"  call Decho("DBG: NetMethod: (ftp) host file")
  if exists("g:netrw_uid") && exists("g:netrw_passwd")
   let b:netrw_method  = 3
   let g:netrw_machine = substitute(a:choice,mf,'\1',"")
   let b:netrw_fname   = substitute(a:choice,mf,'\2',"")

  elseif filereadable(expand("$HOME/.netrc"))
   let b:netrw_method  = 2
   let g:netrw_machine = substitute(a:choice,mf,'\1',"")
   let b:netrw_fname   = substitute(a:choice,mf,'\2',"")
  endif

 else
  echoerr "***error*** cannot determine method"
  let b:netrw_method  = -1
 endif

" call Decho("DBG: NetMethod: a:choice       <".a:choice.">")
" call Decho("DBG: NetMethod: b:netrw_method <".b:netrw_method.">")
" call Decho("DBG: NetMethod: g:netrw_machine<".g:netrw_machine.">")
" call Decho("DBG: NetMethod: g:netrw_port   <".g:netrw_port.">")
" if exists("g:netrw_uid")		"DBG Decho
"  call Decho("DBG: NetMethod: g:netrw_uid    <".g:netrw_uid.">")
" endif					"DBG Decho
" if exists("g:netrw_passwd")		"DBG Decho
"  call Decho("DBG: NetMethod: g:netrw_passwd <".g:netrw_passwd.">")
" endif					"DBG Decho
" call Decho("DBG: NetMethod: b:netrw_fname  <".b:netrw_fname.">")
" call Decho("DBG: NetMethod return }")
endfunction
" end of NetMethod

" ------------------------------------------------------------------------

" NetUserPass: set username and password for subsequent ftp transfer
"   Usage:  :call NetUserPass()			-- will prompt for userid and password
"	    :call NetUserPass("uid")		-- will prompt for password
"	    :call NetUserPass("uid","password") -- sets global userid and password
function! NetUserPass(...)

 " get/set userid
 if a:0 == 0
"  call Decho("DBG: NetUserPass(a:0<".a:0.">) {")
  if !exists("g:netrw_uid") || g:netrw_uid == ""
   " via prompt
   let g:netrw_uid= input('Enter username: ')
  endif
 else	" from command line
"  call Decho("DBG: NetUserPass(a:1<".a:1.">) {")
  let g:netrw_uid= a:1
 endif

 " get password
 if a:0 <= 1 " via prompt
"  call Decho("DBG: a:0=".a:0." case <=1:")
  let g:netrw_passwd= inputsecret("Enter Password: ")
 else " from command line
"  call Decho("DBG: a:0=".a:0." case >1: a:2<".a:2.">")
  let g:netrw_passwd=a:2
 endif
"  call Decho("DBG: return NetUserPass }")
endfunction
" end NetUserPass

" ------------------------------------------------------------------------

" NetOptionSave: save options and set to "standard" form
fu!s:NetOptionSave()
" call Decho("DBG: NetOptionSave()")
 " Get Temporary Filename
 let s:aikeep	= &ai
 let s:cinkeep	= &cin
 let s:cinokeep	= &cino
 let s:comkeep	= &com
 let s:cpokeep	= &cpo
 let s:dirkeep  = getcwd()
 let s:gdkeep   = &gd
 let s:twkeep	= &tw
 set cino =
 set com  =
 set cpo -=aA
 set nocin noai
 set tw   =0
 if has("win32") && !has("win95")
  let s:swfkeep= &swf
  set noswf
"  call Decho("DBG: setting s:swfkeep to <".&swf.">")
 endif
endfunction

" ------------------------------------------------------------------------

" NetOptionRestore: restore options
function! s:NetOptionRestore()
" call Decho("DBG: NetOptionRestore()")
 let &ai	= s:aikeep
 let &cin	= s:cinkeep
 let &cino	= s:cinokeep
 let &com	= s:comkeep
 let &cpo	= s:cpokeep
 exe "lcd ".s:dirkeep
 let &gd	= s:gdkeep
 let &tw	= s:twkeep
 if exists("s:swfkeep")
  let &swf= s:swfkeep
  unlet s:swfkeep
 endif
 unlet s:aikeep
 unlet s:cinkeep
 unlet s:cinokeep
 unlet s:comkeep
 unlet s:cpokeep
 unlet s:gdkeep
 unlet s:twkeep
 unlet s:dirkeep
endfunction

" ------------------------------------------------------------------------

" NetReadFixup: this sort of function is typically written by
"		the user to handle extra junk that their system's
"		ftp dumps into the transfer.  This function is
"		provided as an example and as a fix for a
"		Windows 95 problem: in my experience, it always
"		dumped four blank lines at the end of the transfer.
if has("win95") && g:netrw_win95ftp
 fu! NetReadFixup(method, line1, line2)
   if method == 3   " ftp (no <.netrc>)
    let fourblanklines= line2 - 3
    silent fourblanklines.",".line2."g/^\s*/d"
   endif
 endfunction
endif

" ------------------------------------------------------------------------

" Restore
let &cpo= s:save_cpo
unlet s:save_cpo
" vim:ts=8
