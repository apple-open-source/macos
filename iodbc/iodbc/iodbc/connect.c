/*
 *  connect.c
 *
 *  $Id: connect.c,v 1.53 2007/09/10 07:27:13 source Exp $
 *
 *  Connect (load) driver
 *
 *  The iODBC driver manager.
 *
 *  Copyright (C) 1995 by Ke Jin <kejin@empress.com>
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

#include <iodbc.h>

#include <assert.h>
#include <sql.h>
#include <sqlext.h>
#include <sqlucode.h>
#include <iodbcext.h>
#include <odbcinst.h>

#include <dlproc.h>

#include <herr.h>
#include <henv.h>
#include <hdbc.h>
#include <hstmt.h>

#include <itrace.h>

#include <unicode.h>

#if defined (__APPLE__) && !(defined (NO_FRAMEWORKS) || defined (_LP64))
#include <Carbon/Carbon.h>
#endif

#include "misc.h"
#include "iodbc_misc.h"

/*
 *  Identification strings
 */
static char sccsid[] = "@(#)iODBC driver manager " VERSION "\n";
char *iodbc_version = VERSION;


/*
 *  Prototypes
 */
extern SQLRETURN _iodbcdm_driverunload (HDBC hdbc, int ver);
extern SQLRETURN SQL_API _iodbcdm_SetConnectOption (SQLHDBC hdbc,
    SQLUSMALLINT fOption, SQLULEN vParam, SQLCHAR waMode);


#define CHECK_DRVCONN_DIALBOX(path) \
  { \
    if ((handle = DLL_OPEN(path)) != NULL) \
      { \
        if (DLL_PROC(handle, "_iodbcdm_drvconn_dialboxw") != NULL) \
          { \
            DLL_CLOSE(handle); \
            retVal = TRUE; \
            goto quit; \
          } \
        else \
          { \
            if (DLL_PROC(handle, "_iodbcdm_drvconn_dialbox") != NULL) \
              { \
                DLL_CLOSE(handle); \
                retVal = TRUE; \
                goto quit; \
              } \
          } \
        DLL_CLOSE(handle); \
      } \
  }



static BOOL
_iodbcdm_CheckDriverLoginDlg (
    LPSTR drv,
    LPSTR dsn
)
{
  char tokenstr[4096];
  char drvbuf[4096] = { L'\0'};
  HDLL handle;
  BOOL retVal = FALSE;

  /* Check if the driver is provided */
  if (drv == NULL)
    {
      SQLSetConfigMode (ODBC_BOTH_DSN);
      SQLGetPrivateProfileString ("ODBC Data Sources",
        dsn && dsn[0] != '\0' ? dsn : "Default",
        "", tokenstr, sizeof (tokenstr), NULL);
      drv = tokenstr;
    }

  /* Call the iodbcdm_drvconn_dialbox */

  SQLSetConfigMode (ODBC_USER_DSN);
  if (!access (drv, X_OK))
    { CHECK_DRVCONN_DIALBOX (drv); }
  if (SQLGetPrivateProfileString (drv, "Driver", "", drvbuf,
    sizeof (drvbuf), "odbcinst.ini"))
    { CHECK_DRVCONN_DIALBOX (drvbuf); }
  if (SQLGetPrivateProfileString (drv, "Setup", "", drvbuf,
    sizeof (drvbuf), "odbcinst.ini"))
    { CHECK_DRVCONN_DIALBOX (drvbuf); }
  if (SQLGetPrivateProfileString ("Default", "Driver", "", drvbuf,
    sizeof (drvbuf), "odbcinst.ini"))
    { CHECK_DRVCONN_DIALBOX (drvbuf); }
  if (SQLGetPrivateProfileString ("Default", "Setup", "", drvbuf,
    sizeof (drvbuf), "odbcinst.ini"))
    { CHECK_DRVCONN_DIALBOX (drvbuf); }


  SQLSetConfigMode (ODBC_SYSTEM_DSN);
  if (!access (drv, X_OK))
    { CHECK_DRVCONN_DIALBOX (drv); }
  if (SQLGetPrivateProfileString (drv, "Driver", "", drvbuf,
    sizeof (drvbuf), "odbcinst.ini"))
    { CHECK_DRVCONN_DIALBOX (drvbuf); }
  if (SQLGetPrivateProfileString (drv, "Setup", "", drvbuf,
    sizeof (drvbuf), "odbcinst.ini"))
    { CHECK_DRVCONN_DIALBOX (drvbuf); }
  if (SQLGetPrivateProfileString ("Default", "Driver", "", drvbuf,
    sizeof (drvbuf), "odbcinst.ini"))
    { CHECK_DRVCONN_DIALBOX (drvbuf); }
  if (SQLGetPrivateProfileString ("Default", "Setup", "", drvbuf,
    sizeof (drvbuf), "odbcinst.ini"))
    { CHECK_DRVCONN_DIALBOX (drvbuf); }

quit:

  return retVal;
}


#define RETURN(_ret)							\
  do {									\
    retcode = _ret;							\
    goto end;								\
  } while (0)

#if 0
#define DPRINTF(a)	fprintf a
#else
#define DPRINTF(a)
#endif


static SQLRETURN
_iodbcdm_SetConnectOption_init (
    SQLHDBC		  hdbc,
    SQLUSMALLINT	  fOption,
    SQLULEN		  vParam,
    UCHAR		  waMode)
{
  CONN (pdbc, hdbc);
  ENVR (penv, pdbc->henv);
  HPROC hproc = SQL_NULL_HPROC;
  SQLRETURN retcode = SQL_SUCCESS;
  int retinfo = 0;

  SQLINTEGER strLength = 0;
  void *ptr = (void *) vParam;
  void *_vParam = NULL;

  if (fOption >= 1000)
    {
      retinfo = 1;		/* Change SQL_ERROR -> SQL_SUCCESS_WITH_INFO */
    }

  if ((penv->unicode_driver && waMode != 'W')
      || (!penv->unicode_driver && waMode == 'W'))
    {
      switch (fOption)
        {
          case SQL_ATTR_TRACEFILE:
          case SQL_CURRENT_QUALIFIER:
          case SQL_TRANSLATE_DLL:
          case SQL_APPLICATION_NAME:
          case SQL_COPT_SS_ENLIST_IN_DTC:
          case SQL_COPT_SS_PERF_QUERY_LOG:
          case SQL_COPT_SS_PERF_DATA_LOG:
          case SQL_CURRENT_SCHEMA:
            if (waMode != 'W')
              {
              /* ansi=>unicode*/
                _vParam = dm_SQL_A2W((SQLCHAR *)vParam, SQL_NTS);
              }
            else
              {
              /* unicode=>ansi*/
                _vParam = dm_SQL_W2A((SQLWCHAR *)vParam, SQL_NTS);
              }
            ptr = _vParam;
            strLength = SQL_NTS;
            break;
        }
    }

  if (penv->unicode_driver)
    {
      /* SQL_XXX_W */
      if ((hproc = _iodbcdm_getproc (pdbc, en_SetConnectOptionW))
          != SQL_NULL_HPROC)
        {
          CALL_DRIVER (hdbc, pdbc, retcode, hproc,
	      (pdbc->dhdbc, fOption, ptr));
        }
#if (ODBCVER >= 0x300)
      else if ((hproc = _iodbcdm_getproc (pdbc, en_SetConnectAttrW))
          != SQL_NULL_HPROC)
        {
          CALL_DRIVER (hdbc, pdbc, retcode, hproc,
	      (pdbc->dhdbc, fOption, ptr, strLength));
        }
#endif
    }
  else
    {
      /* SQL_XXX */
      /* SQL_XXX_A */
      if ((hproc = _iodbcdm_getproc (pdbc, en_SetConnectOption))
           != SQL_NULL_HPROC)
        {
          CALL_DRIVER (hdbc, pdbc, retcode, hproc,
	      (pdbc->dhdbc, fOption, vParam));
        }
      else if ((hproc = _iodbcdm_getproc (pdbc, en_SetConnectOptionA))
           != SQL_NULL_HPROC)
        {
          CALL_DRIVER (hdbc, pdbc, retcode, hproc,
	      (pdbc->dhdbc, fOption, vParam));
        }
#if (ODBCVER >= 0x300)
      else if ((hproc = _iodbcdm_getproc (pdbc, en_SetConnectAttr))
          != SQL_NULL_HPROC)
        {
          CALL_DRIVER (hdbc, pdbc, retcode, hproc,
	      (pdbc->dhdbc, fOption, vParam, strLength));
        }
      else if ((hproc = _iodbcdm_getproc (pdbc, en_SetConnectAttrA))
          != SQL_NULL_HPROC)
        {
          CALL_DRIVER (hdbc, pdbc, retcode, hproc,
	      (pdbc->dhdbc, fOption, vParam, strLength));
        }
#endif
    }
  MEM_FREE(_vParam);

  if (hproc == SQL_NULL_HPROC)
    {
      PUSHSQLERR (pdbc->herr, en_IM004);
      return SQL_SUCCESS_WITH_INFO;
    }

  if (retcode != SQL_SUCCESS && retinfo)
    return SQL_SUCCESS_WITH_INFO;

  return retcode;
}


static SQLRETURN
_iodbcdm_getInfo_init (SQLHDBC hdbc,
    SQLUSMALLINT fInfoType,
    SQLPOINTER rgbInfoValue,
    SQLSMALLINT cbInfoValueMax,
    SQLSMALLINT * pcbInfoValue,
    SQLCHAR waMode)
{
  CONN (pdbc, hdbc);
  ENVR (penv, pdbc->henv);
  HPROC hproc = SQL_NULL_HPROC;
  SQLRETURN retcode = SQL_SUCCESS;

  waMode = waMode; /*UNUSED*/

  switch(fInfoType)
    {
      case SQL_CURSOR_COMMIT_BEHAVIOR:
      case SQL_CURSOR_ROLLBACK_BEHAVIOR:
        break;
      default:
        return SQL_ERROR;
    }

  CALL_UDRIVER(hdbc, pdbc, retcode, hproc, penv->unicode_driver,
    en_GetInfo, (
       pdbc->dhdbc,
       fInfoType,
       rgbInfoValue,
       cbInfoValueMax,
       pcbInfoValue));

  if (hproc == SQL_NULL_HPROC)
    {
      PUSHSQLERR (pdbc->herr, en_IM004);
      return SQL_SUCCESS_WITH_INFO;
    }

  return retcode;
}


static SQLRETURN
_iodbcdm_finish_disconnect (HDBC hdbc, BOOL driver_disconnect)
{
  CONN (pdbc, hdbc);
  HPROC hproc = SQL_NULL_HPROC;

  DPRINTF ((stderr,
      "DEBUG: _iodbcdm_finish_disconnect (conn %p, driver_disconnect %d)\n",
      hdbc, driver_disconnect));

  if (driver_disconnect)
    {
      SQLRETURN retcode;

      hproc = _iodbcdm_getproc (pdbc, en_Disconnect);
      if (hproc == SQL_NULL_HPROC)
        {
          PUSHSQLERR (pdbc->herr, en_IM001);
          return SQL_ERROR;
        }

      ODBC_UNLOCK ();
      CALL_DRIVER (hdbc, pdbc, retcode, hproc, (pdbc->dhdbc));
      ODBC_LOCK ();

      if (!SQL_SUCCEEDED (retcode))
        {
          /* diff from MS specs. We disallow
           * driver SQLDisconnect() return
           * SQL_SUCCESS_WITH_INFO and post
           * error message.
           */
          return retcode;
        }
    }

  /* free all statement handle(s) on this connection */
  while (pdbc->hstmt != NULL)
    _iodbcdm_dropstmt (pdbc->hstmt);

  /* state transition */
  pdbc->state = en_dbc_allocated;

  return SQL_SUCCESS;
}


#if (ODBCVER >= 0x300)
/*
 * Set Retry Wait timeout
 */
static void
_iodbcdm_pool_set_retry_wait (HDBC hdbc)
{
  CONN (pdbc, hdbc);
  int retry_wait = 0;
  char buf[1024];

  /* Get the "Retry Wait" keyword value from the Pooling section */
  SQLSetConfigMode (ODBC_BOTH_DSN);
  if (SQLGetPrivateProfileString ("ODBC Connection Pooling", "Retry Wait", "",
         buf, sizeof (buf), "odbcinst.ini") != 0 &&
         buf[0] != '\0')
    retry_wait = atoi(buf);

  DPRINTF ((stderr, "DEBUG: setting RetryWait %d (conn %p)\n",
      retry_wait, hdbc));
  pdbc->cp_retry_wait = time(NULL) + retry_wait;
}


extern SQLRETURN SQLFreeConnect_Internal (SQLHDBC hdbc);

/*
 * Drop connection from the pool
 */
void
_iodbcdm_pool_drop_conn (HDBC hdbc, HDBC hdbc_prev)
{
  CONN (pdbc, hdbc);
  CONN (pdbc_prev, hdbc_prev);

  assert (!pdbc->cp_in_use);

  DPRINTF ((stderr, "DEBUG: dropping connection %p (prev %p) from the pool\n",
      hdbc, hdbc_prev));

  /* remove from pool */
  if (pdbc_prev != NULL)
    pdbc_prev->next = pdbc->next;
  else
    {
      GENV (genv, pdbc->genv);

      genv->pdbc_pool = pdbc->next;
    }

  /* finish disconnect and free connection */
  _iodbcdm_finish_disconnect (hdbc, TRUE);
  SQLFreeConnect_Internal (hdbc);
  MEM_FREE (hdbc);
}


/*
 * Copy connection parameters from src to dst and reset src parameters
 * so that src can be correctly freed by SQLDisconnect.
 */
static void
_iodbcdm_pool_copy_conn (HDBC hdbc_dst, HDBC hdbc_src)
{
  CONN (pdbc_dst, hdbc_dst);
  CONN (pdbc_src, hdbc_src);
  HDBC next;
  time_t cp_timeout, cp_expiry_time;

  /* Preserve `next', `cp_timeout' and `cp_expiry_time' */
  next = pdbc_dst->next;
  cp_timeout = pdbc_dst->cp_timeout;
  cp_expiry_time = pdbc_dst->cp_expiry_time;
  *pdbc_dst = *pdbc_src;
  pdbc_dst->next = next;
  pdbc_dst->cp_timeout = cp_timeout;
  pdbc_dst->cp_expiry_time = cp_expiry_time;

  /* Reset parameters of source connection */
  pdbc_src->herr = SQL_NULL_HERR;
  pdbc_src->dhdbc = SQL_NULL_HDBC;
  pdbc_src->henv = SQL_NULL_HENV;
  pdbc_src->hstmt = SQL_NULL_HSTMT;
  pdbc_src->hdesc = SQL_NULL_HDESC;
  pdbc_src->current_qualifier = NULL;
  pdbc_src->drvopt = NULL;

  pdbc_src->cp_probe = NULL;
  pdbc_src->cp_dsn = NULL;
  pdbc_src->cp_uid = NULL;
  pdbc_src->cp_pwd = NULL;
  pdbc_src->cp_connstr = NULL;
}


