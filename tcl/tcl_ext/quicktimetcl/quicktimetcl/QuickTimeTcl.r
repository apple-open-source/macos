/*
 * QuickTimeTcl.r --
 *
 *	This file creates resources used by the QuickTimeTcl package.
 *
 *  Copyright (c) 1998 Jim Ingham and Bruce O'Neel
 *
 * $Id: QuickTimeTcl.r,v 1.1.1.1 2003/04/04 16:24:54 matben Exp $
 */

#include <Types.r>
#include <SysTypes.r>

#define RESOURCE_INCLUDED
#define RC_INVOKED
#include "tcl.h"

/*
 * The folowing include and defines help construct
 * the version string for Tcl.
 */

#define SCRIPT_MAJOR_VERSION 3		/* Major number */
#define SCRIPT_MINOR_VERSION 1  	/* Minor number */
#define SCRIPT_RELEASE_SERIAL  0	/* Really minor number! */
#define RELEASE_LEVEL alpha		/* alpha, beta, or final */
#define SCRIPT_VERSION "3.1"
#define SCRIPT_PATCH_LEVEL "3.1"
#define FINAL 0		/* Change to 1 if final version. */

#if FINAL
#   define MINOR_VERSION (SCRIPT_MINOR_VERSION * 16) + SCRIPT_RELEASE_SERIAL
#else
#   define MINOR_VERSION SCRIPT_MINOR_VERSION * 16
#endif

#define RELEASE_CODE 0x00

resource 'vers' (1) {
	SCRIPT_MAJOR_VERSION, MINOR_VERSION,
	RELEASE_LEVEL, 0x00, verUS,
	SCRIPT_PATCH_LEVEL,
	SCRIPT_PATCH_LEVEL ",  © 1998 Bruce O'Neel, © 2000-2003 Mats Bengtsson"
};

resource 'vers' (2) {
	SCRIPT_MAJOR_VERSION, MINOR_VERSION,
	RELEASE_LEVEL, 0x00, verUS,
	SCRIPT_PATCH_LEVEL,
	"QuickTimeTcl " SCRIPT_PATCH_LEVEL " © 1998-2003"
};

/*
 * The -16397 string will be displayed by Finder when a user
 * tries to open the shared library. The string should
 * give the user a little detail about the library's capabilities
 * and enough information to install the library in the correct location.  
 * A similar string should be placed in all shared libraries.
 */
resource 'STR ' (-16397, purgeable) {
	"QuickTimeTcl Library\n\n"
	"This library provides the ability to run QuickTime "
	" commands from Tcl/Tk programs.  To work properly, it "
	"should be placed in the ‘Tool Command Language’ folder "
	"within the Extensions folder."
};


/* 
 * We now load the Tk library into the resource fork of the library.
 */

data 'TEXT' (4000, "pkgIndex", purgeable, preload) {
	"# Tcl package index file, version 1.0\n"
	"if {[info tclversion] != "TCL_VERSION"} return\n"
	"package ifneeded QuickTimeTcl 3.1 [list load [file join $dir QuickTimeTcl"TCL_VERSION".shlb]]\n"
};
