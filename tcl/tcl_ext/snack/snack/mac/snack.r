/*
 * snack.r --
 *
 *	This file creates resources used by the Snack package.
 *
 * Copyright (c) 2000 Leonid Spektor, Kåre Sjölander
 *
 */

#include "Types.r"
#include "SysTypes.r"

/*
 * The following include and defines help construct
 * the version string for Snack.
 */

#define SCRIPT_MAJOR_VERSION  2		/* Major number */
#define SCRIPT_MINOR_VERSION  2		/* Minor number */
#define SCRIPT_RELEASE_SERIAL 0 	/* Really minor number! */
#define RELEASE_LEVEL         final	/* alpha, beta, or final */
#define SCRIPT_VERSION        "2.2"
#define SCRIPT_PATCH_LEVEL    "2.2"
#define FINAL                 1		/* Change to 1 if final version. */

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
	SCRIPT_PATCH_LEVEL ", by Kare Sjolander <kare@speech.kth.se>"
};

resource 'vers' (2) {
	SCRIPT_MAJOR_VERSION, MINOR_VERSION,
	RELEASE_LEVEL, 0x00, verUS,
	SCRIPT_PATCH_LEVEL,
	"Snack " SCRIPT_PATCH_LEVEL " © 1997-2001"
};

/*
 * The -16397 string will be displayed by Finder when a user
 * tries to open the shared library. The string should
 * give the user a little detail about the library's capabilities
 * and enough information to install the library in the correct location.  
 * A similar string should be placed in all shared libraries.
 */
resource 'STR ' (-16397, purgeable) {
	"Snack\n\n"
	"This library is a sound extension "
	"for Tcl/Tk programs.  To work properly, it "
	"should be placed in the Tool Command Language folder "
	"within the Extensions folder."
};


/* 
 * Set up the pkgIndex in the resource fork of the library.
 */

#if defined(__POWERPC__)
  #define TARGET 
#else
  #define TARGET "CFM68K"
#endif

read 'TEXT' (4000,"pkgIndex",purgeable,preload) "snack.res";
