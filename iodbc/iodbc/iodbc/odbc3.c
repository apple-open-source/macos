/*
 *  odbc3.c
 *
 *  $Id: odbc3.c,v 1.1.1.1 2002/04/08 22:48:10 miner Exp $
 *
 *  ODBC 3.x functions
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

#include "iodbc.h"

#include <sql.h>
#include <sqlext.h>

#if (ODBCVER >= 0x300)
#include <dlproc.h>

#include <herr.h>
#include <henv.h>
#include <hdesc.h>
#include <hdbc.h>
#include <hstmt.h>

#include <itrace.h>


RETCODE SQL_API
SQLAllocHandle (SQLSMALLINT handleType,
    SQLHANDLE inputHandle,
    SQLHANDLE * outputHandlePtr)
{
  switch (handleType)
    {
    case SQL_HANDLE_ENV:
      return SQLAllocEnv (outputHandlePtr);

    case SQL_HANDLE_DBC:
      {
	GENV (genv, inputHandle);

	ODBC_LOCK ();
	if (!IS_VALID_HENV (genv))
	  {
	    ODBC_UNLOCK ();
	    return SQL_INVALID_HANDLE;
	  }
	CLEAR_ERRORS (genv);
	if (genv->odbc_ver == 0)
	  {
	    PUSHSQLERR (genv->herr, en_HY010);
	    ODBC_UNLOCK ();
	    return SQL_ERROR;
	  }
	ODBC_UNLOCK ();

	return SQLAllocConnect (inputHandle, outputHandlePtr);
      }

    case SQL_HANDLE_STMT:
      return SQLAllocStmt (inputHandle, outputHandlePtr);

    case SQL_HANDLE_DESC:
      {
	CONN (con, inputHandle);
	HPROC hproc = SQL_NULL_HPROC;
	RETCODE retcode;
	DESC_t FAR *new_desc;

	ENTER_HDBC (con);

	if (((ENV_t *)(con->henv))->dodbc_ver == SQL_OV_ODBC2)
	  {
	    PUSHSQLERR (con->herr, en_HYC00);
	    LEAVE_HDBC (con, SQL_ERROR);
	  }
	if (!outputHandlePtr)
	  {
	    PUSHSQLERR (con->herr, en_HY009);
	    LEAVE_HDBC (con, SQL_ERROR);
	  }

	hproc = _iodbcdm_getproc (con, en_AllocHandle);

	if (hproc == SQL_NULL_HPROC)
	  {
	    PUSHSQLERR (con->herr, en_IM001);
	    LEAVE_HDBC (con, SQL_ERROR);
	  }

	new_desc = (DESC_t FAR *) MEM_ALLOC (sizeof (DESC_t));
	memset (new_desc, 0, sizeof (DESC_t));
	if (!new_desc)
	  {
	    PUSHSQLERR (con->herr, en_HY001);
	    LEAVE_HDBC (con, SQL_ERROR);
	  }
	CALL_DRIVER (con, con, retcode, hproc, en_AllocHandle,
	    (handleType, con->dhdbc, &new_desc->dhdesc));

	if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
	  {
	    MEM_FREE (new_desc);
	    LEAVE_HDBC (con, SQL_ERROR);
	  }

	new_desc->type = SQL_HANDLE_DESC;
	new_desc->hdbc = con;
	new_desc->hstmt = NULL;
	new_desc->herr = NULL;
	new_desc->desc_cip = 0;
	*outputHandlePtr = new_desc;

	new_desc->next = con->hdesc;
	con->hdesc = new_desc;

	LEAVE_HDBC (con, SQL_SUCCESS);
      }

    default:
      ODBC_LOCK ();
      if (IS_VALID_HDBC (inputHandle))
	{
	  CONN (con, inputHandle);
	  PUSHSQLERR (con->herr, en_HY092);
	  ODBC_UNLOCK ();
	  return SQL_ERROR;
	}
      else if (IS_VALID_HENV (inputHandle))
	{
	  GENV (genv, inputHandle);
	  PUSHSQLERR (genv->herr, en_HY092);
	  ODBC_UNLOCK ();
	  return SQL_ERROR;
	}
      ODBC_UNLOCK ();
      return SQL_INVALID_HANDLE;
    }
}


/**** SQLFreeHandle ****/

RETCODE SQL_API
SQLFreeHandle (SQLSMALLINT handleType,
    SQLHANDLE handle)
{
  switch (handleType)
    {
    case SQL_HANDLE_ENV:
	return SQLFreeEnv ((SQLHENV) handle);

    case SQL_HANDLE_DBC:
	return SQLFreeConnect ((SQLHDBC) handle);

    case SQL_HANDLE_STMT:
	return SQLFreeStmt ((SQLHSTMT) handle, SQL_DROP);

    case SQL_HANDLE_DESC:

      if (IS_VALID_HDESC (handle))
	{
	  DESC (pdesc, handle);
	  CONN (pdbc, pdesc->hdbc);
	  HPROC hproc;
	  RETCODE retcode;
	  DESC_t FAR *curr_desc;

	  if (IS_VALID_HSTMT (pdesc->hstmt))
	    {			/* the desc handle is implicit */
	      PUSHSQLERR (pdesc->herr, en_HY017);
	      return SQL_ERROR;
	    }
	  CLEAR_ERRORS (pdesc);

	  /* remove it from the dbc's list */
	  curr_desc = pdbc->hdesc;
	  while (curr_desc)
	    {
	      if (curr_desc == pdesc)
		{
		  pdbc->hdesc = pdesc->next;
		  break;
		}
	      if (curr_desc->next == pdesc)
		{
		  curr_desc->next = pdesc->next;
		  break;
		}
	      curr_desc = curr_desc->next;
	    }
	  if (!curr_desc)
	    return SQL_INVALID_HANDLE;

	  /* and call the driver's function */
	  hproc = SQL_NULL_HPROC;
	  if (pdesc->dhdesc)
	    {			/* the driver has descriptors */
	      hproc = _iodbcdm_getproc (pdbc, en_FreeHandle);
	      if (hproc == SQL_NULL_HPROC)
		{
		  PUSHSQLERR (pdesc->herr, en_IM001);
		  retcode = SQL_ERROR;
		}
	      else
		CALL_DRIVER (pdbc, pdesc, retcode, hproc, en_AllocHandle,
		    (handleType, pdesc->dhdesc));
	    }

	  _iodbcdm_freesqlerrlist (pdesc->herr);

	  /* invalidate the handle */
	  pdesc->type = 0;
	  MEM_FREE (pdesc);

	  return retcode;
	}
      else
	return SQL_INVALID_HANDLE;

    default:
      ODBC_LOCK ();
      if (IS_VALID_HDBC (handle))
	{
	  CONN (con, handle);
	  PUSHSQLERR (con->herr, en_HY092);
	  ODBC_UNLOCK ();
	  return SQL_ERROR;
	}
      else if (IS_VALID_HENV (handle))
	{
	  GENV (genv, handle);
	  PUSHSQLERR (genv->herr, en_HY092);
	  ODBC_UNLOCK ();
	  return SQL_ERROR;
	}
      ODBC_UNLOCK ();
      return SQL_INVALID_HANDLE;
    }
}


/**** SQLSetEnvAttr ****/