/*
 * Check if attributes of two connections match
 */
static BOOL
_iodbcdm_pool_check_attr_match (HDBC hdbc, HDBC cp_hdbc)
{
  CONN (pdbc, hdbc);
  GENV (genv, pdbc->genv);
  CONN (cp_pdbc, cp_hdbc);
  BOOL strict_match = (genv->cp_match == SQL_CP_STRICT_MATCH);

  DPRINTF ((stderr, "DEBUG: check attr match (conn %p, cp_conn %p)\n",
      hdbc, cp_hdbc));

  /*
   * Check attrs that must be set before connection has been made:
   * - SQL_ATTR_PACKET_SIZE (packet_size)
   * - SQL_ATTR_ODBC_CURSORS (odbc_cursors)
   *
   * SQL_ATTR_PACKET_SIZE can be different if !strict_match.
   * The value of SQL_ATTR_LOGIN_TIMEOUT is not examined.
   */
  if (strict_match && pdbc->packet_size != cp_pdbc->packet_size)
    {
      DPRINTF ((stderr, "DEBUG: packet_size does not match (conn %p, cp_conn %p, strict_match %d)",
        hdbc, cp_hdbc, strict_match));
      return FALSE;
    }

  if (pdbc->odbc_cursors != cp_pdbc->odbc_cursors)
    {
      DPRINTF ((stderr, "DEBUG: odbc_cursors does not match (conn %p, cp_conn %p, strict_match %d)",
        hdbc, cp_hdbc, strict_match));
      return FALSE;
    }

  /*
   * Check attrs that must be set either before or after the connection
   * has been made:
   * - SQL_ATTR_ACCESS_MODE (access_mode, default SQL_MODE_DEFAULT)
   * - SQL_ATTR_AUTOCOMMIT (autocommit, default SQL_AUTOCOMMIT_DEFAULT)
   * - SQL_ATTR_CURRENT_CATALOG (current_qualifier)
   * - SQL_ATTR_QUIET_MODE (quiet_mode)
   * - SQL_ATTR_TXN_ISOLATION (txn_isolation, default SQL_TXN_READ_UNCOMMITTED)
   *
   * If an attr is not set by the application but set in the pool:
   * - if there is a default, an attr is reset to the default value
   * (see _iodbcdm_pool_reset_conn_attrs()).
   * - if there is no default value, pooled connection is not considered
   * a match
   *
   * If an attr is set by the application, this value overrides the
   * value from the pool.
   */
  if (pdbc->current_qualifier == NULL && cp_pdbc->current_qualifier != NULL)
    {
      /* has not been set by application, but set in the pool */
      DPRINTF ((stderr, "DEBUG: current_qualifier has not been set by application, but is set in the pool (conn %p, cp_conn %p)",
        hdbc, cp_hdbc));
      return FALSE;
    }

  if (pdbc->quiet_mode == 0 && cp_pdbc->quiet_mode != 0)
    {
      /* has not been set by application, but set in the pool */
      DPRINTF ((stderr, "DEBUG: quiet_mode has not been set by application, but is set in the pool (conn %p, cp_conn %p)",
        hdbc, cp_hdbc));
      return FALSE;
    }

  return TRUE;
}


/*
 * Reset connection attributes to the default values (if an attr is not set
 * by application and there is a default) or to the value set by application.
 */
SQLRETURN
_iodbcdm_pool_reset_conn_attrs (SQLHDBC hdbc, SQLHDBC cp_hdbc)
{
  CONN (pdbc, hdbc);
  CONN (cp_pdbc, cp_hdbc);
  SQLRETURN retcode = SQL_SUCCESS;
  SQLRETURN ret;

  if (pdbc->access_mode != cp_pdbc->access_mode)
    {
      cp_pdbc->access_mode = pdbc->access_mode;

      ret = _iodbcdm_SetConnectOption_init (
          cp_pdbc, SQL_ACCESS_MODE, cp_pdbc->access_mode, 'A');
      retcode |= ret;
    }

  if (pdbc->autocommit != cp_pdbc->autocommit)
    {
      cp_pdbc->autocommit = pdbc->autocommit;

      ret = _iodbcdm_SetConnectOption_init (
	  cp_pdbc, SQL_AUTOCOMMIT, cp_pdbc->autocommit, 'A');
      retcode |= ret;
    }

  if (pdbc->current_qualifier != NULL)
    {
      if (cp_pdbc->current_qualifier != NULL)
        MEM_FREE (cp_pdbc->current_qualifier);
      cp_pdbc->current_qualifier = pdbc->current_qualifier;
      pdbc->current_qualifier = NULL;
      cp_pdbc->current_qualifier_WA = pdbc->current_qualifier_WA;

      ret = _iodbcdm_SetConnectOption_init (
          cp_pdbc, SQL_CURRENT_QUALIFIER,
	  (SQLULEN) cp_pdbc->current_qualifier, cp_pdbc->current_qualifier_WA);
      retcode |= ret;
    }

  if (cp_pdbc->quiet_mode != pdbc->quiet_mode)
    {
      cp_pdbc->quiet_mode = pdbc->quiet_mode;

      ret = _iodbcdm_SetConnectOption_init (
	  cp_pdbc, SQL_QUIET_MODE, cp_pdbc->quiet_mode, 'A');
      retcode |= ret;
    }

  if (pdbc->txn_isolation != cp_pdbc->txn_isolation)
    {
      cp_pdbc->txn_isolation = pdbc->txn_isolation;

      ret = _iodbcdm_SetConnectOption_init (
          cp_pdbc, SQL_TXN_ISOLATION, cp_pdbc->txn_isolation, 'A');
      retcode |= ret;
    }

  return retcode;
}


extern SQLRETURN
SQLAllocStmt_Internal (SQLHDBC hdbc, SQLHSTMT *phstmt);
extern SQLRETURN
SQLFreeStmt_Internal (SQLHSTMT hstmt, SQLUSMALLINT fOption);
extern SQLRETURN SQL_API
SQLExecDirect_Internal (SQLHSTMT hstmt,
    SQLPOINTER szSqlStr, SQLINTEGER cbSqlStr, SQLCHAR waMode);
extern SQLRETURN SQLFetch_Internal (SQLHSTMT hstmt);

/*
 * Execute CPProbe statement to check if connection is dead
 */
static SQLRETURN
_iodbcdm_pool_exec_cpprobe (HDBC hdbc, char *cp_probe)
{
  HSTMT hstmt = SQL_NULL_HSTMT;
  SQLRETURN retcode;
  SQLSMALLINT num_cols;

  DPRINTF ((stderr, "DEBUG: executing CPProbe (conn %p, stmt [%s])\n",
      hdbc, cp_probe));

  /* allocate statement handle */
  retcode = SQLAllocStmt_Internal (hdbc, &hstmt);
  if (!SQL_SUCCEEDED (retcode))
    RETURN (retcode);

  /* execute statement */
  retcode = SQLExecDirect_Internal (hstmt, cp_probe, SQL_NTS, 'A');
  if (!SQL_SUCCEEDED (retcode))
    RETURN (retcode);

  /* check that there is a result set */
  retcode = _iodbcdm_NumResultCols (hstmt, &num_cols);
  if (!SQL_SUCCEEDED (retcode))
    RETURN (retcode);

  /* if there was no result set -- success */
  if (num_cols == 0)
    RETURN (SQL_SUCCESS);

  /* fetch results */
  do
    {
      retcode = SQLFetch_Internal (hstmt);
      if (!SQL_SUCCEEDED (retcode))
        RETURN (retcode);
    }
  while (retcode != SQL_NO_DATA);

  /* success */
  RETURN (SQL_SUCCESS);

end:
  if (hstmt != SQL_NULL_HSTMT)
    SQLFreeStmt_Internal (hstmt, SQL_DROP);
  return retcode;
}


/*
 * Check if connection is dead
 */
static BOOL
_iodbcdm_pool_conn_dead (HDBC hdbc)
{
  CONN (pdbc, hdbc);
  HPROC hproc = SQL_NULL_HPROC;
  SQLRETURN retcode;
  SQLINTEGER attr_dead;

  DPRINTF ((stderr, "DEBUG: checking if connection is dead (conn %p)\n",
      hdbc));

  /* first try SQLGetConnectAttr */
  CALL_UDRIVER(pdbc, pdbc, retcode, hproc, 'A', en_GetConnectAttr,
      (pdbc->dhdbc, SQL_ATTR_CONNECTION_DEAD, &attr_dead, 0, NULL));
  if (hproc != SQL_NULL_HPROC && SQL_SUCCEEDED (retcode))
    {
      DPRINTF ((stderr, "DEBUG: GetConnectAttr: attr_dead = %ld (conn %p)\n",
          attr_dead, hdbc));
      return attr_dead == SQL_CD_TRUE;
    }

  /* try SQLGetConnectOption */
  CALL_UDRIVER(pdbc, pdbc, retcode, hproc, 'A', en_GetConnectOption,
      (pdbc->dhdbc, SQL_ATTR_CONNECTION_DEAD, &attr_dead));
  if (hproc != SQL_NULL_HPROC && SQL_SUCCEEDED (retcode))
    {
      DPRINTF ((stderr, "DEBUG: GetConnectOption: attr_dead = %ld (conn %p)\n",
          attr_dead, hdbc));
      return attr_dead == SQL_CD_TRUE;
    }

  /* try CPProbe statement */
  if (pdbc->cp_probe != NULL && STRLEN(pdbc->cp_probe) > 0)
    {
      retcode = _iodbcdm_pool_exec_cpprobe (pdbc, pdbc->cp_probe);
      return SQL_SUCCEEDED (retcode);
    }

  /* don't know, assume it is alive */
  return FALSE;
}


/*
 * Get the connection from to the pool
 *
 * Returns 0 if the connection was put successfully
 * Returns -1 otherwise
 */
static SQLRETURN
_iodbcdm_pool_get_conn (
    HDBC hdbc, char *dsn, char *uid, char *pwd, char *connstr)
{
  CONN (pdbc, hdbc);
  GENV (genv, pdbc->genv);
  DBC_t *cp_pdbc, *cp_pdbc_next, *cp_pdbc_prev = NULL;
  time_t current_time;

  DPRINTF ((stderr, "DEBUG: getting connection %p from the pool (dsn [%s], uid [%s], pwd [%s], connstr [%s])\n",
      hdbc, dsn, uid, pwd, connstr));

  current_time = time(NULL);

  for (cp_pdbc = genv->pdbc_pool; cp_pdbc != NULL;
       cp_pdbc_prev = cp_pdbc, cp_pdbc = cp_pdbc_next)
    {
      SQLRETURN retcode;

      cp_pdbc_next = cp_pdbc->next;

      /* skip connections in use */
      if (cp_pdbc->cp_in_use)
        {
	  DPRINTF ((stderr, "DEBUG: skipping connection %p (in use)\n",
            cp_pdbc));
	  continue;
	}

      /*
       * Check that pooled connection timeout has not expired
       */
      if (current_time >= cp_pdbc->cp_expiry_time)
        {
	  DPRINTF ((stderr, "DEBUG: connection %p expired (cp_expiry_time %d, current_time %d)\n",
            cp_pdbc, cp_pdbc->cp_expiry_time, current_time));
	  _iodbcdm_pool_drop_conn (cp_pdbc, cp_pdbc_prev);
	  continue;
        }

      /*
       * Check that requested dsn, uid, pwd and connstr match
       * pooled connection
       */
      if (dsn != NULL)
        {
          if (cp_pdbc->cp_dsn == NULL ||
              strcmp (dsn, cp_pdbc->cp_dsn) != 0)
            continue;
	}
      else if (cp_pdbc->cp_dsn != NULL)
        continue;
      if (uid != NULL)
        {
          if (cp_pdbc->cp_uid == NULL ||
              strcmp (uid, cp_pdbc->cp_uid) != 0)
            continue;
        }
      else if (cp_pdbc->cp_uid != NULL)
        continue;
      if (pwd != NULL)
        {
          if (cp_pdbc->cp_pwd == NULL ||
              strcmp (pwd, cp_pdbc->cp_pwd) != 0)
            continue;
        }
      else if (cp_pdbc->cp_pwd != NULL)
        continue;
      if (connstr != NULL)
        {
          if (cp_pdbc->cp_connstr == NULL ||
              strcmp (connstr, cp_pdbc->cp_connstr) != 0)
            continue;
        }
      else if (cp_pdbc->cp_connstr != NULL)
        continue;

      DPRINTF ((stderr, "DEBUG: found matching pooled connection %p\n",
          cp_pdbc));

      /* check that connection attributes match */
      if (!_iodbcdm_pool_check_attr_match (pdbc, cp_pdbc))
	continue;

      /*
       * Match found!
       */

      /*
       * Check Retry Wait timeout
       */
      if (cp_pdbc->cp_retry_wait != 0)
	{
	  if (current_time < cp_pdbc->cp_retry_wait)
	    {
	      /* Retry Wait timeout has not expired yet */
              DPRINTF ((stderr,
		  "DEBUG: RetryWait timeout has not expired yet (cp_pdbc %p, cp_retry_wait %d, current_time %d)\n",
                  cp_pdbc, cp_pdbc->cp_retry_wait, current_time));

              /* remember matching pooled connection */
	      pdbc->cp_pdbc = cp_pdbc;

	      return SQL_ERROR;
	    }

          DPRINTF ((stderr, "DEBUG: RetryWait timeout reset (cp_pdbc %p)\n",
             cp_pdbc));
          /* reset Retry Wait timeout */
          cp_pdbc->cp_retry_wait = 0;
        }

      /*
       * Check if connection is dead
       */
      if (_iodbcdm_pool_conn_dead (cp_pdbc))
        {
	  /* Connection is dead -- try to reconnect */
          DPRINTF ((stderr, "DEBUG: pooled connection is dead (cp_pdbc %p)\n",
             cp_pdbc));

          /* remember matching pooled connection */
	  pdbc->cp_pdbc = cp_pdbc;
          cp_pdbc->cp_in_use = TRUE;

	  return SQL_ERROR;
        }

      /* reset connection attrs */
      retcode = _iodbcdm_pool_reset_conn_attrs (pdbc, cp_pdbc);
      if (retcode != SQL_SUCCESS)
        retcode = SQL_SUCCESS_WITH_INFO;

      /* copy parameters */
      _iodbcdm_pool_copy_conn (pdbc, cp_pdbc);

      /* remember matching pooled connection */
      pdbc->cp_pdbc = cp_pdbc;
      cp_pdbc->cp_in_use = TRUE;

      DPRINTF ((stderr, "DEBUG: got connection from the pool (cp_pdbc %p)\n",
          cp_pdbc));
      /* found a connection in a pool */
      return retcode;
    }

  DPRINTF ((stderr, "DEBUG: no matching connection in the pool\n"));
  /* can't find a connection in a pool */
  return SQL_ERROR;
}


