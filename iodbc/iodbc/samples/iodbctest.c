/*
 *  iodbctest.c
 *
 *  $Id: iodbctest.c,v 1.26 2007/09/04 06:20:47 source Exp $
 *
 *  Sample ODBC program
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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>

#include <sql.h>
#include <sqlext.h>
#include <sqlucode.h>
#include <iodbcext.h>

/*
 *  Prototypes
 */
int ODBC_Connect (char *connStr);
int ODBC_Disconnect (void);
int ODBC_Errors (char *where);
int ODBC_Test (void);

#define MAXCOLS		32


#ifdef UNICODE

#define TEXT(x)   	(SQLWCHAR *) L##x
#define TEXTC(x)   	(SQLWCHAR) L##x
#define TXTLEN(x) 	wcslen((wchar_t *) x)
#define TXTCMP(x1,x2) 	wcscmp((wchar_t *) x1, (wchar_t *) x2)

# ifdef WIN32
#define OPL_A2W(a, w, cb)     \
	MultiByteToWideChar(CP_ACP, 0, a, -1, w, cb)
# else
#define OPL_A2W(XA, XW, SIZE)      mbstowcs(XW, XA, SIZE)
# endif /* WIN32 */

#else

#define TEXT(x)   	(SQLCHAR *) x
#define TEXTC(x)	(SQLCHAR) x
#define TXTLEN(x) 	strlen((char *) x)
#define TXTCMP(x1,x2) 	strcmp((char *) x1, (char *) x2)

#endif /* UNICODE */

#define NUMTCHAR(X)	(sizeof (X) / sizeof (SQLTCHAR))


/*
 *  Global variables
 */
HENV henv = SQL_NULL_HANDLE;
HDBC hdbc = SQL_NULL_HANDLE;
HSTMT hstmt = SQL_NULL_HANDLE;

int connected = 0;


/*
 *  Unicode conversion routines
 */
#ifdef UNICODE
static SQLWCHAR *
strcpy_A2W (SQLWCHAR * destStr, char *sourStr)
{
  size_t length;

  if (!sourStr || !destStr)
    return destStr;

  length = strlen (sourStr);
  if (length > 0)
    OPL_A2W (sourStr, destStr, length);
  destStr[length] = L'\0';

  return destStr;
}
#endif



/*
 *  Connect to the datasource
 *
 *  The connect string can have the following parts and they refer to
 *  the values in the odbc.ini file
 *
 *	DSN=<data source name>		[mandatory]
 *	HOST=<server host name>		[optional - value of Host]
 *	SVT=<database server type>	[optional - value of ServerType]
 *	DATABASE=<database path>	[optional - value of Database]
 *	OPTIONS=<db specific opts>	[optional - value of Options]
 *	UID=<user name>			[optional - value of LastUser]
 *	PWD=<password>			[optional]
 *	READONLY=<N|Y>			[optional - value of ReadOnly]
 *	FBS=<fetch buffer size>		[optional - value of FetchBufferSize]
 *
 *   Examples:
 *
 *	HOST=star;SVT=SQLServer 2000;UID=demo;PWD=demo;DATABASE=pubs
 *
 *	DSN=pubs_sqlserver;PWD=demo
 */

SQLTCHAR outdsn[4096];			/* Store completed DSN for later use */

