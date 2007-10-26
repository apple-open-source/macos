/*
 *  gui.h
 *
 *  $Id: gui.h,v 1.9 2006/07/10 13:46:56 source Exp $
 *
 *  The iODBC driver manager.
 *
 *  Copyright (C) 1996-2006 by OpenLink Software <iodbc@openlinksw.com>
 *  All Rights Reserved.
 *
 *  This software is released under the terms of either of the following
 *  licenses:
 *
 *      - GNU Library General Public License (see LICENSE.LGPL)
 *      - The BSD License (see LICENSE.BSD).
 *
 *  Note that the only valid version of the LGPL license as far as this
 *  project is concerned is the original GNU Library General Public License
 *  Version 2, dated June 1991.
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
 *  License as published by the Free Software Foundation; only
 *  Version 2 of the License dated June 1991.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
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


#include <iodbc.h>
#include <odbcinst.h>
#include "iodbcadm.h"

#if defined(__BEOS__)
#  include "be/gui.h"
#elif defined(_MAC)
#  include "mac/gui.h"
#elif defined(__GTK__)
#  include "gtk/gui.h"
#elif defined(_MACX)
#  include "macosx/gui.h"
#else
#  error GUI for this platform not supported ...
#endif

#ifndef	_GUI_H
#define _GUI_H

BOOL create_confirm (HWND hwnd, LPCSTR dsn, LPCSTR text);
BOOL create_confirmw (HWND hwnd, LPCWSTR dsn, LPCWSTR text);

#if 0
 void create_login (HWND hwnd, LPCSTR username, LPCSTR password, LPCSTR dsn,
     TLOGIN * log_t);
#endif

void create_dsnchooser (HWND hwnd, TDSNCHOOSER * choose_t);
void create_driverchooser (HWND hwnd, TDRIVERCHOOSER * choose_t);
void create_fdriverchooser (HWND hwnd, TFDRIVERCHOOSER * choose_t);
void create_translatorchooser (HWND hwnd, TTRANSLATORCHOOSER * choose_t);
void create_administrator (HWND hwnd);
void create_error (HWND hwnd, LPCSTR dsn, LPCSTR text, LPCSTR errmsg);
void create_errorw (HWND hwnd, LPCWSTR dsn, LPCWSTR text, LPCWSTR errmsg);
void create_message (HWND hwnd, LPCSTR dsn, LPCSTR text);
void create_messagew (HWND hwnd, LPCWSTR dsn, LPCWSTR text);
LPSTR create_driversetup (HWND hwnd, LPCSTR driver, LPCSTR attrs, BOOL add, BOOL user);
LPSTR create_filedsn (HWND hwnd);
BOOL create_connectionpool (HWND hwnd, TCONNECTIONPOOLING *choose_t);

typedef SQLRETURN SQL_API (*pSQLGetInfoFunc) (SQLHDBC hdbc,
    SQLUSMALLINT fInfoType, SQLPOINTER rgbInfoValue,
    SQLSMALLINT cbInfoValueMax, SQLSMALLINT * pcbInfoValue);
typedef SQLRETURN SQL_API (*pSQLAllocHandle) (SQLSMALLINT hdl_type,
    SQLHANDLE hdl_in, SQLHANDLE * hdl_out);
typedef SQLRETURN SQL_API (*pSQLAllocEnv) (SQLHENV * henv);
typedef SQLRETURN SQL_API (*pSQLAllocConnect) (SQLHENV henv, SQLHDBC * hdbc);
typedef SQLRETURN SQL_API (*pSQLFreeHandle) (SQLSMALLINT hdl_type,
    SQLHANDLE hdl_in);
typedef SQLRETURN SQL_API (*pSQLFreeEnv) (SQLHENV henv);
typedef SQLRETURN SQL_API (*pSQLFreeConnect) (SQLHDBC hdbc);
	
#endif