/*
 * Put the conneciton back to the pool
 *
 * Return 0 if the connection was put successfully
 * Return -1 otherwise
 */
static int
_iodbcdm_pool_put_conn (HDBC hdbc)
{
  CONN (pdbc, hdbc);
  GENV (genv, NULL);
  DBC_t *cp_pdbc = pdbc->cp_pdbc;

  DPRINTF ((stderr, "DEBUG: putting connection back to the pool (conn %p, dsn [%s], uid [%s], pwd [%s], connstr [%s])\n",
      hdbc, pdbc->cp_dsn, pdbc->cp_uid, pdbc->cp_pwd, pdbc->cp_connstr));

  if (cp_pdbc == NULL)
    {
      cp_pdbc = (DBC_t *) MEM_ALLOC (sizeof (DBC_t));
      if (cp_pdbc == NULL)
        {
	  return -1;
	}

      /* put to the pool */
      genv = (GENV_t *) pdbc->genv;
      cp_pdbc->next = genv->pdbc_pool;
      genv->pdbc_pool = cp_pdbc;

      cp_pdbc->cp_timeout = pdbc->cp_timeout;
      DPRINTF ((stderr, "DEBUG: new pooled connection %p\n", cp_pdbc));
    }

  /* copy out parameters */
  _iodbcdm_pool_copy_conn(cp_pdbc, pdbc);
  pdbc->cp_pdbc = NULL;

  /* free all statement handle(s) on connection in pool */
  while (cp_pdbc->hstmt != NULL)
    _iodbcdm_dropstmt (cp_pdbc->hstmt);

  /* set expiration time and other parameters for connection in pool */
  cp_pdbc->cp_pdbc = NULL;
  if (cp_pdbc->cp_retry_wait == 0)
    {
      /* set new expiry time only if we are not returning the connection
	 to the pool after unsuccessfull reconnect attempt */
      cp_pdbc->cp_expiry_time = time(NULL) + cp_pdbc->cp_timeout;
    }
  cp_pdbc->cp_in_use = FALSE;

  DPRINTF ((stderr, "DEBUG: connection %p put back to the pool (cp_pdbc %p, cp_timeout %d)\n",
      hdbc, cp_pdbc, cp_pdbc->cp_timeout));
  return 0;
}
#endif /* (ODBCVER >= 0x300) */


/* - Load driver share library( or increase its reference count
 *   if it has already been loaded by another active connection)
 * - Call driver's SQLAllocEnv() (for the first reference only)
 * - Call driver's SQLAllocConnect()
 * - Call driver's SQLSetConnectOption() (set login time out)
 * - Increase the bookkeeping reference count
 */
SQLRETURN
_iodbcdm_driverload (
    char * dsn,
    char * drv,
    HDBC hdbc,
    SWORD thread_safe,
    SWORD unload_safe,
    UCHAR waMode)
{
  CONN (pdbc, hdbc);
  ENVR (penv, NULL);
  GENV (genv, NULL);
  HDLL hdll = SQL_NULL_HDLL;
  HPROC hproc;
  SQLRETURN retcode = SQL_SUCCESS;
  sqlstcode_t sqlstat = en_00000;
  char buf[1024];
  char path_tmp[1024];
  char *path = drv;
  char cp_probe[1024] = {""};
  int cp_timeout = 0;

  if (drv == NULL || ((char*)drv)[0] == '\0')
    {
      PUSHSQLERR (pdbc->herr, en_IM002);
      return SQL_ERROR;
    }

  if (!IS_VALID_HDBC (pdbc) || pdbc->genv == SQL_NULL_HENV)
    {
      return SQL_INVALID_HANDLE;
    }

  /*
   *  If drv does not start with / or ., we may have a symbolic driver name
   */
  if (!(drv[0] == '/' || drv[0] == '.'))
    {
      char *tmp_drv = NULL;

      /*
       *  Remove curly braces
       */
      if (drv[0] == '{')
	{
	  tmp_drv = strdup (drv);
	  if (tmp_drv[strlen (drv) - 1] == '}')
	    tmp_drv[strlen (drv) - 1] = '\0';
	  drv = &tmp_drv[1];
	}

      /*
       *  Hopefully the driver was registered under that name in the 
       *  odbcinst.ini file
       */
      if (SQLGetPrivateProfileString ((char *) drv, "Driver", "",
	      path_tmp, sizeof (path_tmp), "odbcinst.ini") && path_tmp[0])
	path = path_tmp;

      if (tmp_drv)
	free (tmp_drv);

      /*
       *  Get CPTimeout value
       */
      SQLSetConfigMode (ODBC_BOTH_DSN);
      if (SQLGetPrivateProfileString (drv, "CPTimeout", "",
	    buf, sizeof(buf), "odbcinst.ini") && buf[0])
        cp_timeout = atoi(buf);

      /*
       *  Get CPProbe value
       */
      SQLGetPrivateProfileString (drv, "CPProbe", "",
   	    cp_probe, sizeof(cp_probe), "odbcinst.ini");
    }
  else if (dsn != NULL && *dsn != '\0')
    {
      char tmp_drv[1024] = {""};

      SQLSetConfigMode (ODBC_BOTH_DSN);
      if (SQLGetPrivateProfileString ("ODBC Data Sources", dsn, "",
	    tmp_drv, sizeof(tmp_drv), NULL) && tmp_drv[0])
	{
          /*
           *  Get CPTimeout value
           */
          if (SQLGetPrivateProfileString (tmp_drv, "CPTimeout", "",
	        buf, sizeof(buf), "odbcinst.ini") && buf[0])
            cp_timeout = atoi(buf);

          /*
           *  Get CPProbe value
           */
          SQLGetPrivateProfileString (tmp_drv, "CPProbe", "",
  	      cp_probe, sizeof(cp_probe), "odbcinst.ini");
  	}
    }

  genv = (GENV_t *) pdbc->genv;

  /* This will either load the driver dll or increase its reference count */
  hdll = _iodbcdm_dllopen ((char *) path);
  
  /* Set flag if it is safe to unload the driver after use */
  if (unload_safe)
    _iodbcdm_safe_unload (hdll);

  if (hdll == SQL_NULL_HDLL)
    {
      PUSHSYSERR (pdbc->herr, _iodbcdm_dllerror ());
      PUSHSQLERR (pdbc->herr, en_IM003);
      return SQL_ERROR;
    }

  penv = (ENV_t *) (pdbc->henv);

  if (penv != NULL)
    {
      if (penv->hdll != hdll)
	{
	  _iodbcdm_driverunload (hdbc, 3);
	  penv->hdll = hdll;
	}
      else
	{
	  /*
	   * this will not unload the driver but only decrease its internal
	   * reference count
	   */
	  _iodbcdm_dllclose (hdll);
	}
    }

  if (penv == NULL)
    {
      /*
       * find out whether this dll has already been loaded on another
       * connection
       */
      for (penv = (ENV_t *) genv->henv;
	  penv != NULL;
	  penv = (ENV_t *) penv->next)
	{
	  if (penv->hdll == hdll)
	    {
	      /*
	       * this will not unload the driver but only decrease its internal
	       * reference count
	       */
	      _iodbcdm_dllclose (hdll);
	      break;
	    }
	}

      if (penv == NULL)
	/* no connection attaching with this dll */
	{
	  int i;

	  /* create a new dll env instance */
	  penv = (ENV_t *) MEM_ALLOC (sizeof (ENV_t));

	  if (penv == NULL)
	    {
	      _iodbcdm_dllclose (hdll);

	      PUSHSQLERR (pdbc->herr, en_S1001);

	      return SQL_ERROR;
	    }

	  /*
	   *  Initialize array of ODBC functions
	   */
	  for (i = 0; i < __LAST_API_FUNCTION__; i++)
	    {
#if 1 
	      (penv->dllproc_tab)[i] = SQL_NULL_HPROC;
#else
	      (penv->dllproc_tab)[i] = _iodbcdm_getproc(pdbc, i);
#endif
	    }

	  pdbc->henv = penv;
	  penv->hdll = hdll;

          /*
           *  If the driver appears not to be thread safe, use a
           *  driver mutex to serialize all calls to this driver
           */
          penv->thread_safe = thread_safe;
          if (!penv->thread_safe)
            MUTEX_INIT (penv->drv_lock);

          penv->unicode_driver = 0;
          /*
           *  If the driver is Unicode
           */
	  if ( _iodbcdm_getproc (pdbc, en_ConnectW))
            penv->unicode_driver = 1;

	  /* call driver's SQLAllocHandle() or SQLAllocEnv() */

#if (ODBCVER >= 0x0300)
	  hproc = _iodbcdm_getproc (pdbc, en_AllocHandle);

	  if (hproc)
	    {
	      CALL_DRIVER (hdbc, genv, retcode, hproc,
		  (SQL_HANDLE_ENV, SQL_NULL_HANDLE, &(penv->dhenv)));
	      if (SQL_SUCCEEDED (retcode))
		{		
		  /* 
		   * This appears to be an ODBC3 driver
		   *
		   * Try to set the app's requested version
		   */
		  SQLRETURN save_retcode = retcode;

		  penv->dodbc_ver = SQL_OV_ODBC2;
		  hproc = _iodbcdm_getproc (pdbc, en_SetEnvAttr);
		  if (hproc != SQL_NULL_HPROC)
		    {
		      CALL_DRIVER (hdbc, genv, retcode, hproc,
			  (penv->dhenv, SQL_ATTR_ODBC_VERSION, genv->odbc_ver, 
			  0));
		      if (retcode == SQL_SUCCESS)
			penv->dodbc_ver = SQL_OV_ODBC3;
		    }
		  retcode = save_retcode;
		}
	    }
	  else			/* try driver's SQLAllocEnv() */
#endif
	    {
	      hproc = _iodbcdm_getproc (pdbc, en_AllocEnv);

	      if (hproc == SQL_NULL_HPROC)
		{
		  sqlstat = en_IM004;
		}
	      else
		{
#if (ODBCVER >= 0x0300)
		  penv->dodbc_ver = SQL_OV_ODBC2;
#endif
		  CALL_DRIVER (hdbc, genv, retcode, hproc, (&(penv->dhenv)));
		}
	    }

	  if (retcode == SQL_ERROR)
	    {
	      sqlstat = en_IM004;
	    }

	  if (sqlstat != en_00000)
	    {
	      _iodbcdm_dllclose (hdll);
	      MEM_FREE (penv);
	      PUSHSQLERR (pdbc->herr, en_IM004);

	      return SQL_ERROR;
	    }

	  /* insert into dll env list */
	  penv->next = (ENV_t *) genv->henv;
	  genv->henv = penv;

	  /* initiate this new env entry */
	  penv->refcount = 0;	/* we will increase it after
				 * driver's SQLAllocConnect()
				 * success
				 */
	}

      pdbc->henv = penv;

      if (pdbc->dhdbc == SQL_NULL_HDBC)
	{

#if (ODBCVER >= 0x0300)
	  hproc = _iodbcdm_getproc (pdbc, en_AllocHandle);

	  if (hproc)
	    {
	      CALL_DRIVER (hdbc, genv, retcode, hproc,
		  (SQL_HANDLE_DBC, penv->dhenv, &(pdbc->dhdbc)));
	    }
	  else
#endif

	    {
	      hproc = _iodbcdm_getproc (pdbc, en_AllocConnect);

	      if (hproc == SQL_NULL_HPROC)
		{
		  sqlstat = en_IM005;
		}
	      else
		{
		  CALL_DRIVER (hdbc, genv, retcode, hproc,
		      (penv->dhenv, &(pdbc->dhdbc)));
		}
	    }

	  if (retcode == SQL_ERROR)
	    {
	      sqlstat = en_IM005;
	    }

	  if (sqlstat != en_00000)
	    {
	      _iodbcdm_driverunload (hdbc, 3);

	      pdbc->dhdbc = SQL_NULL_HDBC;
	      PUSHSQLERR (pdbc->herr, en_IM005);

	      return SQL_ERROR;
	    }
	}

      pdbc->henv = penv;
      penv->refcount++;		/* bookkeeping reference count on this driver */
    }

  /* driver's login timeout option must been set before
   * its SQLConnect() call */
  if (pdbc->login_timeout != 0UL)
    {
      retcode = _iodbcdm_SetConnectOption_init (hdbc, SQL_LOGIN_TIMEOUT,
	pdbc->login_timeout, waMode);

      if (retcode == SQL_ERROR)
        {
          PUSHSQLERR (pdbc->herr, en_IM006);
          return SQL_SUCCESS_WITH_INFO;
        }
    }

  /*
   *  Now set the driver specific options we saved earlier
   */
  if (pdbc->drvopt != NULL)
    {
      DRVOPT *popt;

      for (popt = pdbc->drvopt; popt != NULL; popt = popt->next)
        {
          retcode = _iodbcdm_SetConnectOption_init (hdbc, popt->Option,
	    popt->Param, popt->waMode);

          if (retcode == SQL_ERROR)
            {
	      PUSHSQLERR (pdbc->herr, en_IM006);
	      return SQL_SUCCESS_WITH_INFO;
	    }
        }
    }

  pdbc->cp_timeout = cp_timeout;
  pdbc->cp_probe = strdup (cp_probe);

  return SQL_SUCCESS;
}


/* - Call driver's SQLFreeConnect()
 * - Call driver's SQLFreeEnv() ( for the last reference only)
 * - Unload the share library( or decrease its reference
 *   count if it is not the last reference )
 * - decrease bookkeeping reference count
 * - state transition to allocated
 */
