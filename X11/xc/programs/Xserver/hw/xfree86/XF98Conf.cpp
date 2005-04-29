XCOMM $XFree86: xc/programs/Xserver/hw/xfree86/XF98Conf.cpp,v 1.4 2004/02/13 23:58:34 dawes Exp $
XCOMM
XCOMM Copyright (c) 1994-1998 by The XFree86 Project, Inc.
XCOMM All rights reserved.
XCOMM
XCOMM Permission is hereby granted, free of charge, to any person obtaining
XCOMM a copy of this software and associated documentation files (the
XCOMM "Software"), to deal in the Software without restriction, including
XCOMM without limitation the rights to use, copy, modify, merge, publish,
XCOMM distribute, sublicense, and/or sell copies of the Software, and to
XCOMM permit persons to whom the Software is furnished to do so, subject
XCOMM to the following conditions:
XCOMM
XCOMM   1.  Redistributions of source code must retain the above copyright
XCOMM       notice, this list of conditions, and the following disclaimer.
XCOMM
XCOMM   2.  Redistributions in binary form must reproduce the above copyright
XCOMM       notice, this list of conditions and the following disclaimer
XCOMM       in the documentation and/or other materials provided with the
XCOMM       distribution, and in the same place and form as other copyright,
XCOMM       license and disclaimer information.
XCOMM
XCOMM   3.  The end-user documentation included with the redistribution,
XCOMM       if any, must include the following acknowledgment: "This product
XCOMM       includes software developed by The XFree86 Project, Inc
XCOMM       (http://www.xfree86.org/) and its contributors", in the same
XCOMM       place and form as other third-party acknowledgments.  Alternately,
XCOMM       this acknowledgment may appear in the software itself, in the
XCOMM       same form and location as other such third-party acknowledgments.
XCOMM
XCOMM   4.  Except as contained in this notice, the name of The XFree86
XCOMM       Project, Inc shall not be used in advertising or otherwise to
XCOMM       promote the sale, use or other dealings in this Software without
XCOMM       prior written authorization from The XFree86 Project, Inc.
XCOMM
XCOMM THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
XCOMM WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
XCOMM MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
XCOMM IN NO EVENT SHALL THE XFREE86 PROJECT, INC OR ITS CONTRIBUTORS BE
XCOMM LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
XCOMM OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
XCOMM OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
XCOMM BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
XCOMM WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
XCOMM OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
XCOMM EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
XCOMM
XCOMM $XConsortium: XF86Conf.cpp /main/22 1996/10/23 11:43:51 kaleb $

XCOMM **********************************************************************
XCOMM This is a sample configuration file only, intended to illustrate
XCOMM what a config file might look like.  Refer to the XF86Config(4/5)
XCOMM man page for details about the format of this file. This man page
XCOMM is installed as MANPAGE 
XCOMM **********************************************************************

XCOMM The ordering of sections is not important in version 4.0 and later.

XCOMM **********************************************************************
XCOMM Files section.  This allows default font and rgb paths to be set
XCOMM **********************************************************************

Section "Files"

XCOMM The location of the RGB database.  Note, this is the name of the
XCOMM file minus the extension (like ".txt" or ".db").  There is normally
XCOMM no need to change the default.

    RgbPath	RGBPATH

XCOMM Multiple FontPath entries are allowed (which are concatenated together),
XCOMM as well as specifying multiple comma-separated entries in one FontPath
XCOMM command (or a combination of both methods)

    FontPath	LOCALFONTPATH
    FontPath	MISCFONTPATH
    FontPath	DPI75USFONTPATH
    FontPath	DPI100USFONTPATH
    FontPath	T1FONTPATH
    FontPath	CIDFONTPATH
    FontPath	SPFONTPATH
    FontPath	DPI75FONTPATH
    FontPath	DPI100FONTPATH

XCOMM ModulePath can be used to set a search path for the X server modules.
XCOMM The default path is shown here.

XCOMM    ModulePath	MODULEPATH

EndSection

XCOMM **********************************************************************
XCOMM Module section -- this is an optional section which is used to specify
XCOMM which run-time loadable modules to load when the X server starts up.
XCOMM **********************************************************************

Section "Module"

XCOMM This loads the PEX extension module.

    Load	"pex5"

XCOMM This loads the XIE extension module.

    Load	"xie"

XCOMM This loads the GLX extension module.

    Load	"glx"

XCOMM This loads the DBE extension module.

    Load	"dbe"

XCOMM This loads the RECORD extension module.

    Load	"record"

XCOMM This loads the miscellaneous extensions module, and disables
XCOMM initialisation of the XFree86-DGA extension within that module.

    SubSection	"extmod"
	Option	"omit xfree86-dga"
    EndSubSection

XCOMM This loads the Type1, Speedo and FreeType font modules

    Load	"type1"
    Load	"speedo"
    Load	"freetype"

EndSection


XCOMM **********************************************************************
XCOMM Server flags section.  This contains various server-wide Options.
XCOMM **********************************************************************

Section "ServerFlags"

XCOMM Uncomment this to cause a core dump at the spot where a signal is 
XCOMM received.  This may leave the console in an unusable state, but may
XCOMM provide a better stack trace in the core dump to aid in debugging

XCOMM    Option	"NoTrapSignals"

XCOMM Uncomment this to disable the <Crtl><Alt><BS> server abort sequence
XCOMM This allows clients to receive this key event.

XCOMM    Option	"DontZap"

XCOMM Uncomment this to disable the <Crtl><Alt><KP_+>/<KP_-> mode switching
XCOMM sequences.  This allows clients to receive these key events.

XCOMM    Option	"DontZoom"

XCOMM Uncomment this to disable tuning with the xvidtune client. With
XCOMM it the client can still run and fetch card and monitor attributes,
XCOMM but it will not be allowed to change them. If it tries it will
XCOMM receive a protocol error.

XCOMM    Option	"DisableVidModeExtension"

XCOMM Uncomment this to enable the use of a non-local xvidtune client.

XCOMM    Option	"AllowNonLocalXvidtune"

XCOMM Uncomment this to disable dynamically modifying the input device
XCOMM (mouse and keyboard) settings.

XCOMM    Option	"DisableModInDev"

XCOMM Uncomment this to enable the use of a non-local client to
XCOMM change the keyboard or mouse settings (currently only xset).

XCOMM    Option	"AllowNonLocalModInDev"

XCOMM Set the basic blanking screen saver timeout.

    Option	"blank time"	"10"	# 10 minutes

XCOMM Set the DPMS timeouts.  These are set here because they are global
XCOMM rather than screen-specific.  These settings alone don't enable DPMS.
XCOMM It is enabled per-screen (or per-monitor), and even then only when
XCOMM the driver supports it.

    Option	"standby time"	"20"
    Option	"suspend time"	"30"
    Option	"off time"	"60"

XCOMM Specify PC98 architecture

XCOMM    Option	"PC98"

EndSection

XCOMM **********************************************************************
XCOMM Input devices
XCOMM **********************************************************************

XCOMM **********************************************************************
XCOMM Core keyboard's InputDevice section
XCOMM **********************************************************************

Section "InputDevice"

    Identifier	"Keyboard1"
    Driver	"keyboard"

XCOMM For most OSs the protocol can be omitted (it defaults to "Standard").
XCOMM When using XQUEUE (only for SVR3 and SVR4, but not Solaris), comment
XCOMM out the above line, and uncomment the following line.

XCOMM    Option	"Protocol"	"Xqueue"

XCOMM Set the keyboard auto repeat parameters.  Not all platforms implement
XCOMM this.

    Option	"AutoRepeat"	"500 5"

XCOMM Specifiy which keyboard LEDs can be user-controlled (eg, with xset(1)).

XCOMM    Option	"Xleds"	"1 2 3"

XCOMM To disable the XKEYBOARD extension, uncomment XkbDisable.

XCOMM    Option	"XkbDisable"

    Option	"XkbRules"	"xfree86"
    Option	"XkbModel"	"pc98"
    Option	"XkbLayout"	"nec/jp"
XCOMM    Option	"XkbVariant"	""
XCOMM    Option	"XkbOptions"	""

EndSection


XCOMM **********************************************************************
XCOMM Core Pointer's InputDevice section
XCOMM **********************************************************************

Section "InputDevice"

XCOMM Identifier and driver

    Identifier	"Mouse1"
    Driver	"mouse"

XCOMM The mouse protocol and device.  The device is normally set to /dev/mouse,
XCOMM which is usually a symbolic link to the real device.

    Option	"Protocol"	"BusMouse"
XCOMM For FreeBSD(98)-2.X
FREEBSDMOUSEDEV
XCOMM For NetBSD/pc98 (based on NetBSD 1.1 or later)
NETBSDNEWMOUSEDEV
XCOMM For NetBSD/pc98 (based on NetBSD 1.0)
NETBSDOLDMOUSEDEV
XCOMM For Linux/98
LINUXMOUSEDEV

XCOMM When using XQUEUE (only for SVR3 and SVR4, but not Solaris), use
XCOMM the following instead of any of the lines above.  The Device line
XCOMM is not required in this case.

XCOMM    Option	"Protocol"	"Xqueue"

XCOMM Baudrate and SampleRate are only for some older Logitech mice.  In
XCOMM almost every case these lines should be omitted.

XCOMM    Option	"BaudRate"	"9600"
XCOMM    Option	"SampleRate"	"150"

XCOMM Emulate3Buttons is an option for 2-button mice
XCOMM Emulate3Timeout is the timeout in milliseconds (default is 50ms)

    Option	"Emulate3Buttons"
XCOMM    Option	"Emulate3Timeout"	"50"

XCOMM ChordMiddle is an option for some 3-button Logitech mice, or any
XCOMM 3-button mouse where the middle button generates left+right button
XCOMM events.

XCOMM    Option	"ChordMiddle"

EndSection

Section "InputDevice"
    Identifier	"Mouse2"
    Driver	"mouse"
    Option	"Protocol"	"MouseMan"
    Option	"Device"	"/dev/mouse2"
EndSection

XCOMM Some examples of extended input devices

XCOMM Section "InputDevice"
XCOMM    Identifier	"spaceball"
XCOMM    Driver	"magellan"
XCOMM    Option	"Device"	"/dev/cua0"
XCOMM EndSection
XCOMM
XCOMM Section "InputDevice"
XCOMM    Identifier	"spaceball2"
XCOMM    Driver	"spaceorb"
XCOMM    Option	"Device"	"/dev/cua0"
XCOMM EndSection
XCOMM
XCOMM Section "InputDevice"
XCOMM    Identifier	"touchscreen0"
XCOMM    Driver	"microtouch"
XCOMM    Option	"Device"	"/dev/ttyS0"
XCOMM    Option	"MinX"		"1412"
XCOMM    Option	"MaxX"		"15184"
XCOMM    Option	"MinY"		"15372"
XCOMM    Option	"MaxY"		"1230"
XCOMM    Option	"ScreenNumber"	"0"
XCOMM    Option	"ReportingMode"	"Scaled"
XCOMM    Option	"ButtonNumber"	"1"
XCOMM    Option	"SendCoreEvents"
XCOMM EndSection
XCOMM
XCOMM Section "InputDevice"
XCOMM    Identifier	"touchscreen1"
XCOMM    Driver	"elo2300"
XCOMM    Option	"Device"	"/dev/ttyS0"
XCOMM    Option	"MinX"		"231"
XCOMM    Option	"MaxX"		"3868"
XCOMM    Option	"MinY"		"3858"
XCOMM    Option	"MaxY"		"272"
XCOMM    Option	"ScreenNumber"	"0"
XCOMM    Option	"ReportingMode"	"Scaled"
XCOMM    Option	"ButtonThreshold"	"17"
XCOMM    Option	"ButtonNumber"	"1"
XCOMM    Option	"SendCoreEvents"
XCOMM EndSection

XCOMM **********************************************************************
XCOMM Monitor section
XCOMM **********************************************************************

XCOMM Any number of monitor sections may be present

Section "Monitor"

XCOMM The identifier line must be present.

    Identifier	"Generic Monitor"

XCOMM The VendorName and ModelName lines are optional.
    VendorName	"Unknown"
    ModelName	"Unknown"

XCOMM HorizSync is in kHz unless units are specified.
XCOMM HorizSync may be a comma separated list of discrete values, or a
XCOMM comma separated list of ranges of values.
XCOMM NOTE: THE VALUES HERE ARE EXAMPLES ONLY.  REFER TO YOUR MONITOR'S
XCOMM USER MANUAL FOR THE CORRECT NUMBERS.

    HorizSync	31.5  # typical for a single frequency fixed-sync monitor

XCOMM    HorizSync	30-64         # multisync
XCOMM    HorizSync	31.5, 35.2    # multiple fixed sync frequencies
XCOMM    HorizSync	15-25, 30-50  # multiple ranges of sync frequencies

XCOMM VertRefresh is in Hz unless units are specified.
XCOMM VertRefresh may be a comma separated list of discrete values, or a
XCOMM comma separated list of ranges of values.
XCOMM NOTE: THE VALUES HERE ARE EXAMPLES ONLY.  REFER TO YOUR MONITOR'S
XCOMM USER MANUAL FOR THE CORRECT NUMBERS.

    VertRefresh	60  # typical for a single frequency fixed-sync monitor

XCOMM    VertRefresh	50-100        # multisync
XCOMM    VertRefresh	60, 65        # multiple fixed sync frequencies
XCOMM    VertRefresh	40-50, 80-100 # multiple ranges of sync frequencies

XCOMM Modes can be specified in two formats.  A compact one-line format, or
XCOMM a multi-line format.

XCOMM A generic VGA 640x480 mode (hsync = 31.5kHz, refresh = 60Hz)
XCOMM These two are equivalent

XCOMM    ModeLine "640x480" 25.175 640 664 760 800 480 491 493 525

    Mode "640x480"
        DotClock	25.175
        HTimings	640 664 760 800
        VTimings	480 491 493 525
    EndMode

XCOMM These two are equivalent

XCOMM    ModeLine "1024x768i" 45 1024 1048 1208 1264 768 776 784 817 Interlace

XCOMM    Mode "1024x768i"
XCOMM        DotClock	45
XCOMM        HTimings	1024 1048 1208 1264
XCOMM        VTimings	768 776 784 817
XCOMM        Flags		"Interlace"
XCOMM    EndMode

XCOMM If a monitor has DPMS support, that can be indicated here.  This will
XCOMM enable DPMS when the monitor is used with drivers that support it.

XCOMM    Option	"dpms"

XCOMM If a monitor requires that the sync signals be superimposed on the
XCOMM green signal, the following option will enable this when used with
XCOMM drivers that support it.  Only a relatively small range of hardware
XCOMM (and drivers) actually support this.

XCOMM    Option	"sync on green"

EndSection

Section "Monitor"
XCOMM NOTE: THE VALUES HERE ARE EXAMPLES ONLY.  REFER TO YOUR MONITOR'S
XCOMM USER MANUAL FOR THE CORRECT NUMBERS.
    Identifier	"Multi sync"
    VendorName	"IDEK"
    ModelName	"MF8615"
    HorizSync	24-70
    VertRefresh	50-100

    Mode "640x400"
        DotClock	28.322
        HTimings	640 664 712 808
        VTimings	400 400 402 417
    EndMode

    Mode "640x480"
        DotClock	28.0
        HTimings	640 690 752 800
        VTimings	480 496 544 560
    EndMode

    Mode "NEC480"
        DotClock	31.5
        HTimings	640 664 760 800
        VTimings	480 491 493 525
    EndMode

    Mode "800x600"
        DotClock	36.00
        HTimings	800 840 900 1000
        VTimings	600 602 610 664
    EndMode

    Mode "1024x768"
        DotClock	65.00
        HTimings	1024 1188 1210 1370
        VTimings	768   768  770  790
    EndMode

    Mode "1024x768i"
        DotClock	45.00
        HTimings	1024 1030 1230 1260
        VTimings	768   768  790  830
	Flags		"Interlace"
    EndMode

    Mode "1024x768H"
        DotClock	75.00
        HTimings	1024 1068 1184 1328
        VTimings	768   771  777  806
    EndMode

    Mode "1280x1024"
        DotClock	109.00
        HTimings	1280 1290 1680 1696
        VTimings	1024 1027 1030 1064
    EndMode

    Mode "1280x1024H"
        DotClock	110.00
        HTimings	1280 1300 1315 1700
        VTimings	1024 1027 1030 1064
    EndMode

EndSection

XCOMM **********************************************************************
XCOMM Graphics device section
XCOMM **********************************************************************

XCOMM Any number of graphics device sections may be present

Section "Device"
    Identifier	"MGA"
    VendorName	"Matrox"
    BoardName	"Millennium"
    Driver	"mga"
XCOMM    BusID	"PCI:0:10:0"
EndSection

Section "Device"
    Identifier "NECTrident"
    VendorName "NEC"
    BoardName  "NEC Trident"
    Driver     "trident"
XCOMM    BusID	"PCI:0:8:0"
XCOMM    Option	"NoPciBurst"
XCOMM    Option	"XaaNoScreenToScreenCopy"
XCOMM    Option	"XaaNoCPUToScreenColorExpandFill"
XCOMM    Option	"XaaNoScanlineCPUToScreenColorExpandFill"
XCOMM    Option	"XaaNoScreenToScreenColorExpandFill"
XCOMM    VideoRam	2048
Endsection

XCOMM **********************************************************************
XCOMM Screen sections.
XCOMM **********************************************************************

XCOMM Any number of screen sections may be present.  Each describes
XCOMM the configuration of a single screen.  A single specific screen section
XCOMM may be specified from the X server command line with the "-screen"
XCOMM option.

Section "Screen"

XCOMM The Identifier, Device and Monitor lines must be present

    Identifier	"Screen 1"
    Device	"MGA"
    Monitor	"Multi sync"

XCOMM The favoured Depth and/or Bpp may be specified here

    DefaultDepth 8

    SubSection "Display"
        Depth		8
        Modes		"1280x1024" "1024x768" "800x600" "640x480"
    EndSubsection

    SubSection "Display"
        Depth		16
        Modes		"1280x1024" "1024x768" "800x600" "640x480"
    EndSubsection

    SubSection "Display"
        Depth		24
        Modes		"1280x1024" "1024x768" "800x600" "640x480"
    EndSubsection

    SubSection "Display"
        Depth		32
        Modes		"1024x768" "800x600" "640x480"
    EndSubsection

EndSection

XCOMM **********************************************************************
XCOMM ServerLayout sections.
XCOMM **********************************************************************

Section "ServerLayout"
    Identifier	"simple layout"
    Screen	"Screen 1"
    InputDevice	"Mouse1" "CorePointer"
    InputDevice "Keyboard1" "CoreKeyboard"
EndSection
