/*
 *  herr.c
 *
 *  $Id: herr.c,v 1.1.1.2 2002/04/30 00:40:23 miner Exp $
 *
 *  Error stack management functions
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
#if (ODBCVER >= 0x0300)
#include <hdesc.h>
#endif
#include <henv.h>
#include <hdbc.h>
#include <hstmt.h>

#include <itrace.h>

#include "herr.ci"

static HERR 
_iodbcdm_popsqlerr (HERR herr)
{
  sqlerr_t *list = (sqlerr_t *) herr;
  sqlerr_t *next;

  if (herr == SQL_NULL_HERR)
    {
      return herr;
    }

  next = list->next;

  MEM_FREE (list);

  return next;
}


void 
_iodbcdm_freesqlerrlist (HERR herrlist)
{
  HERR list = herrlist;

  while (list != SQL_NULL_HERR)
    {
      list = _iodbcdm_popsqlerr (list);
    }
}


HERR 
_iodbcdm_pushsqlerr (
    HERR herr,
    sqlstcode_t code,
    char *msg)
{
  sqlerr_t *ebuf;
  sqlerr_t *perr = (sqlerr_t *) herr;
  int idx = 0;

  if (herr != SQL_NULL_HERR)
    {
      idx = perr->idx + 1;
    }

  if (idx == 64)
    /* overwrite the top entry to prevent error stack blow out */
    {
      perr->code = code;
      perr->msg = msg;

      return herr;
    }

  ebuf = (sqlerr_t *) MEM_ALLOC (sizeof (sqlerr_t));

  if (ebuf == NULL)
    {
      return NULL;
    }

  ebuf->msg = msg;
  ebuf->code = code;
  ebuf->idx = idx;
  ebuf->next = (sqlerr_t *) herr;
  return (HERR) ebuf;
}


static char FAR *
_iodbcdm_getsqlstate (
    HERR herr,
    void FAR * tab)
{
  sqlerr_t *perr = (sqlerr_t *) herr;
  sqlerrmsg_t *ptr;
  int perr_code;

  if (herr == SQL_NULL_HERR || tab == NULL)
    {
      return (char FAR *) NULL;
    }

  perr_code = perr->code;
#if (ODBCVER >= 0x0300)
  switch (perr_code)
    {
      case en_S1009 : perr_code = en_HY009; break;
    }
#endif  
  for (ptr = tab;
      ptr->code != en_sqlstat_total;
      ptr++)
    {
      if (ptr->code == perr_code)
	{
	  return (char FAR *) (ptr->stat);
	}
    }

  return (char FAR *) NULL;
}


static char FAR *
_iodbcdm_getsqlerrmsg (
    HERR herr,
    void FAR * errtab)
{
  sqlerr_t *perr = (sqlerr_t *) herr;
  sqlerrmsg_t *ptr;

  if (herr == SQL_NULL_HERR)
    {
      return NULL;
    }

  if (perr->msg == NULL && errtab == NULL)
    {
      return NULL;
    }

  if (perr->msg != NULL)
    {
      return perr->msg;
    }

  for (ptr = (sqlerrmsg_t *) errtab;
      ptr->code != en_sqlstat_total;
      ptr++)
    {
      if (ptr->code == perr->code)
	{
	  return (char FAR *) ptr->msg;
	}
    }

  return (char FAR *) NULL;
}