RETCODE SQL_API
SQLSetEnvAttr (SQLHENV environmentHandle,
    SQLINTEGER Attribute,
    SQLPOINTER ValuePtr,
    SQLINTEGER StringLength)
{
  GENV (genv, environmentHandle);

  ODBC_LOCK ();
  if (!IS_VALID_HENV (genv))
    {
      ODBC_UNLOCK ();
      return (SQL_INVALID_HANDLE);
    }
  CLEAR_ERRORS (genv);

  if (genv->hdbc)
    {
      PUSHSQLERR (genv->herr, en_HY010);
      ODBC_UNLOCK ();
      return SQL_ERROR;
    }

  switch (Attribute)
    {
    case SQL_ATTR_CONNECTION_POOLING:
      switch ((SQLINTEGER) ValuePtr)
	{
	case SQL_CP_OFF:
	case SQL_CP_ONE_PER_DRIVER:
	case SQL_CP_ONE_PER_HENV:
	  ODBC_UNLOCK ();
	  return SQL_SUCCESS;	/* not implemented yet */

	default:
	  PUSHSQLERR (genv->herr, en_HY024);
	  ODBC_UNLOCK ();
	  return SQL_ERROR;
	}

    case SQL_ATTR_CP_MATCH:
      switch ((SQLINTEGER) ValuePtr)
	{
	case SQL_CP_STRICT_MATCH:
	case SQL_CP_RELAXED_MATCH:
	  ODBC_UNLOCK ();
	  return SQL_SUCCESS;	/* not implemented yet */

	default:
	  PUSHSQLERR (genv->herr, en_HY024);
	  ODBC_UNLOCK ();
	  return SQL_ERROR;
	}

    case SQL_ATTR_ODBC_VERSION:
      switch ((SQLINTEGER) ValuePtr)
	{
	case SQL_OV_ODBC2:
	case SQL_OV_ODBC3:
	  genv->odbc_ver = (SQLINTEGER) ValuePtr;
	  ODBC_UNLOCK ();
	  return (SQL_SUCCESS);

	default:
	  PUSHSQLERR (genv->herr, en_HY024);
	  ODBC_UNLOCK ();
	  return SQL_ERROR;
	}

    case SQL_ATTR_OUTPUT_NTS:
      switch ((SQLINTEGER) ValuePtr)
	{
	case SQL_TRUE:
	  ODBC_UNLOCK ();
	  return SQL_SUCCESS;

	case SQL_FALSE:
	  PUSHSQLERR (genv->herr, en_HYC00);
	  ODBC_UNLOCK ();
	  return SQL_ERROR;

	default:
	  PUSHSQLERR (genv->herr, en_HY024);
	  ODBC_UNLOCK ();
	  return SQL_ERROR;
	}

    default:
      PUSHSQLERR (genv->herr, en_HY092);
      ODBC_UNLOCK ();
      return SQL_ERROR;
    }
}


RETCODE SQL_API
SQLGetEnvAttr (SQLHENV environmentHandle,
    SQLINTEGER Attribute,
    SQLPOINTER ValuePtr,
    SQLINTEGER BufferLength,
    SQLINTEGER * StringLengthPtr)
{
  GENV (genv, environmentHandle);
  HDBC con;
  RETCODE retcode;

  ODBC_LOCK ();
  if (!IS_VALID_HENV (genv))
    {
      ODBC_UNLOCK ();
      return (SQL_INVALID_HANDLE);
    }
  CLEAR_ERRORS (genv);

  if (Attribute != SQL_ATTR_CONNECTION_POOLING &&
      Attribute != SQL_ATTR_CP_MATCH &&
      Attribute != SQL_ATTR_ODBC_VERSION && 
      Attribute != SQL_ATTR_OUTPUT_NTS)
    {
      PUSHSQLERR (genv->herr, en_HY092);
      ODBC_UNLOCK ();
      return SQL_ERROR;
    }

  /* ODBC DM env attributes */
  if (Attribute == SQL_ATTR_ODBC_VERSION)
    {
      if (ValuePtr)
	*((SQLINTEGER *) ValuePtr) = genv->odbc_ver;
      ODBC_UNLOCK ();
      return SQL_SUCCESS;
    }
  if (Attribute == SQL_ATTR_CONNECTION_POOLING)
    {
      if (ValuePtr)
	*((SQLUINTEGER *) ValuePtr) = SQL_CP_OFF;
      ODBC_UNLOCK ();
      return SQL_SUCCESS;
    }
  if (Attribute == SQL_ATTR_CP_MATCH)
    {
      if (ValuePtr)
	*((SQLUINTEGER *) ValuePtr) = SQL_CP_STRICT_MATCH;
      ODBC_UNLOCK ();
      return SQL_SUCCESS;
    }
  if (Attribute == SQL_ATTR_OUTPUT_NTS)
    {
      if (ValuePtr)
	*((SQLINTEGER *) ValuePtr) = SQL_TRUE;
      ODBC_UNLOCK ();
      return SQL_SUCCESS;
    }

  /* fall back to the first driver */
  if (IS_VALID_HDBC (genv->hdbc))
    {
      CONN (con, genv->hdbc);
      HPROC hproc = _iodbcdm_getproc (con, en_GetEnvAttr);
      if (hproc != SQL_NULL_HPROC)
	{
	  ENVR (env, con->henv);
	  CALL_DRIVER (con, genv, retcode, hproc, en_GetEnvAttr,
	      (env->dhenv, Attribute, ValuePtr, BufferLength,
		  StringLengthPtr));
	  ODBC_UNLOCK ();
	  return retcode;
	}
      else
	{			/* possibly an ODBC2 driver */
	  PUSHSQLERR (genv->herr, en_IM001);
	  ODBC_UNLOCK ();
	  return SQL_ERROR;
	}
    }
  else
    {
      switch ((SQLINTEGER) Attribute)
	{
	case SQL_ATTR_CONNECTION_POOLING:
	  if (ValuePtr)
	    *((SQLINTEGER *) ValuePtr) = SQL_CP_OFF;
	  break;

	case SQL_ATTR_CP_MATCH:
	  if (ValuePtr)
	    *((SQLINTEGER *) ValuePtr) = SQL_CP_STRICT_MATCH;
	  break;

	case SQL_ATTR_ODBC_VERSION:
	  if (ValuePtr)
	    *((SQLINTEGER *) ValuePtr) = genv->odbc_ver;
	  break;
	}
    }
  ODBC_UNLOCK ();
  return SQL_SUCCESS;
}


