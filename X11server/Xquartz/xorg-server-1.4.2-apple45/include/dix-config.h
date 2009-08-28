/* include/dix-config.h.  Generated from dix-config.h.in by configure.  */
/* dix-config.h.in: not at all generated.                      -*- c -*- */

#ifndef _DIX_CONFIG_H_
#define _DIX_CONFIG_H_

/* Support BigRequests extension */
#define BIGREQS 1

/* Builder address */
#define BUILDERADDR "xorg@lists.freedesktop.org"

/* Operating System Name */
#define OSNAME "Darwin 9.8.0 Power Macintosh"

/* Operating System Vendor */
#define OSVENDOR ""

/* Builder string */
#define BUILDERSTRING ""

/* Default font path */
#define COMPILEDDEFAULTFONTPATH "/usr/X11/lib/X11/fonts/misc/,/usr/X11/lib/X11/fonts/TTF/,/usr/X11/lib/X11/fonts/OTF,/usr/X11/lib/X11/fonts/Type1/,/usr/X11/lib/X11/fonts/100dpi/,/usr/X11/lib/X11/fonts/75dpi/,/Library/Fonts,/System/Library/Fonts"

/* Support Composite Extension */
/* #undef COMPOSITE */

/* Define to one of `_getb67', `GETB67', `getb67' for Cray-2 and Cray-YMP
   systems. This function is required for `alloca.c' support on those systems.
   */
/* #undef CRAY_STACKSEG_END */

/* Define to 1 if using `alloca.c'. */
/* #undef C_ALLOCA */

/* Support Damage extension */
#define DAMAGE 1

/* Use OsVendorInit */
#define DDXOSINIT 1

/* Use GetTimeInMillis */
/* #undef DDXTIME */

/* Use OsVendorFatalError */
/* #undef DDXOSFATALERROR */

/* Use OsVendorVErrorF */
/* #undef DDXOSVERRORF */

/* Use ddxBeforeReset */
/* #undef DDXBEFORERESET */

/* Build DPMS extension */
/* #undef DPMSExtension */

/* Build GLX extension */
#define GLXEXT 1

/* Build GLX DRI loader */
/* #undef GLX_DRI */

/* Path to DRI drivers */
#define DRI_DRIVER_PATH "/usr/X11/lib/dri"

/* Include handhelds.org h3600 touchscreen driver */
/* #undef H3600_TS */

/* Support XDM-AUTH*-1 */
#define HASXDMAUTH 1

/* Define to 1 if you have the `getdtablesize' function. */
#define HAS_GETDTABLESIZE 1

/* Define to 1 if you have the `getifaddrs' function. */
#define HAS_GETIFADDRS 1

/* Define to 1 if you have the `getpeereid' function. */
#define HAS_GETPEEREID 1

/* Define to 1 if you have the `getpeerucred' function. */
/* #undef HAS_GETPEERUCRED */

/* Define to 1 if you have the `mmap' function. */
#define HAS_MMAP 1

/* Support SHM */
#define HAS_SHM 1

/* Define to 1 if you have `alloca', as a function or macro. */
#define HAVE_ALLOCA 1

/* Define to 1 if you have <alloca.h> and it should be used (not on Ultrix).
   */
#define HAVE_ALLOCA_H 1

/* Define to 1 if you have the <asm/mtrr.h> header file. */
/* #undef HAVE_ASM_MTRR_H */

/* Define to 1 if you have the <byteswap.h> header file. */
/* #undef HAVE_BYTESWAP_H */

/* Define to 1 if you have cbrt */
#define HAVE_CBRT 1

/* Define to 1 if you have the <dbm.h> header file. */
/* #undef HAVE_DBM_H */

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'.
   */
#define HAVE_DIRENT_H 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you don't have `vprintf' but do have `_doprnt.' */
/* #undef HAVE_DOPRNT */

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `geteuid' function. */
#define HAVE_GETEUID 1

/* Define to 1 if you have the `getisax' function. */
/* #undef HAVE_GETISAX */

