/*
 *  info.c
 *
 *  $Id: info.c,v 1.1.1.1 2002/04/08 22:48:10 miner Exp $
 *
 *  Information functions
 *
 *  The iODBC driver manager.
 *  
 *  Copyright (C) 1995 by Ke Jin <kejin@empress.com> 
 *  Copyright (C) 1996-2002 by OpenLink Software <iodbc@openlinksw.com>
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

#include <sql.h>
#include <sqlext.h>

#include <dlproc.h>

#include <herr.h>
#include <henv.h>
#include <hdbc.h>
#include <hstmt.h>

#include <itrace.h>

#include <stdio.h>
#include <ctype.h>

extern char *_iodbcdm_getinifile (char *buf, int size);

#define SECT1			"[ODBC Data Sources]"
#define SECT2			"Default"
#define SECT3			"ODBC Data Sources"
#define MAX_ENTRIES		1024

static int
stricmp (const char *s1, const char *s2)
{
  int cmp;

  while (*s1)
    {
      if ((cmp = toupper (*s1) - toupper (*s2)) != 0)
	return cmp;
      s1++;
      s2++;
    }
  return (*s2) ? -1 : 0;
}

static int
SectSorter (const void *p1, const void *p2)
{
  char **s1 = (char **) p1;
  char **s2 = (char **) p2;

  return stricmp (*s1, *s2);
}


SQLRETURN SQL_API
SQLDataSources (
    SQLHENV henv,
    SQLUSMALLINT fDir,
    SQLCHAR FAR * szDSN,
    SQLSMALLINT cbDSNMax,
    SQLSMALLINT FAR * pcbDSN,
    SQLCHAR FAR * szDesc,
    SQLSMALLINT cbDescMax,
    SQLSMALLINT FAR * pcbDesc)
{
  GENV (genv, henv);
  char *path;
  char buf[1024];
  FILE *fp;
  int i;
  static int cur_entry = -1;
  static int num_entries = 0;
  static char **sect = NULL;

  ODBC_LOCK ();
  if (!IS_VALID_HENV (genv))
    {
      ODBC_UNLOCK ();
      return SQL_INVALID_HANDLE;
    }
  CLEAR_ERRORS (genv);

  /* check argument */
  if (cbDSNMax < 0 || cbDescMax < 0)
    {
      PUSHSQLERR (genv->herr, en_S1090);
      ODBC_UNLOCK ();
      return SQL_ERROR;
    }

  if (fDir != SQL_FETCH_FIRST && fDir != SQL_FETCH_NEXT)
    {
      PUSHSQLERR (genv->herr, en_S1103);
      ODBC_UNLOCK ();
      return SQL_ERROR;
    }

  if (cur_entry < 0 || fDir == SQL_FETCH_FIRST)
    {
      cur_entry = 0;
      num_entries = 0;

      /* 
       *  Open the odbc.ini file
       */
      path = (char *) _iodbcdm_getinifile (buf, sizeof (buf));
      if ((fp = fopen (path, "r")) == NULL)
	{
	  ODBC_UNLOCK ();
	  return SQL_NO_DATA_FOUND;
	}
      /*
       *  Free old section list
       */
      if (sect)
	{
	  for (i = 0; i < MAX_ENTRIES; i++)
	    if (sect[i])
	      free (sect[i]);
	  free (sect);
	}
      if ((sect = (char **) calloc (MAX_ENTRIES, sizeof (char *))) == NULL)
	{
	  fclose (fp);
	  PUSHSQLERR (genv->herr, en_S1011);
	  ODBC_UNLOCK ();
	  return SQL_ERROR;
	}

      /* Finds the section heading [ODBC Data Sources] */
      while (1)
	{
	  char *str;

	  str = fgets (buf, sizeof (buf), fp);

	  if (str == NULL)
	    break;

	  if (strcmp (str, SECT1) >= 0)
	    break;
	}

      /*
       *  Build a dynamic list of sections
       */
      while (1)
	{
	  char *str, *p;

	  str = fgets (buf, sizeof (buf), fp);

	  if (str == NULL || *str == '[')
	    break;

	  if (*str == '\n')
	    continue;

	  for (p = str; *p; p++)
	    {
	      if (*p == '=' || *p == '\n')
		{
		  *p = '\0';

		  /*
		   *  Trim whitespace from the right
		   */
		  for (; p > str && (*(p - 1) == ' ' || *(p - 1) == '\t');)
		    *--p = '\0';
		  break;
		}
	    }

	  /* Add this section to the comma separated list */

	  if (num_entries >= MAX_ENTRIES)
	    break;		/* Skip the rest */

	  sect[num_entries++] = (char *) strdup (str);
	}
      fclose (fp);

      /*
       *  Sort all entries so we can present a nice list
       */
      if (num_entries > 1)
	qsort (sect, num_entries, sizeof (char *), SectSorter);
    }

  /*
   *  Try to get to the next item
   */
  if (cur_entry >= num_entries)
    {
      cur_entry = 0;		/* Next time, start all over again */
      ODBC_UNLOCK ();
      return SQL_NO_DATA_FOUND;
    }

  /*
   *  Copy DSN information 
   */
  STRNCPY (szDSN, sect[cur_entry], cbDSNMax);

  if (pcbDSN)
    *pcbDSN = strlen (sect[cur_entry]);


  /*
   *  And find the type description that goes with this entry
   */
