/*
 *  iodbcadm.h
 *
 *  $Id: iodbcadm.h,v 1.3 2004/11/11 01:52:39 luesang Exp $
 *
 *  The iODBC driver manager.
 *  
 *  Copyright (C) 1999-2002 by OpenLink Software <iodbc@openlinksw.com>
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

#ifdef _MACX
#  include <iODBCinst/iodbcinst.h>
#else
#  include <iodbc.h>
#  include <iodbcinst.h>
#endif

#ifndef	_IODBCADM_H
#define	_IODBCADM_H

#define USER_DSN		0
#define SYSTEM_DSN		1
#define FILE_DSN		2

SQLRETURN SQL_API iodbcdm_drvconn_dialbox (HWND hwnd, LPSTR szInOutConnStr,
    DWORD cbInOutConnStr, int *sqlStat, SQLUSMALLINT fDriverCompletion,
    UWORD * config);
SQLRETURN SQL_API iodbcdm_drvconn_dialboxw (HWND hwnd, LPWSTR szInOutConnStr,
    DWORD cbInOutConnStr, int *sqlStat, SQLUSMALLINT fDriverCompletion,
    UWORD * config);

SQLRETURN SQL_API _iodbcdm_drvchoose_dialbox (HWND hwnd, LPSTR szInOutDrvStr,
    DWORD cbInOutDrvStr, int *sqlStat);
SQLRETURN SQL_API _iodbcdm_trschoose_dialbox (HWND hwnd, LPSTR szInOutDrvStr,
    DWORD cbInOutDrvStr, int *sqlStat);

void SQL_API _iodbcdm_errorbox (HWND hwnd, LPCSTR szDSN, LPCSTR szText);
void SQL_API _iodbcdm_messagebox (HWND hwnd, LPCSTR szDSN, LPCSTR szText);
BOOL SQL_API _iodbcdm_confirmbox (HWND hwnd, LPCSTR szDSN, LPCSTR szText);
void _iodbcdm_nativeerrorbox (HWND hwnd, HENV henv, HDBC hdbc, HSTMT hstmt);

SQLRETURN SQL_API _iodbcdm_admin_dialbox (HWND hwnd);

typedef SQLRETURN SQL_API (*pAdminBoxFunc) (HWND hwnd);
typedef SQLRETURN SQL_API (*pTrsChooseFunc) (HWND hwnd, LPSTR szInOutDrvStr,
    DWORD cbInOutDrvStr, int *sqlStat);
typedef SQLRETURN SQL_API (*pDrvConnFunc) (HWND hwnd, LPSTR szInOutConnStr,
    DWORD cbInOutConnStr, int *sqlStat, SQLUSMALLINT fDriverCompletion,
    UWORD * config);
#endif