SQLRETURN SQL_API 
_iodbcdm_sqlerror (
    SQLHENV henv,
    SQLHDBC hdbc,
    SQLHSTMT hstmt,
    SQLCHAR FAR * szSqlstate,
    SQLINTEGER FAR * pfNativeError,
    SQLCHAR FAR * szErrorMsg,
    SQLSMALLINT cbErrorMsgMax,
    SQLSMALLINT FAR * pcbErrorMsg,
    int bDelete)
{
  GENV (genv, henv);
  CONN (pdbc, hdbc);
  STMT (pstmt, hstmt);
  HDBC thdbc = SQL_NULL_HDBC;

  HENV dhenv = SQL_NULL_HENV;
  HDBC dhdbc = SQL_NULL_HDBC;
  HSTMT dhstmt = SQL_NULL_HSTMT;

  HERR herr = SQL_NULL_HERR;
  HPROC hproc = SQL_NULL_HPROC;
#if (ODBCVER >= 0x0300)  
  SQLINTEGER handleType;
  SQLHANDLE handle3;
  SQLHANDLE dhandle3;
  SQLUSMALLINT *perr_rec;
#endif

  char FAR *errmsg = NULL;
  char FAR *ststr = NULL;

  int handle = 0;
  SQLRETURN retcode = SQL_SUCCESS;

  if (hstmt != SQL_NULL_HSTMT)	/* retrieve stmt err */
    {
      herr = pstmt->herr;
      thdbc = pstmt->hdbc;

      if (thdbc == SQL_NULL_HDBC)
	{
	  return SQL_INVALID_HANDLE;
	}

#if (ODBCVER >= 0x0300)
      if (((ENV_t FAR *) ((DBC_t FAR *)thdbc)->henv)->dodbc_ver >= SQL_OV_ODBC3)
        hproc = _iodbcdm_getproc (thdbc, en_GetDiagRec);
      else
#endif
        hproc = _iodbcdm_getproc (thdbc, en_Error);

#if (ODBCVER >= 0x0300)
      handleType = SQL_HANDLE_STMT;
      handle3 = hstmt;
      dhandle3 = pstmt->dhstmt;
      perr_rec = &((STMT_t FAR*)pstmt)->err_rec;
#endif      
      dhstmt = pstmt->dhstmt;
      handle = 3;
    }
  else if (hdbc != SQL_NULL_HDBC)	/* retrieve dbc err */
    {
      herr = pdbc->herr;
      thdbc = pdbc;
      if (thdbc == SQL_NULL_HDBC)
	{
	  return SQL_INVALID_HANDLE;
	}

#if (ODBCVER >= 0x0300)
      if (((ENV_t FAR *) ((DBC_t FAR *)thdbc)->henv) &&
          ((ENV_t FAR *) ((DBC_t FAR *)thdbc)->henv)->dodbc_ver >= SQL_OV_ODBC3)
        hproc = _iodbcdm_getproc (thdbc, en_GetDiagRec);
      else
#endif
        hproc = _iodbcdm_getproc (thdbc, en_Error);

#if (ODBCVER >= 0x0300)
      handleType = SQL_HANDLE_DBC;
      handle3 = hdbc;
      dhandle3 = pdbc->dhdbc;
      perr_rec = &((DBC_t FAR*)pdbc)->err_rec;
#endif      
      dhdbc = pdbc->dhdbc;
      handle = 2;

      if (herr == SQL_NULL_HERR
	  && pdbc->henv == SQL_NULL_HENV)
	{
	  return SQL_NO_DATA_FOUND;
	}
    }
  else if (henv != SQL_NULL_HENV)	/* retrieve env err */
    {
      herr = genv->herr;

      /* Drivers shouldn't push error message 
       * on environment handle */

      if (herr == SQL_NULL_HERR)
	{
	  return SQL_NO_DATA_FOUND;
	}

      handle = 1;
    }
  else
    {
      return SQL_INVALID_HANDLE;
    }

  if (szErrorMsg != NULL)
    {
      if (cbErrorMsgMax < 0
	  || cbErrorMsgMax > SQL_MAX_MESSAGE_LENGTH - 1)
	{
	  return SQL_ERROR;
	  /* SQLError() doesn't post error for itself */
	}
    }

  if (herr == SQL_NULL_HERR)	/* no err on drv mng */
    {
      /* call driver */
      if (hproc == SQL_NULL_HPROC)
	{
	  return SQL_ERROR;
	}

      if (hproc != SQL_NULL_HPROC)
	{
#if (ODBCVER >= 0x0300)
          if (((ENV_t FAR *) ((DBC_t FAR *)thdbc)->henv)->dodbc_ver >= SQL_OV_ODBC3)
            {
              (*perr_rec) = (*perr_rec) + 1;
              CALL_DRIVER (thdbc, NULL, retcode, hproc, en_GetDiagRec,
                (handleType, dhandle3, (*perr_rec),
                szSqlstate, pfNativeError, szErrorMsg,
                  cbErrorMsgMax, pcbErrorMsg));
            }
          else
            {
#endif
              CALL_DRIVER (thdbc, NULL, retcode, hproc, en_Error,
                (dhenv, dhdbc, dhstmt, szSqlstate, pfNativeError, szErrorMsg,
                  cbErrorMsgMax, pcbErrorMsg));
#if (ODBCVER >= 0x0300)
            }
#endif
          return retcode;
	}
      else
	  retcode = SQL_NO_DATA_FOUND;
    }

  if (szSqlstate != NULL)
    {
      int len;

      /* get sql state  string */
      ststr = (char FAR *) _iodbcdm_getsqlstate (herr,
	  (void FAR *) sqlerrmsg_tab);

      if (ststr == NULL)
	{
	  len = 0;
	}
      else
	{
	  len = (int) STRLEN (ststr);
	}

      STRNCPY (szSqlstate, ststr, len);
      szSqlstate[len] = 0;
      /* buffer size of szSqlstate is not checked. Applications
       * suppose provide enough ( not less than 6 bytes ) buffer
       * or NULL for it.
       */
    }

  if (pfNativeError != NULL)
    {
      /* native error code is specific to data source */
      *pfNativeError = (SDWORD) 0L;
    }

  if (szErrorMsg == NULL || cbErrorMsgMax == 0)
    {
      if (pcbErrorMsg != NULL)
	{
	  *pcbErrorMsg = (SWORD) 0;
	}
    }
  else
    {
      int len;
      char msgbuf[256] = {'\0'};

      /* get sql state message */
      errmsg = _iodbcdm_getsqlerrmsg (herr, (void FAR *) sqlerrmsg_tab);

      if (errmsg == NULL)
	{
	  errmsg = (char FAR *) "";
	}

      sprintf (msgbuf, "%s%s", sqlerrhd, errmsg);

      len = STRLEN (msgbuf);

      if (len < cbErrorMsgMax - 1)
	{
	  retcode = SQL_SUCCESS;
	}
      else
	{
	  len = cbErrorMsgMax - 1;
	  retcode = SQL_SUCCESS_WITH_INFO;
	  /* and not posts error for itself */
	}

      STRNCPY ((char *) szErrorMsg, msgbuf, len);
      szErrorMsg[len] = 0;

      if (pcbErrorMsg != NULL)
	{
	  *pcbErrorMsg = (SWORD) len;
	}
    }

  if (bDelete)
    switch (handle)		/* free this err */
      {
	case 1:
	    genv->herr = _iodbcdm_popsqlerr (genv->herr);
	    break;

	case 2:
	    pdbc->herr = _iodbcdm_popsqlerr (pdbc->herr);
	    break;

	case 3:
	    pstmt->herr = _iodbcdm_popsqlerr (pstmt->herr);
	    break;

	default:
	    break;
      }

  return retcode;
}


