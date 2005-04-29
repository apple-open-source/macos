                    Information for SCO OpenServer Users

                       J. Kean Johnston (jkj@sco.com)

                              14 February 2003

1.  Requirements

Before you can either compile or execute a binary distribution of XFree86,
the following conditions must be met:

   o Ensure that you are running Release 5.0.4 or later. This is required
     because OSS646 is only supported on those platforms. There are no plans
     to support XFree86 on earlier releases of OpenServer.

   o Ensure that OSS646, the ``Execution Environment Update'' package is
     installed, if appropriate.  Check the release notes for that update to
     see whether or not your current operating system requires this update.
     At least version OSS646B is required.  If you are running SCO OpenServer
     Release 5.0.7, you must have Maintenance Pack 1 installed.

   o Ensure that the "Graphics, Web and X11 Libraries" package is installed.
     At least version 1.3.2Ag is required.  You should always install the
     latest possible version of this supplement that is valid for your oper-
     ating system release.  From time to time this release is bundled with
     Maintenance Packs, so if you are running 5.0.7 or later, the latest ver-
     sion may be in the latest Maintenance Pack.  Always check the release
     notes for GWXLIBS to see if your platform requires the update.  The lat-
     est version can always be found at the SCO FTP site
     <URL:ftp://ftp.sco.com/pub/openserver5/opensrc>.

   o To compile XFree86, you must use the SCO-supported version of the GNU C
     Compiler.  It is possible that Skunkware versions of the compiler will
     work too, but this has not been tested.  The ``GNU Development System''
     is available for all releases from (and including) SCO OpenServer
     Release 5.0.5.  It is provided with the operating system in all versions
     from Release 5.0.7, although you need to run ``custom'' to install it
     from the media.  You can always download the latest latest version of
     the GNU Development System from the SCO FTP site
     <URL:ftp://ftp.sco.com/pub/openserver5/opensrc>.

   o If you are not using OSR 5.0.7 or later, you need to get an updated con-
     sole driver.  See <URL:http://www.sco.com> for details on OpenServer
     supplements.  If you can't or don't want to upgrade your console driver,
     XFree86 will still compile, but you may run into problems with some
     cards such as the Riva TNT and ATI Rage cards.  The problem with the
     console driver in 5.0.6A and earlier is that when the X server sets
     graphics mode, the driver does not set a status bit, so any text that is
     sent directly to /dev/console, such as kernel warning or notice messages
     when you access tape drives or NFS notices, will be sent to the console
     video memory.  This just happens to be slap bang in the middle of
     palette data for the Riva TNT, so you get color map corruption.  The
     updated console driver also has an improved mechanism for allocating
     video memory that XFree86 detects at compile time, and it will use it if
     it exists.  It is STRONGLY recommended that you get the console driver
     update.

2.  Compiling XFree86

Using the GNU Development System, compiling XFree86 should be fairly
straightforward.  Before attempting to compile the system though, you should
make sure that you have met all of the requirements above.  To actually start
the compilation, perform the following steps:

   o Copy the unmodified xf86site.def in xc/config/cf to host.def. Edit
     host.def and make any changes you think you need. The most useful
     options to change are HasTcl, HasTk, HasXdmAuth if you have the file
     WrapHelp.c and GccWarningOptions. Due to the nature of OpenServer's
     header files, the default options for this last setting are a bit
     aggressive, and I recommend you set this option to -Wpointer-arith.

   o Make sure that the official version of the GNU Development System is
     first in your PATH.  The official version lives in /usr/gnu/bin, and the
     Skunkware version (if any) lives in /usr/local/bin. You must ensure that
     /usr/gnu/bin appears first in your PATH.

   o Go to the top level of the source tree and execute the command CC=gcc
     make World BOOTSTRAPCFLAGS=-DSCO5 2>&1 | tee world.log.  This will do a
     full build, and send all of the build results to the file world.log.

   o If the build succeeded, install the new server by executing the command
     make install 2>&1 | tee install.log as root.  This will send the install
     results to the file install.log.

   o If you want to install the manual pages, execute the command make
     install.man 2>&1 | tee -a install.log as root.

3.  Before Running XFree86

The SCO xterm terminfo description is not compatible with the xterm in the R5
distribution.

To use a Bus/Keyboard or PS2 mouse you should configure the mouse drivers
using 'mkdev mouse'.  You may then use the OsMouse option in your XF86Config
to specify that XFree86 should use the SCO mouse drivers.  To do this, set
the Protocol to "OsMouse" in the Pointer section of your XF86Config file.
You can also use "OsMouse" for your serial mouse, especially if you are hav-
ing trouble getting your mouse to work using the XFree86 mouse drivers.

4.  Switching Consoles

XFree86 uses similar console switching keys as the SCO R4 and R5 servers.
That is, Ctrl-PrntScr takes you to the next console along from the one X is
running on.  If this is the last console it will take you to console 1. Ctrl-
Alt-FXX, where XX is a function key between F1 and F12 will switch you to the
console number assigned to that function key. F1 corresponds to tty01 (or
console 1), F2 corresponds to tty02 (or console 2) etc.

Unlike the SCO X server, the "kill me now" key is Alt+Ctrl+Backspace.  This
does not ask for confirmation, it simply kills the X server as immediately as
possible.  Use with extreme caution.  This may cause applications to termi-
nate in an unpredictable way.  You can set the DontZap option in the Server-
Flags section of your XF86Config file to disable this.

5.  Setting up Man Pages

After compiling the tree, or after installing the binary distribution you can
get man to recognise the XFree86 man pages by adding /usr/X11R6/man to the
MANPATH in /etc/default/man. The line should look similar to:

         MANPATH=/usr/man:/usr/gnu/man:/usr/X11R6/man:/usr/local/man

This allows all users to view the X man pages.  You may change your own MAN-
PATH environment variable if you do not want everyone to access the man
pages.

By default the man pages are compressed using ``compress'' to conserve space.
If you do not want to compress the man pages change CompressManPages to NO in
your ``host.def'' file.  Those using the binary distribution can use ``uncom-
press'' to uncompress the man pages.  Binary distributions contain pre-for-
matted versions of all man pages.  If you are compiling the server yourself,
you need to have the GNU Tools package installed to get groff, the GNU nroff
replacement, to format the man pages.  Use the manroff script to format the
manual pages yourself.

6.  Using SCO binaries/servers.

XFree86 will accept connections from SCO binaries (R3 upwards) and the SCO R5
server will also accept connections from XFree86 binaries. This means you may
mix and match the two if you have ODT.  For example you may still use the
Panning Motif window manager (pmwm) if you prefer.

     Generated from XFree86: xc/programs/Xserver/hw/xfree86/doc/sgml/SCO.sgml,v 3.23 dawes Exp $


$XFree86: xc/programs/Xserver/hw/xfree86/doc/README.SCO,v 3.36 2003/12/19 04:36:40 dawes Exp $