RETCODE SQL_API
SQLGetStmtAttr (SQLHSTMT statementHandle,
    SQLINTEGER Attribute,
    SQLPOINTER ValuePtr,
    SQLINTEGER BufferLength,
    UNALIGNED SQLINTEGER * StringLengthPtr)
{
  STMT (stmt, statementHandle);
  HPROC hproc;
  RETCODE retcode;

  ENTER_STMT (stmt);

  switch (Attribute)
    {
    case SQL_ATTR_IMP_PARAM_DESC:

      if (ValuePtr)
	{
	  if (IS_VALID_HDESC (stmt->desc[IMP_PARAM_DESC]))
	    *((SQLHANDLE *) ValuePtr) = (SQLHANDLE *) stmt->desc[IMP_PARAM_DESC];
	  else if (IS_VALID_HDESC (stmt->imp_desc[IMP_PARAM_DESC]))
	    *((SQLHANDLE *) ValuePtr) = (SQLHANDLE *) stmt->imp_desc[IMP_PARAM_DESC];
	  else
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	}
      if (StringLengthPtr)
	*StringLengthPtr = SQL_IS_POINTER;
      LEAVE_STMT (stmt, SQL_SUCCESS);

    case SQL_ATTR_APP_PARAM_DESC:

      if (ValuePtr)
	{
	  if (IS_VALID_HDESC (stmt->desc[APP_PARAM_DESC]))
	    *((SQLHANDLE *) ValuePtr) = (SQLHANDLE *) stmt->desc[APP_PARAM_DESC];
	  else if (IS_VALID_HDESC (stmt->imp_desc[APP_PARAM_DESC]))
	    *((SQLHANDLE *) ValuePtr) = (SQLHANDLE *) stmt->imp_desc[APP_PARAM_DESC];
	  else
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	}
      if (StringLengthPtr)
	*StringLengthPtr = SQL_IS_POINTER;
      LEAVE_STMT (stmt, SQL_SUCCESS);

    case SQL_ATTR_IMP_ROW_DESC:

      if (ValuePtr)
	{
	  if (IS_VALID_HDESC (stmt->desc[IMP_ROW_DESC]))
	    *((SQLHANDLE *) ValuePtr) = (SQLHANDLE *) stmt->desc[IMP_ROW_DESC];
	  else if (IS_VALID_HDESC (stmt->imp_desc[IMP_ROW_DESC]))
	    *((SQLHANDLE *) ValuePtr) = (SQLHANDLE *) stmt->imp_desc[IMP_ROW_DESC];
	  else
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	}
      if (StringLengthPtr)
	*StringLengthPtr = SQL_IS_POINTER;
      LEAVE_STMT (stmt, SQL_SUCCESS);

    case SQL_ATTR_APP_ROW_DESC:

      if (ValuePtr)
	{
	  if (IS_VALID_HDESC (stmt->desc[APP_ROW_DESC]))
	    *((SQLHANDLE *) ValuePtr) = (SQLHANDLE *) stmt->desc[APP_ROW_DESC];
	  else if (IS_VALID_HDESC (stmt->imp_desc[APP_ROW_DESC]))
	    *((SQLHANDLE *) ValuePtr) = (SQLHANDLE *) stmt->imp_desc[APP_ROW_DESC];
	  else
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	}
      if (StringLengthPtr)
	*StringLengthPtr = SQL_IS_POINTER;
      LEAVE_STMT (stmt, SQL_SUCCESS);

    case SQL_ATTR_ROW_ARRAY_SIZE:

      if (((ENV_t FAR *) ((DBC_t FAR *) stmt->hdbc)->henv)->dodbc_ver == SQL_OV_ODBC3)
	{
	  hproc = _iodbcdm_getproc (stmt->hdbc, en_GetStmtAttr);
	  if (hproc)
	    {
	      CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_GetStmtAttr,
		  (stmt->dhstmt, Attribute, ValuePtr, BufferLength, StringLengthPtr));
	      LEAVE_STMT (stmt, retcode);
	    }
	  else
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	}
      else
	{			/* an ODBC2 driver */
	  if (ValuePtr)
	    *((SQLUINTEGER *) ValuePtr) = stmt->row_array_size;
	  LEAVE_STMT (stmt, SQL_SUCCESS);
	}

    case SQL_ATTR_ENABLE_AUTO_IPD:
    case SQL_ATTR_CURSOR_SENSITIVITY:
    case SQL_ATTR_CURSOR_SCROLLABLE:
    case SQL_ATTR_PARAM_BIND_TYPE:
    case SQL_ATTR_PARAM_OPERATION_PTR:
    case SQL_ATTR_PARAM_STATUS_PTR:
    case SQL_ATTR_PARAM_BIND_OFFSET_PTR:
    case SQL_ATTR_ROW_BIND_OFFSET_PTR:
    case SQL_ATTR_ROW_OPERATION_PTR:

      hproc = _iodbcdm_getproc (stmt->hdbc, en_GetStmtAttr);
      if (!hproc)
	{
	  PUSHSQLERR (stmt->herr, en_IM001);
	  LEAVE_STMT (stmt, SQL_ERROR);
	}
      else
	{
	  CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_GetStmtAttr,
	      (stmt->dhstmt, Attribute, ValuePtr, BufferLength, StringLengthPtr));
	  LEAVE_STMT (stmt, retcode);
	}

    case SQL_ATTR_FETCH_BOOKMARK_PTR:

      if (((ENV_t FAR *) ((DBC_t FAR *) stmt->hdbc)->henv)->dodbc_ver == SQL_OV_ODBC3)
	{
	  hproc = _iodbcdm_getproc (stmt->hdbc, en_GetStmtAttr);
	  if (hproc)
	    {
	      CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_GetStmtAttr,
		  (stmt->dhstmt, Attribute, ValuePtr, BufferLength, StringLengthPtr));
	      LEAVE_STMT (stmt, retcode);
	    }
	  else
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	}
      else
	{			/* an ODBC2 driver */
	  if (ValuePtr)
	    *((SQLPOINTER *) ValuePtr) = stmt->fetch_bookmark_ptr;
	  LEAVE_STMT (stmt, SQL_SUCCESS);
	}

    case SQL_ATTR_ROWS_FETCHED_PTR:

      if (((ENV_t FAR *) ((DBC_t FAR *) stmt->hdbc)->henv)->dodbc_ver == SQL_OV_ODBC3)
	{
	  hproc = _iodbcdm_getproc (stmt->hdbc, en_GetStmtAttr);
	  if (hproc)
	    {
	      CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_GetStmtAttr,
		  (stmt->dhstmt, Attribute, ValuePtr, BufferLength, StringLengthPtr));
	      LEAVE_STMT (stmt, retcode);
	    }
	  else
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	}
      else
	{			/* an ODBC2 driver */
	  if (ValuePtr)
	    *((SQLPOINTER *) ValuePtr) = stmt->rows_fetched_ptr;
	  LEAVE_STMT (stmt, SQL_SUCCESS);
	}

    case SQL_ATTR_METADATA_ID:

      if (((ENV_t FAR *) ((DBC_t FAR *) stmt->hdbc)->henv)->dodbc_ver == SQL_OV_ODBC3)
	{
	  hproc = _iodbcdm_getproc (stmt->hdbc, en_GetStmtAttr);
	  if (hproc)
	    {
	      CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_GetStmtAttr,
		  (stmt->dhstmt, Attribute, ValuePtr, BufferLength, StringLengthPtr));
	      LEAVE_STMT (stmt, retcode);
	    }
	  else
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	}
      else
	{			/* an ODBC2 driver */
	  if (ValuePtr)
	    *((SQLUINTEGER *) ValuePtr) = SQL_FALSE;
	  LEAVE_STMT (stmt, SQL_SUCCESS);
	}

    case SQL_ATTR_PARAMS_PROCESSED_PTR:

      if (((ENV_t FAR *) ((DBC_t FAR *) stmt->hdbc)->henv)->dodbc_ver == SQL_OV_ODBC3)
	{
	  hproc = _iodbcdm_getproc (stmt->hdbc, en_GetStmtAttr);
	  if (hproc)
	    {
	      CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_GetStmtAttr,
		  (stmt->dhstmt, Attribute, ValuePtr, BufferLength, StringLengthPtr));
	      LEAVE_STMT (stmt, retcode);
	    }
	  else
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	}
      else
	{			/* an ODBC2 driver */
	  if (ValuePtr)
	    *((SQLUINTEGER **) ValuePtr) = stmt->params_processed_ptr;
	  LEAVE_STMT (stmt, SQL_SUCCESS);
	}

    case SQL_ATTR_PARAMSET_SIZE:

      if (((ENV_t FAR *) ((DBC_t FAR *) stmt->hdbc)->henv)->dodbc_ver == SQL_OV_ODBC3)
	{
	  hproc = _iodbcdm_getproc (stmt->hdbc, en_GetStmtAttr);
	  if (hproc)
	    {
	      CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_GetStmtAttr,
		  (stmt->dhstmt, Attribute, ValuePtr, BufferLength, StringLengthPtr));
	      LEAVE_STMT (stmt, retcode);
	    }
	  else
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	}
      else
	{			/* an ODBC2 driver */
	  if (ValuePtr)
	    *((SQLUINTEGER *) ValuePtr) = stmt->paramset_size;
	  LEAVE_STMT (stmt, SQL_SUCCESS);
	}

    case SQL_ATTR_ROW_STATUS_PTR:

      if (((ENV_t FAR *) ((DBC_t FAR *) stmt->hdbc)->henv)->dodbc_ver == SQL_OV_ODBC3)
	{
	  hproc = _iodbcdm_getproc (stmt->hdbc, en_GetStmtAttr);
	  if (hproc)
	    {
	      CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_GetStmtAttr,
		  (stmt->dhstmt, Attribute, ValuePtr, BufferLength, StringLengthPtr));
	      LEAVE_STMT (stmt, retcode);
	    }
	  else
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	}
      else
	{			/* an ODBC2 driver */
	  if (ValuePtr)
	    *((SQLUINTEGER **) ValuePtr) = stmt->row_status_allocated == SQL_FALSE ?
		stmt->row_status_ptr :
		NULL;
	  LEAVE_STMT (stmt, SQL_SUCCESS);
	}

    case SQL_ATTR_ASYNC_ENABLE:
    case SQL_ATTR_MAX_ROWS:
    case SQL_ATTR_QUERY_TIMEOUT:
    case SQL_ATTR_CONCURRENCY:
    case SQL_ROWSET_SIZE:
    case SQL_ATTR_CURSOR_TYPE:
    case SQL_ATTR_KEYSET_SIZE:
    case SQL_ATTR_NOSCAN:
    case SQL_ATTR_RETRIEVE_DATA:
    case SQL_ATTR_ROW_BIND_TYPE:
    case SQL_ATTR_ROW_NUMBER:
    case SQL_ATTR_SIMULATE_CURSOR:
    case SQL_ATTR_USE_BOOKMARKS:
    case SQL_ATTR_MAX_LENGTH:

      if (((ENV_t FAR *) ((DBC_t FAR *) stmt->hdbc)->henv)->dodbc_ver == SQL_OV_ODBC3)
	{
	  hproc = _iodbcdm_getproc (stmt->hdbc, en_GetStmtAttr);
	  if (hproc)
	    {
	      CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_GetStmtAttr,
		  (stmt->dhstmt, Attribute, ValuePtr, BufferLength, StringLengthPtr));
	      LEAVE_STMT (stmt, retcode);
	    }
	  else
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	}
      else
	{			/* an ODBC2 driver */
	  hproc = _iodbcdm_getproc (stmt->hdbc, en_GetStmtOption);
	  if (hproc)
	    {
	      CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_GetStmtOption,
		  (stmt->dhstmt, Attribute, ValuePtr));
	      LEAVE_STMT (stmt, retcode);
	    }
	  else
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	}
    default:
      hproc = _iodbcdm_getproc (stmt->hdbc, en_GetStmtAttr);
      if (hproc)
	{
	  CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_GetStmtAttr,
	      (stmt->dhstmt, Attribute, ValuePtr, BufferLength, StringLengthPtr));
	  LEAVE_STMT (stmt, retcode);
	}
      else
	{
	  PUSHSQLERR (stmt->herr, en_IM001);
	  LEAVE_STMT (stmt, SQL_ERROR);
	}
    }
  LEAVE_STMT (stmt, SQL_SUCCESS);
}


