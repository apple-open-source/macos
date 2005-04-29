/*
 *  SQLWriteDSNToIni.c
 *
 *  $Id: SQLWriteDSNToIni.c,v 1.2 2004/08/10 22:20:29 luesang Exp $
 *
 *  Write a DSN connect string to a file
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

#include "inifile.h"
#include "misc.h"
#include "iodbc_error.h"

extern BOOL ValidDSN (LPCSTR lpszDSN);

extern int GetPrivateProfileString (LPCSTR lpszSection, LPCSTR lpszEntry,
    LPCSTR lpszDefault, LPSTR lpszRetBuffer, int cbRetBuffer,
    LPCSTR lpszFilename);

BOOL
WriteDSNToIni (LPCSTR lpszDSN, LPCSTR lpszDriver)
{
  char szBuffer[4096];
  BOOL retcode = FALSE;
  PCONFIG pCfg = NULL;

  if (_iodbcdm_cfg_search_init (&pCfg, "odbc.ini", TRUE))
    {
      PUSH_ERROR (ODBC_ERROR_REQUEST_FAILED);
      goto done;
    }

  if (strcmp (lpszDSN, "Default"))
    {
      /* adds a DSN=Driver to the [ODBC data sources] section */
#ifdef WIN32
      if (_iodbcdm_cfg_write (pCfg, "ODBC 32 bit Data Sources",
	      (LPSTR) lpszDSN, (LPSTR) lpszDriver))
#else
      if (_iodbcdm_cfg_write (pCfg, "ODBC Data Sources", (LPSTR) lpszDSN,
	      (LPSTR) lpszDriver))
#endif
	{
	  PUSH_ERROR (ODBC_ERROR_REQUEST_FAILED);
	  goto done;
	}
    }

  /* deletes the DSN section in odbc.ini */
  if (_iodbcdm_cfg_write (pCfg, (LPSTR) lpszDSN, NULL, NULL))
    {
      PUSH_ERROR (ODBC_ERROR_REQUEST_FAILED);
      goto done;
    }

  /* gets the file of the driver if lpszDriver is a valid description */
  wSystemDSN = USERDSN_ONLY;
  if (!GetPrivateProfileString ((LPSTR) lpszDriver, "Driver", "", szBuffer,
	  sizeof (szBuffer) - 1, "odbcinst.ini"))
    {
      wSystemDSN = SYSTEMDSN_ONLY;

      if (!GetPrivateProfileString ((LPSTR) lpszDriver, "Driver", "",
	      szBuffer, sizeof (szBuffer) - 1, "odbcinst.ini"))
	{
	  PUSH_ERROR (ODBC_ERROR_REQUEST_FAILED);
	  goto done;
	}
    }

  /* adds a [DSN] section with Driver key */
  if (_iodbcdm_cfg_write (pCfg, (LPSTR) lpszDSN, "Driver", szBuffer)
      || _iodbcdm_cfg_commit (pCfg))
    {
      PUSH_ERROR (ODBC_ERROR_REQUEST_FAILED);
      goto done;
    }

  retcode = TRUE;

done:
  wSystemDSN = USERDSN_ONLY;
  configMode = ODBC_BOTH_DSN;
  if (pCfg)
    _iodbcdm_cfg_done (pCfg);
  return retcode;
}


BOOL INSTAPI
SQLWriteDSNToIni (LPCSTR lpszDSN, LPCSTR lpszDriver)
{
  BOOL retcode = FALSE;

  /* Check input parameters */
  CLEAR_ERROR ();
  if (!lpszDSN || !ValidDSN (lpszDSN) || !STRLEN (lpszDSN))
    {
      PUSH_ERROR (ODBC_ERROR_INVALID_DSN);
      goto quit;
    }

  if (!lpszDriver || !STRLEN (lpszDriver))
    {
      PUSH_ERROR (ODBC_ERROR_INVALID_NAME);
      goto quit;
    }

  switch (configMode)
    {
    case ODBC_USER_DSN:
      wSystemDSN = USERDSN_ONLY;
      retcode = WriteDSNToIni (lpszDSN, lpszDriver);
      goto quit;

    case ODBC_SYSTEM_DSN:
      wSystemDSN = SYSTEMDSN_ONLY;
      retcode = WriteDSNToIni (lpszDSN, lpszDriver);
      goto quit;

    case ODBC_BOTH_DSN:
      wSystemDSN = USERDSN_ONLY;
      retcode = WriteDSNToIni (lpszDSN, lpszDriver);
      if (!retcode)
	{
	  CLEAR_ERROR ();
	  wSystemDSN = SYSTEMDSN_ONLY;
	  retcode = WriteDSNToIni (lpszDSN, lpszDriver);
	}
      goto quit;
    };

  PUSH_ERROR (ODBC_ERROR_GENERAL_ERR);
  goto quit;

quit:
  wSystemDSN = USERDSN_ONLY;
  configMode = ODBC_BOTH_DSN;
  return retcode;
}
