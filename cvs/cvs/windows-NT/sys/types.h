/*
 * windows-NT/sys/types.h
 */

#ifdef _MSC_VER

#pragma once

/* cl.exe version number legend:                               */
/*                                                             */
/*      ! cl.exe version number confirmed                      */
/*      ? cl.exe version number uncertain                      */
/*                                                             */
/* Verified with Visual C++ 5.0       - cl.exe version 11.00 ! */
/* Verified with Visual C++ 6.0       - cl.exe version 12.00 ! */
/* No tests with Visual C++ .NET      - cl.exe version 13.00 ? */
/* Verified with Visual C++ .NET 2003 - cl.exe version 13.10 ! */
/* No tests with Visual C++ 2005      - cl.exe version 14.00 ? */
/*                                                             */
#if _MSC_VER != 1100 && _MSC_VER != 1200 && _MSC_VER != 1310
#pragma message ( "Please email Microsoft's <sys/types.h> file" )
#pragma message ( "and version number from \"cl /?\" command to" )
#pragma message ( "<conradpino@cvsproject.org>.  Thank you." )
#endif /* _MSC_VER != 1200 */

/***************************************************************************/
/* Mimic what Micrsoft defines in their <sys/types.h> */
#ifndef _INC_TYPES
#define _INC_TYPES



/* Define time_t */
#ifndef _TIME_T_DEFINED
#define _TIME_T_DEFINED

#if defined(_WIN64) && _MSC_VER >= 1300
typedef __int64 time_t;
#else
typedef long time_t;
#endif

#if _INTEGRAL_MAX_BITS >= 64
typedef __int64 __time64_t;
#endif
#endif /* _TIME_T_DEFINED */



/* Define ino_t */
#ifndef _INO_T_DEFINED
#define _INO_T_DEFINED

#if _MSC_VER == 1100

typedef unsigned short _ino_t;		/* i-node number (not used on DOS) */

#if	!__STDC__
/* Non-ANSI name for compatibility */
#ifdef	_NTSDK
#define ino_t _ino_t
#else	/* ndef _NTSDK */
typedef unsigned short ino_t;
#endif	/* _NTSDK */
#endif	/* !__STDC__ */

#else	/*  _MSC_VER != 1100 */

typedef unsigned short ino_t;

/* Microsoft uses _ino_t */
typedef ino_t _ino_t;

#endif	/*  _MSC_VER != 1100 */

#endif	/* _INO_T_DEFINED */



/* Define dev_t */
#ifndef _DEV_T_DEFINED
#define _DEV_T_DEFINED

#if _MSC_VER == 1100

#ifdef	_NTSDK
typedef short _dev_t;			/* device code */
#else	/* ndef _NTSDK */
typedef unsigned int _dev_t;		/* device code */
#endif	/* _NTSDK */

#if	!__STDC__
/* Non-ANSI name for compatibility */
#ifdef	_NTSDK
#define dev_t _dev_t
#else	/* ndef _NTSDK */
typedef unsigned int dev_t;
#endif	/* _NTSDK */
#endif	/* !__STDC__ */

#else	/*  _MSC_VER != 1100 */

typedef unsigned int dev_t;

/* Microsoft uses _dev_t */
typedef dev_t _dev_t;

#endif	/*  _MSC_VER != 1100 */

#endif	/* _DEV_T_DEFINED */



/* Define off_t */
#ifndef _OFF_T_DEFINED
#define _OFF_T_DEFINED

#if _MSC_VER == 1100

typedef long _off_t;			/* file offset value */

#if	!__STDC__
/* Non-ANSI name for compatibility */
#ifdef	_NTSDK
#define off_t _off_t
#else	/* ndef _NTSDK */
typedef long off_t;
#endif	/* _NTSDK */
#endif	/* !__STDC__ */

#else	/*  _MSC_VER != 1100 */

typedef long off_t;

/* Microsoft uses _off_t */
typedef off_t _off_t;

#endif	/*  _MSC_VER != 1100 */

#endif	/* _OFF_T_DEFINED */

#endif	/* _INC_TYPES */

/***************************************************************************/
/* define what Micrsoft doesn't */
typedef int gid_t;
typedef int pid_t;
typedef int uid_t;

typedef unsigned int useconds_t;
/***************************************************************************/

#else /* _MSC_VER */
#error This file is for use with Microsoft compilers only.
#endif /* _MSC_VER */