int
ODBC_Connect (char *connStr)
{
  short buflen;
  SQLCHAR dataSource[1024];
  SQLTCHAR dsn[33];
  SQLTCHAR desc[255];
  SQLTCHAR driverInfo[255];
  SQLSMALLINT len1, len2;
  int status;
#ifdef UNICODE
  SQLWCHAR wdataSource[1024];
#endif

#if (ODBCVER < 0x0300)
  if (SQLAllocEnv (&henv) != SQL_SUCCESS)
    return -1;

  if (SQLAllocConnect (henv, &hdbc) != SQL_SUCCESS)
    return -1;
#else
  if (SQLAllocHandle (SQL_HANDLE_ENV, NULL, &henv) != SQL_SUCCESS)
    return -1;

  SQLSetEnvAttr (henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3,
      SQL_IS_UINTEGER);

  if (SQLAllocHandle (SQL_HANDLE_DBC, henv, &hdbc) != SQL_SUCCESS)
    return -1;
#endif


  /*
   *  Set the application name
   */
  SQLSetConnectOption (hdbc, SQL_APPLICATION_NAME,
	(SQLULEN) TEXT ("odbctest"));


  /*
   *  Show the version number of the driver manager
   */
  status = SQLGetInfo (hdbc, SQL_DM_VER,
      driverInfo, sizeof (driverInfo), &len1);
  if (status == SQL_SUCCESS)
    {
#ifdef UNICODE
      printf ("Driver Manager: %S\n", driverInfo);
#else
      printf ("Driver Manager: %s\n", driverInfo);
#endif
    }


  /*
   *  Either use the connect string provided on the command line or
   *  ask for one. If an empty string or a ? is given, show a nice
   *  list of options
   */
  if (connStr && *connStr)
    strcpy ((char *) dataSource, connStr);
  else
    while (1)
      {
	/*
	 *  Ask for the connect string
	 */
	printf ("\nEnter ODBC connect string (? shows list): ");
	if (fgets ((char *) dataSource, sizeof (dataSource), stdin) == NULL)
	  return 1;

	/*
	 *  Remove trailing '\n'
	 */
	dataSource[strlen ((char *) dataSource) - 1] = '\0';

	/*
	 * Check if the user wants to quit
	 */
	if (!strcmp ((char *)dataSource, "quit") || !strcmp ((char *)dataSource, "exit"))
	  return -1;

	/*
	 *  If the user entered something other than a ?
	 *  break out of the while loop
	 */
	if (*dataSource && *dataSource != '?')
	  break;


	/*
	 *  Print headers
	 */
	fprintf (stderr, "\n%-32s | %-40s\n", "DSN", "Driver");
	fprintf (stderr,
	    "------------------------------------------------------------------------------\n");

	/*
	 *  Goto the first record
	 */
	if (SQLDataSources (henv, SQL_FETCH_FIRST,
		dsn, NUMTCHAR (dsn), &len1,
		desc, NUMTCHAR (desc), &len2) != SQL_SUCCESS)
	  continue;

	/*
	 *  Show all records
	 */
	do
	  {
#ifdef UNICODE
	    fprintf (stderr, "%-32S | %-40S\n", dsn, desc);
#else
	    fprintf (stderr, "%-32s | %-40s\n", dsn, desc);
#endif
	  }
	while (SQLDataSources (henv, SQL_FETCH_NEXT,
		dsn, NUMTCHAR (dsn), &len1,
		desc, NUMTCHAR (desc), &len2) == SQL_SUCCESS);
      }

#ifdef UNICODE
  strcpy_A2W (wdataSource, (char *) dataSource);
  status = SQLDriverConnectW (hdbc, 0, (SQLWCHAR *) wdataSource, SQL_NTS,
      (SQLWCHAR *) outdsn, NUMTCHAR (outdsn), &buflen, SQL_DRIVER_COMPLETE);
  if (status != SQL_SUCCESS)
    ODBC_Errors ("SQLDriverConnectW");
#else
  status = SQLDriverConnect (hdbc, 0, (SQLCHAR *) dataSource, SQL_NTS,
      (SQLCHAR *) outdsn, NUMTCHAR (outdsn), &buflen, SQL_DRIVER_COMPLETE);
  if (status != SQL_SUCCESS)
    ODBC_Errors ("SQLDriverConnect");
#endif

  if (status != SQL_SUCCESS && status != SQL_SUCCESS_WITH_INFO)
    return -1;

  connected = 1;


  /*
   *  Print out the version number and the name of the connected driver
   */
  status = SQLGetInfo (hdbc, SQL_DRIVER_VER,
      driverInfo, NUMTCHAR (driverInfo), &len1);
  if (status == SQL_SUCCESS)
    {
#ifdef UNICODE
      printf ("Driver: %S", driverInfo);
#else
      printf ("Driver: %s", driverInfo);
#endif

      status = SQLGetInfo (hdbc, SQL_DRIVER_NAME,
	  driverInfo, NUMTCHAR (driverInfo), &len1);
      if (status == SQL_SUCCESS)
	{
#ifdef UNICODE
	  printf (" (%S)", driverInfo);
#else
	  printf (" (%s)", driverInfo);
#endif
	}
      printf ("\n");
    }


  /*
   *  Show the list of supported functions in trace log
   */
#if (ODBCVER < 0x0300)
  {
     SQLUSMALLINT exists[100];

     SQLGetFunctions (hdbc, SQL_API_ALL_FUNCTIONS, exists);
  }
#else
  {
     SQLUSMALLINT exists[SQL_API_ODBC3_ALL_FUNCTIONS_SIZE];

     SQLGetFunctions (hdbc, SQL_API_ODBC3_ALL_FUNCTIONS, exists);
  }
#endif



  /*
   *  Allocate statement handle
   */
#if (ODBCVER < 0x0300)
  if (SQLAllocStmt (hdbc, &hstmt) != SQL_SUCCESS)
    return -1;
#else
  if (SQLAllocHandle (SQL_HANDLE_STMT, hdbc, &hstmt) != SQL_SUCCESS)
    return -1;
#endif

  return 0;
}


