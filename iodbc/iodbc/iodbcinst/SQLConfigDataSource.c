/*
 *  SQLConfigDataSource.c
 *
 *  $Id: SQLConfigDataSource.c,v 1.1.1.1 2002/04/08 22:48:11 miner Exp $
 *
 *  Add, modify or delete datasources
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

#include <iodbc.h>
#include <iodbcinst.h>

#ifdef _MACX
#  include <Carbon/Carbon.h>
#endif

#include "dlf.h"
#include "inifile.h"
#include "misc.h"
#include "iodbc_error.h"

#ifndef WIN32
#include <unistd.h>
#define CALL_CONFIG_DSN(path) \
	if ((handle = DLL_OPEN(path)) != NULL) \
	{ \
		if ((pConfigDSN = (pConfigDSNFunc)DLL_PROC(handle, "ConfigDSN")) != NULL) \
		{ \
	  	if (pConfigDSN(hwndParent, fRequest, lpszDriver, lpszAttributes)) \
	  	{ \
	    	DLL_CLOSE(handle); \
	    	retcode = TRUE; \
	    	goto done; \
	  	} \
			else \
			{ \
				PUSH_ERROR(ODBC_ERROR_REQUEST_FAILED); \
	    	DLL_CLOSE(handle); \
	    	retcode = FALSE; \
	    	goto done; \
			} \
		} \
		DLL_CLOSE(handle); \
	}
#endif

extern BOOL RemoveDSNFromIni (LPCSTR lpszDSN);

BOOL
RemoveDefaultDataSource (void)
{
  BOOL retcode = FALSE;
  PCONFIG pCfg = NULL;

  /* removes the default dsn */
  if (!RemoveDSNFromIni ("Default"))
    {
      PUSH_ERROR (ODBC_ERROR_REQUEST_FAILED);
      goto quit;
    }

  /* removes the default driver not regarding the current config mode */
  if (_iodbcdm_cfg_search_init (&pCfg, "odbcinst.ini", TRUE))
    {
      PUSH_ERROR (ODBC_ERROR_REQUEST_FAILED);
      goto quit;
    }

  _iodbcdm_cfg_write (pCfg, "Default", NULL, NULL);

  if (!_iodbcdm_cfg_commit (pCfg))
    retcode = TRUE;
  else
    {
      PUSH_ERROR (ODBC_ERROR_REQUEST_FAILED);
      goto quit;
    }

  /* check now the system only if it was the user first checked */
  if (wSystemDSN != SYSTEMDSN_ONLY)
    {
      if (pCfg)
	{
	  _iodbcdm_cfg_done (pCfg);
	  pCfg = NULL;
	}
      wSystemDSN = SYSTEMDSN_ONLY;

      if (_iodbcdm_cfg_search_init (&pCfg, "odbcinst.ini", TRUE))
	goto quit;
      _iodbcdm_cfg_write (pCfg, "Default", NULL, NULL);
      _iodbcdm_cfg_commit (pCfg);
    }

quit:
  if (pCfg)
    _iodbcdm_cfg_done (pCfg);
  return retcode;
}


BOOL INSTAPI
SQLConfigDataSource (HWND hwndParent, WORD fRequest, LPCSTR lpszDriver,
    LPCSTR lpszAttributes)
{
  PCONFIG pCfg = NULL;
  BOOL retcode = FALSE;
  void *handle;
  pConfigDSNFunc pConfigDSN;
#ifdef _MACX
  CFStringRef libname;
  CFBundleRef bundle;
  CFURLRef liburl;
  char name[1024] = { 0 };
#endif

  /* Check input parameters */
  CLEAR_ERROR ();

  /* Map the request User/System */
  switch (fRequest)
    {
    case ODBC_ADD_DSN:
    case ODBC_CONFIG_DSN:
    case ODBC_REMOVE_DSN:
      configMode = ODBC_USER_DSN;
      break;

    case ODBC_ADD_SYS_DSN:
    case ODBC_CONFIG_SYS_DSN:
    case ODBC_REMOVE_SYS_DSN:
      configMode = ODBC_SYSTEM_DSN;
      fRequest = fRequest - ODBC_ADD_SYS_DSN + ODBC_ADD_DSN;
      break;

    case ODBC_REMOVE_DEFAULT_DSN:
      retcode = RemoveDefaultDataSource ();
      goto resetdsnmode;

    default:
      PUSH_ERROR (ODBC_ERROR_INVALID_REQUEST_TYPE);
      goto resetdsnmode;
    };

  if (!lpszDriver || !STRLEN (lpszDriver))
    {
      PUSH_ERROR (ODBC_ERROR_INVALID_NAME);
      goto resetdsnmode;
    }

  /* Get it from the user odbcinst file */
  wSystemDSN = USERDSN_ONLY;
  if (!_iodbcdm_cfg_search_init (&pCfg, "odbcinst.ini", TRUE))
    {
      if (!_iodbcdm_cfg_find (pCfg, (char *) lpszDriver, "Setup"))
	CALL_CONFIG_DSN (pCfg->value);
      if (!_iodbcdm_cfg_find (pCfg, (char *) lpszDriver, "Driver"))
	CALL_CONFIG_DSN (pCfg->value);
      if (!access (lpszDriver, X_OK))
	CALL_CONFIG_DSN (lpszDriver);
      if (!_iodbcdm_cfg_find (pCfg, "Default", "Setup"))
	CALL_CONFIG_DSN (pCfg->value);
      if (!_iodbcdm_cfg_find (pCfg, "Default", "Driver"))
	CALL_CONFIG_DSN (pCfg->value);
    }

  /* Get it from the system odbcinst file */
  if (pCfg)
    {
      _iodbcdm_cfg_done (pCfg);
      pCfg = NULL;
    }
  wSystemDSN = SYSTEMDSN_ONLY;
  if (!_iodbcdm_cfg_search_init (&pCfg, "odbcinst.ini", TRUE))
    {
      if (!_iodbcdm_cfg_find (pCfg, (char *) lpszDriver, "Setup"))
	CALL_CONFIG_DSN (pCfg->value);
      if (!_iodbcdm_cfg_find (pCfg, (char *) lpszDriver, "Driver"))
	CALL_CONFIG_DSN (pCfg->value);
      if (!access (lpszDriver, X_OK))
	CALL_CONFIG_DSN (lpszDriver);
      if (!_iodbcdm_cfg_find (pCfg, "Default", "Setup"))
	CALL_CONFIG_DSN (pCfg->value);
      if (!_iodbcdm_cfg_find (pCfg, "Default", "Driver"))
	CALL_CONFIG_DSN (pCfg->value);
    }

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
	  CALL_CONFIG_DSN (name);
	}
      if (liburl)
	CFRelease (liburl);
      if (libname)
	CFRelease (libname);
      CFRelease (bundle);
    }
#else
  CALL_CONFIG_DSN ("libdrvproxy.so");
#endif

  /* Error : ConfigDSN could no be found */
  PUSH_ERROR (ODBC_ERROR_LOAD_LIB_FAILED);

done:
  if (pCfg)
    _iodbcdm_cfg_done (pCfg);

resetdsnmode:
  wSystemDSN = USERDSN_ONLY;
  configMode = ODBC_BOTH_DSN;

  return retcode;
}
