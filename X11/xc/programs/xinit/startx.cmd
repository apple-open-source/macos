/* OS/2 REXX */
/* $XFree86: xc/programs/xinit/startx.cmd,v 3.3 1996/10/17 15:23:47 dawes Exp $
 * 
 * This is just a sample implementation of a slightly less primitive 
 * interface than xinit.  It looks for user xinitrc.cmd and xservrc.cmd
 * files, then system xinitrc.cmd and xservrc.cmd files, else lets xinit choose
 * its default.  The system xinitrc should probably do things like check
 * for Xresources files and merge them in, startup up a window manager,
 * and pop a clock and serveral xterms.
 *
 * Site administrators are STRONGLY urged to write nicer versions.
 */
'@echo off'
ADDRESS CMD
env = 'OS2ENVIRONMENT'
x11root = VALUE('X11ROOT',,env)
IF x11root = '' THEN DO
	SAY "The environment variable X11ROOT is not set. XFree86/OS2 won't run without it."
	EXIT
END

home = VALUE('HOME',,env)
IF home = '' THEN home = x11root
os_shell = VALUE('X11SHELL',,env)
IF os_shell = '' THEN os_shell = VALUE('SHELL',,env)
IF os_shell = '' THEN os_shell = VALUE('OS2_SHELL',,env)
IF os_shell = '' THEN DO
	SAY "There is no command interpreter in OS2_SHELL ???"
	EXIT
END

userclientrc = home'\xinitrc.cmd'
userserverrc = home'\xservrc.cmd'
sysclientrc  = x11root'\XFree86\lib\X11\xinit\xinitrc.cmd'
sysserverrc  = x11root'\XFree86\lib\X11\xinit\xservrc.cmd'
clientargs   = ''
serverargs   = ''

IF exists(userclientrc) THEN
	clientargs = userclientrc
ELSE 
IF exists(sysclientrc) THEN
	clientargs = sysclientrc

IF exists(userserverrc) THEN
	serverargs = userserverrc
ELSE 
IF exists(sysserverrc) THEN
	serverargs = sysserverrc

whoseargs = "client"
PARSE ARG all

DO i=1 TO WORDS(all)
	cur = WORD(all,i)
	IF \(FILESPEC('DRIVE',cur) = '') THEN DO
		IF whoseargs = "client" THEN
			clientargs = cur
		ELSE
			serverargs = cur
	END
	ELSE
	IF cur = "--" THEN 
		whoseargs = "server"
	ELSE 
	IF whoseargs = "client" THEN
		clientargs = clientargs' 'cur
	ELSE
		serverargs = serverargs' 'cur
END

xinit = x11root'\XFree86\bin\xinit'
xinit os_shell' /c 'clientargs' -- 'serverargs

RETURN

exists:
	IF STREAM(arg(1), 'C', 'QUERY EXISTS') = '' THEN
		RETURN 0
	ELSE
		RETURN 1
