# $XConsortium: filelist.tcl /main/2 1996/10/19 18:52:58 kaleb $
#
#
#
#
# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/filelist98.tcl,v 1.1 1999/07/11 10:47:13 dawes Exp $
#
# Copyright 1996 by Joseph V. Moss <joe@XFree86.Org>
#
# See the file "LICENSE" for information regarding redistribution terms,
# and for a DISCLAIMER OF ALL WARRANTIES.
#

# List of files that are needed by this program or programs spawned by it.
# These lists are not meant to be exhaustive, but they should be
# complete enough to ensure that all the needed .tgz files have been
# installed

array set FilePermsDescriptions {
	Libs	"X11 libraries"
	Cfg	"configuration and application default files"
	Fonts	"standard font files"
	DB	"device, keysym, error, locale, and color databases"
	Bin	"standard X client programs"
	VidTune	"xvidtune configuration files and/or the xvidtune program"
	XKB	"X keyboard extension programs and configuration files"
}

array set FilePermsLibs {
	lib/libX11*				644
	lib/libXaw*				644
	lib/libXext*				644
	lib/libXi*				644
	lib/libXmu*				644
	lib/libXtst*				644
	lib/libSM*				644
	lib/libICE*				644
}

array set FilePermsCfg {
	lib/X11/xinit/xinitrc			444
	lib/X11/app-defaults/Bitmap		444
	lib/X11/app-defaults/XLogo		444
	lib/X11/app-defaults/XTerm		444
}

array set FilePermsFonts {
	lib/X11/fonts/misc/fonts.dir		444
	lib/X11/fonts/misc/fonts.alias		444
	lib/X11/fonts/misc/6x13.pc*		444
	lib/X11/fonts/75dpi/fonts.dir		444
	lib/X11/fonts/75dpi/symb10.pc*		444
}

array set FilePermsDB {
	lib/X11/XErrorDB			444
	lib/X11/XKeysymDB			444
	lib/X11/rgb.txt				444
	lib/X11/Cards98				444
	lib/X11/locale/locale.dir		444
	lib/X11/locale/C/XLC_LOCALE		444
}

array set FilePermsBin {
	bin/bitmap				755
	bin/mkfontdir				755
	bin/twm					755
	bin/xdpyinfo				755
	bin/xinit				755
	bin/xset				755
	bin/xterm				755
}

array set FilePermsVidTune {
	bin/xvidtune				755
	lib/X11/app-defaults/Xvidtune		444
}

array set FilePermsXKB {
	bin/xkbcomp				755
	lib/X11/xkb/xkbcomp			755
	lib/X11/xkb/compat/default		444
	lib/X11/xkb/compiled/README		444
	lib/X11/xkb/geometry/pc			444
	lib/X11/xkb/keycodes/xfree98		444
	lib/X11/xkb/keymap/xfree98		444
	lib/X11/xkb/semantics/default		444
	lib/X11/xkb/symbols/us			444
	lib/X11/xkb/types/default		444
	lib/X11/xkb/rules/xfree86		444
	lib/X11/xkb/rules/xfree86.lst		444
}

array set FilePermsReadMe {
	lib/X11/doc/README.MGA			444
	lib/X11/doc/README.S3			444
	lib/X11/doc/README.cirrus		444
	lib/X11/doc/README.trident		444
}