SQLRETURN
_iodbcdm_driverunload (HDBC hdbc, int ver)
{
  CONN (pdbc, hdbc);
  ENVR (penv, pdbc->henv);
  GENV (genv, pdbc->genv);
  ENV_t *tpenv;
  HPROC hproc2, hproc3;
  SQLRETURN retcode = SQL_SUCCESS;

  if (!IS_VALID_HDBC (pdbc))
    {
      return SQL_INVALID_HANDLE;
    }

  if (penv == NULL || penv->hdll == SQL_NULL_HDLL ||
      pdbc->dhdbc == SQL_NULL_HDBC)
    {
      return SQL_SUCCESS;
    }

  /*
   *  When calling from an ODBC 2.x application, we favor the ODBC 2.x call 
   *  in the driver if the driver implements both
   */
  hproc2 = _iodbcdm_getproc (pdbc, en_FreeConnect);
#if (ODBCVER >= 0x0300)
  hproc3 = _iodbcdm_getproc (pdbc, en_FreeHandle);

  if (ver == 3 && hproc2 != SQL_NULL_HPROC && hproc3 != SQL_NULL_HPROC)
    hproc2 = SQL_NULL_HPROC;
#else
  hproc3 = SQL_NULL_HPROC;
#endif

  if (hproc2 != SQL_NULL_HPROC)
    {
      CALL_DRIVER (hdbc, pdbc, retcode, hproc2, (pdbc->dhdbc));

      pdbc->dhdbc = SQL_NULL_HDBC;
    }
#if (ODBCVER >= 0x0300)
  else if (hproc3 != SQL_NULL_HPROC)
    {
      CALL_DRIVER (hdbc, pdbc, retcode, hproc3,
	  (SQL_HANDLE_DBC, pdbc->dhdbc));
    }
#endif

  penv->refcount--;

  if (!penv->refcount)
    /* no other connections still attaching with this driver */
    {
      /*
       *  When calling from an ODBC 2.x application, we favor the ODBC 2.x call 
       *  in the driver if the driver implements both
       */
      hproc2 = _iodbcdm_getproc (pdbc, en_FreeEnv);
#if (ODBCVER >= 0x0300)
      hproc3 = _iodbcdm_getproc (pdbc, en_FreeHandle);

      if (ver == 3 && hproc2 != SQL_NULL_HPROC && hproc3 != SQL_NULL_HPROC)
	hproc2 = SQL_NULL_HPROC;
#else
      hproc3 = SQL_NULL_HPROC;
#endif

      if (hproc2 != SQL_NULL_HPROC)
	{
	  CALL_DRIVER (hdbc, genv, retcode, hproc2, (penv->dhenv));

	  penv->dhenv = SQL_NULL_HENV;
	}
#if (ODBCVER >= 0x0300)
      else if (hproc3 != SQL_NULL_HPROC)
	{
	  CALL_DRIVER (hdbc, genv, retcode, hproc3,
	      (SQL_HANDLE_ENV, penv->dhenv));
	}
#endif

      _iodbcdm_dllclose (penv->hdll);

      penv->hdll = SQL_NULL_HDLL;

      for (tpenv = (ENV_t *) genv->henv;
	  tpenv != NULL; tpenv = (ENV_t *) penv->next)
	{
	  if (tpenv == penv)
	    {
	      genv->henv = penv->next;
	      break;
	    }

	  if (tpenv->next == penv)
	    {
	      tpenv->next = penv->next;
	      break;
	    }
	}

      MEM_FREE (penv);
    }

  /* pdbc->henv = SQL_NULL_HENV; */
  pdbc->hstmt = SQL_NULL_HSTMT;
  /* pdbc->herr = SQL_NULL_HERR;
     -- delay to DM's SQLFreeConnect() */
  pdbc->dhdbc = SQL_NULL_HDBC;
  pdbc->state = en_dbc_allocated;

  /* set connect options to default values */
	/**********
	pdbc->access_mode	= SQL_MODE_DEFAULT;
	pdbc->autocommit	= SQL_AUTOCOMMIT_DEFAULT;
	pdbc->login_timeout 	= 0UL;
	**********/
  pdbc->odbc_cursors = SQL_CUR_DEFAULT;
  pdbc->packet_size = 0UL;
  pdbc->quiet_mode = (UDWORD) NULL;
  pdbc->txn_isolation = SQL_TXN_READ_UNCOMMITTED;

#if (ODBCVER >= 0x0300)
  if (pdbc->cp_probe != NULL)
    {
      MEM_FREE (pdbc->cp_probe);
      pdbc->cp_probe = NULL;
    }
  if (pdbc->cp_dsn != NULL)
    {
      MEM_FREE (pdbc->cp_dsn);
      pdbc->cp_dsn = NULL;
    }
  if (pdbc->cp_uid != NULL)
    {
      MEM_FREE (pdbc->cp_uid);
      pdbc->cp_uid = NULL;
    }
  if (pdbc->cp_pwd != NULL)
    {
      MEM_FREE (pdbc->cp_pwd);
      pdbc->cp_pwd = NULL;
    }
  if (pdbc->cp_connstr != NULL)
    {
      MEM_FREE (pdbc->cp_connstr);
      pdbc->cp_connstr = NULL;
    }
#endif

  if (pdbc->current_qualifier != NULL)
    {
      MEM_FREE (pdbc->current_qualifier);
      pdbc->current_qualifier = NULL;
    }

  return SQL_SUCCESS;
}


static SQLRETURN
_iodbcdm_dbcdelayset (HDBC hdbc, UCHAR waMode)
{
  CONN (pdbc, hdbc);
  SQLRETURN retcode = SQL_SUCCESS;
  SQLRETURN ret;

  if (pdbc->access_mode != SQL_MODE_DEFAULT)
    {
      ret = _iodbcdm_SetConnectOption_init (hdbc, SQL_ACCESS_MODE,
	      pdbc->access_mode, waMode);

      retcode |= ret;
    }

  if (pdbc->autocommit != SQL_AUTOCOMMIT_DEFAULT)
    {
      ret = _iodbcdm_SetConnectOption_init (hdbc, SQL_AUTOCOMMIT,
	      pdbc->autocommit, waMode);

      retcode |= ret;
    }

  if (pdbc->current_qualifier != NULL)
    {
      ret = _iodbcdm_SetConnectOption_init (hdbc, SQL_CURRENT_QUALIFIER,
	      (SQLULEN) pdbc->current_qualifier,
	      pdbc->current_qualifier_WA);

      retcode |= ret;
    }

  if (pdbc->packet_size != 0UL)
    {
      ret = _iodbcdm_SetConnectOption_init (hdbc, SQL_PACKET_SIZE,
	      pdbc->packet_size, waMode);

      retcode |= ret;
    }

  if (pdbc->quiet_mode != (UDWORD) NULL)
    {
      ret = _iodbcdm_SetConnectOption_init (hdbc, SQL_QUIET_MODE,
	      pdbc->quiet_mode, waMode);

      retcode |= ret;
    }

  if (pdbc->txn_isolation != SQL_TXN_READ_UNCOMMITTED)
    {
      ret = _iodbcdm_SetConnectOption_init (hdbc, SQL_TXN_ISOLATION,
	      pdbc->txn_isolation, waMode);

      retcode |= ret;
    }

  /* check error code for driver's SQLSetConnectOption() call */
  if (!SQL_SUCCEEDED (retcode))
    {
      PUSHSQLERR (pdbc->herr, en_IM006);
      retcode = SQL_ERROR;
    }


  /* get cursor behavior on transaction commit or rollback */
  ret = _iodbcdm_getInfo_init (hdbc, SQL_CURSOR_COMMIT_BEHAVIOR,
	    (PTR) & (pdbc->cb_commit),
	    sizeof (pdbc->cb_commit),
	    NULL, waMode);
  retcode |= ret;

  ret = _iodbcdm_getInfo_init (hdbc, SQL_CURSOR_ROLLBACK_BEHAVIOR,
	    (PTR) & (pdbc->cb_rollback),
	    sizeof (pdbc->cb_rollback),
	    NULL, waMode);
  retcode |= ret;

  if (retcode != SQL_SUCCESS  && retcode != SQL_SUCCESS_WITH_INFO)
    {
      return SQL_ERROR;
    }

  return retcode;
}


static SQLRETURN
_iodbcdm_con_settracing (HDBC hdbc, SQLCHAR *dsn, int dsnlen, UCHAR waMode)
{
  SQLUINTEGER trace = SQL_OPT_TRACE_OFF;
  char buf[1024];

  /* Unused */
  hdbc=hdbc;
  dsnlen=dsnlen;
  waMode = waMode;

  /* Get the TraceFile keyword value from the ODBC section */
  SQLSetConfigMode (ODBC_BOTH_DSN);
  if ((SQLGetPrivateProfileString ((char *) dsn, "TraceFile", "", 
	buf, sizeof (buf), "odbc.ini") == 0 || !buf[0]))
    STRCPY (buf, SQL_OPT_TRACE_FILE_DEFAULT);

  trace_set_filename (buf);	/* UTF-8 */

  /* Get the Trace keyword value from the ODBC section */
  SQLSetConfigMode (ODBC_BOTH_DSN);
  if (SQLGetPrivateProfileString ((char *) dsn, "Trace", "", 
	buf, sizeof (buf), "odbc.ini")
      && (STRCASEEQ (buf, "on") || STRCASEEQ (buf, "yes")
	  || STRCASEEQ (buf, "1")))
    {
      trace = SQL_OPT_TRACE_ON;
    }

  /* Set the trace flag now */
  if (trace == SQL_OPT_TRACE_ON)
    trace_start ();

  return SQL_SUCCESS;
}


#define CATBUF(buf, str, buf_sz)					\
  do {									\
    if (_iodbcdm_strlcat (buf, str, buf_sz) >= buf_sz)			\
      return -1;							\
  } while (0)


/*
 * Merge the contents of .dsn file into config
 */
static int
_iodbcdm_cfg_merge_filedsn (PCONFIG pconfig, const char *filedsn,
			    char *buf, size_t buf_sz, int wide)
{
  BOOL override;	/* TRUE if params from conn str
			   override params from .dsn file */
  WORD len;
  char *p, *p_next;
  char entries[1024];
  char value[1024];
  char drv_value[1024] = {"\0"};
  char *tmp = NULL;
  int rc = 0;

  /* identify params precedence */
  if (SQLReadFileDSN (filedsn, "ODBC", "DRIVER", value, sizeof (value), &len) &&
      len > 0)
    {
      /* if DRIVER is the same, then conn str params have precedence */
      if (_iodbcdm_cfg_find (pconfig, "ODBC", "DRIVER") == 0 &&
	  !strcasecmp (value, pconfig->value))
        override = TRUE;
      else
        override = FALSE;
    }
  else
    override = TRUE;

  /* get list of entries in .dsn file */
  if (!SQLReadFileDSN (filedsn, "ODBC", NULL,
		       entries, sizeof (entries), &len))
    return -1;

  /* ignore DSN from connection string */
  _iodbcdm_cfg_write (pconfig, "ODBC", "DSN", NULL);

  /* add params from the .dsn file */
  for (p = entries; *p != '\0'; p = p_next)
    {
      /* get next entry */
      p_next = strchr (p, ';');
      if (p_next)
        *p_next++ = '\0';

      if ((override || !strcasecmp (p, "DRIVER")) &&
	  _iodbcdm_cfg_find (pconfig, "ODBC", p) == 0)
        {
	  /* skip param because it is specified in connection string */
          continue;
        }

      if (!SQLReadFileDSN (filedsn, "ODBC", p, value, sizeof(value), &len))
        return -1;
      _iodbcdm_cfg_write (pconfig, "ODBC", p, value);
    }

  /* remove FILEDSN from new config */
  _iodbcdm_cfg_write (pconfig, "ODBC", "FILEDSN", NULL);

  if (_iodbcdm_cfg_find (pconfig, "ODBC", "DRIVER") == 0)
    strncpy(drv_value, pconfig->value, sizeof(drv_value));

  /* remove DRIVER from new config */
  _iodbcdm_cfg_write (pconfig, "ODBC", "DRIVER", NULL);

  /* construct new connection string */
  if ((rc =_iodbcdm_cfg_to_string (pconfig, "ODBC", buf, buf_sz)) == -1)
    goto done;

  tmp = strdup(buf);
  strncpy(buf, "DRIVER=", buf_sz);
  CATBUF(buf, drv_value, buf_sz);
  CATBUF(buf, ";", buf_sz);
  CATBUF(buf, tmp, buf_sz);
  MEM_FREE(tmp);

  if (wide)
    {
      SQLWCHAR *_in = dm_SQL_U8toW (buf, SQL_NTS);
      if (_in == NULL)
        {
          rc = -1;
          goto done;
        }
      WCSNCPY (buf, _in, buf_sz / sizeof (SQLWCHAR));
      MEM_FREE (_in);
    }

  rc = 0;

done:
  if (drv_value[0])
    _iodbcdm_cfg_write (pconfig, "ODBC", "DRIVER", drv_value);
    
  return rc;
}


/*
 * Save connection string into the file
 */
static int
_iodbcdm_cfg_savefile (const char *savefile, void *conn_str, int wide)
{
  int ret = 0;
  PCONFIG pconfig;
  BOOL atsection = FALSE;

  /* parse connection string into pconfig */
  if (_iodbcdm_cfg_init_str (&pconfig, conn_str, SQL_NTS, wide) == -1)
    return -1;

  /* don't save PWD, FILEDSN and SAVEFILE */
  _iodbcdm_cfg_write (pconfig, "ODBC", "PWD", NULL);
  _iodbcdm_cfg_write (pconfig, "ODBC", "FILEDSN", NULL);
  _iodbcdm_cfg_write (pconfig, "ODBC", "SAVEFILE", NULL);
  _iodbcdm_cfg_write (pconfig, "ODBC", "DSN", NULL);

  /* save the file */
  SQLWriteFileDSN (savefile, "ODBC", "DSN", NULL);
  _iodbcdm_cfg_rewind (pconfig);
  while (_iodbcdm_cfg_nextentry (pconfig) == 0)
    {
      if (atsection)
	{
	  if (_iodbcdm_cfg_section (pconfig))
	    {
              /* found next section -- we're done */
              break;
            }
	  else if (_iodbcdm_cfg_define (pconfig))
	    {
              if (!SQLWriteFileDSN (savefile, "ODBC",
				    pconfig->id, pconfig->value))
		{
		  ret = -1;
		  break;
		}
	    }
	}
      else if (_iodbcdm_cfg_section (pconfig)
	  && !strcasecmp (pconfig->section, "ODBC"))
	atsection = TRUE;
    }

  _iodbcdm_cfg_done (pconfig);
  return ret;
}


