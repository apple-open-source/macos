/* OS/2 REXX */
/* $XFree86: xc/programs/xinit/xinitrc.cmd,v 3.5 1997/01/27 08:26:14 dawes Exp $ */
'@echo off'
env = 'OS2ENVIRONMENT'
x11root = VALUE('X11ROOT',,env)
IF x11root = '' THEN DO
	SAY "The environment variable X11ROOT is not set. XFree86/OS2 won't run without it."
	EXIT
END
home = VALUE('HOME',,env)
IF home = '' THEN home = x11root

userresources = home'\.Xresources'
usermodmap    = home'\.Xmodmap'
sysresources  = x11root'\XFree86\lib\X11\xinit\.Xresources'
sysmodmap     = x11root'\XFree86\lib\X11\xinit\.Xmodmap'
xbitmapdir    = x11root'\XFree86\include\X11\bitmaps'
manpath       = VALUE('MANPATH',,env)

/* merge in defaults */
IF exists(sysresources) THEN
	'xrdb -merge 'sysresources

IF exists(sysmodmap) THEN
	'xmodmap 'sysmodmap

IF exists(userresources) THEN
	'xrdb -merge 'userresources

IF exists(usermodmap) THEN
	'xmodmap 'usermodmap

/* start some nice :-) test programs */
'xsetroot -bitmap 'xbitmapdir'\xos2'
/* also try out the following ones: 
 * 'xsetroot -bitmap 'xbitmapdir'\xfree1'
 * 'xsetroot -bitmap 'xbitmapdir'\xfree2'
 */

/****** WARNING! *********
 * Below some programs are started minimized, some are started detached.
 * In general, those that spawn other shells are minimized, others may be
 * detached. You might be tempted to run the xterm's as well as detached.
 * This works, but leaves you with an independent xterm/cmd pair, when the 
 * server shuts down, which you can only see in watchcat, not the process list.
 * If you start and stop x11 multiple times, this will let you run out of
 * PTYs, and will lead to a large number of background sessions.
 */
'detach xclock -update 1 -geometry 100x100-1+1'
'start/min/n "Login Xterm" xterm -sb -geometry 80x25+0+0 -name login'
IF manpath \= '' THEN
	'detach xman -geometry 100x100-105+1'
/* 'startx/min/n "Xterm 1" xterm -sb -geometry 80x50+494+51' */
/* 'startx/min/n "Xterm 2" xterm -sb -geometry 80x20+494-0' */
'twm'

EXIT

exists:
'DIR "'arg(1)'" >nul 2>&1'
if rc = 0 THEN RETURN 1
RETURN 0