SQLRETURN SQL_API 
SQLError (
    SQLHENV henv,
    SQLHDBC hdbc,
    SQLHSTMT hstmt,
    SQLCHAR FAR * szSqlstate,
    SQLINTEGER FAR * pfNativeError,
    SQLCHAR FAR * szErrorMsg,
    SQLSMALLINT cbErrorMsgMax,
    SQLSMALLINT FAR * pcbErrorMsg)
{
  SQLRETURN retcode;

  ODBC_LOCK ();
  retcode = _iodbcdm_sqlerror (henv, hdbc, hstmt, szSqlstate, pfNativeError,
      szErrorMsg, cbErrorMsgMax, pcbErrorMsg, 1);
  ODBC_UNLOCK ();
  return retcode;
}


#if (ODBCVER >= 0x0300)
static int
error_rec_count (HERR herr)
{
  sqlerr_t *err = (sqlerr_t *)herr;
  if (err)
    return err->idx + 1;
  else
    return 0;
}

static sqlerr_t *
get_nth_error(HERR herr, int nIndex)
{
  sqlerr_t *err = herr;
  while (err && err->idx != nIndex)
      err = err->next;
  return err;
}


RETCODE SQL_API
SQLGetDiagRec (SQLSMALLINT HandleType,
    SQLHANDLE Handle,
    SQLSMALLINT RecNumber,
    SQLCHAR * Sqlstate,
    UNALIGNED SQLINTEGER * NativeErrorPtr,
    SQLCHAR * MessageText,
    SQLSMALLINT BufferLength,
    UNALIGNED SQLSMALLINT * TextLengthPtr)
{
  sqlerr_t *curr_err = NULL;
  HERR err = NULL;
  int nRecs;
  HPROC hproc = SQL_NULL_HPROC;
  HDBC hdbc = SQL_NULL_HDBC;
  RETCODE retcode;
  SQLHANDLE dhandle = SQL_NULL_HANDLE;

  if (RecNumber < 1)
    return SQL_ERROR;

  if (BufferLength < 0)
    return SQL_ERROR;

  ODBC_LOCK ();
  switch (HandleType)
    {
    case SQL_HANDLE_ENV:
      if (!IS_VALID_HENV (Handle))
	{
	  ODBC_UNLOCK ();
	  return SQL_INVALID_HANDLE;
	}
      err = ((GENV_t *) Handle)->herr;
      break;

    case SQL_HANDLE_DBC:
      if (!IS_VALID_HDBC (Handle))
	{
	  ODBC_UNLOCK ();
	  return SQL_INVALID_HANDLE;
	}
      err = ((DBC_t *) Handle)->herr;
      dhandle = ((DBC_t *) Handle)->dhdbc;
      hdbc = Handle;
      break;

    case SQL_HANDLE_STMT:
      if (!IS_VALID_HSTMT (Handle))
	{
	  ODBC_UNLOCK ();
	  return SQL_INVALID_HANDLE;
	}
      err = ((STMT_t *) Handle)->herr;
      dhandle = ((STMT_t *) Handle)->dhstmt;
      hdbc = ((STMT_t *) Handle)->hdbc;
      break;

    case SQL_HANDLE_DESC:
      if (!IS_VALID_HDESC (Handle))
	{
	  ODBC_UNLOCK ();
	  return SQL_INVALID_HANDLE;
	}
      err = ((DESC_t *) Handle)->herr;
      dhandle = ((DESC_t *) Handle)->dhdesc;
      hdbc = ((DESC_t *) Handle)->hdbc;
      break;

    default:
      ODBC_UNLOCK ();
      return SQL_INVALID_HANDLE;
    }

  nRecs = error_rec_count (err);

  if (nRecs >= RecNumber)
    {				/* DM error range */
      curr_err = get_nth_error (err, RecNumber - 1);

      if (!curr_err)
	{
	  ODBC_UNLOCK ();
	  return (SQL_NO_DATA_FOUND);
	}

      retcode = SQL_SUCCESS;

      if (Sqlstate != NULL)
	{
	  int len;
	  char *ststr = (char FAR *) _iodbcdm_getsqlstate (curr_err,
	      (void FAR *) sqlerrmsg_tab);

	  if (ststr == NULL)
	    {
	      len = 0;
	    }
	  else
	    {
	      len = (int) STRLEN (ststr);
	    }

	  STRNCPY (Sqlstate, ststr, len);
	  Sqlstate[len] = 0;
	  /* buffer size of szSqlstate is not checked. Applications
	   * suppose provide enough ( not less than 6 bytes ) buffer
	   * or NULL for it.
	   */
	}

      if (MessageText == NULL || BufferLength == 0)
	{
	  if (TextLengthPtr != NULL)
	    {
	      *TextLengthPtr = (SWORD) 0;
	    }
	}
      else
	{
	  int len;
	  char msgbuf[256] = { '\0' };
	  char *errmsg;

	  /* get sql state message */
	  errmsg =
	      _iodbcdm_getsqlerrmsg (curr_err, (void FAR *) sqlerrmsg_tab);

	  if (errmsg == NULL)
	    {
	      errmsg = (char FAR *) "";
	    }

	  sprintf (msgbuf, "%s%s", sqlerrhd, errmsg);

	  len = STRLEN (msgbuf);

	  if (len < BufferLength - 1)
	    {
	      retcode = SQL_SUCCESS;
	    }
	  else
	    {
	      len = BufferLength - 1;
	      retcode = SQL_SUCCESS_WITH_INFO;
	      /* and not posts error for itself */
	    }

	  STRNCPY ((char *) MessageText, msgbuf, len);
	  MessageText[len] = 0;

	  if (TextLengthPtr != NULL)
	    {
	      *TextLengthPtr = (SWORD) len;
	    }
	}
      ODBC_UNLOCK ();
      return retcode;
    }
  else
    {				/* Driver errors */
      if (hdbc == SQL_NULL_HDBC)
	{
	  ODBC_UNLOCK ();
	  return SQL_NO_DATA_FOUND;
	}
      RecNumber -= nRecs;

      hproc = _iodbcdm_getproc (hdbc, en_GetDiagRec);
      if (hproc != SQL_NULL_HPROC)
	{
	  CALL_DRIVER (hdbc, Handle, retcode, hproc, en_GetDiagRec,
	      (HandleType, dhandle, RecNumber, Sqlstate, NativeErrorPtr,
		  MessageText, BufferLength, TextLengthPtr));

	  ODBC_UNLOCK ();
	  return retcode;
	}
      else
	{			/* no SQLGetDiagRec */
	  hproc = _iodbcdm_getproc (hdbc, en_Error);

	  if (hproc == SQL_NULL_HPROC || RecNumber > 1
	      || HandleType == SQL_HANDLE_DESC)
	    {
	      ODBC_UNLOCK ();
	      return SQL_NO_DATA_FOUND;
	    }

	  CALL_DRIVER (hdbc, Handle, retcode, hproc, en_Error,
	      (SQL_NULL_HENV,
	       HandleType == SQL_HANDLE_DBC ? dhandle : SQL_NULL_HDBC,
	       HandleType == SQL_HANDLE_STMT ? dhandle : SQL_NULL_HSTMT,
	       Sqlstate, NativeErrorPtr, MessageText, BufferLength, 
	       TextLengthPtr));

	  ODBC_UNLOCK ();
	  return retcode;
	}
    }
}


