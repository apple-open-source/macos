/*
 *
 *  iodbc.h
 *
 *  $Id: iodbc.h,v 1.4 2002/06/14 15:07:31 miner Exp $
 *
 *  Configuration
 *
 *  The iODBC driver manager.
 *  
 *  Copyright (C) 1995 by Ke Jin <kejin@empress.com> 
 *  Copyright (C) 1996-2002 by OpenLink Software <iodbc@openlinksw.com>
 *  All Rights Reserved.
 *
 *  This software is released under the terms of either of the following
 *  licenses:
 *
 *      - GNU Library General Public License (see LICENSE.LGPL) 
 *      - The BSD License (see LICENSE.BSD).
 *
 *  While not mandated by the BSD license, any patches you make to the
 *  iODBC source code may be contributed back into the iODBC project
 *  at your discretion. Contributions will benefit the Open Source and
 *  Data Access community as a whole. Submissions may be made at:
 *
 *      http://www.iodbc.org
 *
 *
 *  GNU Library Generic Public License Version 2
 *  ============================================
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *  The BSD License
 *  ===============
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *  3. Neither the name of OpenLink Software Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL OPENLINK OR
 *  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef	_IODBC_H
#define _IODBC_H

#if	!defined(WINDOWS) && !defined(WIN32_SYSTEM)
#define _UNIX_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef VERSION
#define VERSION	"3.0.6"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#define MEM_ALLOC(size)	(malloc((size_t)(size)))
#define MEM_FREE(ptr)	{if(ptr) free(ptr);}

#define STRCPY(t, s)	(strcpy((char*)(t), (char*)(s)))
#define STRNCPY(t,s,n)	(strncpy((char*)(t), (char*)(s), (size_t)(n)))
#define STRCAT(t, s)	(strcat((char*)(t), (char*)(s)))
#define STRNCAT(t,s,n)	(strncat((char*)(t), (char*)(s), (size_t)(n)))
#define STREQ(a, b)	(strcmp((char*)(a), (char*)(b)) == 0)
#define STRLEN(str)	((str)? strlen((char*)(str)):0)

#define EXPORT
#define CALLBACK
#define FAR

#ifndef WIN32
#define UNALIGNED
#endif

/* SEM */
# if defined(__APPLE__)
#    define ODBC_INI_APP "/Library/ODBC/odbc.ini"
#    define ODBCINST_INI_APP	"/Library/ODBC/odbcinst.ini"
# endif



/*
 *  If not defined, use this as the system default odbc.ini file
 */
/*
 *  If not defined, use this as the system default odbc.ini file
 */
#ifndef SYS_ODBC_INI
# ifdef _BE
# 	define SYS_ODBC_INI "/boot/beos/etc/odbc.ini"
# elif defined(_MAC)
# 	ifdef __POWERPC__
# 		define SYS_ODBC_INI "Boot:System Folder:Preferences:ODBC Preferences PPC"
# 	else
# 		define SYS_ODBC_INI "Boot:System Folder:Preferences:ODBC Preferences"
# 	endif
# elif defined(_MACX)
# 	define SYS_ODBC_INI "/etc/odbc.ini"
# 	define ODBC_INI_APP "/Library/Preferences/ODBC.preference"
# else
# 	define SYS_ODBC_INI "/etc/odbc.ini"
# endif
#endif

#if !defined(SYS_ODBCINST_INI) && !defined(ODBCINST_INI_APP)
#  if defined(__BEOS__)
#    define SYS_ODBCINST_INI	"/boot/beos/etc/odbcinst.ini"
#  elif defined(macintosh)
#  elif defined(_MACX)
#    define SYS_ODBCINST_INI	"/etc/odbcinst.ini"
#    define ODBCINST_INI_APP	"/Library/Preferences/ODBC Installer.preference"
#  else
#    define SYS_ODBCINST_INI	"/etc/odbcinst.ini"
#  endif
#endif

#endif /* _UNIX_ */

#if	defined(WINDOWS) || defined(WIN32_SYSTEM)
#include <windows.h>
#include <windowsx.h>

#ifdef	_MSVC_
#define MEM_ALLOC(size)	(fmalloc((size_t)(size)))
#define MEM_FREE(ptr)	((ptr)? ffree((PTR)(ptr)):0)
#define STRCPY(t, s)	(fstrcpy((char FAR*)(t), (char FAR*)(s)))
#define STRNCPY(t,s,n)	(fstrncpy((char FAR*)(t), (char FAR*)(s), (size_t)(n)))
#define STRLEN(str)	((str)? fstrlen((char FAR*)(str)):0)
#define STREQ(a, b)	(fstrcmp((char FAR*)(a), (char FAR*)(b) == 0))
#define STRCAT(t, s)	(strcat((char*)(t), (char*)(s)))
#define STRNCAT(t,s,n)	(strncat((char*)(t), (char*)(s), (size_t)(n)))
#define STRNCMP(t,s,n)	(strncmp((char*)(t), (char*)(s), (size_t)(n)))
#endif

#ifdef	_BORLAND_
#define MEM_ALLOC(size)	(farmalloc((unsigned long)(size)))
#define MEM_FREE(ptr)	((ptr)? farfree((void far*)(ptr)):0)
#define STRCPY(t, s)	(_fstrcpy((char FAR*)(t), (char FAR*)(s)))
#define STRNCPY(t,s,n)	(_fstrncpy((char FAR*)(t), (char FAR*)(s), (size_t)(n)))
#define STRLEN(str)     ((str)? _fstrlen((char FAR*)(str)):0)
#define STREQ(a, b)     (_fstrcmp((char FAR*)(a), (char FAR*)(b) == 0))
#define STRCAT(t, s)	(strcat((char*)(t), (char*)(s)))
#define STRNCAT(t,s,n)	(strncat((char*)(t), (char*)(s), (size_t)(n)))
#define STRNCMP(t,s,n)	(strncmp((char*)(t), (char*)(s), (size_t)(n)))
#endif

#endif /* WINDOWS */

#ifdef VMS
/*
 *  VMS also defines _UNIX_ above. This is OK for iODBC since all used UNIX
 *  interfaces are supported.
 *  The DEC C RTL actually supports dlopen(), etc, but I have made my own
 *  implementation that supports:
 *     - Proper error messages from dlopen()
 *     - The ability to place the driver in other directories than SYS$SHARE:
 *     - Neither implementation can do dlopen(NULL,), but my implementation
 *       will not crash in this case.
 *  To use old DEC C dlopen() implementation, remove the following define.
 */
#define DLDAPI_VMS_IODBC	/* Use enhanced dlopen() */
#endif

#define SYSERR		(-1)

#ifndef	NULL
#define NULL		((void FAR*)0UL)
#endif

/*
 *  Map generic pointer to internal pointer 
 */
#define STMT(stmt, var) \
	STMT_t FAR *stmt = (STMT_t FAR *)var

#define CONN(con, var) \
	DBC_t FAR *con = (DBC_t FAR *)var

#define GENV(genv, var) \
	GENV_t FAR *genv = (GENV_t FAR *)var

#define ENVR(env, var) \
	ENV_t FAR *env = (ENV_t FAR *)var

#define DESC(desc, var) \
	DESC_t FAR *desc = (DESC_t FAR *)var

#define NEW_VAR(type, var) \
	type *var = (type *)MEM_ALLOC(sizeof(type))


/* these are deprecated defines from the odbc headers */
#define SQL_CONNECT_OPT_DRVR_START      1000

#endif /* _IODBC_H */
