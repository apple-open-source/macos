/*
 *  drvconn.c
 *
 *  $Id: drvconn.c,v 1.8 2006/01/20 15:58:34 source Exp $
 *
 *  The data_sources dialog for SQLDriverConnect and a login box procedures
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


#include "gui.h"

#include <iodbc.h>
#include <herr.h>
#include <dlproc.h>


SQLRETURN SQL_API
_iodbcdm_drvconn_dialbox (
    HWND	  hwnd,
    LPSTR	  szInOutConnStr,
    DWORD	  cbInOutConnStr,
    int	 	* sqlStat,
    SQLUSMALLINT  fDriverCompletion,
    UWORD	* config)
{
  RETCODE retcode = SQL_ERROR;
  char *szDSN = NULL, *szDriver = NULL, *szUID = NULL, *szPWD = NULL, *curr;
  TLOGIN log_t;

  /* Check input parameters */
  if (!hwnd || !szInOutConnStr || cbInOutConnStr < 1)
    goto quit;

  /* Check if the DSN is already set or DRIVER */
  for (curr = szInOutConnStr; *curr; curr += (STRLEN (curr) + 1))
    {
      if (!strncasecmp (curr, "DSN=", STRLEN ("DSN=")))
	{
	  szDSN = curr + STRLEN ("DSN=");
	  continue;
	}
      if (!strncasecmp (curr, "DRIVER=", STRLEN ("DRIVER=")))
	{
	  szDriver = curr + STRLEN ("DRIVER=");
	  continue;
	}
      if (!strncasecmp (curr, "UID=", STRLEN ("UID=")))
	{
	  szUID = curr + STRLEN ("UID=");
	  continue;
	}
      if (!strncasecmp (curr, "UserName=", STRLEN ("UserName=")))
	{
	  szUID = curr + STRLEN ("UserName=");
	  continue;
	}
      if (!strncasecmp (curr, "LastUser=", STRLEN ("LastUser=")))
	{
	  szUID = curr + STRLEN ("LastUser=");
	  continue;
	}
      if (!strncasecmp (curr, "PWD=", STRLEN ("PWD=")))
	{
	  szPWD = curr + STRLEN ("PWD=");
	  continue;
	}
      if (!strncasecmp (curr, "Password=", STRLEN ("Password=")))
	{
	  szPWD = curr + STRLEN ("Password=");
	  continue;
	}
    }

  if (fDriverCompletion != SQL_DRIVER_NOPROMPT && (!szUID || !szPWD))
    {
      create_login (hwnd, szUID, szPWD, szDSN ? szDSN : "(File DSN)", &log_t);

      if (log_t.user && !szUID)
	{
	  sprintf (curr, "UID=%s", log_t.user);
	  curr += STRLEN (curr);
	  *curr++ = '\0';
	  free (log_t.user);
	}

      if (log_t.pwd && !szPWD)
	{
	  sprintf (curr, "PWD=%s", log_t.pwd);
	  curr += STRLEN (curr);
	  *curr++ = '\0';
	  free (log_t.pwd);
	}

      /* add list-terminating '\0' */
      *curr = '\0';
    }

  retcode = log_t.ok ? SQL_SUCCESS : SQL_NO_DATA_FOUND;

quit:
  for (curr = szInOutConnStr; *curr; curr = szDSN + 1)
    {
      szDSN = curr + STRLEN (curr);
      if (szDSN[1])
	szDSN[0] = ';';
    }

  return retcode;
}