RETCODE SQL_API
SQLGetDiagField (SQLSMALLINT nHandleType,
    SQLHANDLE Handle,
    SQLSMALLINT nRecNumber,
    SQLSMALLINT nDiagIdentifier,
    SQLPOINTER pDiagInfoPtr,
    SQLSMALLINT nBufferLength,
    UNALIGNED SQLSMALLINT * pnStringLengthPtr)
{
  GENV (genv, Handle);
  CONN (con, Handle);
  STMT (stmt, Handle);
  DESC (desc, Handle);
  HERR err;
  HPROC hproc;
  RETCODE retcode;
  SQLHANDLE dhandle = SQL_NULL_HANDLE;

  ODBC_LOCK ();
  switch (nHandleType)
    {
    case SQL_HANDLE_ENV:
      if (!IS_VALID_HENV (Handle))
	{
	  ODBC_UNLOCK ();
	  return SQL_INVALID_HANDLE;
	}
      err = genv->herr;
      con = NULL;
      stmt = NULL;
      desc = NULL;
      break;

    case SQL_HANDLE_DBC:
      if (!IS_VALID_HDBC (Handle))
	{
	  ODBC_UNLOCK ();
	  return SQL_INVALID_HANDLE;
	}
      err = con->herr;
      genv = con->genv;
      stmt = NULL;
      desc = NULL;
      dhandle = con->dhdbc;
      break;

    case SQL_HANDLE_STMT:
      if (!IS_VALID_HSTMT (Handle))
	{
	  ODBC_UNLOCK ();
	  return SQL_INVALID_HANDLE;
	}
      err = stmt->herr;
      con = stmt->hdbc;
      genv = con->genv;
      desc = NULL;
      dhandle = stmt->dhstmt;
      break;

    case SQL_HANDLE_DESC:
      if (!IS_VALID_HDESC (Handle))
	{
	  ODBC_UNLOCK ();
	  return SQL_INVALID_HANDLE;
	}
      err = desc->herr;
      stmt = desc->hstmt;
      con = desc->hdbc;
      genv = con->genv;
      dhandle = desc->dhdesc;
      break;

    default:
      ODBC_UNLOCK ();
      return SQL_INVALID_HANDLE;
    }

  switch (nRecNumber)
    {

    case 0:			/* Header record */
      switch (nDiagIdentifier)
	{
	case SQL_DIAG_ROW_COUNT:
	  {
	    if (nHandleType != SQL_HANDLE_STMT || !stmt)
	      {
		ODBC_UNLOCK ();
		return SQL_ERROR;
	      }

	    if (stmt->state != en_stmt_executed &&
		stmt->state != en_stmt_cursoropen)
	      {
		ODBC_UNLOCK ();
		return SQL_ERROR;
	      }
	    if (!con)
	      {
		ODBC_UNLOCK ();
		return SQL_INVALID_HANDLE;
	      }
	    hproc = _iodbcdm_getproc (con, en_GetDiagField);
	    if (hproc)
	      {
		CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc,
		    en_GetDiagField, (SQL_HANDLE_DBC, stmt->dhstmt,
			nRecNumber, nDiagIdentifier, pDiagInfoPtr,
			nBufferLength, pnStringLengthPtr));
	      }
	    else
	      {
		if (!con)
		  {
		    ODBC_UNLOCK ();
		    return SQL_INVALID_HANDLE;
		  }
		hproc = _iodbcdm_getproc (con, en_RowCount);
		if (!hproc)
		  {
		    ODBC_UNLOCK ();
		    return SQL_ERROR;
		  }
		CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_RowCount,
		    (stmt->dhstmt, pDiagInfoPtr));
	      }
	    ODBC_UNLOCK ();
	    return retcode;
	  }

	case SQL_DIAG_CURSOR_ROW_COUNT:
	case SQL_DIAG_DYNAMIC_FUNCTION:
	case SQL_DIAG_DYNAMIC_FUNCTION_CODE:

	  {
	    if (nHandleType != SQL_HANDLE_STMT || !stmt)
	      {
		ODBC_UNLOCK ();
		return SQL_ERROR;
	      }

	    if (stmt->state != en_stmt_executed &&
		stmt->state != en_stmt_cursoropen)
	      {
		ODBC_UNLOCK ();
		return SQL_ERROR;
	      }
	    if (!con)
	      {
		ODBC_UNLOCK ();
		return SQL_INVALID_HANDLE;
	      }
	    hproc = _iodbcdm_getproc (con, en_GetDiagField);
	    if (hproc)
	      {
		CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc,
		    en_GetDiagField, (SQL_HANDLE_DBC, stmt->dhstmt,
			nRecNumber, nDiagIdentifier, pDiagInfoPtr,
			nBufferLength, pnStringLengthPtr));

		ODBC_UNLOCK ();
		return retcode;
	      }
	    else
	      {
		ODBC_UNLOCK ();
		return SQL_ERROR;
	      }
	  }

	case SQL_DIAG_RETURNCODE:

	  if (pDiagInfoPtr)
	    *((SQLRETURN *) pDiagInfoPtr) = ((GENV_t FAR *) Handle)->rc;
	  {
	    ODBC_UNLOCK ();
	    return SQL_SUCCESS;
	  }

	case SQL_DIAG_NUMBER:

	  if (pDiagInfoPtr)
	    {
	      (*(SQLINTEGER *) pDiagInfoPtr) = 0;
	      /* get the number from the driver */
	      if (con)
		{
		  hproc = _iodbcdm_getproc (con, en_GetDiagField);
		  if (hproc)
		    {
		      CALL_DRIVER (con, Handle, retcode, hproc,
			  en_GetDiagField, (nHandleType, dhandle, 0,
			      nDiagIdentifier, pDiagInfoPtr, nBufferLength,
			      pnStringLengthPtr));

		      if (retcode != SQL_SUCCESS)
			{
			  ODBC_UNLOCK ();
			  return retcode;
			}

		      /* and add the DM's value */
		      (*(SQLINTEGER *) pDiagInfoPtr) += error_rec_count (err);
		    }
		  else if (((ENV_t *) con->henv)->dodbc_ver == SQL_OV_ODBC2 &&
		      ((GENV_t FAR *) Handle)->rc)
		    {		/* ODBC2 drivers can only have one error */
		      (*(SQLINTEGER *) pDiagInfoPtr) = 1;
		    }
		}
	      else if (genv)
		{
		  (*(SQLINTEGER *) pDiagInfoPtr) = error_rec_count (err);
		}

	    }
	  break;

	default:
	  ODBC_UNLOCK ();
	  return SQL_ERROR;
	}
      break;

    default:			/* status records */
      {
	int nRecs = 0;

	if (nRecNumber < 1)
	  {
	    ODBC_UNLOCK ();
	    return SQL_ERROR;
	  }
	nRecs = error_rec_count (err);
	if (nRecNumber <= nRecs)
	  {			/* DM Errors */
	    char *szval = "";
	    int ival = 0;
	    int isInt = 0;
	    sqlerr_t *rec = NULL;

	    rec = get_nth_error (err, nRecNumber - 1);

	    if (!rec)
	      {
		ODBC_UNLOCK ();
		return (SQL_NO_DATA_FOUND);
	      }

	    switch (nDiagIdentifier)
	      {

	      case SQL_DIAG_SUBCLASS_ORIGIN:
	      case SQL_DIAG_CLASS_ORIGIN:
		isInt = 0;

		szval = (rec->code >= en_HY001
		    && rec->code <= en_IM014) ? "ODBC 3.0" : "ISO 9075";
		break;

	      case SQL_DIAG_COLUMN_NUMBER:

		if (nHandleType != SQL_HANDLE_STMT || !stmt)
		  {
		    ODBC_UNLOCK ();
		    return SQL_ERROR;
		  }
		if (!con)
		  {
		    ODBC_UNLOCK ();
		    return SQL_INVALID_HANDLE;
		  }

		if (pDiagInfoPtr)
		  *((SQLINTEGER *) pDiagInfoPtr) = SQL_COLUMN_NUMBER_UNKNOWN;

		ODBC_UNLOCK ();
		return SQL_SUCCESS;

	      case SQL_DIAG_CONNECTION_NAME:
	      case SQL_DIAG_SERVER_NAME:

		isInt = 0;
		if (con)
		  {
		    retcode =
			SQLGetInfo (con, SQL_DATA_SOURCE_NAME, pDiagInfoPtr,
			nBufferLength, pnStringLengthPtr);

		    ODBC_UNLOCK ();
		    return retcode;
		  }
		else
		  break;

	      case SQL_DIAG_MESSAGE_TEXT:

		isInt = 0;
		szval =
		    _iodbcdm_getsqlerrmsg (rec, (void FAR *) sqlerrmsg_tab);
		break;

	      case SQL_DIAG_NATIVE:

		isInt = 1;
		ival = 0;
		break;

	      case SQL_DIAG_ROW_NUMBER:

		isInt = 1;
		if (nHandleType != SQL_HANDLE_STMT || !stmt)
		  {
		    ODBC_UNLOCK ();
		    return SQL_ERROR;
		  }
		if (!con)
		  {
		    ODBC_UNLOCK ();
		    return SQL_INVALID_HANDLE;
		  }
		hproc = _iodbcdm_getproc (con, en_GetDiagField);
		if (hproc)
		  {
		    CALL_DRIVER (con, Handle, retcode, hproc, en_GetDiagField,
			(nHandleType, dhandle, nRecNumber, nDiagIdentifier,
			    pDiagInfoPtr, nBufferLength, pnStringLengthPtr));

		    ODBC_UNLOCK ();
		    return retcode;
		  }
		else
		  {
		    ival = SQL_ROW_NUMBER_UNKNOWN;
		    break;
		  }

	      case SQL_DIAG_SQLSTATE:

		isInt = 0;
		szval =
		    _iodbcdm_getsqlstate (rec, (void FAR *) sqlerrmsg_tab);
		break;

	      default:
		ODBC_UNLOCK ();
		return SQL_ERROR;
	      }
	    if (isInt)
	      {
		if (pDiagInfoPtr)
		  *((SQLINTEGER *) pDiagInfoPtr) = ival;
	      }
	    else
	      {
		int len = strlen (szval), len1;
		len1 = len > nBufferLength ? nBufferLength : len;
		if (pnStringLengthPtr)
		  *pnStringLengthPtr = len;
		if (pDiagInfoPtr)
		  {
		    STRNCPY (pDiagInfoPtr, szval, len1);
		    *(((SQLCHAR FAR *) pDiagInfoPtr) + len1) = 0;
		  }
	      }
	    break;
	  }
	else
	  {			/* Driver's errors */
	    nRecNumber -= nRecs;

	    if (!con)
	      {
		ODBC_UNLOCK ();
		return SQL_NO_DATA_FOUND;
	      }

	    hproc = _iodbcdm_getproc (con, en_GetDiagField);
	    if (hproc != SQL_NULL_HPROC)
	      {
		CALL_DRIVER (con, Handle, retcode, hproc, en_GetDiagField,
		    (nHandleType, dhandle, nRecNumber, nDiagIdentifier,
			pDiagInfoPtr, nBufferLength, pnStringLengthPtr));

		ODBC_UNLOCK ();
		return retcode;
	      }
	    else
	      {			/* an ODBC2->ODBC3 translation */
		char *szval = "";
		char szState[6];
		SQLINTEGER nNative;

		if (nRecNumber > 1)
		  {
		    ODBC_UNLOCK ();
		    return SQL_NO_DATA_FOUND;
		  }

		hproc = _iodbcdm_getproc (con, en_Error);
		if (hproc == SQL_NULL_HPROC || nHandleType == SQL_HANDLE_DESC)
		  {
		    ODBC_UNLOCK ();
		    return SQL_INVALID_HANDLE;
		  }
		switch (nDiagIdentifier)
		  {
		  case SQL_DIAG_SUBCLASS_ORIGIN:
		  case SQL_DIAG_CLASS_ORIGIN:
		    CALL_DRIVER (con, Handle, retcode, hproc, en_Error,
			(SQL_NULL_HENV,
nHandleType == SQL_HANDLE_DBC ? dhandle : SQL_NULL_HDBC,
			    nHandleType ==
			    SQL_HANDLE_STMT ? dhandle : SQL_NULL_HSTMT,
			    szState, &nNative, NULL, 0, NULL));
		    if (retcode != SQL_SUCCESS)
		      {
			ODBC_UNLOCK ();
			return SQL_NO_DATA_FOUND;
		      }
		    szval =
			!strncmp (szState, "IM", 2) ? "ODBC 3.0" : "ISO 9075";
		    break;

		  case SQL_DIAG_ROW_NUMBER:
		  case SQL_DIAG_COLUMN_NUMBER:
		    if (nHandleType != SQL_HANDLE_STMT || !stmt)
		      {
			ODBC_UNLOCK ();
			return SQL_ERROR;
		      }
		    if (!con)
		      {
			ODBC_UNLOCK ();
			return SQL_INVALID_HANDLE;
		      }
		    if (pDiagInfoPtr)
		      *((SQLINTEGER *) pDiagInfoPtr) =
			  SQL_COLUMN_NUMBER_UNKNOWN;
		    {
		      ODBC_UNLOCK ();
		      return SQL_SUCCESS;
		    }

		  case SQL_DIAG_SERVER_NAME:
		  case SQL_DIAG_CONNECTION_NAME:
		    break;

		  case SQL_DIAG_MESSAGE_TEXT:
		    CALL_DRIVER (con, Handle, retcode, hproc, en_Error,
			(SQL_NULL_HENV,
nHandleType == SQL_HANDLE_DBC ? dhandle : SQL_NULL_HDBC,
			    nHandleType ==
			    SQL_HANDLE_STMT ? dhandle : SQL_NULL_HSTMT,
			    szState, &nNative, pDiagInfoPtr, nBufferLength,
			    pnStringLengthPtr));
		    ODBC_UNLOCK ();
		    return retcode;

		  case SQL_DIAG_NATIVE:
		    CALL_DRIVER (con, Handle, retcode, hproc, en_Error,
			(SQL_NULL_HENV,
nHandleType == SQL_HANDLE_DBC ? dhandle : SQL_NULL_HDBC,
			    nHandleType ==
			    SQL_HANDLE_STMT ? dhandle : SQL_NULL_HSTMT,
			    szState, &nNative, NULL, 0, NULL));
		    if (pDiagInfoPtr)
		      *((SQLINTEGER *) pDiagInfoPtr) = nNative;
		    ODBC_UNLOCK ();
		    return retcode;

		  case SQL_DIAG_SQLSTATE:
		    CALL_DRIVER (con, Handle, retcode, hproc, en_Error,
			(SQL_NULL_HENV,
nHandleType == SQL_HANDLE_DBC ? dhandle : SQL_NULL_HDBC,
			    nHandleType ==
			    SQL_HANDLE_STMT ? dhandle : SQL_NULL_HSTMT,
			    pDiagInfoPtr, &nNative, NULL, 0, NULL));

		    if (pnStringLengthPtr)
		      *pnStringLengthPtr = 5;

		    ODBC_UNLOCK ();
		    return retcode;

		  default:
		    ODBC_UNLOCK ();
		    return SQL_ERROR;
		  }
		if (pDiagInfoPtr)
		  {
		    int len = strlen (szval);
		    if (len > nBufferLength)
		      len = nBufferLength;
		    if (len)
		      strncpy (pDiagInfoPtr, szval, len);
		  }
		if (pnStringLengthPtr)
		  *pnStringLengthPtr = strlen (szval);
	      }			/* ODBC3->ODBC2 */
	  }			/* driver's errors */
      }				/* status records */
    }				/* switch (nRecNumber */
  ODBC_UNLOCK ();
  return (SQL_SUCCESS);
}
#endif
