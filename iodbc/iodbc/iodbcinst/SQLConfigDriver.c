/*
 *  SQLConfigDriver.c
 *
 *  $Id: SQLConfigDriver.c,v 1.1.1.1 2002/04/08 22:48:11 miner Exp $
 *
 *  Load the appropriate driver setup DLL and calls the ConfigDriver
 *  function.
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
#define CALL_CONFIG_DRIVER(driverpath) \
  if ((handle = DLL_OPEN((driverpath))) != NULL) \
	{ \
		if ((pConfigDriver = (pConfigDriverFunc)DLL_PROC(handle, "ConfigDriver")) != NULL) \
		{ \
			if (pConfigDriver (hwndParent, fRequest, lpszDriver, lpszArgs, lpszMsg, cbMsgMax, pcbMsgOut))  \
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

BOOL INSTAPI
SQLConfigDriver (HWND hwndParent, WORD fRequest, LPCSTR lpszDriver,
    LPCSTR lpszArgs, LPSTR lpszMsg, WORD cbMsgMax, WORD FAR *pcbMsgOut)
{
  PCONFIG pCfg;
  BOOL retcode = FALSE;
  void *handle;
  pConfigDriverFunc pConfigDriver;
#ifdef _MACX
  CFStringRef libname;
  CFBundleRef bundle;
  CFURLRef liburl;
  char name[1024] = { 0 };
#endif

  /* Check input parameters */
  CLEAR_ERROR ();
  if (!lpszDriver || !STRLEN (lpszDriver))
    {
      PUSH_ERROR (ODBC_ERROR_INVALID_NAME);
      goto quit;
    }

  /* Map the request User/System */
  if (fRequest < ODBC_INSTALL_DRIVER || fRequest > ODBC_CONFIG_DRIVER_MAX)
    {
      PUSH_ERROR (ODBC_ERROR_INVALID_REQUEST_TYPE);
      goto quit;
    }

  /* Get it from the user odbcinst file */
  wSystemDSN = USERDSN_ONLY;
  if (!_iodbcdm_cfg_search_init (&pCfg, "odbcinst.ini", TRUE))
    {
      if (!_iodbcdm_cfg_find (pCfg, (char *) lpszDriver, "Setup"))
	CALL_CONFIG_DRIVER (pCfg->value);
      if (!_iodbcdm_cfg_find (pCfg, (char *) lpszDriver, "Driver"))
	CALL_CONFIG_DRIVER (pCfg->value);
      if (!access (lpszDriver, X_OK))
	CALL_CONFIG_DRIVER (lpszDriver);
      if (!_iodbcdm_cfg_find (pCfg, "Default", "Setup"))
	CALL_CONFIG_DRIVER (pCfg->value);
      if (!_iodbcdm_cfg_find (pCfg, "Default", "Driver"))
	CALL_CONFIG_DRIVER (pCfg->value);
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
	CALL_CONFIG_DRIVER (pCfg->value);
      if (!_iodbcdm_cfg_find (pCfg, (char *) lpszDriver, "Driver"))
	CALL_CONFIG_DRIVER (pCfg->value);
      if (!access (lpszDriver, X_OK))
	CALL_CONFIG_DRIVER (lpszDriver);
      if (!_iodbcdm_cfg_find (pCfg, "Default", "Setup"))
	CALL_CONFIG_DRIVER (pCfg->value);
      if (!_iodbcdm_cfg_find (pCfg, "Default", "Driver"))
	CALL_CONFIG_DRIVER (pCfg->value);
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
	  CALL_CONFIG_DRIVER (name);
	}
      if (liburl)
	CFRelease (liburl);
      if (libname)
	CFRelease (libname);
      CFRelease (bundle);
    }
#else
  CALL_CONFIG_DRIVER ("libdrvproxy.so");
#endif

  /* Error : ConfigDriver could no be found */
  PUSH_ERROR (ODBC_ERROR_LOAD_LIB_FAILED);

done:
  if (pCfg)
    _iodbcdm_cfg_done (pCfg);

quit:
  wSystemDSN = USERDSN_ONLY;
  configMode = ODBC_BOTH_DSN;
  if (pcbMsgOut)
    *pcbMsgOut = 0;

  return retcode;
}