static
SQLRETURN SQL_API
SQLConnect_Internal (SQLHDBC hdbc,
    SQLPOINTER szDSN,
    SQLSMALLINT cbDSN,
    SQLPOINTER szUID,
    SQLSMALLINT cbUID,
    SQLPOINTER szAuthStr,
    SQLSMALLINT cbAuthStr,
    SQLCHAR waMode)
{
  CONN (pdbc, hdbc);
  ENVR (penv, NULL);
#if (ODBCVER >= 0x300)
  GENV (genv, NULL);
#endif
  SQLRETURN retcode = SQL_SUCCESS;
  SQLRETURN setopterr = SQL_SUCCESS;
  /* MS SDK Guide specifies driver path can't longer than 255. */
  char driver[1024] = { '\0' };
  char buf[256];
  HPROC hproc = SQL_NULL_HPROC;
  SWORD thread_safe;
  SWORD unload_safe;
  void * _szDSN = NULL;
  void * _szUID = NULL;
  void * _szAuthStr = NULL;
  SQLCHAR *_dsn = (SQLCHAR *) szDSN;
  SQLSMALLINT _dsn_len = cbDSN;

  /* check arguments */
  if ((cbDSN < 0 && cbDSN != SQL_NTS)
      || (cbUID < 0 && cbUID != SQL_NTS)
      || (cbAuthStr < 0 && cbAuthStr != SQL_NTS)
      || (cbDSN > SQL_MAX_DSN_LENGTH))
    {
      PUSHSQLERR (pdbc->herr, en_S1090);
      RETURN (SQL_ERROR);
    }

  if (szDSN == NULL || cbDSN == 0)
    {
      PUSHSQLERR (pdbc->herr, en_IM002);
      RETURN (SQL_ERROR);
    }

  /* check state */
  if (pdbc->state != en_dbc_allocated)
    {
      PUSHSQLERR (pdbc->herr, en_08002);
      RETURN (SQL_ERROR);
    }


  if (waMode == 'W')
    {
      _szDSN = (void *) dm_SQL_WtoU8((SQLWCHAR *)szDSN, cbDSN);
      _dsn = (SQLCHAR *) _szDSN;
      _dsn_len = SQL_NTS;
      if (_dsn == NULL)
        {
          PUSHSQLERR (pdbc->herr, en_S1001);
          RETURN (SQL_ERROR);
        }
    }

  /* Get the config mode */
  if (_iodbcdm_con_settracing (pdbc, _dsn, _dsn_len, waMode) == SQL_ERROR)
    RETURN (SQL_ERROR);

#if (ODBCVER >= 0x300)
  genv = (GENV_t *) pdbc->genv;

  if (genv->connection_pooling != SQL_CP_OFF)
    {
      char *_uid = szUID;
      char *_pwd = szAuthStr;

      /*
       * _dsn is already an UTF8 string so
       * need to convert to UTF8 only szUID and szAuthStr
       */
      if (waMode == 'W')
        {
          if (szUID != NULL)
	    {
              _szUID = (void *) dm_SQL_WtoU8((SQLWCHAR *) szUID, cbUID);
	      if (_szUID == NULL)
	        {
		  PUSHSQLERR (pdbc->herr, en_S1001);
		  RETURN (SQL_ERROR);
	        }
	    }
          if (szAuthStr != NULL)
	    {
              _szAuthStr = (void *) dm_SQL_WtoU8(
	          (SQLWCHAR *) szAuthStr, cbAuthStr);
	      if (_szAuthStr == NULL)
	        {
		  PUSHSQLERR (pdbc->herr, en_S1001);
		  RETURN (SQL_ERROR);
	        }
	    }
	  _uid = _szUID;
	  _pwd = _szAuthStr;
        }

      retcode = _iodbcdm_pool_get_conn (pdbc, _dsn, _uid, _pwd, NULL);
      if (SQL_SUCCEEDED (retcode))
        {
	  /*
	   * Got connection from the pool
	   */

          /* state transition */
          pdbc->state = en_dbc_connected;

          RETURN (retcode);
        }

      if (pdbc->cp_pdbc != NULL)
        {
	  /*
	   * Dead connection was taken from pool
	   */

          if (pdbc->cp_pdbc->cp_retry_wait != 0)
	    {
	      /*
	       * Retry Wait timeout has not expired yet
	       */
              PUSHSQLERR (pdbc->herr, en_08004);
	      RETURN (SQL_ERROR);
	    }

	  /*
	   * Free connection parameters.
	   */
	  if (waMode == 'W')
	    {
	      if (_szUID != NULL)
	        {
		  MEM_FREE (_szUID);
		  _szUID = NULL;
	        }
	      if (_szAuthStr != NULL)
	        {
		  MEM_FREE (_szAuthStr);
		  _szAuthStr = NULL;
	        }
	    }
	}
      else
        {
          /*
	   * Connection was not found in the pool --
	   * save connection parameters
	   */
	  if (pdbc->cp_dsn != NULL)
	    MEM_FREE (pdbc->cp_dsn);
	  if (pdbc->cp_uid != NULL)
	    MEM_FREE (pdbc->cp_uid);
	  if (pdbc->cp_pwd != NULL)
	    MEM_FREE (pdbc->cp_pwd);

          if (waMode == 'W')
	    {
	      pdbc->cp_dsn = _szDSN;
	      _szDSN = NULL;
	      pdbc->cp_uid = _szUID;
	      _szUID = NULL;
	      pdbc->cp_pwd = _szAuthStr;
	      _szAuthStr = NULL;
	    }
	  else
	    {
	      pdbc->cp_dsn = strdup (_dsn);
	      if (pdbc->cp_dsn == NULL)
	        {
		  PUSHSQLERR (pdbc->herr, en_S1001);
		  RETURN (SQL_ERROR);
		}
	      if (_uid != NULL)
	        {
		  pdbc->cp_uid = strdup (_uid);
		  if (pdbc->cp_uid == NULL)
		    {
		      PUSHSQLERR (pdbc->herr, en_S1001);
		      RETURN (SQL_ERROR);
		    }
		}
	      if (_pwd != NULL)
	        {
		  pdbc->cp_pwd = strdup (_pwd);
		  if (pdbc->cp_pwd == NULL)
		    {
		      PUSHSQLERR (pdbc->herr, en_S1001);
		      RETURN (SQL_ERROR);
		    }
		}
	    }
	}
    }
#endif /* (ODBCVER >= 0x300) */

  /*
   *  Check whether driver is thread safe
   */
  thread_safe = 1;		/* Assume driver is thread safe */

  SQLSetConfigMode (ODBC_BOTH_DSN);
  if ( SQLGetPrivateProfileString ((char *) _dsn, "ThreadManager", "", 
	buf, sizeof(buf), "odbc.ini") &&
      (STRCASEEQ (buf, "on") || STRCASEEQ (buf, "1")))
    {
      thread_safe = 0;	/* Driver needs a thread manager */
    }

  /*
   *  Check if it is safe to unload the driver
   */
  unload_safe = 0;		/* Assume driver is not unload safe */

  SQLSetConfigMode (ODBC_BOTH_DSN);
  if ( SQLGetPrivateProfileString ((char *) _dsn, "UnloadSafe", "", 
	buf, sizeof(buf), "odbc.ini") &&
      (STRCASEEQ (buf, "on") || STRCASEEQ (buf, "1")))
    {
      unload_safe = 1;
    }


  /*
   *  Get the name of the driver module and load it
   */
  SQLSetConfigMode (ODBC_BOTH_DSN);
  if ( SQLGetPrivateProfileString ((char *) _dsn, "Driver", "", 
	(char *) driver, sizeof(driver), "odbc.ini") == 0)
    /* No specified or default dsn section or
     * no driver specification in this dsn section */
    {
      PUSHSQLERR (pdbc->herr, en_IM002);
      RETURN (SQL_ERROR);
    }

  MEM_FREE(_szDSN);
  _szDSN = NULL;

  retcode = _iodbcdm_driverload (_dsn, (char *)driver, pdbc, thread_safe, unload_safe, waMode);

  switch (retcode)
    {
    case SQL_SUCCESS:
      break;

    case SQL_SUCCESS_WITH_INFO:
#if 0
      /* 
       *  Unsuccessful in calling driver's SQLSetConnectOption() to set 
       *  login timeout.
       */
      setopterr = SQL_ERROR;
#endif
      break;

    default:
      return retcode;
    }

  penv = (ENV_t *) pdbc->henv;

  if ((penv->unicode_driver && waMode != 'W')
      || (!penv->unicode_driver && waMode == 'W'))
    {
      if (waMode != 'W')
        {
        /* ansi=>unicode*/
          _szDSN = dm_SQL_A2W((SQLCHAR *)szDSN, cbDSN);
          _szUID = dm_SQL_A2W((SQLCHAR *)szUID, cbUID);
          _szAuthStr = dm_SQL_A2W((SQLCHAR *)szAuthStr, cbAuthStr);
        }
      else
        {
        /* unicode=>ansi*/
          _szDSN = dm_SQL_W2A((SQLWCHAR *)szDSN, cbDSN);
          _szUID = dm_SQL_W2A((SQLWCHAR *)szUID, cbUID);
          _szAuthStr = dm_SQL_W2A((SQLWCHAR *)szAuthStr, cbAuthStr);
        }
      cbDSN = SQL_NTS;
      cbUID = SQL_NTS;
      cbAuthStr = SQL_NTS;
      szDSN = _szDSN;
      szUID = _szUID;
      szAuthStr = _szAuthStr;
    }

  ODBC_UNLOCK ();
  CALL_UDRIVER(hdbc, pdbc, retcode, hproc, penv->unicode_driver,
    en_Connect, (
       pdbc->dhdbc,
       szDSN,
       cbDSN,
       szUID,
       cbUID,
       szAuthStr,
       cbAuthStr));
  ODBC_LOCK ();

  if (hproc == SQL_NULL_HPROC)
    {
      _iodbcdm_driverunload (pdbc, 3);
      PUSHSQLERR (pdbc->herr, en_IM001);
      RETURN (SQL_ERROR);
    }

  if (!SQL_SUCCEEDED (retcode))
    {
      /* not unload driver for retrieve error
       * message from driver */
		/*********
		_iodbcdm_driverunload( hdbc , 3);
		**********/

      RETURN (retcode);
    }

  /* state transition */
  pdbc->state = en_dbc_connected;

  /* do delayed option setting */
  setopterr |= _iodbcdm_dbcdelayset (pdbc, waMode);

  if (setopterr != SQL_SUCCESS)
    retcode = SQL_SUCCESS_WITH_INFO;

end:
#if (ODBCVER >= 0x300)
  if (!SQL_SUCCEEDED (retcode) &&
      pdbc->cp_pdbc != NULL)
    {
      int rc;

      /*
       * Dead connection was taken from the pool
       * but reconnection attempt has failed:
       * set cp_retry_wait time and return connection to the pool.
       */
      _iodbcdm_pool_set_retry_wait (pdbc);
      rc = _iodbcdm_pool_put_conn (pdbc);
      assert (rc == 0);
    }
#endif
  if (_szDSN != NULL)
    MEM_FREE(_szDSN);
  if (_szUID != NULL)
    MEM_FREE (_szUID);
  if (_szAuthStr != NULL)
    MEM_FREE (_szAuthStr);

  return retcode;
}


SQLRETURN SQL_API
SQLConnect (
  SQLHDBC		  hdbc,
  SQLCHAR 		* szDSN,
  SQLSMALLINT		  cbDSN,
  SQLCHAR 		* szUID,
  SQLSMALLINT		  cbUID,
  SQLCHAR 		* szAuthStr,
  SQLSMALLINT		  cbAuthStr)
{
  ENTER_HDBC (hdbc, 1,
    trace_SQLConnect (TRACE_ENTER,
    	hdbc,
	szDSN, cbDSN,
	szUID, cbUID,
	szAuthStr, cbAuthStr));

  retcode =  SQLConnect_Internal (
  	hdbc,
	szDSN, cbDSN,
	szUID, cbUID,
	szAuthStr, cbAuthStr, 'A');

  LEAVE_HDBC (hdbc, 1,
    trace_SQLConnect (TRACE_LEAVE,
    	hdbc,
	szDSN, cbDSN,
	szUID, cbUID,
	szAuthStr, cbAuthStr));
}


SQLRETURN SQL_API
SQLConnectA (
  SQLHDBC		  hdbc,
  SQLCHAR 		* szDSN,
  SQLSMALLINT		  cbDSN,
  SQLCHAR 		* szUID,
  SQLSMALLINT		  cbUID,
  SQLCHAR 		* szAuthStr,
  SQLSMALLINT		  cbAuthStr)
{
  ENTER_HDBC (hdbc, 1,
    trace_SQLConnect (TRACE_ENTER,
    	hdbc,
	szDSN, cbDSN,
	szUID, cbUID,
	szAuthStr, cbAuthStr));

  retcode =  SQLConnect_Internal (
  	hdbc,
	szDSN, cbDSN,
	szUID, cbUID,
	szAuthStr, cbAuthStr, 'A');

  LEAVE_HDBC (hdbc, 1,
    trace_SQLConnect (TRACE_LEAVE,
    	hdbc,
	szDSN, cbDSN,
	szUID, cbUID,
	szAuthStr, cbAuthStr));
}


SQLRETURN SQL_API
SQLConnectW (SQLHDBC hdbc,
    SQLWCHAR * szDSN,
    SQLSMALLINT cbDSN,
    SQLWCHAR * szUID,
    SQLSMALLINT cbUID,
    SQLWCHAR * szAuthStr,
    SQLSMALLINT cbAuthStr)
{
  ENTER_HDBC (hdbc, 1,
    trace_SQLConnectW (TRACE_ENTER,
    	hdbc,
	szDSN, cbDSN,
	szUID, cbUID,
	szAuthStr, cbAuthStr));

  retcode =  SQLConnect_Internal (
  	hdbc,
	szDSN, cbDSN,
	szUID, cbUID,
	szAuthStr, cbAuthStr,
	'W');

  LEAVE_HDBC (hdbc, 1,
    trace_SQLConnectW (TRACE_LEAVE,
    	hdbc,
	szDSN, cbDSN,
	szUID, cbUID,
	szAuthStr, cbAuthStr));
}