/*
 *  Disconnect from the database
 */
int
ODBC_Disconnect (void)
{
#if (ODBCVER < 0x0300)
  if (hstmt)
    SQLFreeStmt (hstmt, SQL_DROP);

  if (connected)
    SQLDisconnect (hdbc);

  if (hdbc)
    SQLFreeConnect (hdbc);

  if (henv)
    SQLFreeEnv (henv);
#else
  if (hstmt)
    {
      SQLCloseCursor (hstmt);
      SQLFreeHandle (SQL_HANDLE_STMT, hstmt);
    }

  if (connected)
    SQLDisconnect (hdbc);

  if (hdbc)
    SQLFreeHandle (SQL_HANDLE_DBC, hdbc);

  if (henv)
    SQLFreeHandle (SQL_HANDLE_ENV, henv);
#endif

  return 0;
}


/*
 *  Perform a disconnect/reconnect using the DSN stored from the original
 *  SQLDriverConnect
 */
int 
ODBC_Reconnect (void)
{
  SQLRETURN status;
  SQLTCHAR buf[4096];
  SQLSMALLINT len;

  /*
   *  Free old statement handle
   */
#if (ODBCVER < 0x0300)
  SQLFreeStmt (hstmt, SQL_DROP);
#else
  SQLFreeHandle (SQL_HANDLE_STMT, hstmt);
#endif

  /*
   *  Disconnect
   */
  SQLDisconnect (hdbc);

  /*
   *  Reconnect
   */
  status = SQLDriverConnect (hdbc, 0, outdsn, SQL_NTS,
      buf, sizeof (buf), &len, SQL_DRIVER_NOPROMPT);

  /*
   *  Allocate new statement handle
   */
  if (SQL_SUCCEEDED (status))
    {
#if (ODBCVER < 0x0300)
      status = SQLAllocStmt (hdbc, &hstmt);
#else
      status = SQLAllocHandle (SQL_HANDLE_STMT, hdbc, &hstmt);
#endif
    }

  /*
   *  Show why we where unsuccessful and return an error
   */
  if (!SQL_SUCCEEDED (status))
    {
      ODBC_Errors ("DriverConnect (reconnect)");
      return -1;
    }

  /*
   *  Success
   */
  return 0;
}


/*
 *  Show all the error information that is available
 */