/**** SQLSetStmtAttr ****/

RETCODE SQL_API
SQLSetStmtAttr (SQLHSTMT statementHandle,
    SQLINTEGER Attribute,
    SQLPOINTER ValuePtr,
    SQLINTEGER StringLength)
{
  STMT (stmt, statementHandle);
  HPROC hproc;
  RETCODE retcode;

  ENTER_STMT (stmt);

  if (stmt->state == en_stmt_needdata)
    {
      PUSHSQLERR (stmt->herr, en_HY010);
      LEAVE_STMT (stmt, SQL_ERROR);
    }

  switch (Attribute)
    {

    case SQL_ATTR_APP_PARAM_DESC:

      if (ValuePtr == SQL_NULL_HDESC || ValuePtr == stmt->imp_desc[APP_PARAM_DESC])
	{
	  HDESC hdesc = ValuePtr == SQL_NULL_HDESC ? ValuePtr : stmt->imp_desc[APP_PARAM_DESC]->dhdesc;
	  hproc = _iodbcdm_getproc (stmt->hdbc, en_SetStmtAttr);
	  if (!hproc)
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }

	  CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_SetStmtAttr,
	      (stmt->dhstmt, Attribute, hdesc, StringLength));
	  if (retcode != SQL_SUCCESS || retcode != SQL_SUCCESS_WITH_INFO)
	    LEAVE_STMT (stmt, SQL_ERROR);
	  stmt->desc[APP_PARAM_DESC] = SQL_NULL_HDESC;
	  LEAVE_STMT (stmt, SQL_SUCCESS);
	}

      if (!IS_VALID_HDESC (ValuePtr))
	{
	  PUSHSQLERR (stmt->herr, en_HY024);
	  LEAVE_STMT (stmt, SQL_ERROR);
	}
      else
	{
	  DESC (pdesc, ValuePtr);
	  if (pdesc->hdbc != stmt->hdbc || IS_VALID_HSTMT (pdesc->hstmt))
	    {
	      PUSHSQLERR (stmt->herr, en_HY017);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }

	  hproc = _iodbcdm_getproc (stmt->hdbc, en_SetStmtAttr);
	  if (!hproc)
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }

	  CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_SetStmtAttr,
	      (stmt->dhstmt, Attribute, pdesc->dhdesc, StringLength));
	  if (retcode != SQL_SUCCESS || retcode != SQL_SUCCESS_WITH_INFO)
	    LEAVE_STMT (stmt, SQL_ERROR);

	  stmt->desc[APP_PARAM_DESC] = ValuePtr;
	  LEAVE_STMT (stmt, SQL_SUCCESS);
	}

    case SQL_ATTR_APP_ROW_DESC:

      if (ValuePtr == SQL_NULL_HDESC || ValuePtr == stmt->imp_desc[APP_ROW_DESC])
	{
	  HDESC hdesc = ValuePtr == SQL_NULL_HDESC ? ValuePtr : stmt->imp_desc[APP_ROW_DESC]->dhdesc;
	  hproc = _iodbcdm_getproc (stmt->hdbc, en_SetStmtAttr);
	  if (!hproc)
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }

	  CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_SetStmtAttr,
	      (stmt->dhstmt, Attribute, hdesc, StringLength));
	  if (retcode != SQL_SUCCESS || retcode != SQL_SUCCESS_WITH_INFO)
	    LEAVE_STMT (stmt, SQL_ERROR);
	  stmt->desc[APP_ROW_DESC] = SQL_NULL_HDESC;
	  LEAVE_STMT (stmt, SQL_SUCCESS);
	}

      if (!IS_VALID_HDESC (ValuePtr))
	{
	  PUSHSQLERR (stmt->herr, en_HY024);
	  LEAVE_STMT (stmt, SQL_ERROR);
	}
      else
	{
	  DESC (pdesc, ValuePtr);
	  if (pdesc->hdbc != stmt->hdbc || IS_VALID_HSTMT (pdesc->hstmt))
	    {
	      PUSHSQLERR (stmt->herr, en_HY017);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }

	  hproc = _iodbcdm_getproc (stmt->hdbc, en_SetStmtAttr);
	  if (!hproc)
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }

	  CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_SetStmtAttr,
	      (stmt->dhstmt, Attribute, pdesc->dhdesc, StringLength));
	  if (retcode != SQL_SUCCESS || retcode != SQL_SUCCESS_WITH_INFO)
	    LEAVE_STMT (stmt, SQL_ERROR);

	  stmt->desc[APP_ROW_DESC] = ValuePtr;
	  LEAVE_STMT (stmt, SQL_SUCCESS);
	}

    case SQL_ATTR_CURSOR_SCROLLABLE:
    case SQL_ATTR_CURSOR_SENSITIVITY:
    case SQL_ATTR_ENABLE_AUTO_IPD:
    case SQL_ATTR_METADATA_ID:
    case SQL_ATTR_PARAM_BIND_OFFSET_PTR:
    case SQL_ATTR_PARAM_BIND_TYPE:
    case SQL_ATTR_PARAM_OPERATION_PTR:
    case SQL_ATTR_PARAM_STATUS_PTR:
    case SQL_ATTR_ROW_BIND_OFFSET_PTR:
    case SQL_ATTR_ROW_OPERATION_PTR:

      hproc = _iodbcdm_getproc (stmt->hdbc, en_SetStmtAttr);
      if (!hproc)
	{
	  PUSHSQLERR (stmt->herr, en_IM001);
	  LEAVE_STMT (stmt, SQL_ERROR);
	}
      else
	{
	  CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_SetStmtAttr,
	      (stmt->dhstmt, Attribute, ValuePtr, StringLength));
	  LEAVE_STMT (stmt, retcode);
	}

    case SQL_ATTR_ROWS_FETCHED_PTR:

      if (((ENV_t FAR *) ((DBC_t FAR *) stmt->hdbc)->henv)->dodbc_ver == SQL_OV_ODBC3)
	{
	  hproc = _iodbcdm_getproc (stmt->hdbc, en_SetStmtAttr);
	  if (hproc)
	    {
	      CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_SetStmtAttr,
		  (stmt->dhstmt, Attribute, ValuePtr, StringLength));
	      LEAVE_STMT (stmt, retcode);
	    }
	  else
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	}
      else
	{			/* an ODBC2 driver */
	  stmt->rows_fetched_ptr = ValuePtr;
	  LEAVE_STMT (stmt, SQL_SUCCESS);
	}

    case SQL_ATTR_FETCH_BOOKMARK_PTR:

      if (((ENV_t FAR *) ((DBC_t FAR *) stmt->hdbc)->henv)->dodbc_ver == SQL_OV_ODBC3)
	{
	  hproc = _iodbcdm_getproc (stmt->hdbc, en_SetStmtAttr);
	  if (hproc)
	    {
	      CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_SetStmtAttr,
		  (stmt->dhstmt, Attribute, ValuePtr, StringLength));
	      LEAVE_STMT (stmt, retcode);
	    }
	  else
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	}
      else
	{			/* an ODBC2 driver */
	  stmt->fetch_bookmark_ptr = ValuePtr;
	  LEAVE_STMT (stmt, SQL_SUCCESS);
	}

    case SQL_ATTR_PARAMS_PROCESSED_PTR:

      if (((ENV_t FAR *) ((DBC_t FAR *) stmt->hdbc)->henv)->dodbc_ver == SQL_OV_ODBC3)
	{
	  hproc = _iodbcdm_getproc (stmt->hdbc, en_SetStmtAttr);
	  if (hproc)
	    {
	      CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_SetStmtAttr,
		  (stmt->dhstmt, Attribute, ValuePtr, StringLength));
	      LEAVE_STMT (stmt, retcode);
	    }
	  else
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	}
      else
	{			/* an ODBC2 driver */
	  stmt->params_processed_ptr = ValuePtr;
	  LEAVE_STMT (stmt, SQL_SUCCESS);
	}

    case SQL_ATTR_PARAMSET_SIZE:

      if (((ENV_t FAR *) ((DBC_t FAR *) stmt->hdbc)->henv)->dodbc_ver == SQL_OV_ODBC3)
	{
	  hproc = _iodbcdm_getproc (stmt->hdbc, en_SetStmtAttr);
	  if (hproc)
	    {
	      CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_SetStmtAttr,
		  (stmt->dhstmt, Attribute, ValuePtr, StringLength));
	      LEAVE_STMT (stmt, retcode);
	    }
	  else
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	}
      else
	{			/* an ODBC2 driver */
	  stmt->paramset_size = (SQLUINTEGER) ValuePtr;
	  LEAVE_STMT (stmt, SQL_SUCCESS);
	}

    case SQL_ATTR_ROW_ARRAY_SIZE:

      if (((ENV_t FAR *) ((DBC_t FAR *) stmt->hdbc)->henv)->dodbc_ver == SQL_OV_ODBC3)
	{
	  hproc = _iodbcdm_getproc (stmt->hdbc, en_SetStmtAttr);
	  if (hproc)
	    {
	      CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_SetStmtAttr,
		  (stmt->dhstmt, Attribute, ValuePtr, StringLength));
	      LEAVE_STMT (stmt, retcode);
	    }
	  else
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	}
      else
	{			/* an ODBC2 driver */
	  if ((SQLUINTEGER) ValuePtr < 1)
	    {
	      PUSHSQLERR (stmt->herr, en_HY024);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	  stmt->row_array_size = (SQLUINTEGER) ValuePtr;

	  /* reallocate the row_status_ptr */
	  if (stmt->row_status_ptr && stmt->row_status_allocated == SQL_TRUE)
	    {
	      MEM_FREE (stmt->row_status_ptr);
	      stmt->row_status_allocated = SQL_FALSE;
	    }
	  stmt->row_status_ptr = MEM_ALLOC (sizeof (SQLUINTEGER) * stmt->row_array_size);
	  if (!stmt->row_status_ptr)
	    {
	      PUSHSQLERR (stmt->herr, en_HY001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	  stmt->row_status_allocated = SQL_TRUE;

	  /*
	   *  Tell the driver the rowset size has changed
	   */
	  hproc = _iodbcdm_getproc (stmt->hdbc, en_SetStmtOption);
	  if (hproc)
	    {
	      CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_SetStmtOption,
		  (stmt->dhstmt, SQL_ROWSET_SIZE, stmt->row_array_size));
	      LEAVE_STMT (stmt, retcode);
	    }
	  else
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }

	  LEAVE_STMT (stmt, SQL_SUCCESS);
	}

    case SQL_ATTR_ROW_STATUS_PTR:

      if (((ENV_t FAR *) ((DBC_t FAR *) stmt->hdbc)->henv)->dodbc_ver == SQL_OV_ODBC3)
	{
	  hproc = _iodbcdm_getproc (stmt->hdbc, en_SetStmtAttr);
	  if (hproc)
	    {
	      CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_SetStmtAttr,
		  (stmt->dhstmt, Attribute, ValuePtr, StringLength));
	      LEAVE_STMT (stmt, retcode);
	    }
	  else
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	}
      else
	{			/* an ODBC2 driver */
	  /* free the implicit row status ptr (if any) */
	  if (stmt->row_status_ptr && stmt->row_status_allocated == SQL_TRUE)
	    {
	      MEM_FREE (stmt->row_status_ptr);
	      stmt->row_status_allocated = SQL_FALSE;
	    }
	  stmt->row_status_ptr = ValuePtr;
	  LEAVE_STMT (stmt, SQL_SUCCESS);
	}

    case SQL_ATTR_ASYNC_ENABLE:
    case SQL_ATTR_CONCURRENCY:
    case SQL_ATTR_CURSOR_TYPE:
    case SQL_ATTR_KEYSET_SIZE:
    case SQL_ATTR_MAX_ROWS:
    case SQL_ATTR_NOSCAN:
    case SQL_ATTR_QUERY_TIMEOUT:
    case SQL_ATTR_RETRIEVE_DATA:
    case SQL_ATTR_ROW_BIND_TYPE:
    case SQL_ATTR_ROW_NUMBER:
    case SQL_ATTR_SIMULATE_CURSOR:
    case SQL_ATTR_USE_BOOKMARKS:
    case SQL_ROWSET_SIZE:
    case SQL_ATTR_MAX_LENGTH:

      if (((ENV_t FAR *) ((DBC_t FAR *) stmt->hdbc)->henv)->dodbc_ver == SQL_OV_ODBC3)
	{
	  hproc = _iodbcdm_getproc (stmt->hdbc, en_SetStmtAttr);
	  if (hproc)
	    {
	      CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_SetStmtAttr,
		  (stmt->dhstmt, Attribute, ValuePtr, StringLength));
	      LEAVE_STMT (stmt, retcode);
	    }
	  else
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	}
      else
	{			/* an ODBC2 driver */
	  hproc = _iodbcdm_getproc (stmt->hdbc, en_SetStmtOption);
	  if (hproc)
	    {
	      CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_SetStmtOption,
		  (stmt->dhstmt, Attribute, ValuePtr));
	      LEAVE_STMT (stmt, retcode);
	    }
	  else
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	}
    default:
      hproc = _iodbcdm_getproc (stmt->hdbc, en_SetStmtOption);
      if (hproc)
	{
	  CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_SetStmtOption,
	      (stmt->dhstmt, Attribute, ValuePtr));
	  LEAVE_STMT (stmt, retcode);
	}
      else
	{
	  PUSHSQLERR (stmt->herr, en_IM001);
	  LEAVE_STMT (stmt, SQL_ERROR);
	}
    }
  LEAVE_STMT (stmt, (SQL_SUCCESS));
}