SQLRETURN SQL_API
SQLDriverConnect_Internal (
    SQLHDBC hdbc,
    SQLHWND hwnd,
    SQLPOINTER szConnStrIn,
    SQLSMALLINT cbConnStrIn,
    SQLPOINTER szConnStrOut,
    SQLSMALLINT cbConnStrOutMax,
    SQLPOINTER pcbConnStrOut,
    SQLUSMALLINT fDriverCompletion,
    SQLCHAR waMode)
{
  CONN (pdbc, hdbc);
  ENVR (penv, NULL);
#if (ODBCVER >= 0x300)
  GENV (genv, NULL);
#endif
  HDLL hdll;
  SQLCHAR *drv = NULL;
  SQLCHAR drvbuf[1024];
  SQLCHAR *dsn = NULL;
  SQLCHAR dsnbuf[SQL_MAX_DSN_LENGTH + 1];
  SQLWCHAR prov[2048];
  SWORD thread_safe;
  SWORD unload_safe;
  SQLCHAR buf[1024];
  HPROC hproc = SQL_NULL_HPROC;
  void *_ConnStrIn = NULL;
  void *_ConnStrOut = NULL;
  void *connStrOut = szConnStrOut;
  void *connStrIn = szConnStrIn;
  SQLSMALLINT connStrOutMax = cbConnStrOutMax;
  SQLWCHAR connStrOut_buf[2048];
  SQLWCHAR connStrIn_buf[2048];
  UWORD config;
  PCONFIG pconfig = NULL;
  BOOL bCallDmDlg = FALSE;
#if defined (__APPLE__) && !(defined (NO_FRAMEWORKS) || defined (_LP64))
  CFStringRef libname = NULL;
  CFBundleRef bundle = NULL;
  CFURLRef liburl = NULL;
  char name[1024] = { 0 };
#endif
  SQLCHAR *filedsn = NULL;
  SQLCHAR *savefile = NULL;

  HPROC dialproc = SQL_NULL_HPROC;

  sqlstcode_t sqlstat = en_00000;
  SQLRETURN retcode = SQL_SUCCESS;
  SQLRETURN setopterr = SQL_SUCCESS;

  /* check arguments */
  if ((cbConnStrIn < 0 && cbConnStrIn != SQL_NTS) ||
      (cbConnStrOutMax < 0 && cbConnStrOutMax != SQL_NTS))
    {
      PUSHSQLERR (pdbc->herr, en_S1090);
      RETURN (SQL_ERROR);
    }

  /* check state */
  if (pdbc->state != en_dbc_allocated)
    {
      PUSHSQLERR (pdbc->herr, en_08002);
      RETURN (SQL_ERROR);
    }

  /* Save config mode */
  SQLGetConfigMode (&config);

  if (_iodbcdm_cfg_init_str (&pconfig, connStrIn, cbConnStrIn,
			     waMode == 'W') == -1)
    {
      PUSHSQLERR (pdbc->herr, en_HY001);
      RETURN (SQL_ERROR);
    }
  assert (_iodbcdm_cfg_valid(pconfig));

  /* lookup and save original SAVEFILE value */
  if (_iodbcdm_cfg_find (pconfig, "ODBC", "SAVEFILE") == 0)
    {
      savefile = strdup (pconfig->value);
      if (savefile == NULL)
        {
          PUSHSQLERR (pdbc->herr, en_HY001);
          RETURN (SQL_ERROR);
        }
    }


#if (ODBCVER >= 0x300)
  genv = (GENV_t *) pdbc->genv;

  /*
   * Try to find pooled connection.
   * Pooling is disabled if SAVEFILE is present.
   */
  if (genv->connection_pooling != SQL_CP_OFF && savefile == NULL)
    {
      char *_connstr = connStrIn;

      if (fDriverCompletion != SQL_DRIVER_NOPROMPT)
        {
          PUSHSQLERR (pdbc->herr, en_HY110);
          RETURN (SQL_ERROR);
        }

      if (waMode == 'W')
        {
          _ConnStrIn = dm_SQL_WtoU8((SQLWCHAR *) connStrIn, cbConnStrIn);
	  if (_ConnStrIn == NULL)
	    {
              PUSHSQLERR (pdbc->herr, en_HY001);
              RETURN (SQL_ERROR);
	    }
	  _connstr = _ConnStrIn;
	}

      retcode = _iodbcdm_pool_get_conn (pdbc, NULL, NULL, NULL, _connstr);
      if (SQL_SUCCEEDED (retcode))
        {
	  /*
	   * Got connection from the pool
	   */

          /* copy out connection string */
          if (szConnStrOut != NULL)
	    {
	      if (waMode == 'W')
	        {
		  WCSNCPY (szConnStrOut, szConnStrIn, cbConnStrOutMax);
                  *(SQLSMALLINT *) pcbConnStrOut =
		      WCSLEN (szConnStrOut) * sizeof (SQLWCHAR);
		}
	      else
	        {
		  _iodbcdm_strlcpy (szConnStrOut, szConnStrIn, cbConnStrOutMax);
                  *(SQLSMALLINT *) pcbConnStrOut = strlen (szConnStrOut);
	        }
	    }

          /* state transition */
          pdbc->state = en_dbc_connected;

          RETURN (retcode);
        }

      if (pdbc->cp_pdbc != NULL)
        {
	  /*
	   * Dead connection was taken from pool
	   */

          if (pdbc->cp_pdbc->cp_retry_wait != 0)
	    {
	      /*
	       * Retry Wait timeout has not expired yet
	       */
              PUSHSQLERR (pdbc->herr, en_08004);
	      RETURN (SQL_ERROR);
	    }

	  /*
	   * Free connection parameters.
	   */
	  if (waMode == 'W')
	    {
	      if (_ConnStrIn != NULL)
	        {
		  MEM_FREE (_ConnStrIn);
		  _ConnStrIn = NULL;
	        }
	    }
        }
      else
        {
          /*
	   * Connection was not found in the pool --
	   * save connection parameters
	   */
	  if (pdbc->cp_connstr != NULL)
	    MEM_FREE (pdbc->cp_connstr);

          if (waMode == 'W')
	    {
	      pdbc->cp_connstr = _ConnStrIn;
	      _ConnStrIn = NULL;
	    }
	  else
	    {
              pdbc->cp_connstr = strdup (_connstr);
	      if (pdbc->cp_connstr == NULL)
	        {
                  PUSHSQLERR (pdbc->herr, en_HY001);
                  RETURN (SQL_ERROR);
	        }
	    }
        }
    }
#endif /* (ODBCVER >= 0x300) */

  /* always get (even if not requested) out connection string for SAVEFILE */
  if (!connStrOut)
    {
      connStrOut = connStrOut_buf;
      connStrOutMax = sizeof(connStrOut_buf);
    }

  /* now look for DSN or FILEDSN, whichever comes first */
  _iodbcdm_cfg_rewind (pconfig);
  while (_iodbcdm_cfg_nextentry (pconfig) == 0)
    {
      if (!_iodbcdm_cfg_define (pconfig))
        continue;

      if (!strcasecmp(pconfig->id, "DSN"))
        {
          /* not a file dsn */
          break;
        }
      else if (!strcasecmp(pconfig->id, "FILEDSN"))
        {
          /* file dsn */
          filedsn = strdup (pconfig->value);
	  if (filedsn == NULL)
	    {
              PUSHSQLERR (pdbc->herr, en_HY001);
              RETURN (SQL_ERROR);
	    }
	  break;
	}
    }


  /* get connect parameters from .dsn file if requested */
  if (filedsn != NULL)
    {
      /* merge params from .dsn file */
      if (_iodbcdm_cfg_merge_filedsn (pconfig, filedsn,
	      (char *) connStrIn_buf, sizeof (connStrIn_buf),
	      waMode == 'W') == -1)
        {
          PUSHSQLERR (pdbc->herr, en_IM015);
          RETURN (SQL_ERROR);
	}

      /* update connection string and its length */
      connStrIn = connStrIn_buf;
      if (cbConnStrIn != SQL_NTS)
	{
	  if (waMode != 'W')
	    cbConnStrIn = STRLEN (connStrIn);
	  else
	    cbConnStrIn = WCSLEN (connStrIn);
	}
    }

  if (_iodbcdm_cfg_find (pconfig, "ODBC", "DRIVER") == 0)
    {
      /* copy because pconfig can be reinitialized later */
      _iodbcdm_strlcpy ((char *) drvbuf, pconfig->value, sizeof (drvbuf));
      drv = drvbuf;
    }
  if (_iodbcdm_cfg_find (pconfig, "ODBC", "DSN") == 0)
    {
      /* copy because pconfig can be reinitialized later */
      _iodbcdm_strlcpy ((char *) dsnbuf, pconfig->value, sizeof (dsnbuf));
      dsn = dsnbuf;
    }

  switch (fDriverCompletion)
    {
    case SQL_DRIVER_NOPROMPT:
      /* Check if there's a DSN or DRIVER */
      if (!dsn && !drv)
	{
	  PUSHSQLERR (pdbc->herr, en_IM007);
	  RETURN (SQL_ERROR);
	}
      break;

    case SQL_DRIVER_COMPLETE:
    case SQL_DRIVER_COMPLETE_REQUIRED:
      if (dsn != NULL || drv != NULL)
	{
	  break;
	}
      /* fall to next case */
    case SQL_DRIVER_PROMPT:
      /* Get data source dialog box function from
       * current executable */
      /* Not really sure here, but should load that from the iodbcadm */
      if (waMode == 'A') 
	_iodbcdm_strlcpy ((char *) prov, connStrIn, sizeof (prov));
      else 
        wcsncpy (prov, connStrIn, sizeof (prov) / sizeof (wchar_t));

#if 0
        if (!dsn && !drv)
          bCallDmDlg = TRUE;
        else if ( _iodbcdm_CheckDriverLoginDlg(drv, dsn) == FALSE)
          bCallDmDlg = TRUE;
  
        /* not call iODBC function "iodbcdm_drvconn_dialbox", if there is
         * the function "_iodbcdm_drvconn_dialbox" in the odbc driver,
         * odbc driver must call its function itself
         */
        if (!bCallDmDlg)
          break;
#endif

      ODBC_UNLOCK (); 
#if defined (__APPLE__) && !(defined (NO_FRAMEWORKS) || defined (_LP64))
      bundle = CFBundleGetBundleWithIdentifier (CFSTR ("org.iodbc.core"));
      if (bundle)
        {
          /* Search for the drvproxy library */
          liburl =
  	      CFBundleCopyResourceURL (bundle, CFSTR ("iODBCadm.bundle"),
	      NULL, NULL);
          if (liburl
              && (libname =
                  CFURLCopyFileSystemPath (liburl, kCFURLPOSIXPathStyle)))
            {
              CFStringGetCString (libname, name, sizeof (name),
                kCFStringEncodingASCII);
	      _iodbcdm_strlcat (name, "/Contents/MacOS/iODBCadm",
		  sizeof (name));
              hdll = _iodbcdm_dllopen (name);
	    }
	  if (liburl)
	    CFRelease (liburl);
	  if (libname)
	    CFRelease (libname);
	}
#else
      hdll = _iodbcdm_dllopen ("libiodbcadm.so");
#endif

      if (!hdll)
	break;

      if (waMode != 'W')
        dialproc = _iodbcdm_dllproc (hdll, "iodbcdm_drvconn_dialbox");
      else
        dialproc = _iodbcdm_dllproc (hdll, "iodbcdm_drvconn_dialboxw");

      if (dialproc == SQL_NULL_HPROC)
        {
          sqlstat = en_IM008;
          break;
        }

      retcode = dialproc (hwnd,	/* window or display handle */
          prov,		        /* input/output dsn buf */
          sizeof (prov) / (waMode == 'A' ? 1 : sizeof (SQLWCHAR)), /* buf size */
          &sqlstat,		/* error code */
          fDriverCompletion,	/* type of completion */
          &config);		/* config mode */

      ODBC_LOCK ();
      fDriverCompletion = SQL_DRIVER_NOPROMPT;

      if (retcode != SQL_SUCCESS)
        {
          if (retcode != SQL_NO_DATA_FOUND)
  	    PUSHSQLERR (pdbc->herr, sqlstat);
	  goto end;
        }

      connStrIn = prov;

      /*
       * Recalculate length of connStrIn if needed, as it may have been
       * changed by iodbcdm_drvconn_dialbox
       */
      if (cbConnStrIn != SQL_NTS)
        {
	  if (waMode != 'W')
	    cbConnStrIn = STRLEN (connStrIn);
	  else
	    cbConnStrIn = WCSLEN (connStrIn);
	}

      if (_iodbcdm_cfg_parse_str (pconfig, connStrIn, cbConnStrIn,
				  waMode == 'W') == -1)
        {
          PUSHSQLERR (pdbc->herr, en_HY001);
          RETURN (SQL_ERROR);
        }
      if (_iodbcdm_cfg_find (pconfig, "ODBC", "DSN") == 0)
        dsn = pconfig->value;
      if (_iodbcdm_cfg_find (pconfig, "ODBC", "DRIVER") == 0)
        {
          /* copy because pconfig can be reinitialized later */
          _iodbcdm_strlcpy ((char *) drvbuf, pconfig->value, sizeof (drvbuf));
          drv = drvbuf;
        }
      break;

    default:
      sqlstat = en_S1110;
      break;
    }

  if (sqlstat != en_00000)
    {
      PUSHSQLERR (pdbc->herr, sqlstat);
      RETURN (SQL_ERROR);
    }

  if (dsn == NULL || *(char *) dsn == '\0')
    {
      dsn = (void *) "default";
    }
  else
    /* if you want tracing, you must use a DSN */
    {
      setopterr |= 
          _iodbcdm_con_settracing (pdbc, (SQLCHAR *) dsn, SQL_NTS, waMode);
    }

  /*
   *  Check whether driver is thread safe
   */
  thread_safe = 1;		/* Assume driver is thread safe */

  SQLSetConfigMode (ODBC_BOTH_DSN);
  if (SQLGetPrivateProfileString ((char *) dsn, "ThreadManager", "", 
	buf, sizeof (buf), "odbc.ini")
      && (STRCASEEQ (buf, "on") || STRCASEEQ (buf, "1")))
    {
      thread_safe = 0;		/* Driver needs a thread manager */
    }

  /*
   *  Check whether driver is unload safe
   */
  unload_safe = 0;		/* Assume driver is not unload safe */

  SQLSetConfigMode (ODBC_BOTH_DSN);
  if (SQLGetPrivateProfileString ((char *) dsn, "UnloadSafe", "", 
	buf, sizeof (buf), "odbc.ini")
      && (STRCASEEQ (buf, "on") || STRCASEEQ (buf, "1")))
    {
      unload_safe = 1;
    }

  /*
   *  Get the name of the driver module
   */
  if (drv == NULL || *(char *) drv == '\0')
    {
      SQLSetConfigMode (ODBC_BOTH_DSN);
      if (SQLGetPrivateProfileString ((char *) dsn, "Driver", "", 
	      (char *) drvbuf, sizeof (drvbuf), "odbc.ini") != 0)
	{
	  drv = drvbuf;
	}
    }

  if (drv == NULL)
    {
      PUSHSQLERR (pdbc->herr, en_IM002);
      RETURN (SQL_ERROR);
    }

  retcode = 
      _iodbcdm_driverload (dsn, (char *) drv, pdbc, thread_safe, unload_safe, 
      waMode);

  switch (retcode)
    {
    case SQL_SUCCESS:
      break;

    case SQL_SUCCESS_WITH_INFO:
#if 0
      /* 
       *  Unsuccessful in calling driver's SQLSetConnectOption() to set 
       *  login timeout.
       */
      setopterr = SQL_ERROR;
#endif
      break;

    default:
      RETURN (retcode);
    }

#if (ODBCVER >= 0x300)
  /*
   * Pooling is disabled if SAVEFILE is present.
   */
  if (savefile != NULL)
    pdbc->cp_timeout = 0;
#endif

  penv = (ENV_t *) pdbc->henv;

  if ((penv->unicode_driver && waMode != 'W')
      || (!penv->unicode_driver && waMode == 'W'))
    {
      if (waMode != 'W')
	{
	  /* ansi=>unicode */
	  if ((_ConnStrOut =
		  malloc (connStrOutMax * sizeof (SQLWCHAR) + 1)) == NULL)
	    {
	      PUSHSQLERR (pdbc->herr, en_HY001);
	      RETURN (SQL_ERROR);
	    }
	  _ConnStrIn = dm_SQL_A2W ((SQLCHAR *) connStrIn, cbConnStrIn);
	}
      else
	{
	  /* unicode=>ansi */
	  if ((_ConnStrOut = malloc (connStrOutMax + 1)) == NULL)
	    {
	      PUSHSQLERR (pdbc->herr, en_HY001);
	      RETURN (SQL_ERROR);
	    }
	  _ConnStrIn = dm_SQL_W2A ((SQLWCHAR *) connStrIn, cbConnStrIn);
	}
      connStrOut = _ConnStrOut;
      connStrIn = _ConnStrIn;
      cbConnStrIn = SQL_NTS;
    }


  /* Restore config mode */
  SQLSetConfigMode (config);

  ODBC_UNLOCK (); 
  CALL_UDRIVER (hdbc, pdbc, retcode, hproc, penv->unicode_driver,
      en_DriverConnect, (pdbc->dhdbc,
	  hwnd,
	  connStrIn,
	  cbConnStrIn,
	  connStrOut, connStrOutMax, pcbConnStrOut, fDriverCompletion));
  ODBC_LOCK ();

  if (hproc == SQL_NULL_HPROC)
    {
      _iodbcdm_driverunload (pdbc, 3);
      PUSHSQLERR (pdbc->herr, en_IM001);
      RETURN (SQL_ERROR);
    }

  if (szConnStrOut
      && SQL_SUCCEEDED (retcode)
      && ((penv->unicode_driver && waMode != 'W')
	  || (!penv->unicode_driver && waMode == 'W')))
    {
      if (waMode != 'W')
	{
	  /* ansi<=unicode */
          dm_StrCopyOut2_W2A ((SQLWCHAR *) connStrOut, 
              (SQLCHAR *) szConnStrOut, cbConnStrOutMax, NULL);
	}
      else
	{
	  /* unicode<=ansi */
          dm_StrCopyOut2_A2W ((SQLCHAR *) connStrOut, 
              (SQLWCHAR *) szConnStrOut, cbConnStrOutMax, NULL);
	}
    }

  if (szConnStrOut != NULL)
    {
      if (filedsn != NULL)
        {
          /* append FILEDSN to the out connection string */
          if (waMode == 'W')
            {
              SQLWCHAR *_tmp = dm_SQL_U8toW (filedsn, SQL_NTS);
	      if (_tmp == NULL)
	        {
                  PUSHSQLERR (pdbc->herr, en_HY001);
                  RETURN (SQL_ERROR);
	        }
	      WCSNCAT (szConnStrOut, L";FILEDSN=", cbConnStrOutMax);
	      WCSNCAT (szConnStrOut, _tmp, cbConnStrOutMax);
              MEM_FREE (_tmp);
	    }
          else
            {
              _iodbcdm_strlcat (szConnStrOut, ";FILEDSN=", cbConnStrOutMax);
              _iodbcdm_strlcat (szConnStrOut, filedsn, cbConnStrOutMax);
            }
        }
      if (savefile != NULL)
        {
          /* append SAVEFILE to the out connection string */
          if (waMode == 'W')
            {
              SQLWCHAR *_tmp = dm_SQL_U8toW (savefile, SQL_NTS);
	      if (_tmp == NULL)
	        {
                  PUSHSQLERR (pdbc->herr, en_HY001);
                  RETURN (SQL_ERROR);
	        }
	      WCSNCAT (szConnStrOut, L";SAVEFILE=", cbConnStrOutMax);
	      WCSNCAT (szConnStrOut, _tmp, cbConnStrOutMax);
              MEM_FREE (_tmp);
	    }
          else
            {
              _iodbcdm_strlcat (szConnStrOut, ";SAVEFILE=", cbConnStrOutMax);
              _iodbcdm_strlcat (szConnStrOut, savefile, cbConnStrOutMax);
            }
        }

      /* fixup pcbConnStrOut */
      if (waMode == 'W')
        {
          *(SQLSMALLINT *) pcbConnStrOut =
	      WCSLEN (szConnStrOut) * sizeof (SQLWCHAR);
	}
      else
        *(SQLSMALLINT *) pcbConnStrOut = strlen (szConnStrOut);
    }

  if (!SQL_SUCCEEDED (retcode))
    {
      /* don't unload driver here for retrieve
       * error message from driver */
		/********
		_iodbcdm_driverunload( hdbc , 3);
		*********/

      RETURN (retcode);
    }

  /* state transition */
  pdbc->state = en_dbc_connected;

  /* do delayed option setting */
  setopterr |= _iodbcdm_dbcdelayset (pdbc, waMode);

  if (setopterr != SQL_SUCCESS)
    retcode = SQL_SUCCESS_WITH_INFO;

  /* save .dsn file if requested */
  if (savefile != NULL)
    {
      assert (connStrOut != NULL);

      if (_iodbcdm_cfg_savefile (savefile, connStrOut,
				 penv->unicode_driver) == -1)
        {
	  PUSHSQLERR (pdbc->herr, en_01S08);
	  retcode = SQL_SUCCESS_WITH_INFO;
	}
    }

end:
#if (ODBCVER >= 0x300)
  if (!SQL_SUCCEEDED (retcode) &&
      pdbc->cp_pdbc != NULL)
    {
      int rc;

      /*
       * Dead connection was taken from the pool
       * but reconnection attempt has failed:
       * set cp_retry_wait time and return connection to the pool.
       */
      _iodbcdm_pool_set_retry_wait (pdbc);
      rc = _iodbcdm_pool_put_conn (pdbc);
      assert (rc == 0);
    }
#endif
  _iodbcdm_cfg_done (pconfig);
  if (_ConnStrIn != NULL)
    MEM_FREE (_ConnStrIn);
  if (_ConnStrOut != NULL)
    MEM_FREE (_ConnStrOut);
  if (savefile != NULL)
    MEM_FREE (savefile);
  if (filedsn != NULL)
    MEM_FREE (filedsn);

  return retcode;
}


