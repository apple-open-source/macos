xf86Fpit.c
(and associated files).

Documentation updated by John Apfelbaum, linuxslate.com Oct 2001

***    P L E A S E     N O T E    ***
*  Due to a Hardrive failure, the version of this driver that was previously
*  on the linuxslate.com website was lost. This is a version came from a
*  directory on my development system that was marked "Works", and I belive it
*  to be the latest version I worked on (about a year ago), but I have not had
*  time to build from this source and verify this driver.
***

Supported Hardware:


	Fujitsu Stylistic LT	(Reported to work, but I have reason to belive that it does NOT.)
	Fujistu Stylistic 500   (Should Work)
	Fujistu Stylistic 1000  (Should Work)
	Fujistu Stylistic 1200  (Should Work)
	Fujistu Stylistic 2300  (Should Work)

History and Contributors:

-  Steven Lang <tiger@tyger.org> wrote a Xinput extension for the AceCad Drawing Tablet.
-  This was modified originaly by Rob Tsuk and John Apfelbaum (http://linuxslate.org/) to
   produce a working version for the Stylistic 500 and 1000 using XFree86 3.3.6.
   (This can be obtained from http://linuxslate.com)
-  Richard Miller-Smith <Richard.Miller-Smith@philips.com> Merged the code from the above
   project into the XFree86 4.0.2 Elographics driver by Patrick Lecoanet.
-  John Apfelbaum continuted the work to produce a working XFree86 4.0.x driver for the
   Stylistic 1200.  


Please visit http://linuxslate.com for the latest information.

License:

Please visit http://XFree86.org for license information. 

 * Copyright 1995, 1999 by Patrick Lecoanet, France. <lecoanet@cena.dgac.fr>
 * 
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Patrick Lecoanet not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission.  Patrick Lecoanet makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.

Installation:

THIS RELEASE IS FOR XFree86 Version 4.0.2. Hopefully it will also work
with other Version 4 systems. The source is written so that hopefully
it can be compiled under 3.3.6 - THIS HAS NOT BEEN TESTED (yet).

Copied from original xf86fpit.c readme:

1.  Install and configure Linux w/o consideration of the digitizer tablet.
2.  Get X working the way you want it. 
3.  Add or Change your appropriate startup scripts to include:

setserial /dev/ttyS3 autoconfig
setserial /dev/ttyS3 IRQ 15 baud_base 115200
(Some models may also have to specify:  port 0xfce8)


New/Different for Ver 4.0.2

4.  Copy fpit_drv.o to /usr/X11R6/lib/modules/input

5.  Add the following to your XF86Config(-4) file:

Section "InputDevice"
	Identifier	"mouse0"
	Driver		"fpit"
	Option		"Device"	"/dev/ttyS3"
	Option		"BaudRate"	"19200"
	Option		"MaximumXPosition"	"6250"
	Option		"MaximumYPosition"	"4950"
	Option		"MinimumXPosition"	"130"
	Option		"MinimumYPosition"	"0"
	Option		"InvertY"
EndSection

6. Remember to add this Input Device to the server description (Near the end of the file.)

7. Start or restart X.

8. If required adjust the Min/Max X/Y positions so that the pointer
   tracks the pen correctly.


Hints if you are having problems (Thanks to Aron Hsiao):

Problem 1:  Side switch being reported as wild button numbers
	   (like 249 instead of 3): 

Solution:  Add the following to your xinitrc: 

	xsetpointer TOUCHSCREEN
	xmodmap -e 'pointer = 1 2 3'

This should be re-stating the defaults, but Aron Hsiao agrees that it appears
to be an XFree86 4.x bug. 

Problem 2:  X Server crash during GUI startup (Particularly Gnome). 

Solution:   You must have a regular mouse defined as the default pointer
	    even if no mouse is used.  During startup, Gnome attempts to
	    set mouse acceleration for the default pointer.  Since the
	    pendrivers are absolute pointers, and acceleration is meaningless,
	    they do not take well to attempts to set it :-)

Problem 3:  Jittery cursor and undesired mouse clicks (both buttons),
            particuarly on the Stylistic 1200, and particuarly after the
	    system has warmed up.
	    
Solution:   (Not really a solution) This is a hardware problem.  Some
	    people have reported good results by modifying the CPU heatsink
	    on the Stylistic 1200.  Putting the system to sleep when not
	    actively using it extends battery life, and keeps the system from
	    getting too hot.

Bugs and Needed Work:

(See above)

X rotation (Portrait mode is not supported). -- I plan to add this soon.

Adjusting the constants in the XF86Config(-4) is teedious and requires
multiple restarts of the X Window system.  -- Somebody PLEASE write a
calibration program !





/* $XFree86: xc/programs/Xserver/hw/xfree86/input/fpit/readme.txt,v 1.2 2002/11/22 03:37:37 dawes Exp $ */
