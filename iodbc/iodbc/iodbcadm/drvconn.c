/*
 *  drvconn.c
 *
 *  $Id: drvconn.c,v 1.1.1.1 2002/04/08 22:48:10 miner Exp $
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
#define CALL_DRVCONN_DIALBOX(path) \
	if ((handle = DLL_OPEN(path)) != NULL) \
	{ \
		if ((pDrvConn = (pDrvConnFunc)DLL_PROC(handle, "_iodbcdm_drvconn_dialbox")) != NULL) \
		{ \
	  	if (pDrvConn (hwnd, connstr, cbInOutConnStr, sqlStat) == SQL_SUCCESS) \
	  	{ \
	    	DLL_CLOSE(handle); \
	    	retcode = SQL_SUCCESS; \
	    	goto login; \
	  	} \
		} \
		DLL_CLOSE(handle); \
	}
#endif

extern SQLRETURN iodbcdm_loginbox (HWND, LPSTR, DWORD, int FAR *);
extern void create_translatorchooser (HWND, TTRANSLATORCHOOSER *);

SQLRETURN SQL_API
iodbcdm_drvconn_dialbox (
    HWND hwnd,
    LPSTR szInOutConnStr,
    DWORD cbInOutConnStr,
    int FAR * sqlStat)
{
  RETCODE retcode = SQL_ERROR;
  TDSNCHOOSER choose_t;
  UWORD configMode = ODBC_BOTH_DSN;
  char *szDSN = NULL, *curr, *cour, *connstr = NULL, *szDriver = NULL;
  char tokenstr[4096], eltstr[4096], drvbuf[4096];
  HDLL handle;
  pDrvConnFunc pDrvConn;
  int i;
#ifdef _MACX
  CFStringRef libname;
  CFBundleRef bundle;
  CFURLRef liburl;
  char name[1024] = { 0 };
#endif

  /* Check input parameters */
  if (!hwnd || !szInOutConnStr || cbInOutConnStr < 1)
    goto quit;

  /* Check if the DSN is already set or DRIVER */
  for (curr = szInOutConnStr; *curr; curr += (STRLEN (curr) + 1))
    {
      if (!strncasecmp (curr, "DSN=", STRLEN ("DSN=")))
	szDSN = curr + STRLEN ("DSN=");
      if (!strncasecmp (curr, "DRIVER=", STRLEN ("DRIVER=")))
	szDriver = curr + STRLEN ("DRIVER=");
    }

  if (!szDSN)
    {
      create_dsnchooser (hwnd, &choose_t);

      /* Check output parameters */
      if (choose_t.dsn)
	{
	  if (cbInOutConnStr > STRLEN (choose_t.dsn) + STRLEN ("DSN="))
	    {
	      sprintf (szInOutConnStr, "DSN=%s", choose_t.dsn);
	      szDSN = szInOutConnStr + STRLEN ("DSN=");
	      retcode = SQL_SUCCESS;
	    }
	  else
	    {
	      if (sqlStat)
#if (ODBCVER>=0x3000)
		*sqlStat = en_HY092;
#else
		*sqlStat = en_S1000;
#endif
	      retcode = SQL_ERROR;
	    }
	}
      else
	retcode = SQL_NO_DATA;

      if (choose_t.dsn)
	free (choose_t.dsn);
      if (retcode != SQL_SUCCESS)
	goto quit;
    }

  if (szDSN == NULL || szDSN[0] == '\0')
    szDSN = "default";

  /* Get the config mode */
  SQLGetConfigMode (&configMode);

  /* Read the file DSN and check if enough parameters are provided */
  connstr = (LPSTR) malloc (sizeof (char) * cbInOutConnStr);
  if (!connstr)
    {
      if (sqlStat)
#if (ODBCVER>=0x3000)
	*sqlStat = en_HY092;
#else
	*sqlStat = en_S1000;
#endif
      retcode = SQL_ERROR;
      goto quit;
    }

  sprintf (connstr, "DSN=%s", szDSN);
  i = STRLEN (connstr) + 1;

  /* Retrieve some information */
  SQLSetConfigMode (configMode);
  if (SQLGetPrivateProfileString (szDSN, NULL, "", tokenstr,
	  sizeof (tokenstr), NULL))
    for (curr = connstr + i, cour = tokenstr; *cour;
	i += (STRLEN (curr) + 1), cour += (STRLEN (cour) + 1), curr +=
	(STRLEN (curr) + 1))
      {
	SQLSetConfigMode (configMode);
	SQLGetPrivateProfileString (szDSN, cour, "", eltstr, sizeof (eltstr),
	    NULL);

	if (i + STRLEN (eltstr) + STRLEN (cour) + 2 < cbInOutConnStr)
	  {
	    STRCPY (curr, cour);
	    STRCAT (curr, "=");
	    STRCAT (curr, eltstr);
	  }
	else
	  {
	    if (sqlStat)
#if (ODBCVER>=0x3000)
	      *sqlStat = en_HY092;
#else
	      *sqlStat = en_S1000;
#endif
	    retcode = SQL_ERROR;
	    goto quit;
	  }
      }
  else
    memcpy (connstr, szInOutConnStr, cbInOutConnStr);

  *curr = 0;

  SQLSetConfigMode (configMode);