SQLRETURN SQL_API
SQLDriverConnect (SQLHDBC hdbc,
    SQLHWND hwnd,
    SQLCHAR * szConnStrIn,
    SQLSMALLINT cbConnStrIn,
    SQLCHAR * szConnStrOut,
    SQLSMALLINT cbConnStrOutMax,
    SQLSMALLINT * pcbConnStrOut,
    SQLUSMALLINT fDriverCompletion)
{
  ENTER_HDBC (hdbc, 1,
    trace_SQLDriverConnect (TRACE_ENTER,
	hdbc,
	hwnd,
	szConnStrIn, cbConnStrIn,
	szConnStrOut, cbConnStrOutMax, pcbConnStrOut,
	fDriverCompletion));

  retcode = SQLDriverConnect_Internal(
      hdbc,
      hwnd,
      szConnStrIn, cbConnStrIn,
      szConnStrOut, cbConnStrOutMax, pcbConnStrOut,
      fDriverCompletion,
      'A');

  LEAVE_HDBC (hdbc, 1,
    trace_SQLDriverConnect (TRACE_LEAVE,
	hdbc,
	hwnd,
	szConnStrIn, cbConnStrIn,
	szConnStrOut, cbConnStrOutMax, pcbConnStrOut,
	fDriverCompletion));
}


SQLRETURN SQL_API
SQLDriverConnectA (SQLHDBC hdbc,
    SQLHWND hwnd,
    SQLCHAR * szConnStrIn,
    SQLSMALLINT cbConnStrIn,
    SQLCHAR * szConnStrOut,
    SQLSMALLINT cbConnStrOutMax,
    SQLSMALLINT * pcbConnStrOut,
    SQLUSMALLINT fDriverCompletion)
{
  ENTER_HDBC (hdbc, 1,
    trace_SQLDriverConnect (TRACE_ENTER,
	hdbc,
	hwnd,
	szConnStrIn, cbConnStrIn,
	szConnStrOut, cbConnStrOutMax, pcbConnStrOut,
	fDriverCompletion));

  retcode = SQLDriverConnect_Internal(
      hdbc,
      hwnd,
      szConnStrIn, cbConnStrIn,
      szConnStrOut, cbConnStrOutMax, pcbConnStrOut,
      fDriverCompletion,
      'A');

  LEAVE_HDBC (hdbc, 1,
    trace_SQLDriverConnect (TRACE_LEAVE,
	hdbc,
	hwnd,
	szConnStrIn, cbConnStrIn,
	szConnStrOut, cbConnStrOutMax, pcbConnStrOut,
	fDriverCompletion));
}


SQLRETURN SQL_API
SQLDriverConnectW (SQLHDBC hdbc,
    SQLHWND hwnd,
    SQLWCHAR * szConnStrIn,
    SQLSMALLINT cbConnStrIn,
    SQLWCHAR * szConnStrOut,
    SQLSMALLINT cbConnStrOutMax,
    SQLSMALLINT * pcbConnStrOut,
    SQLUSMALLINT fDriverCompletion)
{
  ENTER_HDBC (hdbc, 1,
    trace_SQLDriverConnectW (TRACE_ENTER,
	hdbc,
	hwnd,
	szConnStrIn, cbConnStrIn,
	szConnStrOut, cbConnStrOutMax, pcbConnStrOut,
	fDriverCompletion));

  retcode = SQLDriverConnect_Internal(
      hdbc,
      hwnd,
      szConnStrIn, cbConnStrIn,
      szConnStrOut, cbConnStrOutMax, pcbConnStrOut,
      fDriverCompletion,
      'W');

  LEAVE_HDBC (hdbc, 1,
    trace_SQLDriverConnectW (TRACE_LEAVE,
	hdbc,
	hwnd,
	szConnStrIn, cbConnStrIn,
	szConnStrOut, cbConnStrOutMax, pcbConnStrOut,
	fDriverCompletion));
}


SQLRETURN SQL_API
SQLBrowseConnect_Internal (SQLHDBC hdbc,
    SQLPOINTER szConnStrIn,
    SQLSMALLINT cbConnStrIn,
    SQLPOINTER szConnStrOut,
    SQLSMALLINT cbConnStrOutMax, SQLSMALLINT * pcbConnStrOut,
    SQLCHAR waMode)
{
  CONN (pdbc, hdbc);
  ENVR (penv, NULL);
  char buf[1024];
  SWORD thread_safe;
  SWORD unload_safe;
  HPROC hproc = SQL_NULL_HPROC;
  void * _ConnStrIn = NULL;
  void * _ConnStrOut = NULL;
  void * connStrOut = szConnStrOut;
  void * connStrIn = szConnStrIn;

  SQLRETURN retcode = SQL_SUCCESS;
  SQLRETURN setopterr = SQL_SUCCESS;

  /* check arguments */
  if ((cbConnStrIn < 0 && cbConnStrIn != SQL_NTS) || cbConnStrOutMax < 0)
    {
      PUSHSQLERR (pdbc->herr, en_S1090);
      return SQL_ERROR;
    }

  if (pdbc->state == en_dbc_allocated)
    {
        PCONFIG pconfig;
        void *drv = NULL, *dsn = NULL;

        if (_iodbcdm_cfg_init_str (&pconfig, szConnStrIn, cbConnStrIn,
			     waMode == 'W') == -1)
          {
            PUSHSQLERR (pdbc->herr, en_HY001);
            return SQL_ERROR;
          }
        if (_iodbcdm_cfg_find (pconfig, "ODBC", "DRIVER") == 0)
          drv = pconfig->value;
        if (_iodbcdm_cfg_find (pconfig, "ODBC", "DSN") == 0)
          dsn = pconfig->value;

        if (dsn == NULL || ((char*)dsn)[0] == '\0')
          dsn = (void *) "default";
        else
          /* if you want tracing, you must use a DSN */
          {
	    if (_iodbcdm_con_settracing (pdbc, (SQLCHAR *) dsn, SQL_NTS, waMode) == SQL_ERROR)
	      {
                _iodbcdm_cfg_done (pconfig);
	        return SQL_ERROR;
	      }
	  }

        /*
         *  Check whether driver is thread safe
         */
        thread_safe = 1;		/* Assume driver is thread safe */

        SQLSetConfigMode (ODBC_BOTH_DSN);
        if ( SQLGetPrivateProfileString ((char *) dsn, "ThreadManager", "", 
		buf, sizeof(buf), "odbc.ini") &&
            (STRCASEEQ (buf, "on") || STRCASEEQ (buf, "1")))
          {
            thread_safe = 0;	/* Driver needs a thread manager */
          }

        /*
         *  Check whether driver is unload safe
         */
        unload_safe = 0;		/* Assume driver is not unload safe */

        SQLSetConfigMode (ODBC_BOTH_DSN);
        if ( SQLGetPrivateProfileString ((char *) dsn, "ThreadManager", "", 
		buf, sizeof(buf), "odbc.ini") &&
            (STRCASEEQ (buf, "on") || STRCASEEQ (buf, "1")))
          {
            unload_safe = 1;
          }

        /*
         *  Get the name of the driver module and load it
         */
        if (drv == NULL || *(char*)drv == '\0')
          {
            SQLSetConfigMode (ODBC_BOTH_DSN);
            if ( SQLGetPrivateProfileString ((char *) dsn, "Driver", "", 
		buf, sizeof(buf), "odbc.ini") != 0)
              {
                drv = buf;
              }
          }

      if (drv == NULL)
	{
	  PUSHSQLERR (pdbc->herr, en_IM002);
          _iodbcdm_cfg_done (pconfig);
	  return SQL_ERROR;
	}

      retcode = _iodbcdm_driverload (dsn, (char *) drv, pdbc, thread_safe, unload_safe, waMode);
      _iodbcdm_cfg_done (pconfig);

      switch (retcode)
	{
	case SQL_SUCCESS:
	  break;

	case SQL_SUCCESS_WITH_INFO:
#if 0
	  /* 
	   *  Unsuccessful in calling driver's SQLSetConnectOption() to set 
	   *  login timeout.
	   */
	  setopterr = SQL_ERROR;
#endif
	  break;

	default:
          return retcode;
	}
    }
  else if (pdbc->state != en_dbc_needdata)
    {
      PUSHSQLERR (pdbc->herr, en_08002);
      return SQL_ERROR;
    }

  penv = (ENV_t *) pdbc->henv;

  if ((penv->unicode_driver && waMode != 'W')
      || (!penv->unicode_driver && waMode == 'W'))
    {
      if (waMode != 'W')
        {
        /* ansi=>unicode*/
          if ((_ConnStrOut = malloc((cbConnStrOutMax + 1) * sizeof(SQLWCHAR))) == NULL)
	    {
              PUSHSQLERR (pdbc->herr, en_HY001);
	      return SQL_ERROR;
            }
          _ConnStrIn = dm_SQL_A2W((SQLCHAR *)szConnStrIn, SQL_NTS);
        }
      else
        {
        /* unicode=>ansi*/
          if ((_ConnStrOut = malloc(cbConnStrOutMax + 1)) == NULL)
	    {
              PUSHSQLERR (pdbc->herr, en_HY001);
	      return SQL_ERROR;
            }
          _ConnStrIn = dm_SQL_W2A((SQLWCHAR *)szConnStrIn, SQL_NTS);
        }
      connStrIn = _ConnStrIn;
      cbConnStrIn = SQL_NTS;
      connStrOut = _ConnStrOut;
    }

  ODBC_UNLOCK ();
  CALL_UDRIVER(hdbc, pdbc, retcode, hproc, penv->unicode_driver,
    en_BrowseConnect, (
       pdbc->dhdbc,
       connStrIn,
       cbConnStrIn,
       connStrOut,
       cbConnStrOutMax,
       pcbConnStrOut));
  ODBC_LOCK ();

  MEM_FREE(_ConnStrIn);

  if (hproc == SQL_NULL_HPROC)
    {
      MEM_FREE(_ConnStrOut);
      _iodbcdm_driverunload (pdbc, 3);
      pdbc->state = en_dbc_allocated;
      PUSHSQLERR (pdbc->herr, en_IM001);
      return SQL_ERROR;
    }

  if (szConnStrOut
      && SQL_SUCCEEDED (retcode)
      && ((penv->unicode_driver && waMode != 'W')
          || (!penv->unicode_driver && waMode == 'W')))
    {
      if (waMode != 'W')
        {
        /* ansi<=unicode*/
          dm_StrCopyOut2_W2A ((SQLWCHAR *) connStrOut, (SQLCHAR *) szConnStrOut, cbConnStrOutMax, NULL);
        }
      else
        {
        /* unicode<=ansi*/
          dm_StrCopyOut2_A2W ((SQLCHAR *) connStrOut, (SQLWCHAR *) szConnStrOut, cbConnStrOutMax, NULL);
        }
    }

  MEM_FREE(_ConnStrOut);

  switch (retcode)
    {
    case SQL_SUCCESS:
    case SQL_SUCCESS_WITH_INFO:
      pdbc->state = en_dbc_connected;
      setopterr |= _iodbcdm_dbcdelayset (pdbc, waMode);
      if (setopterr != SQL_SUCCESS)
	{
	  retcode = SQL_SUCCESS_WITH_INFO;
	}
      break;

    case SQL_NEED_DATA:
      pdbc->state = en_dbc_needdata;
      break;

    case SQL_ERROR:
      pdbc->state = en_dbc_allocated;
      /* but the driver will not unloaded
       * to allow application retrieve err
       * message from driver
       */
      break;

    default:
      break;
    }

  return retcode;
}


