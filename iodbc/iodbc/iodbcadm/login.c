/*
 *  login.c
 *
 *  $Id: login.c,v 1.1.1.1 2002/04/08 22:48:10 miner Exp $
 *
 *  The data_sources dialog for SQLDriverConnect and a login box procedures
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

#include "gui.h"

#include <herr.h>
#include <dlproc.h>

#ifndef WIN32
#include <unistd.h>
#define CALL_LOGIN_DIALBOX(path) \
	if ((handle = DLL_OPEN(path)) != NULL) \
	{ \
		if ((pLogin = (pLoginFunc)DLL_PROC(handle, "_iodbcdm_loginbox")) != NULL) \
		{ \
	  	if (pLogin (hwnd, szInOutConnStr, cbInOutConnStr, sqlStat) == SQL_SUCCESS) \
	  	{ \
	    	DLL_CLOSE(handle); \
	    	goto done; \
	  	} \
		} \
		DLL_CLOSE(handle); \
	}
#endif

SQLRETURN SQL_API
iodbcdm_loginbox (
    HWND hwnd,
    LPSTR szInOutConnStr,
    DWORD cbInOutConnStr,
    int FAR * sqlStat)
{
  RETCODE retcode = SQL_ERROR;
  char *szUID = NULL, *szPWD = NULL, *szDSN = NULL, *curr, *szDriver = NULL;
  char tokenstr[4096], drvbuf[4096];
  HDLL handle;
  pLoginFunc pLogin;
#ifdef _MACX
  CFStringRef libname;
  CFBundleRef bundle;
  CFURLRef liburl;
  char name[1024] = { 0 };
#endif

  /* Check input parameters */
  if (!hwnd || !szInOutConnStr || cbInOutConnStr < 1)
    goto quit;

  /* Check if the user and password are put */
  for (curr = szInOutConnStr; *curr; curr += (STRLEN (curr) + 1))
    {
      if (!strncasecmp (curr, "DSN=", STRLEN ("DSN=")))
	szDSN = curr + STRLEN ("DSN=");
      if (!strncasecmp (curr, "UID=", STRLEN ("UID=")))
	szUID = curr + STRLEN ("UID=");
      if (!strncasecmp (curr, "Username=", STRLEN ("Username=")))
	szUID = curr + STRLEN ("Username=");
      if (!strncasecmp (curr, "LastUser=", STRLEN ("LastUser=")))
	szUID = curr + STRLEN ("LastUser=");
      if (!strncasecmp (curr, "PWD=", STRLEN ("PWD=")))
	szPWD = curr + STRLEN ("PWD=");
      if (!strncasecmp (curr, "Password=", STRLEN ("Password=")))
	szPWD = curr + STRLEN ("Password=");
      if (!strncasecmp (curr, "DRIVER=", STRLEN ("DRIVER=")))
	szDriver = curr + STRLEN ("DRIVER=");
    }

#ifdef WIN32
  if (SQLGetPrivateProfileString ("ODBC 32 bit Data Sources", szDSN, "",
	  tokenstr, sizeof (tokenstr), NULL))
#else
  if (SQLGetPrivateProfileString ("ODBC Data Sources", szDSN, "", tokenstr,
	  sizeof (tokenstr), NULL))
#endif
    szDriver = tokenstr;

  if (!szUID || !szPWD)
    {
      /* Call the _iodbcadm_loginbox of the specific driver */
      SQLSetConfigMode (ODBC_USER_DSN);
      if (SQLGetPrivateProfileString (szDriver, "Driver", "", drvbuf,
	      sizeof (drvbuf), "odbcinst.ini"))
	CALL_LOGIN_DIALBOX (drvbuf);
      if (SQLGetPrivateProfileString (szDriver, "Setup", "", drvbuf,
	      sizeof (drvbuf), "odbcinst.ini"))
	CALL_LOGIN_DIALBOX (drvbuf);
      if (szDriver && !access (szDriver, X_OK))
	CALL_LOGIN_DIALBOX (szDriver);
      if (SQLGetPrivateProfileString ("Default", "Driver", "", drvbuf,
	      sizeof (drvbuf), "odbcinst.ini"))
	CALL_LOGIN_DIALBOX (drvbuf);
      if (SQLGetPrivateProfileString ("Default", "Setup", "", drvbuf,
	      sizeof (drvbuf), "odbcinst.ini"))
	CALL_LOGIN_DIALBOX (drvbuf);

      SQLSetConfigMode (ODBC_SYSTEM_DSN);
      if (SQLGetPrivateProfileString (szDriver, "Driver", "", drvbuf,
	      sizeof (drvbuf), "odbcinst.ini"))
	CALL_LOGIN_DIALBOX (drvbuf);
      if (SQLGetPrivateProfileString (szDriver, "Setup", "", drvbuf,
	      sizeof (drvbuf), "odbcinst.ini"))
	CALL_LOGIN_DIALBOX (drvbuf);
      if (szDriver && !access (szDriver, X_OK))
	CALL_LOGIN_DIALBOX (szDriver);
      if (SQLGetPrivateProfileString ("Default", "Driver", "", drvbuf,
	      sizeof (drvbuf), "odbcinst.ini"))
	CALL_LOGIN_DIALBOX (drvbuf);
      if (SQLGetPrivateProfileString ("Default", "Setup", "", drvbuf,
	      sizeof (drvbuf), "odbcinst.ini"))
	CALL_LOGIN_DIALBOX (drvbuf);

      /* The last ressort, a proxy driver */
#ifdef _MACX
      bundle = CFBundleGetBundleWithIdentifier (CFSTR ("org.iodbc.core"));
      if (bundle)
	{
	  /* Search for the drvproxy library */
	  liburl =
	      CFBundleCopyResourceURL (bundle, CFSTR ("iODBCdrvproxy.bundle"),
	      NULL, NULL);
	  if (liburl
	      && (libname =
		  CFURLCopyFileSystemPath (liburl, kCFURLPOSIXPathStyle)))
	    {
	      CFStringGetCString (libname, name, sizeof (name),
		  kCFStringEncodingASCII);
	      strcat (name, "/Contents/MacOS/iODBCdrvproxy");
	      CALL_LOGIN_DIALBOX (name);
	    }
	  if (liburl)
	    CFRelease (liburl);
	  if (libname)
	    CFRelease (libname);
	  CFRelease (bundle);
	}
#else
      CALL_LOGIN_DIALBOX ("libdrvproxy.so");
#endif

      if (sqlStat)
	*sqlStat = en_IM003;
      goto quit;
    }

done:
  retcode = SQL_SUCCESS;

quit:
  return retcode;
}