#if 0
  _iodbcdm_getkeyvalbydsn (sect[cur_entry], strlen (sect[cur_entry]),
      "Description", szDesc, cbDescMax);
#else
  _iodbcdm_getkeyvalbydsn (SECT3, strlen (SECT3),
      sect[cur_entry], szDesc, cbDescMax);
#endif

  if (pcbDesc)
    *pcbDesc = strlen(szDesc);


  /*
   *  Next record
   */
  cur_entry++;

  ODBC_UNLOCK ();
  return SQL_SUCCESS;
}


SQLRETURN SQL_API
SQLDrivers (
    SQLHENV henv,
    SQLUSMALLINT fDir,
    SQLCHAR FAR * szDrvDesc,
    SQLSMALLINT cbDrvDescMax,
    SQLSMALLINT FAR * pcbDrvDesc,
    SQLCHAR FAR * szDrvAttr,
    SQLSMALLINT cbDrvAttrMax,
    SQLSMALLINT FAR * pcbDrvAttr)
{
  GENV (genv, henv);

  ODBC_LOCK ();
  if (!IS_VALID_HENV (genv))
    {
      ODBC_UNLOCK ();
      return SQL_INVALID_HANDLE;
    }
  CLEAR_ERRORS (genv);

  if (cbDrvDescMax < 0 || cbDrvAttrMax < 0 || cbDrvAttrMax == 1)
    {
      PUSHSQLERR (genv->herr, en_S1090);
      ODBC_UNLOCK ();
      return SQL_ERROR;
    }

  if (fDir != SQL_FETCH_FIRST && fDir != SQL_FETCH_NEXT)
    {
      PUSHSQLERR (genv->herr, en_S1103);
      ODBC_UNLOCK ();
      return SQL_ERROR;
    }
  if (!szDrvDesc || !szDrvAttr || !cbDrvDescMax || !cbDrvAttrMax)
    {
      PUSHSQLERR (genv->herr, en_01004);
      ODBC_UNLOCK ();
      return SQL_SUCCESS_WITH_INFO;
    }

/*********************/
  ODBC_UNLOCK ();
  return SQL_NO_DATA_FOUND;
}