#ifdef WIN32
  if (SQLGetPrivateProfileString ("ODBC 32 bit Data Sources", szDSN, "",
	  tokenstr, sizeof (tokenstr), NULL))
#else
  if (SQLGetPrivateProfileString ("ODBC Data Sources", szDSN, "", tokenstr,
	  sizeof (tokenstr), NULL))
#endif
    szDriver = tokenstr;

  /* Call the _iodbcadm_drvconn_dialbox of the specific driver */
  SQLSetConfigMode (ODBC_USER_DSN);
  if (SQLGetPrivateProfileString (szDriver, "Driver", "", drvbuf,
	  sizeof (drvbuf), "odbcinst.ini"))
    CALL_DRVCONN_DIALBOX (drvbuf);
  if (SQLGetPrivateProfileString (szDriver, "Setup", "", drvbuf,
	  sizeof (drvbuf), "odbcinst.ini"))
    CALL_DRVCONN_DIALBOX (drvbuf);
  if (szDriver && !access (szDriver, X_OK))
    CALL_DRVCONN_DIALBOX (szDriver);
  if (SQLGetPrivateProfileString ("Default", "Driver", "", drvbuf,
	  sizeof (drvbuf), "odbcinst.ini"))
    CALL_DRVCONN_DIALBOX (drvbuf);
  if (SQLGetPrivateProfileString ("Default", "Setup", "", drvbuf,
	  sizeof (drvbuf), "odbcinst.ini"))
    CALL_DRVCONN_DIALBOX (drvbuf);

  SQLSetConfigMode (ODBC_SYSTEM_DSN);
  if (SQLGetPrivateProfileString (szDriver, "Driver", "", drvbuf,
	  sizeof (drvbuf), "odbcinst.ini"))
    CALL_DRVCONN_DIALBOX (drvbuf);
  if (SQLGetPrivateProfileString (szDriver, "Setup", "", drvbuf,
	  sizeof (drvbuf), "odbcinst.ini"))
    CALL_DRVCONN_DIALBOX (drvbuf);
  if (szDriver && !access (szDriver, X_OK))
    CALL_DRVCONN_DIALBOX (szDriver);
  if (SQLGetPrivateProfileString ("Default", "Driver", "", drvbuf,
	  sizeof (drvbuf), "odbcinst.ini"))
    CALL_DRVCONN_DIALBOX (drvbuf);
  if (SQLGetPrivateProfileString ("Default", "Setup", "", drvbuf,
	  sizeof (drvbuf), "odbcinst.ini"))
    CALL_DRVCONN_DIALBOX (drvbuf);

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
	  CALL_DRVCONN_DIALBOX (name);
	}
      if (liburl)
	CFRelease (liburl);
      if (libname)
	CFRelease (libname);
      CFRelease (bundle);
    }