RETCODE SQL_API
SQLSetConnectAttr (SQLHDBC connectionHandle,
    SQLINTEGER Attribute,
    SQLPOINTER ValuePtr,
    SQLINTEGER StringLength)
{
  CONN (con, connectionHandle);
  HPROC hproc;
  RETCODE retcode;

  ENTER_HDBC (con);

  if (con->state == en_dbc_needdata)
    {
      PUSHSQLERR (con->herr, en_HY010);
      LEAVE_HDBC (con, SQL_ERROR);
    }

  hproc = _iodbcdm_getproc (con, en_SetConnectAttr);
  if (hproc)
    {
      CALL_DRIVER (con, con, retcode, hproc, en_SetConnectAttr,
	  (con->dhdbc, Attribute, ValuePtr, StringLength));
      LEAVE_HDBC (con, retcode);
    }

  switch (Attribute)
    {

    case SQL_ATTR_AUTO_IPD:
      PUSHSQLERR (con->herr, en_HY092);
      LEAVE_HDBC (con, SQL_ERROR);

    default:
      retcode = 
	_iodbcdm_SetConnectOption (con, Attribute, (SQLUINTEGER) ValuePtr);
      LEAVE_HDBC (con, retcode);
    }
}


RETCODE SQL_API
SQLGetConnectAttr (SQLHDBC connectionHandle,
    SQLINTEGER Attribute,
    SQLPOINTER ValuePtr,
    SQLINTEGER StringLength,
    UNALIGNED SQLINTEGER * StringLengthPtr)
{
  CONN (con, connectionHandle);
  HPROC hproc;
  RETCODE retcode;

  ENTER_HDBC (con);

  if (con->state == en_dbc_needdata)
    {
      PUSHSQLERR (con->herr, en_HY010);
      LEAVE_HDBC (con, SQL_ERROR);
    }

  hproc = _iodbcdm_getproc (con, en_GetConnectAttr);
  if (hproc)
    {
      CALL_DRIVER (con, con, retcode, hproc, en_GetConnectAttr,
	  (con->dhdbc, Attribute, ValuePtr, StringLength, StringLengthPtr));
      LEAVE_HDBC (con, retcode);
    }

  retcode = _iodbcdm_GetConnectOption (con, Attribute, ValuePtr);
  if (retcode != SQL_SUCCESS || retcode != SQL_SUCCESS_WITH_INFO)
    LEAVE_HDBC (con, retcode);

  if (StringLengthPtr)
    {
      *StringLengthPtr = 0;
      if (ValuePtr)
	switch (Attribute)
	  {
	  case SQL_ATTR_CURRENT_CATALOG:
	  case SQL_ATTR_TRACEFILE:
	  case SQL_ATTR_TRANSLATE_LIB:
	    *StringLengthPtr = strlen (ValuePtr);
	  }
    }
  LEAVE_HDBC (con, retcode);
}