int
ODBC_Errors (char *where)
{
  SQLTCHAR buf[512];
  SQLTCHAR sqlstate[15];
  SQLINTEGER native_error = 0;
  int force_exit = 0;
  SQLRETURN sts;

#if (ODBCVER < 0x0300)
  /*
   *  Get statement errors
   */
  while (hstmt)
    {
      sts = SQLError (henv, hdbc, hstmt, sqlstate, &native_error,
	  buf, NUMTCHAR (buf), NULL);
      if (!SQL_SUCCEEDED (sts))
	break;

#ifdef UNICODE
      fprintf (stderr, "%s = %S (%ld) SQLSTATE=%S\n",
	  where, buf, (long) native_error, sqlstate);
#else
      fprintf (stderr, "%s = %s (%ld) SQLSTATE=%s\n",
	  where, buf, (long) native_error, sqlstate);
#endif

      /*
       *  If the driver could not be loaded, there is no point in
       *  continuing, after reading all the error messages
       */
      if (!TXTCMP (sqlstate, TEXT ("IM003")))
	force_exit = 1;
    }

  /*
   *  Get connection errors
   */
  while (hdbc)
    {
      sts = SQLError (henv, hdbc, SQL_NULL_HSTMT, sqlstate, &native_error,
	  buf, NUMTCHAR (buf), NULL);
      if (!SQL_SUCCEEDED (sts))
	break;

#ifdef UNICODE
      fprintf (stderr, "%s = %S (%ld) SQLSTATE=%S\n",
	  where, buf, (long) native_error, sqlstate);
#else
      fprintf (stderr, "%s = %s (%ld) SQLSTATE=%s\n",
	  where, buf, (long) native_error, sqlstate);
#endif

      /*
       *  If the driver could not be loaded, there is no point in
       *  continuing, after reading all the error messages
       */
      if (!TXTCMP (sqlstate, TEXT ("IM003")))
	force_exit = 1;
    }

  /*
   *  Get environment errors
   */
  while (henv)
    {
      sts SQLError (henv, SQL_NULL_HDBC, SQL_NULL_HSTMT, sqlstate,
	  &native_error, buf, NUMTCHAR (buf), NULL);
      if (!SQL_SUCCEEDED (sts))
	break;

#ifdef UNICODE
      fprintf (stderr, "%s = %S (%ld) SQLSTATE=%S\n",
	  where, buf, (long) native_error, sqlstate);
#else
      fprintf (stderr, "%s = %s (%ld) SQLSTATE=%s\n",
	  where, buf, (long) native_error, sqlstate);
#endif

      /*
       *  If the driver could not be loaded, there is no point in
       *  continuing, after reading all the error messages
       */
      if (!TXTCMP (sqlstate, TEXT ("IM003")))
	force_exit = 1;
    }
#else /* ODBCVER */
  int i;

  /*
   *  Get statement errors
   */
  i = 0;
  while (hstmt && i < 5)
    {
      sts = SQLGetDiagRec (SQL_HANDLE_STMT, hstmt, ++i,
	  sqlstate, &native_error, buf, NUMTCHAR (buf), NULL);
      if (!SQL_SUCCEEDED (sts))
	break;

#ifdef UNICODE
      fprintf (stderr, "%d: %s = %S (%ld) SQLSTATE=%S\n",
	  i, where, buf, (long) native_error, sqlstate);
#else
      fprintf (stderr, "%d: %s = %s (%ld) SQLSTATE=%s\n",
	  i, where, buf, (long) native_error, sqlstate);
#endif

      /*
       *  If the driver could not be loaded, there is no point in
       *  continuing, after reading all the error messages
       */
      if (!TXTCMP (sqlstate, TEXT ("IM003")))
	force_exit = 1;
    }

  /*
   *  Get connection errors
   */
  i = 0;
  while (hdbc && i < 5)
    {
      sts = SQLGetDiagRec (SQL_HANDLE_DBC, hdbc, ++i,
	  sqlstate, &native_error, buf, NUMTCHAR (buf), NULL);
      if (!SQL_SUCCEEDED (sts))
	break;

#ifdef UNICODE
      fprintf (stderr, "%d: %s = %S (%ld) SQLSTATE=%S\n",
	  i, where, buf, (long) native_error, sqlstate);
#else
      fprintf (stderr, "%d: %s = %s (%ld) SQLSTATE=%s\n",
	  i, where, buf, (long) native_error, sqlstate);
#endif

      /*
       *  If the driver could not be loaded, there is no point in
       *  continuing, after reading all the error messages
       */
      if (!TXTCMP (sqlstate, TEXT ("IM003")))
	force_exit = 1;
    }

  /*
   *  Get environment errors
   */
  i = 0;
  while (henv && i < 5)
    {
      sts = SQLGetDiagRec (SQL_HANDLE_ENV, henv, ++i,
	  sqlstate, &native_error, buf, NUMTCHAR (buf), NULL);
      if (!SQL_SUCCEEDED (sts))
	break;

#ifdef UNICODE
      fprintf (stderr, "%d: %s = %S (%ld) SQLSTATE=%S\n",
	  i, where, buf, (long) native_error, sqlstate);
#else
      fprintf (stderr, "%d: %s = %s (%ld) SQLSTATE=%s\n",
	  i, where, buf, (long) native_error, sqlstate);
#endif

      /*
       *  If the driver could not be loaded, there is no point in
       *  continuing, after reading all the error messages
       */
      if (!TXTCMP (sqlstate, TEXT ("IM003")))
	force_exit = 1;
    }
#endif /* ODBCVER */

  /*
   *  Force an exit status
   */
  if (force_exit)
    exit (-1);

  return -1;
}