SQLRETURN SQL_API
SQLGetInfo (
    SQLHDBC hdbc,
    SQLUSMALLINT fInfoType,
    SQLPOINTER rgbInfoValue,
    SQLSMALLINT cbInfoValueMax,
    SQLSMALLINT FAR * pcbInfoValue)
{
  CONN (pdbc, hdbc);
  ENV_t FAR *penv;
  STMT_t FAR *pstmt = NULL;
  STMT_t FAR *tpstmt;
  HPROC hproc;
  SQLRETURN retcode = SQL_SUCCESS;

  DWORD dword;
  int size = 0, len = 0;
  char buf[16] =
  {'\0'};

  ENTER_HDBC(pdbc);

  if (cbInfoValueMax < 0)
    {
      PUSHSQLERR (pdbc->herr, en_S1090);

      LEAVE_HDBC (pdbc, SQL_ERROR);
    }

#if (ODBCVER < 0x0300)
  if (				/* fInfoType < SQL_INFO_FIRST || */
      (fInfoType > SQL_INFO_LAST
	  && fInfoType < SQL_INFO_DRIVER_START))
    {
      PUSHSQLERR (pdbc->herr, en_S1096);

      LEAVE_HDBC (pdbc, SQL_ERROR);
    }
#endif
  if (fInfoType == SQL_ODBC_VER)
    {
      sprintf (buf, "%02d.%02d",
	  (ODBCVER) >> 8, 0x00FF & (ODBCVER));


      if (rgbInfoValue != NULL
	  && cbInfoValueMax > 0)
	{
	  len = STRLEN (buf);

	  if (len > cbInfoValueMax - 1)
	    {
	      len = cbInfoValueMax - 1;
	      PUSHSQLERR (pdbc->herr, en_01004);

	      retcode = SQL_SUCCESS_WITH_INFO;
	    }

	  STRNCPY (rgbInfoValue, buf, len);
	  ((char FAR *) rgbInfoValue)[len] = '\0';
	}

      if (pcbInfoValue != NULL)
	{
	  *pcbInfoValue = (SWORD) len;
	}

      LEAVE_HDBC (pdbc, retcode);
    }

  if (pdbc->state == en_dbc_allocated || pdbc->state == en_dbc_needdata)
    {
      PUSHSQLERR (pdbc->herr, en_08003);

      LEAVE_HDBC (pdbc, SQL_ERROR);
    }

  switch (fInfoType)
    {
    case SQL_DRIVER_HDBC:
      dword = (DWORD) (pdbc->dhdbc);
      size = sizeof (dword);
      break;

    case SQL_DRIVER_HENV:
      penv = (ENV_t FAR *) (pdbc->henv);
      dword = (DWORD) (penv->dhenv);
      size = sizeof (dword);
      break;

    case SQL_DRIVER_HLIB:
      penv = (ENV_t FAR *) (pdbc->henv);
      dword = (DWORD) (penv->hdll);
      size = sizeof (dword);
      break;

    case SQL_DRIVER_HSTMT:
      if (rgbInfoValue != NULL)
	{
	  pstmt = *((STMT_t FAR **) rgbInfoValue);
	}

      for (tpstmt = (STMT_t FAR *) (pdbc->hstmt);
	  tpstmt != NULL;
	  tpstmt = tpstmt->next)
	{
	  if (tpstmt == pstmt)
	    {
	      break;
	    }
	}

      if (tpstmt == NULL)
	{
	  PUSHSQLERR (pdbc->herr, en_S1009);

	  LEAVE_HDBC (pdbc, SQL_ERROR);
	}

      dword = (DWORD) (pstmt->dhstmt);
      size = sizeof (dword);
      break;

    default:
      break;
    }

  if (size)
    {
      if (rgbInfoValue != NULL)
	{
	  *((DWORD *) rgbInfoValue) = dword;
	}

      if (pcbInfoValue != NULL)
	{
	  *(pcbInfoValue) = (SWORD) size;
	}

      LEAVE_HDBC (pdbc, SQL_SUCCESS);
    }

#if (ODBCVER >= 0x0300)
  /*
   *  This was a temp value in ODBC 2
   */
  if (((ENV_t FAR *) pdbc->henv)->dodbc_ver == SQL_OV_ODBC2 && 
	  fInfoType == SQL_OJ_CAPABILITIES)
      fInfoType = 65003;
#endif /* ODBCVER >= 0x0300 */

  hproc = _iodbcdm_getproc (pdbc, en_GetInfo);

  if (hproc == SQL_NULL_HPROC)
    {
      PUSHSQLERR (pdbc->herr, en_IM001);

      LEAVE_HDBC (pdbc, SQL_ERROR);
    }

  CALL_DRIVER (hdbc, pdbc, retcode, hproc, en_GetInfo,
      (pdbc->dhdbc, fInfoType, rgbInfoValue, cbInfoValueMax, pcbInfoValue));

  if (retcode == SQL_ERROR
      && fInfoType == SQL_DRIVER_ODBC_VER)
    {
      STRCPY (buf, "01.00");

      if (rgbInfoValue != NULL
	  && cbInfoValueMax > 0)
	{
	  len = STRLEN (buf);

	  if (len < cbInfoValueMax - 1)
	    {
	      len = cbInfoValueMax - 1;
	      PUSHSQLERR (pdbc->herr, en_01004);
	    }

	  STRNCPY (rgbInfoValue, buf, len);
	  ((char FAR *) rgbInfoValue)[len] = '\0';
	}

      if (pcbInfoValue != NULL)
	{
	  *pcbInfoValue = (SWORD) len;
	}

      /* what should we return in this case ???? */
    }

  LEAVE_HDBC (pdbc, retcode);
}