RETCODE SQL_API
SQLGetDescField (SQLHDESC descriptorHandle,
    SQLSMALLINT RecNumber,
    SQLSMALLINT FieldIdentifier,
    SQLPOINTER ValuePtr,
    SQLINTEGER BufferLength,
    UNALIGNED SQLINTEGER * StringLengthPtr)
{
  DESC (desc, descriptorHandle);
  HPROC hproc;
  RETCODE retcode;

  ENTER_HDESC (desc);

  hproc = _iodbcdm_getproc (desc->hdbc, en_GetDescField);
  if (!hproc)
    {
      PUSHSQLERR (desc->herr, en_IM001);
      LEAVE_HDESC (desc, SQL_ERROR);
    }

  CALL_DRIVER (desc->hdbc, desc, retcode, hproc, en_GetDescField,
      (desc->dhdesc, RecNumber, FieldIdentifier, ValuePtr, BufferLength,
	  StringLengthPtr));

  LEAVE_HDESC (desc, retcode);
}


RETCODE SQL_API
SQLSetDescField (SQLHDESC descriptorHandle,
    SQLSMALLINT RecNumber,
    SQLSMALLINT FieldIdentifier,
    SQLPOINTER ValuePtr,
    SQLINTEGER BufferLength)
{
  DESC (desc, descriptorHandle);
  HPROC hproc;
  RETCODE retcode;

  ENTER_HDESC (desc);

  hproc = _iodbcdm_getproc (desc->hdbc, en_SetDescField);
  if (!hproc)
    {
      PUSHSQLERR (desc->herr, en_IM001);
      LEAVE_HDESC (desc, SQL_ERROR);
    }

  CALL_DRIVER (desc->hdbc, desc, retcode, hproc, en_SetDescField,
      (desc->dhdesc, RecNumber, FieldIdentifier, ValuePtr, BufferLength));

  LEAVE_HDESC (desc, retcode);
}


RETCODE SQL_API
SQLGetDescRec (SQLHDESC descriptorHandle,
    SQLSMALLINT RecNumber,
    SQLCHAR * Name,
    SQLSMALLINT BufferLength,
    UNALIGNED SQLSMALLINT * StringLengthPtr,
    UNALIGNED SQLSMALLINT * TypePtr,
    UNALIGNED SQLSMALLINT * SubTypePtr,
    UNALIGNED SQLINTEGER * LengthPtr,
    UNALIGNED SQLSMALLINT * PrecisionPtr,
    UNALIGNED SQLSMALLINT * ScalePtr,
    UNALIGNED SQLSMALLINT * NullablePtr)
{
  DESC (desc, descriptorHandle);
  HPROC hproc;
  RETCODE retcode;

  ENTER_HDESC (desc);

  hproc = _iodbcdm_getproc (desc->hdbc, en_GetDescRec);
  if (!hproc)
    {
      PUSHSQLERR (desc->herr, en_IM001);
      LEAVE_HDESC (desc, SQL_ERROR);
    }

  CALL_DRIVER (desc->hdbc, desc, retcode, hproc, en_GetDescRec,
      (desc->dhdesc, RecNumber, Name, BufferLength, StringLengthPtr, TypePtr, SubTypePtr, LengthPtr, PrecisionPtr, ScalePtr, NullablePtr));

  LEAVE_HDESC (desc, retcode);
}


RETCODE SQL_API
SQLSetDescRec (SQLHDESC arg0,
    SQLSMALLINT arg1,
    SQLSMALLINT arg2,
    SQLSMALLINT arg3,
    SQLINTEGER arg4,
    SQLSMALLINT arg5,
    SQLSMALLINT arg6,
    SQLPOINTER arg7,
    UNALIGNED SQLINTEGER * arg8,
    UNALIGNED SQLINTEGER * arg9)
{
  DESC (desc, arg0);
  HPROC hproc;
  RETCODE retcode;

  ENTER_HDESC (desc);

  hproc = _iodbcdm_getproc (desc->hdbc, en_SetDescRec);
  if (!hproc)
    {
      PUSHSQLERR (desc->herr, en_IM001);
      LEAVE_HDESC (desc, SQL_ERROR);
    }

  CALL_DRIVER (desc->hdbc, desc, retcode, hproc, en_SetDescRec,
      (desc->dhdesc, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9));

  LEAVE_HDESC (desc, retcode);
}


