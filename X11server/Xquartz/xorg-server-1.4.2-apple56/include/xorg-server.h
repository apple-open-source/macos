/* include/xorg-server.h.  Generated from xorg-server.h.in by configure.  */
/* xorg-server.h.in						-*- c -*-
 *
 * This file is the template file for the xorg-server.h file which gets
 * installed as part of the SDK.  The #defines in this file overlap
 * with those from config.h, but only for those options that we want
 * to export to external modules.  Boilerplate autotool #defines such
 * as HAVE_STUFF and PACKAGE_NAME is kept in config.h
 *
 * It is still possible to update config.h.in using autoheader, since
 * autoheader only creates a .h.in file for the first
 * AM_CONFIG_HEADER() line, and thus does not overwrite this file.
 *
 * However, it should be kept in sync with this file.
 */

#ifndef _XORG_SERVER_H_
#define _XORG_SERVER_H_

/* Support BigRequests extension */
#define BIGREQS 1

/* Default font path */
#define COMPILEDDEFAULTFONTPATH "/usr/X11/lib/X11/fonts/misc/,/usr/X11/lib/X11/fonts/TTF/,/usr/X11/lib/X11/fonts/OTF,/usr/X11/lib/X11/fonts/Type1/,/usr/X11/lib/X11/fonts/100dpi/,/usr/X11/lib/X11/fonts/75dpi/,/Library/Fonts,/System/Library/Fonts"

/* Support Composite Extension */
/* #undef COMPOSITE */

/* Use OsVendorInit */
#define DDXOSINIT 1

/* Build DPMS extension */
/* #undef DPMSExtension */

/* Built-in output drivers */
/* #undef DRIVERS */

/* Build GLX extension */
#define GLXEXT 1

/* Include handhelds.org h3600 touchscreen driver */
/* #undef H3600_TS */

/* Support XDM-AUTH*-1 */
#define HASXDMAUTH 1

/* Support SHM */
#define HAS_SHM 1

/* Built-in input drivers */
/* #undef IDRIVERS */

/* Support IPv6 for TCP connections */
#define IPv6 1

/* Support MIT Misc extension */
#define MITMISC 1

/* Support MIT-SHM Extension */
#define MITSHM 1

/* Disable some debugging code */
#define NDEBUG 1

/* Need XFree86 helper functions */
/* #undef NEED_XF86_PROTOTYPES */

/* Need XFree86 typedefs */
/* #undef NEED_XF86_TYPES */

/* Internal define for Xinerama */
#define PANORAMIX 1

/* Support pixmap privates */
#define PIXPRIV 1

/* Support RANDR extension */
#define RANDR 1

/* Support RENDER extension */
#define RENDER 1

/* Support X resource extension */
#define RES 1

/* Support MIT-SCREEN-SAVER extension */
#define SCREENSAVER 1

/* Use a lock to prevent multiple servers on a display */
#define SERVER_LOCK 1

/* Support SHAPE extension */
#define SHAPE 1

/* Include time-based scheduler */
#define SMART_SCHEDULE 1

/* Define to 1 on systems derived from System V Release 4 */
/* #undef SVR4 */

/* Support TCP socket connections */
#define TCPCONN 1

/* Enable touchscreen support */
/* #undef TOUCHSCREEN */

/* Support tslib touchscreen abstraction library */
/* #undef TSLIB */

/* Support UNIX socket connections */
#define UNIXCONN 1

/* Use builtin rgb color database */
/* #undef USE_RGB_BUILTIN */

/* Use rgb.txt directly */
#define USE_RGB_TXT 1

/* unaligned word accesses behave as expected */
/* #undef WORKING_UNALIGNED_INT */

/* Support XCMisc extension */
#define XCMISC 1

/* Support Xdmcp */
#define XDMCP 1

/* Build XFree86 BigFont extension */
/* #undef XF86BIGFONT */

/* Support XFree86 miscellaneous extensions */
/* #undef XF86MISC */

/* Support XFree86 Video Mode extension */
/* #undef XF86VIDMODE */

/* Build XDGA support */
/* #undef XFreeXDGA */

/* Support Xinerama extension */
#define XINERAMA 1

/* Support X Input extension */
#define XINPUT 1

/* Build XKB */
#define XKB 1

/* Enable XKB per default */
#define XKB_DFLT_DISABLED 0

/* Build XKB server */
#define XKB_IN_SERVER 1

/* Support loadable input and output drivers */
/* #undef XLOADABLE */

/* Build DRI extension */
/* #undef XF86DRI */

/* Build Xorg server */
/* #undef XORGSERVER */

/* Vendor release */
/* #undef XORG_RELEASE */

/* Current Xorg version */
/* #undef XORG_VERSION_CURRENT */

/* Build Xv Extension */
#define XvExtension 1

/* Build XvMC Extension */
#define XvMCExtension 1

/* Build XRes extension */
#define XResExtension 1

/* Support XSync extension */
#define XSYNC 1

/* Support XTest extension */
#define XTEST 1

/* Support XTrap extension */
#define XTRAP 1

/* Support Xv Extension */
#define XV 1

/* Vendor name */
#define XVENDORNAME "The X.Org Foundation"

/* BSD-compliant source */
/* #undef _BSD_SOURCE */

/* POSIX-compliant source */
/* #undef _POSIX_SOURCE */

/* X/Open-compliant source */
/* #undef _XOPEN_SOURCE */

/* Vendor web address for support */
/* #undef __VENDORDWEBSUPPORT__ */

/* Location of configuration file */
/* #undef __XCONFIGFILE__ */

/* XKB default rules */
#define __XKBDEFRULES__ "xorg"

/* Name of X server */
/* #undef __XSERVERNAME__ */

/* Define to 1 if unsigned long is 64 bits. */
/* #undef _XSERVER64 */

/* Building vgahw module */
/* #undef WITH_VGAHW */

/* System is BSD-like */
#define CSRG_BASED 1

/* Solaris 8 or later? */
/* #undef __SOL8__ */

/* System has PC console */
/* #undef PCCONS_SUPPORT */

/* System has PCVT console */
/* #undef PCVT_SUPPORT */

/* System has syscons console */
/* #undef SYSCONS_SUPPORT */

/* System has wscons console */
/* #undef WSCONS_SUPPORT */

/* Loadable XFree86 server awesomeness */
/* #undef XFree86LOADER */

#endif /* _XORG_SERVER_H_ */