SQLRETURN SQL_API
SQLBrowseConnect (SQLHDBC hdbc,
    SQLCHAR * szConnStrIn,
    SQLSMALLINT cbConnStrIn,
    SQLCHAR * szConnStrOut,
    SQLSMALLINT cbConnStrOutMax,
    SQLSMALLINT * pcbConnStrOut)
{
  ENTER_HDBC (hdbc, 1,
    trace_SQLBrowseConnect (TRACE_ENTER,
      	hdbc,
	szConnStrIn, cbConnStrIn,
	szConnStrOut, cbConnStrOutMax, pcbConnStrOut));

  retcode = SQLBrowseConnect_Internal (
  	hdbc,
	szConnStrIn, cbConnStrIn,
	szConnStrOut, cbConnStrOutMax, pcbConnStrOut,
	'A');

  LEAVE_HDBC (hdbc, 1,
    trace_SQLBrowseConnect (TRACE_LEAVE,
      	hdbc,
	szConnStrIn, cbConnStrIn,
	szConnStrOut, cbConnStrOutMax, pcbConnStrOut));
}


SQLRETURN SQL_API
SQLBrowseConnectA (SQLHDBC hdbc,
    SQLCHAR * szConnStrIn,
    SQLSMALLINT cbConnStrIn,
    SQLCHAR * szConnStrOut,
    SQLSMALLINT cbConnStrOutMax,
    SQLSMALLINT * pcbConnStrOut)
{
  ENTER_HDBC (hdbc, 1,
    trace_SQLBrowseConnect (TRACE_ENTER,
      	hdbc,
	szConnStrIn, cbConnStrIn,
	szConnStrOut, cbConnStrOutMax, pcbConnStrOut));

  retcode = SQLBrowseConnect_Internal (
  	hdbc,
	szConnStrIn, cbConnStrIn,
	szConnStrOut, cbConnStrOutMax, pcbConnStrOut,
	'A');

  LEAVE_HDBC (hdbc, 1,
    trace_SQLBrowseConnect (TRACE_LEAVE,
      	hdbc,
	szConnStrIn, cbConnStrIn,
	szConnStrOut, cbConnStrOutMax, pcbConnStrOut));
}


SQLRETURN SQL_API
SQLBrowseConnectW (SQLHDBC hdbc,
    SQLWCHAR * szConnStrIn,
    SQLSMALLINT cbConnStrIn,
    SQLWCHAR * szConnStrOut,
    SQLSMALLINT cbConnStrOutMax,
    SQLSMALLINT * pcbConnStrOut)
{
  ENTER_HDBC (hdbc, 1,
    trace_SQLBrowseConnectW (TRACE_ENTER,
      	hdbc,
	szConnStrIn, cbConnStrIn,
	szConnStrOut, cbConnStrOutMax, pcbConnStrOut));

  retcode = SQLBrowseConnect_Internal (
  	hdbc,
	szConnStrIn, cbConnStrIn,
	szConnStrOut, cbConnStrOutMax, pcbConnStrOut,
	'W');

  LEAVE_HDBC (hdbc, 1,
    trace_SQLBrowseConnectW (TRACE_LEAVE,
      	hdbc,
	szConnStrIn, cbConnStrIn,
	szConnStrOut, cbConnStrOutMax, pcbConnStrOut));
}


static SQLRETURN
SQLDisconnect_Internal (SQLHDBC hdbc)
{
  CONN (pdbc, hdbc);
#if (ODBCVER >= 0x300)
  GENV (genv, pdbc->genv);
#endif
  STMT (pstmt, NULL);

  /* check hdbc state */
  if (pdbc->state == en_dbc_allocated)
    {
      PUSHSQLERR (pdbc->herr, en_08003);
      return SQL_ERROR;
    }

  /* check stmt(s) state */
  for (pstmt = (STMT_t *) pdbc->hstmt;
      pstmt != NULL;
      pstmt = (STMT_t *) pstmt->next)
    {
      if (pstmt->state >= en_stmt_needdata
	  || pstmt->asyn_on != en_NullProc)
	/* In this case one need to call
	 * SQLCancel() first */
	{
          PUSHSQLERR (pdbc->herr, en_S1010);
	  return SQL_ERROR;
	}
    }

#if (ODBCVER >= 0x300)
  /*
   * Try to return the connected connection to the pool if
   * - connection was taken from the pool
   * - pooling is enabled and CPTimeout > 0
   */
  if ((pdbc->state == en_dbc_connected || pdbc->state == en_dbc_hstmt)
      && (pdbc->cp_pdbc != NULL ||
           (genv->connection_pooling != SQL_CP_OFF && pdbc->cp_timeout > 0)))
    {
      if (_iodbcdm_pool_put_conn (pdbc) == 0)
        {
          _iodbcdm_finish_disconnect (pdbc, FALSE);
          return SQL_SUCCESS;
        }
    }
#endif /* (ODBCVER >= 0x300) */

  return _iodbcdm_finish_disconnect (pdbc, TRUE);
}


SQLRETURN SQL_API
SQLDisconnect (SQLHDBC hdbc)
{
  ENTER_HDBC (hdbc, 1,
    trace_SQLDisconnect (TRACE_ENTER, hdbc));

  retcode = SQLDisconnect_Internal (hdbc);

  LEAVE_HDBC (hdbc, 1,
    trace_SQLDisconnect (TRACE_LEAVE, hdbc));
}


SQLRETURN SQL_API
SQLNativeSql_Internal (SQLHDBC hdbc,
    SQLPOINTER szSqlStrIn,
    SQLINTEGER cbSqlStrIn,
    SQLPOINTER szSqlStr,
    SQLINTEGER cbSqlStrMax,
    SQLINTEGER * pcbSqlStr,
    SQLCHAR waMode)
{
  CONN (pdbc, hdbc);
  ENVR (penv, pdbc->henv);
  sqlstcode_t sqlstat = en_00000;
  SQLRETURN retcode = SQL_SUCCESS;
  HPROC hproc = SQL_NULL_HPROC;
  void * _SqlStrIn = NULL;
  void * _SqlStr = NULL;
  void * sqlStr = szSqlStr;

  /* check argument */
  if (szSqlStrIn == NULL)
    {
      sqlstat = en_S1009;
    }
  else if (cbSqlStrIn < 0 && cbSqlStrIn != SQL_NTS)
    {
      sqlstat = en_S1090;
    }

  if (sqlstat != en_00000)
    {
      PUSHSQLERR (pdbc->herr, sqlstat);
      return SQL_ERROR;
    }

  /* check state */
  if (pdbc->state <= en_dbc_needdata)
    {
      PUSHSQLERR (pdbc->herr, en_08003);
      return SQL_ERROR;
    }

  if ((penv->unicode_driver && waMode != 'W')
      || (!penv->unicode_driver && waMode == 'W'))
    {
      if (waMode != 'W')
        {
        /* ansi=>unicode*/
          if ((_SqlStr = malloc(cbSqlStrMax * sizeof(SQLWCHAR) + 1)) == NULL)
	    {
              PUSHSQLERR (pdbc->herr, en_HY001);

	      return SQL_ERROR;
            }
          _SqlStrIn = dm_SQL_A2W((SQLCHAR *)szSqlStrIn, SQL_NTS);
        }
      else
        {
        /* unicode=>ansi*/
          if ((_SqlStr = malloc(cbSqlStrMax + 1)) == NULL)
	    {
              PUSHSQLERR (pdbc->herr, en_HY001);

	      return SQL_ERROR;
            }
          _SqlStrIn = dm_SQL_W2A((SQLWCHAR *)szSqlStrIn, SQL_NTS);
        }
      szSqlStrIn = _SqlStrIn;
      cbSqlStrIn = SQL_NTS;
      sqlStr = _SqlStr;
    }

  /* call driver */
  CALL_UDRIVER(hdbc, pdbc, retcode, hproc, penv->unicode_driver,
    en_NativeSql, (
       pdbc->dhdbc,
       szSqlStrIn,
       cbSqlStrIn,
       sqlStr,
       cbSqlStrMax,
       pcbSqlStr));

  MEM_FREE(_SqlStrIn);

  if (hproc == SQL_NULL_HPROC)
    {
      MEM_FREE(_SqlStr);
      PUSHSQLERR (pdbc->herr, en_IM001);

      return SQL_ERROR;
    }

  if (szSqlStr
      && SQL_SUCCEEDED (retcode)
      && ((penv->unicode_driver && waMode != 'W')
          || (!penv->unicode_driver && waMode == 'W')))
    {
      if (waMode != 'W')
        {
        /* ansi<=unicode*/
          dm_StrCopyOut2_W2A ((SQLWCHAR *) sqlStr, (SQLCHAR *) szSqlStr, cbSqlStrMax, NULL);
        }
      else
        {
        /* unicode<=ansi*/
          dm_StrCopyOut2_A2W ((SQLCHAR *) sqlStr, (SQLWCHAR *) szSqlStr, cbSqlStrMax, NULL);
        }
    }

  MEM_FREE(_SqlStr);

  return retcode;
}


SQLRETURN SQL_API
SQLNativeSql (
    SQLHDBC hdbc,
    SQLCHAR * szSqlStrIn,
    SQLINTEGER cbSqlStrIn,
    SQLCHAR * szSqlStr,
    SQLINTEGER cbSqlStrMax,
    SQLINTEGER * pcbSqlStr)
{
  ENTER_HDBC (hdbc, 0,
    trace_SQLNativeSql (TRACE_ENTER,
    	hdbc,
	szSqlStrIn, cbSqlStrIn,
	szSqlStr, cbSqlStrMax, pcbSqlStr));

  retcode = SQLNativeSql_Internal (
  	hdbc,
	szSqlStrIn, cbSqlStrIn,
	szSqlStr, cbSqlStrMax, pcbSqlStr,
	'A');

  LEAVE_HDBC (hdbc, 0,
    trace_SQLNativeSql (TRACE_LEAVE,
    	hdbc,
	szSqlStrIn, cbSqlStrIn,
	szSqlStr, cbSqlStrMax, pcbSqlStr));
}


SQLRETURN SQL_API
SQLNativeSqlA (
    SQLHDBC hdbc,
    SQLCHAR * szSqlStrIn,
    SQLINTEGER cbSqlStrIn,
    SQLCHAR * szSqlStr,
    SQLINTEGER cbSqlStrMax,
    SQLINTEGER * pcbSqlStr)
{
  ENTER_HDBC (hdbc, 0,
    trace_SQLNativeSql (TRACE_ENTER,
    	hdbc,
	szSqlStrIn, cbSqlStrIn,
	szSqlStr, cbSqlStrMax, pcbSqlStr));

  retcode = SQLNativeSql_Internal(
  	hdbc,
	szSqlStrIn, cbSqlStrIn,
	szSqlStr, cbSqlStrMax, pcbSqlStr,
	'A');

  LEAVE_HDBC (hdbc, 0,
    trace_SQLNativeSql (TRACE_LEAVE,
    	hdbc,
	szSqlStrIn, cbSqlStrIn,
	szSqlStr, cbSqlStrMax, pcbSqlStr));
}


SQLRETURN SQL_API
SQLNativeSqlW (
    SQLHDBC hdbc,
    SQLWCHAR * szSqlStrIn,
    SQLINTEGER cbSqlStrIn,
    SQLWCHAR * szSqlStr,
    SQLINTEGER cbSqlStrMax,
    SQLINTEGER * pcbSqlStr)
{
  ENTER_HDBC (hdbc, 0,
    trace_SQLNativeSqlW (TRACE_ENTER,
    	hdbc,
	szSqlStrIn, cbSqlStrIn,
	szSqlStr, cbSqlStrMax, pcbSqlStr));

  retcode = SQLNativeSql_Internal(
  	hdbc,
	szSqlStrIn, cbSqlStrIn,
	szSqlStr, cbSqlStrMax, pcbSqlStr,
	'W');

  LEAVE_HDBC (hdbc, 0,
    trace_SQLNativeSqlW (TRACE_LEAVE,
    	hdbc,
	szSqlStrIn, cbSqlStrIn,
	szSqlStr, cbSqlStrMax, pcbSqlStr));
}