RETCODE SQL_API
SQLCopyDesc (
    SQLHDESC arg0,
    SQLHDESC arg1)
{

  DESC (desc, arg0);
  DESC (desc1, arg1);
  HPROC hproc;
  RETCODE retcode;

  ENTER_HDESC (desc);

  ODBC_LOCK ();
  if (!IS_VALID_HDESC (desc1))
    {
      ODBC_UNLOCK ();
      LEAVE_HDESC (desc, SQL_INVALID_HANDLE);
    }
  CLEAR_ERRORS (desc1);
  ODBC_UNLOCK ();

  hproc = _iodbcdm_getproc (desc->hdbc, en_CopyDesc);
  if (!hproc)
    {
      PUSHSQLERR (desc->herr, en_IM001);
      LEAVE_HDESC (desc, SQL_ERROR);
    }

  CALL_DRIVER (desc->hdbc, desc, retcode, hproc, en_CopyDesc,
      (desc->dhdesc, desc1->dhdesc));

  LEAVE_HDESC (desc, retcode);
}


RETCODE SQL_API
SQLColAttribute (
    SQLHSTMT statementHandle,
    SQLUSMALLINT ColumnNumber,
    SQLUSMALLINT FieldIdentifier,
    SQLPOINTER CharacterAttributePtr,
    SQLSMALLINT BufferLength,
    UNALIGNED SQLSMALLINT * StringLengthPtr,
    SQLPOINTER NumericAttributePtr)
{
  STMT (stmt, statementHandle);
  HPROC hproc;
  RETCODE retcode;

  ENTER_STMT (stmt);

  hproc = _iodbcdm_getproc (stmt->hdbc, en_ColAttribute);
  if (hproc)
    {
      CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_ColAttribute,
	  (stmt->dhstmt, ColumnNumber, FieldIdentifier, CharacterAttributePtr, BufferLength, StringLengthPtr, NumericAttributePtr));
      LEAVE_STMT (stmt, retcode);
    }

  if (ColumnNumber == 0)
    {
      char *szval = "";
      int isSz = 0;
      SQLINTEGER val = 0;

      switch (FieldIdentifier)
	{
	case SQL_DESC_AUTO_UNIQUE_VALUE:
	case SQL_DESC_CASE_SENSITIVE:
	case SQL_DESC_FIXED_PREC_SCALE:
	case SQL_DESC_UNSIGNED:
	  val = SQL_FALSE;
	  break;

	case SQL_DESC_LABEL:
	case SQL_DESC_CATALOG_NAME:
	case SQL_DESC_LITERAL_PREFIX:
	case SQL_DESC_LITERAL_SUFFIX:
	case SQL_DESC_LOCAL_TYPE_NAME:
	case SQL_DESC_NAME:
	case SQL_DESC_SCHEMA_NAME:
	case SQL_DESC_TABLE_NAME:
	case SQL_DESC_TYPE_NAME:
	  isSz = 1;
	  break;

	case SQL_DESC_CONCISE_TYPE:
	case SQL_DESC_TYPE:
	  val = SQL_BINARY;
	  break;

	case SQL_DESC_COUNT:
	  hproc = _iodbcdm_getproc (stmt->hdbc, en_NumResultCols);
	  if (!hproc)
	    {
	      PUSHSQLERR (stmt->herr, en_IM001);
	      LEAVE_STMT (stmt, SQL_ERROR);
	    }
	  CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_NumResultCols,
	      (stmt->dhstmt, NumericAttributePtr));
	  LEAVE_STMT (stmt, retcode);

	case SQL_DESC_LENGTH:
	case SQL_DESC_DATETIME_INTERVAL_CODE:
	case SQL_DESC_SCALE:
	  val = 0;
	  break;

	case SQL_DESC_DISPLAY_SIZE:
	  val = 8;
	  break;

	case SQL_DESC_NULLABLE:
	  val = SQL_NO_NULLS;
	  break;

	case SQL_DESC_OCTET_LENGTH:
	case SQL_DESC_PRECISION:
	  val = 4;
	  break;

	case SQL_DESC_SEARCHABLE:
	  val = SQL_PRED_NONE;
	  break;

	case SQL_DESC_UNNAMED:
	  val = SQL_UNNAMED;
	  break;

	case SQL_DESC_UPDATABLE:
	  val = SQL_ATTR_READONLY;
	  break;

	default:
	  PUSHSQLERR (stmt->herr, en_HYC00);
	  LEAVE_STMT (stmt, SQL_ERROR);
	}
      if (isSz)
	{
	  int len = STRLEN (szval), len1;
	  len1 = len > BufferLength ? BufferLength - 1 : len;
	  if (CharacterAttributePtr)
	    {
	      STRNCPY (CharacterAttributePtr, szval, len1);
	      ((SQLCHAR FAR *) CharacterAttributePtr)[len1] = 0;
	    }
	  if (StringLengthPtr)
	    *StringLengthPtr = len;
	}
      else
	{
	  if (NumericAttributePtr)
	    *((SQLINTEGER *) NumericAttributePtr) = val;
	}
      LEAVE_STMT (stmt, SQL_SUCCESS);
    }
  else
    {				/* all other */
      switch (FieldIdentifier)
	{
	case SQL_DESC_SCALE:
	  FieldIdentifier = SQL_COLUMN_SCALE;
	  break;

	case SQL_DESC_LENGTH:
	  FieldIdentifier = SQL_COLUMN_LENGTH;
	  break;

	case SQL_DESC_PRECISION:
	  FieldIdentifier = SQL_COLUMN_PRECISION;
	  break;

	case SQL_DESC_COUNT:
	  FieldIdentifier = SQL_COLUMN_COUNT;
	  break;

	case SQL_DESC_NAME:
	  FieldIdentifier = SQL_COLUMN_NAME;
	  break;

	case SQL_DESC_NULLABLE:
	  FieldIdentifier = SQL_COLUMN_NULLABLE;
	  break;

	case SQL_DESC_TYPE:
	  FieldIdentifier = SQL_COLUMN_TYPE;
	  break;

	case SQL_DESC_BASE_COLUMN_NAME:
	case SQL_DESC_BASE_TABLE_NAME:
	case SQL_DESC_LITERAL_PREFIX:
	case SQL_DESC_LITERAL_SUFFIX:
	case SQL_DESC_LOCAL_TYPE_NAME:
	case SQL_DESC_NUM_PREC_RADIX:
	case SQL_DESC_OCTET_LENGTH:
	case SQL_DESC_UNNAMED:
	  PUSHSQLERR (stmt->herr, en_HY091);
	  LEAVE_STMT (stmt, SQL_ERROR);
	}
      hproc = _iodbcdm_getproc (stmt->hdbc, en_ColAttributes);
      if (hproc)
	{

	  CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_ColAttribute,
	      (stmt->dhstmt, ColumnNumber, FieldIdentifier, CharacterAttributePtr, BufferLength, StringLengthPtr, NumericAttributePtr));
	  LEAVE_STMT (stmt, retcode);
	}
      else
	{
	  PUSHSQLERR (stmt->herr, en_IM001);
	  LEAVE_STMT (stmt, SQL_ERROR);
	}
    }
}


RETCODE SQL_API
SQLEndTran (SQLSMALLINT handleType,
    SQLHANDLE Handle,
    SQLSMALLINT completionType)
{
  switch (handleType)
    {
    case SQL_HANDLE_DBC:
    case SQL_HANDLE_ENV:
      break;
    default:
      return SQL_INVALID_HANDLE;
    }

  return SQLTransact (
      handleType == SQL_HANDLE_ENV ? Handle : SQL_NULL_HENV,
      handleType == SQL_HANDLE_DBC ? Handle : SQL_NULL_HDBC,
      completionType);
}