/* Define to 1 if you have the `getopt' function. */
#define HAVE_GETOPT 1

/* Define to 1 if you have the `getopt_long' function. */
#define HAVE_GETOPT_LONG 1

/* Define to 1 if you have the `getuid' function. */
#define HAVE_GETUID 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have version 2.2 (or newer) of the drm library */
/* #undef HAVE_LIBDRM_2_2 */

/* Have Quartz */
#define XQUARTZ 1

/* Build a standalone xpbproxy */
/* #undef STANDALONE_XPBPROXY */

/* Define to 1 if you have the `m' library (-lm). */
#define HAVE_LIBM 1

/* Define to 1 if you have the `link' function. */
#define HAVE_LINK 1

/* Define to 1 if you have the <linux/agpgart.h> header file. */
/* #undef HAVE_LINUX_AGPGART_H */

/* Define to 1 if you have the <linux/apm_bios.h> header file. */
/* #undef HAVE_LINUX_APM_BIOS_H */

/* Define to 1 if you have the <linux/fb.h> header file. */
/* #undef HAVE_LINUX_FB_H */

/* Define to 1 if you have the <linux/h3600_ts.h> header file. */
/* #undef HAVE_LINUX_H3600_TS_H */

/* Define to 1 if you have the `memmove' function. */
#define HAVE_MEMMOVE 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `memset' function. */
#define HAVE_MEMSET 1

/* Define to 1 if you have the `mkstemp' function. */
#define HAVE_MKSTEMP 1

/* Define to 1 if you have the <ndbm.h> header file. */
#define HAVE_NDBM_H 1

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_NDIR_H */

/* Define to 1 if you have the <rpcsvc/dbm.h> header file. */
/* #undef HAVE_RPCSVC_DBM_H */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strchr' function. */
#define HAVE_STRCHR 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strrchr' function. */
#define HAVE_STRRCHR 1

/* Define to 1 if you have the `strtol' function. */
#define HAVE_STRTOL 1

/* Define to 1 if SYSV IPC is available */
#define HAVE_SYSV_IPC 1

/* Define to 1 if you have the <sys/agpio.h> header file. */
/* #undef HAVE_SYS_AGPIO_H */

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_DIR_H */

/* Define to 1 if you have the <sys/io.h> header file. */
/* #undef HAVE_SYS_IO_H */

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_NDIR_H */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/vm86.h> header file. */
/* #undef HAVE_SYS_VM86_H */

