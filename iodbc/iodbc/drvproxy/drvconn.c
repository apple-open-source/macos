/*
 *  drvconn.c
 *
 *  $Id: drvconn.c,v 1.1.1.1 2002/04/08 22:48:09 miner Exp $
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

#include <iodbc.h>
#include <herr.h>
#include <dlproc.h>

SQLRETURN SQL_API
_iodbcdm_drvconn_dialbox (HWND hwnd,
    LPSTR szInOutConnStr, DWORD cbInOutConnStr, int FAR * sqlStat)
{
  RETCODE retcode = SQL_ERROR;
  char *szDSN = NULL, *szDriver = NULL, *curr;
  char dsnbuf[SQL_MAX_DSN_LENGTH + 1];
  int driver_type = -1, flags = 0, i;

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

  /* Retrieve the corresponding driver */
  /*if( strstr(szDriver, "OpenLink") || strstr(szDriver, "Openlink") || strstr(szDriver, "oplodbc") )
     driver_type = 0;
     else if( (strstr(szDriver, "Virtuoso") || strstr(szDriver, "virtodbc")) )
     driver_type = 1; */

  switch (driver_type)
    {
    case 0:
      /*curr = create_oplsetup(hwnd, szDSN, szInOutConnStr, TRUE);
         if( curr && curr!=(LPSTR)-1L )
         {
         STRNCPY(szInOutConnStr, curr, (cbInOutConnStr==SQL_NTS) ? STRLEN(curr) : cbInOutConnStr);
         free(curr);
         } */

      break;

    case 1:
      /*if( szFile )
         {
         curr = create_virtsetup(hwnd, szDSN, szInOutConnStr, TRUE);
         if( curr && curr!=(LPSTR)-1L )
         {
         STRNCPY(szInOutConnStr, curr, (cbInOutConnStr==SQL_NTS) ? STRLEN(curr) : cbInOutConnStr);
         free(curr);
         }
         } */
      break;

    default:
      /*if( szFile )
         {
         curr = create_gensetup(hwnd, szDSN, szInOutConnStr, TRUE);
         if( curr && curr!=(LPSTR)-1L )
         {
         STRNCPY(szInOutConnStr, curr, (cbInOutConnStr==SQL_NTS) ? STRLEN(curr) : cbInOutConnStr);
         free(curr);
         }
         } */
      break;
    };

  retcode = SQL_SUCCESS;

quit:
  return retcode;
}