/*
 *  Test program to run on the connected database
 */
int
ODBC_Test ()
{
  SQLTCHAR request[4096];
  SQLTCHAR fetchBuffer[1024];
  char buf[4096];
  size_t displayWidths[MAXCOLS];
  size_t displayWidth;
  short numCols;
  short colNum;
  SQLTCHAR colName[50];
  SQLSMALLINT colType;
  SQLULEN colPrecision;
  SQLLEN colIndicator;
  SQLSMALLINT colScale;
  SQLSMALLINT colNullable;
  unsigned long totalRows;
  unsigned long totalSets;
  int i;
  SQLRETURN sts;

  while (1)
    {
      /*
       *  Ask the user for a dynamic SQL statement
       */
      printf ("\nSQL>");
      if (fgets (buf, sizeof (buf), stdin) == NULL)
	break;

#ifndef UNICODE
      strcpy ((char *) request, (char *) buf);
#else
      strcpy_A2W (request, buf);
#endif

      request[TXTLEN (request) - 1] = TEXTC ('\0');

      if (request[0] == TEXTC ('\0'))
	continue;

      /*
       *  If the user just types tables, give him a list
       */
      if (!TXTCMP (request, TEXT ("tables")))
	{
	  if (SQLTables (hstmt, NULL, 0, NULL, 0, NULL, 0,
		  NULL, 0) != SQL_SUCCESS)
	    {
	      ODBC_Errors ("SQLTables(tables)");
	      continue;
	    }
	}
      /*
       *  If the user just types qualifiers, give him a list
       */
      else if (!TXTCMP (request, TEXT ("qualifiers")))
	{
	  if (SQLTables (hstmt, TEXT ("%"), SQL_NTS, TEXT (""), 0,
		  TEXT (""), 0, TEXT (""), 0) != SQL_SUCCESS)
	    {
	      ODBC_Errors ("SQLTables(qualifiers)");
	      continue;
	    }
	}
      /*
       *  If the user just types owners, give him a list
       */
      else if (!TXTCMP (request, TEXT ("owners")))
	{
	  if (SQLTables (hstmt, TEXT (""), 0, TEXT ("%"), SQL_NTS,
		  TEXT (""), 0, TEXT (""), 0) != SQL_SUCCESS)
	    {
	      ODBC_Errors ("SQLTables(owners)");
	      continue;
	    }
	}
      /*
       *  If the user just types "types", give him a list
       */
      else if (!TXTCMP (request, TEXT ("types")))
	{
	  if (SQLTables (hstmt, TEXT (""), 0, TEXT (""), 0,
		  TEXT (""), 0, TEXT ("%"), SQL_NTS) != SQL_SUCCESS)
	    {
	      ODBC_Errors ("SQLTables(types)");
	      continue;
	    }
	}
      /*
       *  If the user just types "datatypes", give him a list
       */
      else if (!TXTCMP (request, TEXT ("datatypes")))
	{
	  if (SQLGetTypeInfo (hstmt, 0) != SQL_SUCCESS)
	    {
	      ODBC_Errors ("SQLGetTypeInfo");
	      continue;
	    }
	}
      else if (!TXTCMP (request, TEXT ("reconnect")))
	{
  	  if (ODBC_Reconnect())
	    return -1;

	  continue;
	}
#if defined (unix)
      else if (!TXTCMP (request, TEXT ("environment")))
	{
	  extern char **environ;
	  int i;

	  for (i = 0; environ[i]; i++)
	    fprintf (stderr, "%03d: [%s]\n", i, environ[i]);

	  continue;
	}
#endif
      else if (!TXTCMP (request, TEXT ("quit"))
	  || !TXTCMP (request, TEXT ("exit")))
	break;			/* If you want to quit, just say so */
      else
	{
	  /*
	   *  Prepare & Execute the statement
	   */
	  if (SQLPrepare (hstmt, (SQLTCHAR *) request,
		  SQL_NTS) != SQL_SUCCESS)
	    {
	      ODBC_Errors ("SQLPrepare");
	      continue;
	    }
	  if ((sts = SQLExecute (hstmt)) != SQL_SUCCESS)
	    {
	      ODBC_Errors ("SQLExec");

	      if (sts != SQL_SUCCESS_WITH_INFO)
		continue;
	    }
	}

      /*
       *  Loop through all the result sets
       */
      totalSets = 1;
      do
	{
	  /*
	   *  Get the number of result columns for this cursor.
	   *  If it is 0, then the statement was probably a select
	   */
	  if (SQLNumResultCols (hstmt, &numCols) != SQL_SUCCESS)
	    {
	      ODBC_Errors ("SQLNumResultCols");
	      goto endCursor;
	    }
	  if (numCols == 0)
	    {
	      SQLLEN nrows = 0;

	      SQLRowCount (hstmt, &nrows);
	      printf ("Statement executed. %ld rows affected.\n",
		  nrows > 0 ? (long) nrows : 0L);
	      goto endCursor;
	    }

	  if (numCols > MAXCOLS)
	    {
	      numCols = MAXCOLS;
	      fprintf (stderr,
		  "NOTE: Resultset truncated to %d columns.\n", MAXCOLS);
	    }

	  /*
	   *  Get the names for the columns
	   */
	  putchar ('\n');
	  for (colNum = 1; colNum <= numCols; colNum++)
	    {
	      /*
	       *  Get the name and other type information
	       */
	      if (SQLDescribeCol (hstmt, colNum,
		      (SQLTCHAR *) colName, NUMTCHAR (colName), NULL,
		      &colType, &colPrecision, &colScale,
		      &colNullable) != SQL_SUCCESS)
		{
		  ODBC_Errors ("SQLDescribeCol");
		  goto endCursor;
		}

	      /*
	       *  Calculate the display width for the column
	       */
	      switch (colType)
		{
		case SQL_VARCHAR:
		case SQL_CHAR:
		case SQL_WVARCHAR:
		case SQL_WCHAR:
		case SQL_GUID:
		  displayWidth = colPrecision;
		  break;

		case SQL_BINARY:
		  displayWidth = colPrecision * 2;
		  break;

		case SQL_LONGVARCHAR:
		case SQL_WLONGVARCHAR:
		case SQL_LONGVARBINARY:
		  displayWidth = 30;	/* show only first 30 */
		  break;

		case SQL_BIT:
		  displayWidth = 1;
		  break;

		case SQL_TINYINT:
		case SQL_SMALLINT:
		case SQL_INTEGER:
		case SQL_BIGINT:
		  displayWidth = colPrecision + 1;	/* sign */
		  break;

		case SQL_DOUBLE:
		case SQL_DECIMAL:
		case SQL_NUMERIC:
		case SQL_FLOAT:
		case SQL_REAL:
		  displayWidth = colPrecision + 2;	/* sign, comma */
		  break;

#ifdef SQL_TYPE_DATE
		case SQL_TYPE_DATE:
#endif
		case SQL_DATE:
		  displayWidth = 10;
		  break;

#ifdef SQL_TYPE_TIME
		case SQL_TYPE_TIME:
#endif
		case SQL_TIME:
		  displayWidth = 8;
		  break;

#ifdef SQL_TYPE_TIMESTAMP
		case SQL_TYPE_TIMESTAMP:
#endif
		case SQL_TIMESTAMP:
		  displayWidth = 19;
		  if (colScale > 0)
		    displayWidth = displayWidth + colScale + 1;
		  break;

		default:
		  displayWidths[colNum - 1] = 0;	/* skip other data types */
		  continue;
		}

	      if (displayWidth < TXTLEN (colName))
		displayWidth = TXTLEN (colName);
	      if (displayWidth > NUMTCHAR (fetchBuffer) - 1)
		displayWidth = NUMTCHAR (fetchBuffer) - 1;

	      displayWidths[colNum - 1] = displayWidth;

	      /*
	       *  Print header field
	       */
#ifdef UNICODE
	      printf ("%-*.*S", displayWidth, displayWidth, colName);
#else
	      printf ("%-*.*s", displayWidth, displayWidth, colName);
#endif
	      if (colNum < numCols)
		putchar ('|');
	    }
	  putchar ('\n');

	  /*
	   *  Print second line
	   */
	  for (colNum = 1; colNum <= numCols; colNum++)
	    {
	      for (i = 0; i < displayWidths[colNum - 1]; i++)
		putchar ('-');
	      if (colNum < numCols)
		putchar ('+');
	    }
	  putchar ('\n');

	  /*
	   *  Print all the fields
	   */
	  totalRows = 0;
	  while (1)
	    {
#if (ODBCVER < 0x0300)
	      int sts = SQLFetch (hstmt);
#else
	      int sts = SQLFetchScroll (hstmt, SQL_FETCH_NEXT, 1);
#endif

	      if (sts == SQL_NO_DATA_FOUND)
		break;

	      if (sts != SQL_SUCCESS)
		{
		  ODBC_Errors ("Fetch");
		  break;
		}
	      for (colNum = 1; colNum <= numCols; colNum++)
		{
		  /*
		   *  Fetch this column as character
		   */
#ifdef UNICODE
		  sts = SQLGetData (hstmt, colNum, SQL_C_WCHAR, fetchBuffer,
		      NUMTCHAR (fetchBuffer), &colIndicator);
#else
		  sts = SQLGetData (hstmt, colNum, SQL_C_CHAR, fetchBuffer,
		      NUMTCHAR (fetchBuffer), &colIndicator);
#endif
		  if (sts != SQL_SUCCESS_WITH_INFO && sts != SQL_SUCCESS)
		    {
		      ODBC_Errors ("SQLGetData");
		      goto endCursor;
		    }

		  /*
		   *  Show NULL fields as ****
		   */
		  if (colIndicator == SQL_NULL_DATA)
		    {
		      for (i = 0; i < displayWidths[colNum - 1]; i++)
			fetchBuffer[i] = TEXTC ('*');
		      fetchBuffer[i] = TEXTC ('\0');
		    }

#ifdef UNICODE
		  printf ("%-*.*S", displayWidths[colNum - 1],
		      displayWidths[colNum - 1], fetchBuffer);
#else
		  printf ("%-*.*s", displayWidths[colNum - 1],
		      displayWidths[colNum - 1], fetchBuffer);
#endif
		  if (colNum < numCols)
		    putchar ('|');
		}
	      putchar ('\n');
	      totalRows++;
	    }

	  printf ("\n result set %lu returned %lu rows.\n\n",
	      totalSets, totalRows);
	  totalSets++;
	}
      while ((sts = SQLMoreResults (hstmt)) == SQL_SUCCESS);

      if (sts == SQL_ERROR)
	ODBC_Errors ("SQLMoreResults");

    endCursor:
#if (ODBCVER < 0x0300)
      SQLFreeStmt (hstmt, SQL_CLOSE);
#else
      SQLCloseCursor (hstmt);
#endif
    }

  return 0;
}


int
main (int argc, char **argv)
{
  /*
   *  Set locale based on environment variables
   */
  setlocale (LC_ALL, "");

  /*
   *  Show welcome message
   */
#ifdef UNICODE
  printf ("iODBC Unicode Demonstration program\n");
#else
  printf ("iODBC Demonstration program\n");
#endif
  printf ("This program shows an interactive SQL processor\n");

  /*
   *  Show a usage string when the user asks for this
   */
  if (argc > 2 || (argc == 2 && argv[1][0] == '-'))
    {
      fprintf (stderr,
	  "\nUsage:\n  iodbctest [\"DSN=xxxx;UID=xxxx;PWD=xxxx\"]\n");
      exit (0);
    }

  /*
   *  If we can connect to this datasource, run the test program
   */
  if (ODBC_Connect (argv[1]) != 0)
    {
      ODBC_Errors ("ODBC_Connect");
    }
  else if (ODBC_Test () != 0)
    {
      ODBC_Errors ("ODBC_Test");
    }

  /*
   *  End the connection
   */
  ODBC_Disconnect ();

  printf ("\nHave a nice day.");

  return 0;
}