/* Define to 1 if you have the <tslib.h> header file. */
/* #undef HAVE_TSLIB_H */

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the `vprintf' function. */
#define HAVE_VPRINTF 1

/* Support IPv6 for TCP connections */
#define IPv6 1

/* Support os-specific local connections */
/* #undef LOCALCONN */

/* Support MIT Misc extension */
#define MITMISC 1

/* Support MIT-SHM Extension */
#define MITSHM 1

/* Disable some debugging code */
#define NDEBUG 1

/* Enable some debugging code */
/* #undef DEBUG */

/* Name of package */
#define PACKAGE "xorg-server"

/* Internal define for Xinerama */
#define PANORAMIX 1

/* Support pixmap privates */
#define PIXPRIV 1

/* Overall prefix */
#define PROJECTROOT "/usr/X11"

/* Support RANDR extension */
#define RANDR 1

/* Support Record extension */
#define XRECORD 1

/* Support RENDER extension */
#define RENDER 1

/* Support X resource extension */
#define RES 1

/* Support MIT-SCREEN-SAVER extension */
#define SCREENSAVER 1

/* Support Secure RPC ("SUN-DES-1") authentication for X11 clients */
/* #undef SECURE_RPC */

/* Use a lock to prevent multiple servers on a display */
#define SERVER_LOCK 1

/* Support SHAPE extension */
#define SHAPE 1

/* Include time-based scheduler */
#define SMART_SCHEDULE 1

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at run-time.
	STACK_DIRECTION > 0 => grows toward higher addresses
	STACK_DIRECTION < 0 => grows toward lower addresses
	STACK_DIRECTION = 0 => direction of growth unknown */
/* #undef STACK_DIRECTION */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

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

/* Define to use byteswap macros from <sys/endian.h> */
/* #undef USE_SYS_ENDIAN_H */

/* unaligned word accesses behave as expected */
/* #undef WORKING_UNALIGNED_INT */

/* Build X-ACE extension */
#define XACE 1

/* Support XCMisc extension */
#define XCMISC 1

/* Build Security extension */
#define XCSECURITY 1

/* Support Xdmcp */
#define XDMCP 1

/* Build XEvIE extension */
#define XEVIE 1

/* Build XFree86 BigFont extension */
/* #undef XF86BIGFONT */

/* Support XFree86 miscellaneous extensions */
/* #undef XF86MISC */

/* Support XFree86 Video Mode extension */
/* #undef XF86VIDMODE */

/* Support XFixes extension */
#define XFIXES 1

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

/* Vendor release */
/* #undef XORG_RELEASE */

/* Current Xorg version */
/* #undef XORG_VERSION_CURRENT */

/* Xorg release date */
#define XORG_DATE "11 June 2008"

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

/* Support Xv extension */
#define XV 1

/* Build TOG-CUP extension */
#define TOGCUP 1

/* Build Extended-Visual-Information extension */
#define EVI 1

/* Build Multibuffer extension */
/* #undef MULTIBUFFER */

/* Support DRI extension */
/* #undef XF86DRI */

/* Build DBE support */
#define DBE 1

/* Vendor name */
#define XVENDORNAME "The X.Org Foundation"

/* Enable GNU and other extensions to the C environment for GLIBC */
/* #undef _GNU_SOURCE */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef pid_t */

/* Build Rootless code */
#define ROOTLESS 1

/* Define to 1 if unsigned long is 64 bits. */
/* #undef _XSERVER64 */

/* Define to location of RGB database */
#define RGB_DB "/usr/X11/share/X11/rgb"

/* System is BSD-like */
#define CSRG_BASED 1

/* Define to 1 if `struct sockaddr_in' has a `sin_len' member */
#define BSD44SOCKETS 1

/* Define to 1 if modules should avoid the libcwrapper */
#define NO_LIBCWRAPPER 1

/* Support D-Bus */
/* #undef HAVE_DBUS */

/* Support the D-Bus hotplug API */
/* #undef CONFIG_DBUS_API */

/* Support HAL for hotplug */
/* #undef CONFIG_HAL */

/* Use only built-in fonts */
/* #undef BUILTIN_FONTS */

/* Use an empty root cursor */
/* #undef NULL_ROOT_CURSOR */

/* Have a monotonic clock from clock_gettime() */
/* #undef MONOTONIC_CLOCK */

/* Define to 1 if the DTrace Xserver provider probes should be built in */
/* #undef XSERVER_DTRACE */

/* Path to XErrorDB file */
#define XERRORDB_PATH "/usr/X11/share/X11/XErrorDB"

/* Define to 16-bit byteswap macro */
/* #undef bswap_16 */

/* Define to 32-bit byteswap macro */
/* #undef bswap_32 */

/* Define to 64-bit byteswap macro */
/* #undef bswap_64 */

/* Correctly set _XSERVER64 for OSX fat binaries */
#ifdef __APPLE__
#if defined(__LP64__) && !defined(_XSERVER64)
#define _XSERVER64 1
#elif !defined(__LP64__) && defined(_XSERVER64)
/* configure mangles #undef, so we fix this in AC_CONFIG_HEADERS post process */
#undef _XSERVER64
#endif
#endif

#endif /* _DIX_CONFIG_H_ */
