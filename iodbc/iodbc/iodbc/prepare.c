/*
 *  prepare.c
 *
 *  $Id: prepare.c,v 1.1.1.1 2002/04/08 22:48:10 miner Exp $
 *
 *  Prepare a query
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

SQLRETURN SQL_API
SQLPrepare (
    SQLHSTMT hstmt,
    SQLCHAR FAR * szSqlStr,
    SQLINTEGER cbSqlStr)
{
  STMT (pstmt, hstmt);

  HPROC hproc = SQL_NULL_HPROC;
  SQLRETURN retcode = SQL_SUCCESS;
  int sqlstat = en_00000;

  ENTER_STMT (pstmt);

  /* check state */
  if (pstmt->asyn_on == en_NullProc)
    {
      /* not on asyn state */
      switch (pstmt->state)
	{
	case en_stmt_fetched:
	case en_stmt_xfetched:
	  sqlstat = en_24000;
	  break;

	case en_stmt_needdata:
	case en_stmt_mustput:
	case en_stmt_canput:
	  sqlstat = en_S1010;
	  break;

	default:
	  break;
	}
    }
  else if (pstmt->asyn_on != en_Prepare)
    {
      /* asyn on other */
      sqlstat = en_S1010;
    }

  if (sqlstat != en_00000)
    {
      PUSHSQLERR (pstmt->herr, sqlstat);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  if (szSqlStr == NULL)
    {
      PUSHSQLERR (pstmt->herr, en_S1009);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  if (cbSqlStr < 0 && cbSqlStr != SQL_NTS)
    {
      PUSHSQLERR (pstmt->herr, en_S1090);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  hproc = _iodbcdm_getproc (pstmt->hdbc, en_Prepare);

  if (hproc == SQL_NULL_HPROC)
    {
      PUSHSQLERR (pstmt->herr, en_IM001);
      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc, en_Prepare,
      (pstmt->dhstmt, szSqlStr, cbSqlStr));

  /* stmt state transition */
  if (pstmt->asyn_on == en_Prepare)
    {
      switch (retcode)
	{
	case SQL_SUCCESS:
	case SQL_SUCCESS_WITH_INFO:
	case SQL_ERROR:
	  pstmt->asyn_on = en_NullProc;
	   LEAVE_STMT (pstmt, retcode);

	case SQL_STILL_EXECUTING:
	default:
	   LEAVE_STMT (pstmt, retcode);
	}
    }

  switch (retcode)
    {
    case SQL_STILL_EXECUTING:
      pstmt->asyn_on = en_Prepare;
      break;

    case SQL_SUCCESS:
    case SQL_SUCCESS_WITH_INFO:
      pstmt->state = en_stmt_prepared;
      pstmt->prep_state = 1;
      break;

    case SQL_ERROR:
      switch (pstmt->state)
	{
	case en_stmt_prepared:
	case en_stmt_executed:
	  pstmt->state = en_stmt_allocated;
	  pstmt->prep_state = 0;
	  break;

	default:
	  break;
	}

    default:
      break;
    }

  LEAVE_STMT (pstmt, retcode);
}


SQLRETURN SQL_API
SQLSetCursorName (
    SQLHSTMT hstmt,
    SQLCHAR FAR * szCursor,
    SQLSMALLINT cbCursor)
{
  STMT (pstmt, hstmt);
  HPROC hproc = SQL_NULL_HPROC;

  SQLRETURN retcode = SQL_SUCCESS;
  int sqlstat = en_00000;

  ENTER_STMT (pstmt);

  if (szCursor == NULL)
    {
      PUSHSQLERR (pstmt->herr, en_S1009);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  if (cbCursor < 0 && cbCursor != SQL_NTS)
    {
      PUSHSQLERR (pstmt->herr, en_S1090);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  /* check state */
  if (pstmt->asyn_on != en_NullProc)
    {
      sqlstat = en_S1010;
    }
  else
    {
      switch (pstmt->state)
	{
	case en_stmt_executed:
	case en_stmt_cursoropen:
	case en_stmt_fetched:
	case en_stmt_xfetched:
	  sqlstat = en_24000;
	  break;

	case en_stmt_needdata:
	case en_stmt_mustput:
	case en_stmt_canput:
	  sqlstat = en_S1010;
	  break;

	default:
	  break;
	}
    }

  if (sqlstat != en_00000)
    {
      PUSHSQLERR (pstmt->herr, sqlstat);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  hproc = _iodbcdm_getproc (pstmt->hdbc, en_SetCursorName);

  if (hproc == SQL_NULL_HPROC)
    {
      PUSHSQLERR (pstmt->herr, en_IM001);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc, en_SetCursorName,
      (pstmt->dhstmt, szCursor, cbCursor));

  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    {
      pstmt->cursor_state = en_stmt_cursor_named;
    }

  LEAVE_STMT (pstmt, retcode);
}



SQLRETURN SQL_API
SQLBindParameter (
    SQLHSTMT hstmt,
    SQLUSMALLINT ipar,
    SQLSMALLINT fParamType,
    SQLSMALLINT fCType,
    SQLSMALLINT fSqlType,
    SQLUINTEGER cbColDef,
    SQLSMALLINT ibScale,
    SQLPOINTER rgbValue,
    SQLINTEGER cbValueMax,
    SQLINTEGER FAR * pcbValue)
{
  STMT (pstmt, hstmt);
  HPROC hproc = SQL_NULL_HPROC;

  int sqlstat = en_00000;
  SQLRETURN retcode = SQL_SUCCESS;

  ENTER_STMT (pstmt);

#if (ODBCVER >= 0x0300)
  if (0)
#else
  /* check param */
  if (fSqlType > SQL_TYPE_MAX ||
      (fSqlType < SQL_TYPE_MIN && fSqlType > SQL_TYPE_DRIVER_START))
    /* Note: SQL_TYPE_DRIVER_START is a negative number 
     * So, we use ">" */
#endif
    {
      sqlstat = en_S1004;
    }
  else if (ipar < 1)
    {
      sqlstat = en_S1093;
    }
  else if ((rgbValue == NULL && pcbValue == NULL)
      && fParamType != SQL_PARAM_OUTPUT)
    {
      sqlstat = en_S1009;
      /* This means, I allow output to nowhere
       * (i.e. * junk output result). But I can't  
       * allow input from nowhere. 
       */
    }
/**********
	else if( cbValueMax < 0L && cbValueMax != SQL_SETPARAM_VALUE_MAX )
	{
		sqlstat = en_S1090;
	}
**********/
  else if (fParamType != SQL_PARAM_INPUT
	&& fParamType != SQL_PARAM_OUTPUT
      && fParamType != SQL_PARAM_INPUT_OUTPUT)
    {
      sqlstat = en_S1105;
    }
  else
    {
      switch (fCType)
	{
	case SQL_C_DEFAULT:
	case SQL_C_BINARY:
	case SQL_C_BIT:
	case SQL_C_CHAR:
	case SQL_C_DATE:
	case SQL_C_DOUBLE:
	case SQL_C_FLOAT:
	case SQL_C_LONG:
	case SQL_C_SHORT:
	case SQL_C_SLONG:
	case SQL_C_SSHORT:
	case SQL_C_STINYINT:
	case SQL_C_TIME:
	case SQL_C_TIMESTAMP:
	case SQL_C_TINYINT:
	case SQL_C_ULONG:
	case SQL_C_USHORT:
	case SQL_C_UTINYINT:
#if (ODBCVER >= 0x0300)
	case SQL_C_GUID:
	case SQL_C_INTERVAL_DAY:
	case SQL_C_INTERVAL_DAY_TO_HOUR:
	case SQL_C_INTERVAL_DAY_TO_MINUTE:
	case SQL_C_INTERVAL_DAY_TO_SECOND:
	case SQL_C_INTERVAL_HOUR:
	case SQL_C_INTERVAL_HOUR_TO_MINUTE:
	case SQL_C_INTERVAL_HOUR_TO_SECOND:
	case SQL_C_INTERVAL_MINUTE:
	case SQL_C_INTERVAL_MINUTE_TO_SECOND:
	case SQL_C_INTERVAL_MONTH:
	case SQL_C_INTERVAL_SECOND:
	case SQL_C_INTERVAL_YEAR:
	case SQL_C_INTERVAL_YEAR_TO_MONTH:
	case SQL_C_NUMERIC:
	case SQL_C_SBIGINT:
	case SQL_C_TYPE_DATE:
	case SQL_C_TYPE_TIME:
	case SQL_C_TYPE_TIMESTAMP:
	case SQL_C_UBIGINT:
#endif
	  break;

	default:
	  sqlstat = en_S1003;
	  break;
	}
    }

  if (sqlstat != en_00000)
    {
      PUSHSQLERR (pstmt->herr, sqlstat);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  /* check state */
  if (pstmt->state >= en_stmt_needdata || pstmt->asyn_on != en_NullProc)
    {
      PUSHSQLERR (pstmt->herr, en_S1010);

      retcode = SQL_ERROR;
    }

#if (ODBCVER >=0x0300)
  if (fParamType == SQL_PARAM_INPUT)
    {
      hproc = _iodbcdm_getproc (pstmt->hdbc, en_BindParam);
      if (hproc)
	{
	  CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc, en_BindParam,
	      (pstmt->dhstmt, ipar, fCType, fSqlType, cbColDef,
		  ibScale, rgbValue, pcbValue));
	  LEAVE_STMT (pstmt, retcode);
	}
    }
#endif

  hproc = _iodbcdm_getproc (pstmt->hdbc, en_BindParameter);

  if (hproc == SQL_NULL_HPROC)
    {

      PUSHSQLERR (pstmt->herr, en_IM001);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc, en_BindParameter,
      (pstmt->dhstmt, ipar, fParamType, fCType, fSqlType, cbColDef,
	  ibScale, rgbValue, cbValueMax, pcbValue));

  LEAVE_STMT (pstmt, retcode);
}


SQLRETURN SQL_API
SQLParamOptions (
    SQLHSTMT hstmt,
    SQLUINTEGER crow,
    SQLUINTEGER FAR * pirow)
{
  STMT (pstmt, hstmt);
  HPROC hproc;
  SQLRETURN retcode;

  ENTER_STMT (pstmt);

  if (crow == (UDWORD) 0UL)
    {
      PUSHSQLERR (pstmt->herr, en_S1107);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  if (pstmt->state >= en_stmt_needdata || pstmt->asyn_on != en_NullProc)
    {
      PUSHSQLERR (pstmt->herr, en_S1010);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

#if (ODBCVER >= 0x0300)

  hproc = _iodbcdm_getproc (pstmt->hdbc, en_SetStmtAttr);

  if (hproc != SQL_NULL_HPROC)
    {
      CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc, en_SetStmtAttr,
	  (pstmt->dhstmt, SQL_ATTR_PARAMSET_SIZE, crow, 0));
      if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
	{
	  CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc, en_SetStmtAttr,
	      (pstmt->dhstmt, SQL_ATTR_PARAMS_PROCESSED_PTR, pirow, 0));
	}
    }
  else
#endif
    {

      hproc = _iodbcdm_getproc (pstmt->hdbc, en_ParamOptions);

      if (hproc == SQL_NULL_HPROC)
	{
	  PUSHSQLERR (pstmt->herr, en_IM001);

	  LEAVE_STMT (pstmt, SQL_ERROR);
	}

      CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc, en_ParamOptions,
	  (pstmt->dhstmt, crow, pirow));
    }

  LEAVE_STMT (pstmt, retcode);
}


SQLRETURN SQL_API
SQLSetScrollOptions (
    SQLHSTMT hstmt,
    SQLUSMALLINT fConcurrency,
    SQLINTEGER crowKeyset,
    SQLUSMALLINT crowRowset)
{
  STMT (pstmt, hstmt);
  HPROC hproc;
  int sqlstat = en_00000;
  SQLRETURN retcode;

  ENTER_STMT (pstmt);

  for (;;)
    {
      if (crowRowset == (UWORD) 0)
	{
	  sqlstat = en_S1107;
	  break;
	}

      if (crowKeyset > (SDWORD) 0L && crowKeyset < (SDWORD) crowRowset)
	{
	  sqlstat = en_S1107;
	  break;
	}

      if (crowKeyset < 1)
	{
	  if (crowKeyset != SQL_SCROLL_FORWARD_ONLY
	      && crowKeyset != SQL_SCROLL_STATIC
	      && crowKeyset != SQL_SCROLL_KEYSET_DRIVEN
	      && crowKeyset != SQL_SCROLL_DYNAMIC)
	    {
	      sqlstat = en_S1107;
	      break;
	    }
	}

      if (fConcurrency != SQL_CONCUR_READ_ONLY
	  && fConcurrency != SQL_CONCUR_LOCK
	  && fConcurrency != SQL_CONCUR_ROWVER
	  && fConcurrency != SQL_CONCUR_VALUES)
	{
	  sqlstat = en_S1108;
	  break;
	}

#if (ODBCVER < 0x0300)
      if (pstmt->state != en_stmt_allocated)
	{
	  sqlstat = en_S1010;
	  break;
	}
#endif

      hproc = _iodbcdm_getproc (pstmt->hdbc, en_SetScrollOptions);

      if (hproc == SQL_NULL_HPROC)
	{
#if (ODBCVER >= 0x0300)
	  SQLINTEGER InfoValue, InfoType, Value;
	  HPROC hproc1 = _iodbcdm_getproc (pstmt->hdbc, en_SetStmtAttr);

	  if (hproc1 == SQL_NULL_HPROC)
	    {
	      PUSHSQLERR (pstmt->herr, en_IM001);
	      LEAVE_STMT (pstmt, SQL_ERROR);
	    }

	  switch (crowKeyset)
	    {
	    case SQL_SCROLL_FORWARD_ONLY:
	      InfoType = SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2;
	      Value = SQL_CURSOR_FORWARD_ONLY;
	      break;

	    case SQL_SCROLL_STATIC:
	      InfoType = SQL_STATIC_CURSOR_ATTRIBUTES2;
	      Value = SQL_CURSOR_STATIC;
	      break;

	    case SQL_SCROLL_DYNAMIC:
	      InfoType = SQL_DYNAMIC_CURSOR_ATTRIBUTES2;
	      Value = SQL_CURSOR_DYNAMIC;
	      break;

	    case SQL_SCROLL_KEYSET_DRIVEN:
	    default:
	      InfoType = SQL_KEYSET_CURSOR_ATTRIBUTES2;
	      Value = SQL_CURSOR_KEYSET_DRIVEN;
	      break;
	    }

	  retcode = SQLGetInfo (pstmt->hdbc, InfoType, &InfoValue, 0, NULL);
	  if (retcode != SQL_SUCCESS)
	    {
	      LEAVE_STMT (pstmt, retcode);
	    }

	  switch (fConcurrency)
	    {
	    case SQL_CONCUR_READ_ONLY:
	      if (!(InfoValue & SQL_CA2_READ_ONLY_CONCURRENCY))
		{
		  PUSHSQLERR (pstmt->herr, en_S1C00);
		  LEAVE_STMT (pstmt, SQL_ERROR);
		}
	      break;

	    case SQL_CONCUR_LOCK:
	      if (!(InfoValue & SQL_CA2_LOCK_CONCURRENCY))
		{
		  PUSHSQLERR (pstmt->herr, en_S1C00);
		  LEAVE_STMT (pstmt, SQL_ERROR);
		}
	      break;

	    case SQL_CONCUR_ROWVER:
	      if (!(InfoValue & SQL_CA2_OPT_ROWVER_CONCURRENCY))
		{
		  PUSHSQLERR (pstmt->herr, en_S1C00);
		  LEAVE_STMT (pstmt, SQL_ERROR);
		}
	      break;

	    case SQL_CONCUR_VALUES:
	      if (!(InfoValue & SQL_CA2_OPT_VALUES_CONCURRENCY))
		{
		  PUSHSQLERR (pstmt->herr, en_S1C00);
		  LEAVE_STMT (pstmt, SQL_ERROR);
		}
	      break;
	    }
	  CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc1, en_SetStmtAttr,
	      (pstmt->dhstmt, SQL_ATTR_CURSOR_TYPE, Value, 0));

	  if (retcode != SQL_SUCCESS)
	    LEAVE_STMT (pstmt, retcode);

	  CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc1, en_SetStmtAttr,
	      (pstmt->dhstmt, SQL_ATTR_CONCURRENCY, fConcurrency, 0));

	  if (retcode != SQL_SUCCESS)
	    LEAVE_STMT (pstmt, retcode);

	  if (crowKeyset > 0)
	    {
	      CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc1, en_SetStmtAttr,
		  (pstmt->dhstmt, SQL_ATTR_KEYSET_SIZE, crowKeyset, 0));

	      if (retcode != SQL_SUCCESS)
		LEAVE_STMT (pstmt, retcode);
	    }

	  CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc1, en_SetStmtAttr,
	      (pstmt->dhstmt, SQL_ROWSET_SIZE, crowRowset, 0));

	  if (retcode != SQL_SUCCESS)
	    LEAVE_STMT (pstmt, retcode);
#else
	  sqlstat = en_IM001;
	  break;
#endif
	}

      sqlstat = en_00000;
      if (1)			/* turn off solaris warning message */
	break;
    }

  if (sqlstat != en_00000)
    {
      PUSHSQLERR (pstmt->herr, sqlstat);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc, en_SetScrollOptions,
      (pstmt->dhstmt, fConcurrency, crowKeyset, crowRowset));

  LEAVE_STMT (pstmt, retcode);
}


SQLRETURN SQL_API
SQLSetParam (
    SQLHSTMT hstmt,
    SQLUSMALLINT ipar,
    SQLSMALLINT fCType,
    SQLSMALLINT fSqlType,
    SQLUINTEGER cbColDef,
    SQLSMALLINT ibScale,
    SQLPOINTER rgbValue,
    SQLINTEGER FAR * pcbValue)
{
  return SQLBindParameter (hstmt,
      ipar,
      (SWORD) SQL_PARAM_INPUT_OUTPUT,
      fCType,
      fSqlType,
      cbColDef,
      ibScale,
      rgbValue,
      SQL_SETPARAM_VALUE_MAX,
      pcbValue);
}