RETCODE SQL_API
SQLBulkOperations (SQLHSTMT statementHandle,
    SQLSMALLINT Operation)
{
  STMT (stmt, statementHandle);
  HPROC hproc;
  RETCODE retcode;

  ENTER_STMT (stmt);

  switch (Operation)
    {
    case SQL_ADD:
    case SQL_UPDATE_BY_BOOKMARK:
    case SQL_DELETE_BY_BOOKMARK:
    case SQL_FETCH_BY_BOOKMARK:
      break;
    default:
      PUSHSQLERR (stmt->herr, en_HY092);
      LEAVE_STMT (stmt, SQL_ERROR);
    }

  hproc = _iodbcdm_getproc (stmt->hdbc, en_BulkOperations);
  if (hproc)
    {
      CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_BulkOperations,
	  (stmt->dhstmt, Operation));
      LEAVE_STMT (stmt, retcode);
    }

  switch (Operation)
    {
    case SQL_ADD:
      retcode = _iodbcdm_SetPos (statementHandle, 
		0, SQL_ADD, SQL_LOCK_NO_CHANGE);
      LEAVE_STMT (stmt, retcode);

    default:
      PUSHSQLERR (stmt->herr, en_HYC00);
      LEAVE_STMT (stmt, SQL_ERROR);
    }
}


RETCODE SQL_API
SQLFetchScroll (SQLHSTMT statementHandle,
    SQLSMALLINT fetchOrientation,
    SQLINTEGER fetchOffset)
{
  STMT (stmt, statementHandle);
  HPROC hproc;
  RETCODE retcode;

  ENTER_STMT (stmt);

  switch (fetchOrientation)
    {
    case SQL_FETCH_NEXT:
    case SQL_FETCH_PRIOR:
    case SQL_FETCH_FIRST:
    case SQL_FETCH_LAST:
    case SQL_FETCH_ABSOLUTE:
    case SQL_FETCH_RELATIVE:
    case SQL_FETCH_BOOKMARK:
      break;
    default:
      PUSHSQLERR (stmt->herr, en_HY092);
      LEAVE_STMT (stmt, SQL_ERROR);
    }

  /* check state */
  if (stmt->asyn_on == en_NullProc)
    {
      switch (stmt->state)
	{
	case en_stmt_allocated:
	case en_stmt_prepared:
	case en_stmt_fetched:
	case en_stmt_needdata:
	case en_stmt_mustput:
	case en_stmt_canput:
	  PUSHSQLERR (stmt->herr, en_S1010);
	  return SQL_ERROR;

	default:
	  break;
	}
    }
  else if (stmt->asyn_on != en_FetchScroll)
    {
      PUSHSQLERR (stmt->herr, en_S1010);
      return SQL_ERROR;
    }

  hproc = _iodbcdm_getproc (stmt->hdbc, en_FetchScroll);
  if (hproc)
    {
      CALL_DRIVER (stmt->hdbc, stmt, retcode, hproc, en_FetchScroll,
	  (stmt->dhstmt, fetchOrientation, fetchOffset));
    }
  else
    {
  if (!stmt->row_status_ptr)
    {
      PUSHSQLERR (stmt->herr, en_HYC00);
      LEAVE_STMT (stmt, SQL_ERROR);
    }

  if (fetchOrientation == SQL_FETCH_BOOKMARK)
    {
      if (fetchOffset)
	{
	  PUSHSQLERR (stmt->herr, en_HYC00);
	  LEAVE_STMT (stmt, SQL_ERROR);
	}
      retcode = _iodbcdm_ExtendedFetch (statementHandle, fetchOrientation,
	      stmt->
	      fetch_bookmark_ptr ? *((SQLINTEGER *) stmt->fetch_bookmark_ptr)
	      : 0, stmt->rows_fetched_ptr, stmt->row_status_ptr);
    }
  else
    retcode =
	_iodbcdm_ExtendedFetch (statementHandle, fetchOrientation,
	fetchOffset, stmt->rows_fetched_ptr, stmt->row_status_ptr);

    }

  /* state transition */
  if (stmt->asyn_on == en_FetchScroll)
    {
      switch (retcode)
	{
	case SQL_SUCCESS:
	case SQL_SUCCESS_WITH_INFO:
	case SQL_NO_DATA_FOUND:
	case SQL_ERROR:
	  stmt->asyn_on = en_NullProc;
	  break;

	case SQL_STILL_EXECUTING:
	default:
	  return retcode;
	}
    }

  switch (stmt->state)
    {
    case en_stmt_cursoropen:
    case en_stmt_xfetched:
      switch (retcode)
	{
	case SQL_SUCCESS:
	case SQL_SUCCESS_WITH_INFO:
	case SQL_NO_DATA_FOUND:
	  stmt->state = en_stmt_xfetched;
	  stmt->cursor_state = en_stmt_cursor_xfetched;
	  break;

	case SQL_STILL_EXECUTING:
	  stmt->asyn_on = en_FetchScroll;
	  break;

	default:
	  break;
	}
      break;

    default:
      break;
    }

  LEAVE_STMT (stmt, retcode);
}


SQLRETURN SQL_API
SQLBindParam (
    SQLHSTMT hstmt,
    SQLUSMALLINT ipar,
    SQLSMALLINT fCType,
    SQLSMALLINT fSqlType,
    SQLUINTEGER cbParamDef,
    SQLSMALLINT ibScale,
    SQLPOINTER rgbValue,
    SQLINTEGER FAR * pcbValue)
{
  return SQLBindParameter (hstmt, ipar, SQL_PARAM_INPUT, fCType, fSqlType, cbParamDef, ibScale, rgbValue, SQL_MAX_OPTION_STRING_LENGTH, pcbValue);
}


SQLRETURN SQL_API
SQLCloseCursor (SQLHSTMT hstmt)
{
  STMT (pstmt, hstmt);
  DBC_t FAR *pdbc;
  HPROC hproc = SQL_NULL_HPROC;
  SQLRETURN retcode;

  ENTER_STMT (pstmt);

  pdbc = (DBC_t FAR *) (pstmt->hdbc);

  /* check state */
  if (pstmt->state >= en_stmt_needdata || pstmt->asyn_on != en_NullProc)
    {
      PUSHSQLERR (pstmt->herr, en_S1010);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }
  else if (pstmt->state < en_stmt_cursoropen)
    {
      PUSHSQLERR (pstmt->herr, en_24000);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  hproc = _iodbcdm_getproc (pstmt->hdbc, en_CloseCursor);

  if (hproc)
    {
      CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc, en_CloseCursor,
	  (pstmt->dhstmt));
    }

  if (hproc == SQL_NULL_HPROC)
    {
      hproc = _iodbcdm_getproc (pstmt->hdbc, en_FreeStmt);

      if (hproc == SQL_NULL_HPROC)
	{
	  PUSHSQLERR (pstmt->herr, en_IM001);

	  LEAVE_STMT (pstmt, SQL_ERROR);
	}

      CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc, en_FreeStmt,
	  (pstmt->dhstmt, SQL_CLOSE));
    }

  if (retcode != SQL_SUCCESS
      && retcode != SQL_SUCCESS_WITH_INFO)
    {
      LEAVE_STMT (pstmt, retcode);
    }

  /*
   *  State transition
   */
  pstmt->cursor_state = en_stmt_cursor_no;

  switch (pstmt->state)
    {
    case en_stmt_allocated:
    case en_stmt_prepared:
      break;

    case en_stmt_executed:
    case en_stmt_cursoropen:
    case en_stmt_fetched:
    case en_stmt_xfetched:
      if (pstmt->prep_state)
	pstmt->state = en_stmt_prepared;
      else
	pstmt->state = en_stmt_allocated;
      break;

    default:
      break;
    }

  LEAVE_STMT (pstmt, retcode);
}

#endif	/* ODBCVER >= 0x0300 */
