/*
 *  drvconn.c
 *
 *  $Id: drvconn.c,v 1.3 2004/11/11 01:52:39 luesang Exp $
 *
 *  The data_sources dialog for SQLDriverConnect
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
#include <unicode.h>

SQLRETURN SQL_API
iodbcdm_drvconn_dialbox (
    HWND hwnd,
    LPSTR szInOutConnStr,
    DWORD cbInOutConnStr,
    int * sqlStat,
    SQLUSMALLINT fDriverCompletion,
	 UWORD *config)
{
  RETCODE retcode = SQL_ERROR;
  TDSNCHOOSER choose_t;

  /* Check input parameters */
  if (!szInOutConnStr || cbInOutConnStr < 1)
    goto quit;

  /* Display the DSN chooser dialog box */
  create_dsnchooser (hwnd, &choose_t);

  /* Check output parameters */
  if (choose_t.dsn)
    {
      /* Change the config mode */
      switch (choose_t.type_dsn)
		  {
		    case USER_DSN:
				*config = ODBC_USER_DSN;
				break;
		    case SYSTEM_DSN:
				*config = ODBC_SYSTEM_DSN;
			   break;
		  };

      /* Try to copy the DSN */
      if (cbInOutConnStr > STRLEN (choose_t.dsn) + STRLEN ("DSN="))
		  {
#ifdef _MAC
	  STRCPY (szInOutConnStr, "DSN=");
			 STRCAT (szInOutConnStr, choose_t.dsn);
#else
	  sprintf (szInOutConnStr, "DSN=%s", choose_t.dsn);
#endif
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
		  }
    }
  else
    retcode = SQL_NO_DATA_FOUND;

quit:
  return retcode;
}


SQLRETURN SQL_API
iodbcdm_drvconn_dialboxw (
    HWND hwnd,
    LPWSTR szInOutConnStr,
    DWORD cbInOutConnStr,
    int * sqlStat,
    SQLUSMALLINT fDriverCompletion,
    UWORD *config)
{
  RETCODE retcode = SQL_ERROR;
  LPSTR _szInOutConnStr = NULL;

  /* Check input parameters */
  if (!szInOutConnStr || cbInOutConnStr < 1)
    goto quit;

  if ((_szInOutConnStr =
	  malloc (cbInOutConnStr * UTF8_MAX_CHAR_LEN + 1)) == NULL)
    {
      *sqlStat = en_S1001;
      goto quit;
    }

  retcode = iodbcdm_drvconn_dialbox (hwnd, _szInOutConnStr,
    cbInOutConnStr * UTF8_MAX_CHAR_LEN, sqlStat, fDriverCompletion, config);

  MEM_FREE (_szInOutConnStr);

quit:
  return retcode;
}


SQLRETURN SQL_API
_iodbcdm_drvchoose_dialbox (
    HWND hwnd,
    LPSTR szInOutConnStr,
    DWORD cbInOutConnStr,
    int * sqlStat)
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
    int * sqlStat)
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
