/*
 *  connect.c
 *
 *  $Id: connect.c,v 1.1.1.2 2002/04/30 00:40:22 miner Exp $
 *
 *  Connect (load) driver
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

#if defined(_MACX) && defined(GUI)
#include <Carbon/Carbon.h>
#endif /* _MACX */

extern char *_iodbcdm_getkeyvalbydsn ();
extern char *_iodbcdm_getkeyvalinstr ();
extern SQLRETURN _iodbcdm_driverunload ();

/*
 *  Identification strings
 */
static char sccsid[] = "@(#)iODBC driver manager " VERSION ", (LGPL)\n";

char *version = VERSION;
char *libname = "iODBC Driver Manager";


/* - Load driver share library( or increase its reference count 
 *   if it has already been loaded by another active connection)
 * - Call driver's SQLAllocEnv() (for the first reference only)
 * - Call driver's SQLAllocConnect()
 * - Call driver's SQLSetConnectOption() (set login time out)
 * - Increase the bookkeeping reference count
 */
static SQLRETURN
_iodbcdm_driverload (
    char FAR * path,
    HDBC hdbc,
    SWORD thread_safe)
{
  CONN (pdbc, hdbc);
  GENV_t FAR *genv;
  ENV_t FAR *penv = NULL;
  HDLL hdll;
  HPROC hproc;
  SQLRETURN retcode = SQL_SUCCESS;
  int sqlstat = en_00000;

  if (path == NULL || path[0] == '\0')
    {
      PUSHSQLERR (pdbc->herr, en_IM002);

      return SQL_ERROR;
    }

  if (!IS_VALID_HDBC (pdbc) || pdbc->genv == SQL_NULL_HENV)
    {
      return SQL_INVALID_HANDLE;
    }

  genv = (GENV_t FAR *) pdbc->genv;

  /* This will either load the driver dll or increase its reference count */
  hdll = _iodbcdm_dllopen ((char FAR *) path);

  if (hdll == SQL_NULL_HDLL)
    {
      PUSHSYSERR (pdbc->herr, _iodbcdm_dllerror ());
      PUSHSQLERR (pdbc->herr, en_IM003);
      return SQL_ERROR;
    }

  penv = (ENV_t FAR *) (pdbc->henv);

  if (penv != NULL)
    {
      if (penv->hdll != hdll)
	{
	  _iodbcdm_driverunload (hdbc);
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
      for (penv = (ENV_t FAR *) genv->henv;
	  penv != NULL;
	  penv = (ENV_t FAR *) penv->next)
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
	  penv = (ENV_t FAR *) MEM_ALLOC (sizeof (ENV_t));

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
     
	  /* call driver's SQLAllocHandle() or SQLAllocEnv() */

#if (ODBCVER >= 0x0300)
	  hproc = _iodbcdm_getproc (pdbc, en_AllocHandle);

	  if (hproc)
	    {
	      penv->dodbc_ver = SQL_OV_ODBC3;
	      CALL_DRIVER (hdbc, genv, retcode, hproc, en_AllocHandle,
		  (SQL_HANDLE_ENV, SQL_NULL_HANDLE, &(penv->dhenv)));
	      if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
		{		/* this is an ODBC3 driver - introduce the app's requested version  */
		  hproc = _iodbcdm_getproc (pdbc, en_SetEnvAttr);
		  if (hproc != SQL_NULL_HPROC)
		    {
		      CALL_DRIVER (hdbc, genv, retcode, hproc, en_SetStmtAttr,
			  (penv->dhenv, SQL_ATTR_ODBC_VERSION, genv->odbc_ver, 0));
		    }
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
		  CALL_DRIVER (hdbc, genv, retcode, hproc,
		      en_AllocEnv, (&(penv->dhenv)));
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
	  penv->next = (ENV_t FAR *) genv->henv;
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
	      CALL_DRIVER (hdbc, genv, retcode, hproc, en_AllocHandle,
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
		      en_AllocConnect, (penv->dhenv, &(pdbc->dhdbc)));
		}
	    }

	  if (retcode == SQL_ERROR)
	    {
	      sqlstat = en_IM005;
	    }

	  if (sqlstat != en_00000)
	    {
	      _iodbcdm_driverunload (hdbc);

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
      hproc = _iodbcdm_getproc (pdbc, en_SetConnectOption);

      if (hproc == SQL_NULL_HPROC)
	{
	  sqlstat = en_IM004;
	}
      else
	{
	  CALL_DRIVER (hdbc, pdbc, retcode, hproc,
	      en_SetConnectOption, (
		  pdbc->dhdbc,
		  SQL_LOGIN_TIMEOUT,
		  pdbc->login_timeout));

	  if (retcode == SQL_ERROR)
	    {
	      PUSHSQLERR (pdbc->herr, en_IM006);

	      return SQL_SUCCESS_WITH_INFO;
	    }
	}
    }

  /*
   *  Now set the driver specific options we saved earlier
   */
  if (pdbc->drvopt != NULL)
    {
      hproc = _iodbcdm_getproc (pdbc, en_SetConnectOption);

      if (hproc == SQL_NULL_HPROC)
	{
	  sqlstat = en_IM004;
	}
      else
	{
	  DRVOPT *popt;

	  for (popt = pdbc->drvopt; popt != NULL; popt = popt->next)
	    {
	      CALL_DRIVER (hdbc, pdbc, retcode, hproc,
		  en_SetConnectOption, (
		      pdbc->dhdbc,
		      popt->Option,
		      popt->Param));

	      if (retcode == SQL_ERROR)
		{
		  PUSHSQLERR (pdbc->herr, en_IM006);

		  return SQL_SUCCESS_WITH_INFO;
		}
	    }
	}
    }

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
_iodbcdm_driverunload (HDBC hdbc)
{
  CONN (pdbc, hdbc);
  ENV_t FAR *penv;
  ENV_t FAR *tpenv;
  GENV_t FAR *genv;
  HPROC hproc;
  SQLRETURN retcode = SQL_SUCCESS;

  if (!IS_VALID_HDBC (pdbc))
    {
      return SQL_INVALID_HANDLE;
    }

  /* no pointer check will be performed in this function */
  penv = (ENV_t FAR *) pdbc->henv;
  genv = (GENV_t FAR *) pdbc->genv;

  if (penv == NULL || penv->hdll == SQL_NULL_HDLL)
    {
      return SQL_SUCCESS;
    }

#if (ODBCVER >= 0x0300)
  hproc = _iodbcdm_getproc (pdbc, en_FreeHandle);

  if (hproc)
    {
      CALL_DRIVER (hdbc, pdbc, retcode, hproc, en_FreeHandle,
	  (SQL_HANDLE_DBC, pdbc->dhdbc));
    }
  else
#endif

    {
      hproc = _iodbcdm_getproc (pdbc, en_FreeConnect);

      if (hproc != SQL_NULL_HPROC)
	{
	  CALL_DRIVER (hdbc, pdbc, retcode, hproc,
	      en_FreeConnect, (pdbc->dhdbc));

	  pdbc->dhdbc = SQL_NULL_HDBC;
	}
    }

  penv->refcount--;

  if (!penv->refcount)
    /* no other connections still attaching with this driver */
    {

#if (ODBCVER >= 0x0300)
      hproc = _iodbcdm_getproc (pdbc, en_FreeHandle);

      if (hproc)
	{
	  CALL_DRIVER (hdbc, genv, retcode, hproc, en_FreeHandle,
	      (SQL_HANDLE_ENV, penv->dhenv));
	}
      else
#endif

	{
	  hproc = _iodbcdm_getproc (pdbc, en_FreeEnv);

	  if (hproc != SQL_NULL_HPROC)
	    {
	      CALL_DRIVER (hdbc, genv, retcode, hproc, en_FreeEnv,
		  (penv->dhenv));

	      penv->dhenv = SQL_NULL_HENV;
	    }
	}

      _iodbcdm_dllclose (penv->hdll);

      penv->hdll = SQL_NULL_HDLL;

      for (tpenv = (ENV_t FAR *) genv->henv;
	  tpenv != NULL;
	  tpenv = (ENV_t FAR *) penv->next)
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

  if (pdbc->current_qualifier != NULL)
    {
      MEM_FREE (pdbc->current_qualifier);
      pdbc->current_qualifier = NULL;
    }

  return SQL_SUCCESS;
}


static SQLRETURN
_iodbcdm_dbcdelayset (HDBC hdbc)
{
  CONN (pdbc, hdbc);
  ENV_t FAR *penv;
  HPROC hproc;
  SQLRETURN retcode = SQL_SUCCESS;
  SQLRETURN ret;

  penv = pdbc->henv;

  hproc = _iodbcdm_getproc (pdbc, en_SetConnectOption);

  if (hproc == SQL_NULL_HPROC)
    {
      PUSHSQLERR (pdbc->herr, en_IM006);

      return SQL_SUCCESS_WITH_INFO;
    }

  if (pdbc->access_mode != SQL_MODE_DEFAULT)
    {
      CALL_DRIVER (hdbc, pdbc, ret, hproc,
	  en_SetConnectOption, (
	      SQL_ACCESS_MODE,
	      pdbc->access_mode));

      retcode |= ret;
    }

  if (pdbc->autocommit != SQL_AUTOCOMMIT_DEFAULT)
    {
      CALL_DRIVER (hdbc, pdbc, ret, hproc,
	  en_SetConnectOption, (
	      pdbc->dhdbc,
	      SQL_AUTOCOMMIT,
	      pdbc->autocommit));

      retcode |= ret;
    }

  if (pdbc->current_qualifier != NULL)
    {
      CALL_DRIVER (hdbc, pdbc, ret, hproc,
	  en_SetConnectOption, (
	      pdbc->dhdbc,
	      SQL_CURRENT_QUALIFIER,
	      pdbc->current_qualifier));

      retcode |= ret;
    }

  if (pdbc->packet_size != 0UL)
    {
      CALL_DRIVER (hdbc, pdbc, ret, hproc,
	  en_SetConnectOption, (
	      pdbc->dhdbc,
	      SQL_PACKET_SIZE,
	      pdbc->packet_size));

      retcode |= ret;
    }

  if (pdbc->quiet_mode != (UDWORD) NULL)
    {
      CALL_DRIVER (hdbc, pdbc, ret, hproc,
	  en_SetConnectOption, (
	      pdbc->dhdbc,
	      SQL_QUIET_MODE,
	      pdbc->quiet_mode));

      retcode |= ret;
    }

  if (pdbc->txn_isolation != SQL_TXN_READ_UNCOMMITTED)
    {
      CALL_DRIVER (hdbc, pdbc, ret, hproc,
	  en_SetConnectOption, (
	      pdbc->dhdbc,
	      SQL_TXN_ISOLATION,
	      pdbc->txn_isolation));
    }

  /* check error code for driver's SQLSetConnectOption() call */
  if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
    {
      PUSHSQLERR (pdbc->herr, en_IM006);

      retcode = SQL_ERROR;
    }

  /* get cursor behavior on transaction commit or rollback */
  hproc = _iodbcdm_getproc (pdbc, en_GetInfo);

  if (hproc == SQL_NULL_HPROC)
    {
      PUSHSQLERR (pdbc->herr, en_01000);

      return retcode;
    }

  CALL_DRIVER (hdbc, pdbc, ret, hproc,
      en_GetInfo, (
	  pdbc->dhdbc,
	  SQL_CURSOR_COMMIT_BEHAVIOR,
	  (PTR) & (pdbc->cb_commit),
	  sizeof (pdbc->cb_commit),
	  NULL));

  retcode |= ret;

  CALL_DRIVER (hdbc, pdbc, ret, hproc,
      en_GetInfo, (
	  pdbc->dhdbc,
	  SQL_CURSOR_ROLLBACK_BEHAVIOR,
	  (PTR) & (pdbc->cb_rollback),
	  sizeof (pdbc->cb_rollback),
	  NULL));

  retcode |= ret;

  if (retcode != SQL_SUCCESS
      && retcode != SQL_SUCCESS_WITH_INFO)
    {
      return SQL_ERROR;
    }

  return retcode;
}


static SQLRETURN
_iodbcdm_settracing (HDBC hdbc, char *dsn, int dsnlen)
{
  char buf[256];
  char *ptr;
  SQLRETURN setopterr = SQL_SUCCESS;

  /* Get Driver's DLL path from specified or default dsn section */
  ptr = _iodbcdm_getkeyvalbydsn (dsn, dsnlen, "TraceFile",
      (char FAR *) buf, sizeof (buf));

  if (ptr == NULL || ptr[0] == '\0')
    {
      ptr = (char FAR *) (SQL_OPT_TRACE_FILE_DEFAULT);
    }

  setopterr |= _iodbcdm_SetConnectOption ((SQLHDBC) hdbc,
      SQL_OPT_TRACEFILE, (UDWORD) (ptr));

  ptr = _iodbcdm_getkeyvalbydsn (dsn, dsnlen, "Trace",
      (char FAR *) buf, sizeof (buf));

  if (ptr != NULL)
    {
      UDWORD opt = (UDWORD) (-1L);

      if (STREQ (ptr, "ON")
	  || STREQ (ptr, "On")
	  || STREQ (ptr, "on")
	  || STREQ (ptr, "1"))
	{
	  opt = SQL_OPT_TRACE_ON;
	}

      if (STREQ (ptr, "OFF")
	  || STREQ (ptr, "Off")
	  || STREQ (ptr, "off")
	  || STREQ (ptr, "0"))
	{
	  opt = SQL_OPT_TRACE_OFF;
	}

      if (opt != (UDWORD) (-1L))
	{
	  setopterr |= _iodbcdm_SetConnectOption ((SQLHDBC) hdbc,
	      SQL_OPT_TRACE, opt);
	}
    }

  return setopterr;
}


SQLRETURN SQL_API
SQLConnect (
    SQLHDBC hdbc,
    SQLCHAR FAR * szDSN,
    SQLSMALLINT cbDSN,
    SQLCHAR FAR * szUID,
    SQLSMALLINT cbUID,
    SQLCHAR FAR * szAuthStr,
    SQLSMALLINT cbAuthStr)
{
  CONN (pdbc, hdbc);
  SQLRETURN retcode = SQL_SUCCESS;
  SQLRETURN setopterr = SQL_SUCCESS;
  /* MS SDK Guide specifies driver path can't longer than 255. */
  char driver[1024] =
  {'\0'};
  char *ptr;
  HPROC hproc;
  SWORD thread_safe;
  char buf[100];

  ODBC_LOCK ();
  if (!IS_VALID_HDBC (pdbc))
    {
      ODBC_UNLOCK ();
      return SQL_INVALID_HANDLE;
    }
  CLEAR_ERRORS (pdbc);

  /* check arguments */
  if ((cbDSN < 0 && cbDSN != SQL_NTS)
      || (cbUID < 0 && cbUID != SQL_NTS)
      || (cbAuthStr < 0 && cbAuthStr != SQL_NTS)
      || (cbDSN > SQL_MAX_DSN_LENGTH))
    {
      PUSHSQLERR (pdbc->herr, en_S1090);
      ODBC_UNLOCK ();
      return SQL_ERROR;
    }

  if (szDSN == NULL || cbDSN == 0)
    {
      PUSHSQLERR (pdbc->herr, en_IM002);
      ODBC_UNLOCK ();
      return SQL_ERROR;
    }

  /* check state */
  if (pdbc->state != en_dbc_allocated)
    {
      PUSHSQLERR (pdbc->herr, en_08002);
      ODBC_UNLOCK ();
      return SQL_ERROR;
    }

  setopterr |= _iodbcdm_settracing (pdbc,
      (char *) szDSN, cbDSN);

  /*
   *  Check whether driver is thread safe
   */
  ptr = _iodbcdm_getkeyvalbydsn (szDSN, cbDSN, "ThreadManager",
      (char FAR *) buf, sizeof (buf));

  thread_safe = 1;		/* Assume driver is thread safe */
  if (ptr != NULL)
    {
      if (STREQ (ptr, "ON")
	  || STREQ (ptr, "On")
	  || STREQ (ptr, "on")
	  || STREQ (ptr, "1"))
	{
	  thread_safe = 0;	/* Driver needs a thread manager */
	}
    }

  /*
   *  Get the name of the driver module and load it
   */
  ptr = _iodbcdm_getkeyvalbydsn (szDSN, cbDSN, "Driver",
      (char FAR *) driver, sizeof (driver));

  if (ptr == NULL)
    /* No specified or default dsn section or
     * no driver specification in this dsn section */
    {
      PUSHSQLERR (pdbc->herr, en_IM002);
      ODBC_UNLOCK ();
      return SQL_ERROR;
    }

  retcode = _iodbcdm_driverload (driver, pdbc, thread_safe);

  switch (retcode)
    {
    case SQL_SUCCESS:
      break;

    case SQL_SUCCESS_WITH_INFO:
      setopterr = SQL_ERROR;
      /* unsuccessful in calling driver's 
       * SQLSetConnectOption() to set login
       * timeout.
       */
      break;

    default:
      ODBC_UNLOCK ();
      return retcode;
    }

  hproc = _iodbcdm_getproc (pdbc, en_Connect);

  if (hproc == SQL_NULL_HPROC)
    {
      _iodbcdm_driverunload (pdbc);
      PUSHSQLERR (pdbc->herr, en_IM001);
      ODBC_UNLOCK ();
      return SQL_ERROR;
    }

  CALL_DRIVER (hdbc, pdbc, retcode, hproc, en_Connect, (
	  pdbc->dhdbc,
	  szDSN, cbDSN,
	  szUID, cbUID,
	  szAuthStr, cbAuthStr));

  if (retcode != SQL_SUCCESS
      && retcode != SQL_SUCCESS_WITH_INFO)
    {
      /* not unload driver for retrieve error 
       * message from driver */
		/*********
		_iodbcdm_driverunload( hdbc );
		**********/

      ODBC_UNLOCK ();
      return retcode;
    }

  /* state transition */
  pdbc->state = en_dbc_connected;

  /* do delayed option setting */
  setopterr |= _iodbcdm_dbcdelayset (pdbc);

  if (setopterr != SQL_SUCCESS)
    {
      ODBC_UNLOCK ();
      return SQL_SUCCESS_WITH_INFO;
    }

  ODBC_UNLOCK ();
  return retcode;
}

#if defined(_MACX) && defined(GUI)
extern SQLRETURN SQL_API _iodbcdm_drvconn_dialbox (HWND hwnd,
    LPSTR szInOutConnStr, DWORD cbInOutConnStr, int FAR * sqlStat);
#endif

SQLRETURN SQL_API
SQLDriverConnect (
    SQLHDBC hdbc,
    SQLHWND hwnd,
    SQLCHAR FAR * szConnStrIn,
    SQLSMALLINT cbConnStrIn,
    SQLCHAR FAR * szConnStrOut,
    SQLSMALLINT cbConnStrOutMax,
    SQLSMALLINT FAR * pcbConnStrOut,
    SQLUSMALLINT fDriverCompletion)
{
  CONN (pdbc, hdbc);
  HDLL hdll;
  char FAR *drv = NULL;
  char drvbuf[1024];
  char FAR *dsn = NULL;
  char dsnbuf[SQL_MAX_DSN_LENGTH + 1];
  UCHAR cnstr2drv[1024];
  SWORD thread_safe;
  char buf[100];
  char *ptr;
  int i;

  HPROC hproc;
  HPROC dialproc;

  int sqlstat = en_00000;
  SQLRETURN retcode = SQL_SUCCESS;
  SQLRETURN setopterr = SQL_SUCCESS;

  ODBC_LOCK ();

  if (!IS_VALID_HDBC (pdbc))
    {
      ODBC_UNLOCK ();
      return SQL_INVALID_HANDLE;
    }
  CLEAR_ERRORS (pdbc);

  /* check arguments */
  if ((cbConnStrIn < 0 && cbConnStrIn != SQL_NTS) || cbConnStrOutMax < 0)
    {
      PUSHSQLERR (pdbc->herr, en_S1090);
      ODBC_UNLOCK ();
      return SQL_ERROR;
    }

  /* check state */
  if (pdbc->state != en_dbc_allocated)
    {
      PUSHSQLERR (pdbc->herr, en_08002);
      ODBC_UNLOCK ();
      return SQL_ERROR;
    }

  drv = _iodbcdm_getkeyvalinstr (szConnStrIn, cbConnStrIn,
      "DRIVER", drvbuf, sizeof (drvbuf));

  dsn = _iodbcdm_getkeyvalinstr (szConnStrIn, cbConnStrIn,
      "DSN", dsnbuf, sizeof (dsnbuf));

  switch (fDriverCompletion)
    {
    case SQL_DRIVER_NOPROMPT:
      break;

    case SQL_DRIVER_COMPLETE:
    case SQL_DRIVER_COMPLETE_REQUIRED:
      if (dsn != NULL || drv != NULL)
	{
	  break;
	}
      /* fall to next case */

    case SQL_DRIVER_PROMPT:
      /* 
       *  Get data source dialog box function from
       * current executable or the driver DLL 
       */
      if (dsn == NULL || dsn[0] == '\0')
	dsn = "default";

      if (drv == NULL || drv[0] == '\0')
	drv = _iodbcdm_getkeyvalbydsn (dsn, SQL_NTS, "Driver", drvbuf,
	    sizeof (drvbuf));

#ifndef _MACX
      if (!(hdll = _iodbcdm_dllopen ("libiodbcadm.so"))
	  || !(dialproc = _iodbcdm_dllproc (hdll, "iodbcdm_drvconn_dialbox")))
	{
	  sqlstat = en_IM008;
	  retcode = SQL_ERROR;
	  break;
	}
#endif

      if (cbConnStrOutMax < cbConnStrIn && cbConnStrIn != SQL_NTS)
	{
#if (ODBCVER>=0x3000)
	  sqlstat = en_HY092;
#else
	  sqlstat = en_S1000;
#endif
	  retcode = SQL_ERROR;
	  break;
	}

      STRNCPY (szConnStrOut, szConnStrIn, (cbConnStrOutMax != SQL_NTS) ?
	  cbConnStrOutMax : STRLEN (szConnStrIn) + 1);

      for (i = STRLEN (szConnStrOut); i; i--)
	if (szConnStrOut[i] == ';')
	  szConnStrOut[i] = 0;

#if defined(GUI)
#  if defined(_MACX)
      retcode = iodbcdm_drvconn_dialbox (
#  else
      retcode = dialproc (
#  endif
	  hwnd,			/* window or display handle */
	  szConnStrOut,		/* input/output dsn buf */
	  cbConnStrOutMax,	/* buf size */
	  &sqlstat);		/* error code */
#else	/* GUI */
      sqlstat = en_IM008;
      retcode = SQL_ERROR;
#endif	/* GUI */

      if (retcode != SQL_SUCCESS)
	{
	  break;
	}

      dsn = dsnbuf;

      if (dsn[0] == '\0')
	dsn = "default";

      if (STRLEN (szConnStrOut) > sizeof (cnstr2drv) - 1)
	{
	  sqlstat = en_S1001;	/* a lazy way to avoid using heap memory */
	  break;
	}

      STRNCPY (cnstr2drv, szConnStrOut, sizeof (cnstr2drv));
      szConnStrIn = cnstr2drv;
      break;

    default:
      sqlstat = en_S1110;
      break;
    }

  if (sqlstat != en_00000)
    {
      PUSHSQLERR (pdbc->herr, sqlstat);
      ODBC_UNLOCK ();
      return SQL_ERROR;
    }

  if (dsn == NULL || dsn[0] == '\0')
    {
      dsn = "default";
    }
  else
    /* if you want tracing, you must use a DSN */
    {
      setopterr |= _iodbcdm_settracing (pdbc, (char *) dsn, SQL_NTS);
    }

  /*
   *  Check whether driver is thread safe
   */
  ptr = _iodbcdm_getkeyvalbydsn (dsn, SQL_NTS, "ThreadManager",
      (char FAR *) buf, sizeof (buf));

  thread_safe = 1;		/* Assume driver is thread safe */
  if (ptr != NULL)
    {
      if (STREQ (ptr, "ON")
	  || STREQ (ptr, "On") || STREQ (ptr, "on") || STREQ (ptr, "1"))
	{
	  thread_safe = 0;	/* Driver needs a thread manager */
	}
    }

  /*
   *  Get the name of the driver module and load it
   */
  if (drv == NULL || drv[0] == '\0')
    {
      drv = _iodbcdm_getkeyvalbydsn (dsn, SQL_NTS, "Driver",
	  drvbuf, sizeof (drvbuf));
    }

  if (drv == NULL)
    {
      PUSHSQLERR (pdbc->herr, en_IM002);
      ODBC_UNLOCK ();
      return SQL_ERROR;
    }

  retcode = _iodbcdm_driverload (drv, pdbc, thread_safe);

  switch (retcode)
    {
    case SQL_SUCCESS:
      break;

    case SQL_SUCCESS_WITH_INFO:
      setopterr = SQL_ERROR;
      /* unsuccessful in calling driver's 
       * SQLSetConnectOption() to set login
       * timeout.
       */
      break;

    default:
      ODBC_UNLOCK ();
      return retcode;
    }

  hproc = _iodbcdm_getproc (pdbc, en_DriverConnect);

  if (hproc == SQL_NULL_HPROC)
    {
      _iodbcdm_driverunload (pdbc);
      PUSHSQLERR (pdbc->herr, en_IM001);
      ODBC_UNLOCK ();
      return SQL_ERROR;
    }

  if (cbConnStrIn == SQL_NTS)
    {
      cbConnStrIn = STRLEN (szConnStrIn);
    }

  CALL_DRIVER (hdbc, pdbc, retcode, hproc, en_DriverConnect, (pdbc->dhdbc,
	  hwnd, szConnStrIn, cbConnStrIn, szConnStrOut, cbConnStrOutMax,
	  pcbConnStrOut, fDriverCompletion));

  if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
    {
      /* don't unload driver here for retrieve 
       * error message from driver */
		/********
		_iodbcdm_driverunload( hdbc );
		*********/

      ODBC_UNLOCK ();
      return retcode;
    }

  /* state transition */
  pdbc->state = en_dbc_connected;

  /* do delayed option setting */
  setopterr |= _iodbcdm_dbcdelayset (pdbc);

  if (setopterr != SQL_SUCCESS)
    {
      ODBC_UNLOCK ();
      return SQL_SUCCESS_WITH_INFO;
    }

  ODBC_UNLOCK ();
  return retcode;
}


SQLRETURN SQL_API
SQLBrowseConnect (SQLHDBC hdbc,
    SQLCHAR FAR * szConnStrIn,
    SQLSMALLINT cbConnStrIn,
    SQLCHAR FAR * szConnStrOut,
    SQLSMALLINT cbConnStrOutMax, SQLSMALLINT FAR * pcbConnStrOut)
{
  CONN (pdbc, hdbc);
  char FAR *drv;
  char drvbuf[1024];
  char FAR *dsn;
  char dsnbuf[SQL_MAX_DSN_LENGTH + 1];
  SWORD thread_safe;
  char buf[100];
  char *ptr;

  HPROC hproc;

  SQLRETURN retcode = SQL_SUCCESS;
  SQLRETURN setopterr = SQL_SUCCESS;

  ODBC_LOCK ();
  if (!IS_VALID_HDBC (pdbc))
    {
      ODBC_UNLOCK ();
      return SQL_INVALID_HANDLE;
    }
  CLEAR_ERRORS (pdbc);

  /* check arguments */
  if ((cbConnStrIn < 0 && cbConnStrIn != SQL_NTS) || cbConnStrOutMax < 0)
    {
      PUSHSQLERR (pdbc->herr, en_S1090);
      ODBC_UNLOCK ();
      return SQL_ERROR;
    }

  if (pdbc->state == en_dbc_allocated)
    {
      drv = _iodbcdm_getkeyvalinstr (szConnStrIn, cbConnStrIn,
	  "DRIVER", drvbuf, sizeof (drvbuf));

      dsn = _iodbcdm_getkeyvalinstr (szConnStrIn, cbConnStrIn,
	  "DSN", dsnbuf, sizeof (dsnbuf));

      if (dsn == NULL || dsn[0] == '\0')
	{
	  dsn = "default";
	}
      else
	/* if you want tracing, you must use a DSN */
	{
	  setopterr |= _iodbcdm_settracing (pdbc, (char *) dsn, SQL_NTS);
	}

      /*
       *  Check whether driver is thread safe
       */
      ptr = _iodbcdm_getkeyvalbydsn (dsn, SQL_NTS, "ThreadManager",
	  (char FAR *) buf, sizeof (buf));

      thread_safe = 1;		/* Assume driver is thread safe */
      if (ptr != NULL)
	{
	  if (STREQ (ptr, "ON") || STREQ (ptr, "On") ||
	      STREQ (ptr, "on") || STREQ (ptr, "1"))
	    {
	      thread_safe = 0;	/* Driver needs a thread manager */
	    }
	}

      /* 
       *  Get the name of the driver module and load it
       */
      if (drv == NULL || drv[0] == '\0')
	{
	  drv = _iodbcdm_getkeyvalbydsn (dsn, SQL_NTS, "Driver",
	      drvbuf, sizeof (drvbuf));
	}

      if (drv == NULL)
	{
	  PUSHSQLERR (pdbc->herr, en_IM002);
	  ODBC_UNLOCK ();
	  return SQL_ERROR;
	}

      retcode = _iodbcdm_driverload (drv, pdbc, thread_safe);

      switch (retcode)
	{
	case SQL_SUCCESS:
	  break;

	case SQL_SUCCESS_WITH_INFO:
	  setopterr = SQL_ERROR;
	  /* unsuccessful in calling driver's 
	   * SQLSetConnectOption() to set login
	   * timeout.
	   */
	  break;

	default:
	  ODBC_UNLOCK ();
	  return retcode;
	}
    }
  else if (pdbc->state != en_dbc_needdata)
    {
      PUSHSQLERR (pdbc->herr, en_08002);
      ODBC_UNLOCK ();
      return SQL_ERROR;
    }

  hproc = _iodbcdm_getproc (pdbc, en_BrowseConnect);

  if (hproc == SQL_NULL_HPROC)
    {
      _iodbcdm_driverunload (pdbc);
      pdbc->state = en_dbc_allocated;
      PUSHSQLERR (pdbc->herr, en_IM001);
      ODBC_UNLOCK ();
      return SQL_ERROR;
    }

  CALL_DRIVER (hdbc, pdbc, retcode, hproc, en_BrowseConnect, (pdbc->dhdbc,
	  szConnStrIn, cbConnStrIn,
	  szConnStrOut, cbConnStrOutMax, pcbConnStrOut));

  switch (retcode)
    {
    case SQL_SUCCESS:
    case SQL_SUCCESS_WITH_INFO:
      pdbc->state = en_dbc_connected;
      setopterr |= _iodbcdm_dbcdelayset (pdbc);
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

  ODBC_UNLOCK ();
  return retcode;
}


SQLRETURN SQL_API
SQLDisconnect (SQLHDBC hdbc)
{
  CONN (pdbc, hdbc);
  STMT_t FAR *pstmt;
  SQLRETURN retcode;
  HPROC hproc;

  int sqlstat = en_00000;

  ODBC_LOCK ();

  if (!IS_VALID_HDBC (pdbc))
    {
      ODBC_UNLOCK ();
      return SQL_INVALID_HANDLE;
    }
  CLEAR_ERRORS (pdbc);

  /* check hdbc state */
  if (pdbc->state == en_dbc_allocated)
    {
      sqlstat = en_08003;
    }

  /* check stmt(s) state */
  for (pstmt = (STMT_t FAR *) pdbc->hstmt;
      pstmt != NULL && sqlstat == en_00000;
      pstmt = (STMT_t FAR *) pstmt->next)
    {
      if (pstmt->state >= en_stmt_needdata
	  || pstmt->asyn_on != en_NullProc)
	/* In this case one need to call 
	 * SQLCancel() first */
	{
	  sqlstat = en_S1010;
	}
    }

  if (sqlstat == en_00000)
    {
      hproc = _iodbcdm_getproc (pdbc, en_Disconnect);

      if (hproc == SQL_NULL_HPROC)
	{
	  sqlstat = en_IM001;
	}
    }

  if (sqlstat != en_00000)
    {
      PUSHSQLERR (pdbc->herr, sqlstat);
      ODBC_UNLOCK ();
      return SQL_ERROR;
    }

  CALL_DRIVER (hdbc, pdbc, retcode, hproc, en_Disconnect, (
	  pdbc->dhdbc));

  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    {
      /* diff from MS specs. We disallow
       * driver SQLDisconnect() return
       * SQL_SUCCESS_WITH_INFO and post
       * error message.
       */
      retcode = SQL_SUCCESS;
    }
  else
    {
      ODBC_UNLOCK ();
      return retcode;
    }

  /* free all statement handle(s) on this connection */
  for (; pdbc->hstmt;)
    {
      _iodbcdm_dropstmt (pdbc->hstmt);
    }

  /* state transition */
  if (retcode == SQL_SUCCESS)
    {
      pdbc->state = en_dbc_allocated;
    }

  ODBC_UNLOCK ();
  return retcode;
}


SQLRETURN SQL_API
SQLNativeSql (
    SQLHDBC hdbc,
    SQLCHAR FAR * szSqlStrIn,
    SQLINTEGER cbSqlStrIn,
    SQLCHAR FAR * szSqlStr,
    SQLINTEGER cbSqlStrMax,
    SQLINTEGER FAR * pcbSqlStr)
{
  CONN (pdbc, hdbc);
  HPROC hproc;
  int sqlstat = en_00000;
  SQLRETURN retcode;

  ENTER_HDBC (pdbc);

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
      LEAVE_HDBC (pdbc, SQL_ERROR);
    }

  /* check state */
  if (pdbc->state <= en_dbc_needdata)
    {
      PUSHSQLERR (pdbc->herr, en_08003);
      LEAVE_HDBC (pdbc, SQL_ERROR);
    }

  /* call driver */
  hproc = _iodbcdm_getproc (pdbc, en_NativeSql);

  if (hproc == SQL_NULL_HPROC)
    {
      PUSHSQLERR (pdbc->herr, en_IM001);
      LEAVE_HDBC (pdbc, SQL_ERROR);
    }

  CALL_DRIVER (hdbc, pdbc, retcode, hproc, en_NativeSql,
      (pdbc->dhdbc, szSqlStrIn, cbSqlStrIn, szSqlStr, cbSqlStrMax, pcbSqlStr));

  LEAVE_HDBC (pdbc, retcode);
}
