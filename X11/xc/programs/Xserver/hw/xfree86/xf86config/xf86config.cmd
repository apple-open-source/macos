/* OS/2 REXX */
/* $XFree86: xc/programs/Xserver/hw/xfree86/xf86config/xf86config.cmd,v 1.1 2000/04/05 18:14:01 dawes Exp $ */

call RxFuncAdd 'SysCls','RexxUtil','SysCls'

env = 'OS2ENVIRONMENT'
home= VALUE('HOME',,env)
bootdrv= LEFT(VALUE('SYSTEM_INI',,env),2)
x11root= TRANSLATE(VALUE('X11ROOT',,env),'\','/')
editor= VALUE('EDITOR',,env)
if editor = '' then
  editor = 'epm'

call SysCls
if \(exists(home'\XF86Config.new')) then
do 
  say "This script will create a default XF86Config file. This is"
  say "done by running the XFree86-4.0 server's configuration feature."
  say " "

  call configx86
end

do until cmd=7
  call SysCls
  say "XFree86 created a default config file at "home"\XF86Config.new."
  say " "
  say "You may now either:"
  say "  1. View the logfile"
  say "  2. Start the graphical configuration tool (alpha version)"
  say "  3. Start the X server with the default file"
  say "  4. Edit the config file manually, using the "editor" editor"
  say "  5. Copy the config file to the standard location"
  say "  6. Re-run X Server for configuration"
  say "  7. Exit this program."
  parse pull cmd
  select 
  when cmd=1 then do
    call show_file
  end
  when cmd=2 then do
    "xinit xf86cfg -xf86config "home"\XF86Config.new -- :0"
  end
  when cmd=3 then do
    "xfree86 -xf86config "home"\XF86Config.new"
  end
  when cmd=4 then do
    editor" "home"\XF86Config.new"
  end
  when cmd=5 then do
    say "Copying file to "x11root"\lib\X11\XF86Config"
    "copy "home"\XF86Config.new "x11root"\XFree86\lib\X11\XF86Config"
  end
  when cmd=6 then do
    call configx86
  end
  when cmd=7 then do
    say "Exiting from xf86config"
    exit
  end
  otherwise
    say "***ERROR*** Please enter a number from 1..7!"
    say " "
  end
end
exit

configx86:
  say "ATTENTION! If in the following the screen becomes blank or does not"
  say "change for a minute (be patient), something went wrong -"
  say "please reboot then (CTRL-ALT-DEL)"
  say ""
  say "Press RETURN to start the configuration process"
  pull input
  'xfree86 -configure'
  return

show_file:
  file=bootdrv"\xf86log.os2"
  i=0
  call SysCls
  do until stream(file,s)="NOTREADY"
    say linein(file)
    i = i+1
    if i = 20 then do
      say " "
      say "Press Return for next page"
      pull input
      i = 0
      call SysCls
    end
  end
  return

/* returns 1, if file exists */
exists:
  'DIR "'arg(1)'" >nul 2>&1'
  IF rc = 0 THEN return 1
  RETURN 0