#else
  CALL_DRVCONN_DIALBOX ("libdrvproxy.so");
#endif

  if (sqlStat)
    *sqlStat = en_IM003;
  goto quit;

login:
  if (iodbcdm_loginbox (hwnd, connstr, cbInOutConnStr,
	  sqlStat) != SQL_SUCCESS)
    goto quit;

  retcode = SQL_SUCCESS;

quit:
  if (connstr)
    {
      for (i = 0; connstr[i] || connstr[i + 1]; i++)
	if (!connstr[i])
	  connstr[i] = ';';
      STRNCPY (szInOutConnStr, connstr,
	  (cbInOutConnStr !=
	      SQL_NTS) ? cbInOutConnStr : STRLEN (connstr) + 1);
      free (connstr);
    }

  return retcode;
}


SQLRETURN SQL_API
_iodbcdm_drvchoose_dialbox (
    HWND hwnd,
    LPSTR szInOutConnStr,
    DWORD cbInOutConnStr,
    int FAR * sqlStat)
{
  RETCODE retcode = SQL_ERROR;
  TDRIVERCHOOSER choose_t;

  /* Check input parameters */
  if (!hwnd || !szInOutConnStr || cbInOutConnStr < 1)
    goto quit;

  create_driverchooser (hwnd, &choose_t);

  /* Check output parameters */
  if (choose_t.driver)
    {
      if (cbInOutConnStr > STRLEN (choose_t.driver) + STRLEN ("DRIVER="))
	{
	  sprintf (szInOutConnStr, "DRIVER=%s", choose_t.driver);
	  retcode = SQL_SUCCESS;
	}
      else
	{
	  if (sqlStat)
#if (ODBCVER>=0x3000)
	    *sqlStat = en_HY092;
#else
	    *sqlStat = en_S1000;
#endif
	  retcode = SQL_ERROR;
	}
    }
  else
    retcode = SQL_NO_DATA;

  if (choose_t.driver)
    free (choose_t.driver);

quit:
  return retcode;
}


SQLRETURN SQL_API
_iodbcdm_admin_dialbox (HWND hwnd)
{
  RETCODE retcode = SQL_ERROR;

  /* Check input parameters */
  if (!hwnd)
    goto quit;

  create_administrator (hwnd);
  retcode = SQL_SUCCESS;

quit:
  return retcode;
}


SQLRETURN SQL_API
_iodbcdm_trschoose_dialbox (
    HWND hwnd,
    LPSTR szInOutConnStr,
    DWORD cbInOutConnStr,
    int FAR * sqlStat)
{
  RETCODE retcode = SQL_ERROR;
  TTRANSLATORCHOOSER choose_t;

  /* Check input parameters */
  if (!hwnd || !szInOutConnStr || cbInOutConnStr < 1)
    goto quit;

  create_translatorchooser (hwnd, &choose_t);

  /* Check output parameters */
  if (choose_t.translator)
    {
      if (cbInOutConnStr >
	  STRLEN (choose_t.translator) + STRLEN ("TranslationName="))
	{
	  sprintf (szInOutConnStr, "TranslationName=%s", choose_t.translator);
	  retcode = SQL_SUCCESS;
	}
      else
	{
	  if (sqlStat)
#if (ODBCVER>=0x3000)
	    *sqlStat = en_HY092;
#else
	    *sqlStat = en_S1000;
#endif
	  retcode = SQL_ERROR;
	}
    }
  else
    retcode = SQL_NO_DATA;

  if (choose_t.translator)
    free (choose_t.translator);

quit:
  return retcode;
}