static int FunctionNumbers[] =
{
    0
#define FUNCDEF(A,B,C)	,A
#include "henv.ci"
#undef FUNCDEF
};

#if (ODBCVER >= 0x0300)

#define SQL_ODBC3_SET_FUNC_ON(pfExists, uwAPI) \
	*( ((UWORD*) (pfExists)) + ((uwAPI) >> 4) ) |= (1 << ((uwAPI) & 0x000F))

#define SQL_ODBC3_SET_FUNC_OFF(pfExists, uwAPI) \
	*( ((UWORD*) (pfExists)) + ((uwAPI) >> 4) ) &= !(1 << ((uwAPI) & 0x000F))

#endif

SQLRETURN SQL_API
SQLGetFunctions (
    SQLHDBC hdbc,
    SQLUSMALLINT fFunc,
    SQLUSMALLINT FAR * pfExists)
{
  CONN (pdbc, hdbc);
  HPROC hproc;
  SQLRETURN retcode;
  int i;
  UWORD functions2[100];
#if (ODBCVER >= 0x0300)
  UWORD functions3[SQL_API_ODBC3_ALL_FUNCTIONS_SIZE];
#endif


  ENTER_HDBC (pdbc);

  if (pdbc->state == en_dbc_allocated
      || pdbc->state == en_dbc_needdata)
    {
      PUSHSQLERR (pdbc->herr, en_S1010);

      LEAVE_HDBC (pdbc, SQL_ERROR);
    }

  if (pfExists == NULL)
    {
      LEAVE_HDBC (pdbc, SQL_SUCCESS);
    }

  /*
   *  These functions are supported by the iODBC driver manager
   */
  if (fFunc == SQL_API_SQLDATASOURCES
      || fFunc == SQL_API_SQLDRIVERS
#if (ODBCVER >= 0x0300)
      || fFunc == SQL_API_SQLGETENVATTR
      || fFunc == SQL_API_SQLSETENVATTR
#endif
      )
    {
      *pfExists = (UWORD) 1;
      LEAVE_HDBC (pdbc, SQL_SUCCESS);
    }

  /*
   *  Check if function number is within ODBC version context
   */
#if (ODBCVER < 0x0300)
  if (fFunc > SQL_EXT_API_LAST)
    {
      PUSHSQLERR (pdbc->herr, en_S1095);

      LEAVE_HDBC (pdbc, SQL_ERROR);
    }
#endif

  /*
   *  In a ODBC 2.x driver context, the ODBC 3.x API calls are 
   *  mapped by the driver manager.
   */
#if (ODBCVER >= 0x0300)
  if (((ENV_t FAR *) pdbc->henv)->dodbc_ver == SQL_OV_ODBC2)
    {
      switch (fFunc)
	{
	case SQL_API_ALL_FUNCTIONS:
	case SQL_API_ODBC3_ALL_FUNCTIONS:
	  break;

	  /* Mapped ODBC3 app -> ODBC2 driver functions */
	case SQL_API_SQLALLOCHANDLE:
	case SQL_API_SQLFREEHANDLE:
	case SQL_API_SQLSETCONNECTATTR:
	case SQL_API_SQLGETCONNECTATTR:
	case SQL_API_SQLGETSTMTATTR:
	case SQL_API_SQLSETSTMTATTR:
	case SQL_API_SQLCOLATTRIBUTE:
	case SQL_API_SQLENDTRAN:
	case SQL_API_SQLBULKOPERATIONS:
	case SQL_API_SQLFETCHSCROLL:
	case SQL_API_SQLGETDIAGREC:
	case SQL_API_SQLGETDIAGFIELD:
	  *pfExists = SQL_TRUE;
	  LEAVE_HDBC (pdbc, SQL_SUCCESS);

	case SQL_API_SQLBINDPARAM:
	  fFunc = SQL_API_SQLBINDPARAMETER;
	  break;

	default:
	  if (fFunc > SQL_API_SQLBINDPARAMETER)
	    {
	      *pfExists = SQL_FALSE;

	      LEAVE_HDBC (pdbc, SQL_SUCCESS);
	    }
	  break;
	}
    }
#endif


  /*
   *  If the driver exports a SQLGetFunctions call, use it
   */
  hproc = _iodbcdm_getproc (pdbc, en_GetFunctions);

  if (hproc != SQL_NULL_HPROC)
    {
      CALL_DRIVER (hdbc, pdbc, retcode, hproc, en_GetFunctions,
	  (pdbc->dhdbc, fFunc, pfExists));

      LEAVE_HDBC (pdbc, retcode);
    }

  /*
   *  Map deprecated functions
   */
  if (fFunc == SQL_API_SQLSETPARAM)
    {
      fFunc = SQL_API_SQLBINDPARAMETER;
    }

  /*
   *  Initialize intermediate result arrays
   */
  memset (functions2, '\0', sizeof (functions2));
#if (ODBCVER > 0x0300)
  memset (functions3, '\0', sizeof (functions3));
#endif

  /*
   *  Build result array by scanning for all API calls
   */
  for (i = 1; i < __LAST_API_FUNCTION__; i++)
    {
      int j = FunctionNumbers[i];

      hproc = _iodbcdm_getproc (pdbc, i);

      if (hproc != SQL_NULL_HPROC)
	{
	  if (j < 100)
	    functions2[j] = 1;
#if (ODBCVER >= 0x0300)
	  functions3[j >> 4] |= (1 << (j & 0x000F));
#endif
	}
    }

  /*
   *  Finally return the information
   */
  if (fFunc == SQL_API_ALL_FUNCTIONS)
    {
      memcpy (pfExists, &functions2, sizeof (functions2));
    }
#if (ODBCVER < 0x0300)
  else
    {
      *pfExists = functions2[fFunc];
    }
#else
  else if (fFunc == SQL_API_ODBC3_ALL_FUNCTIONS)
    {
      memcpy (pfExists, &functions3, sizeof (functions3));
    }
  else
    {
      *pfExists = SQL_FUNC_EXISTS (functions3, fFunc);
    }
#endif

  LEAVE_HDBC (pdbc, SQL_SUCCESS);
}
