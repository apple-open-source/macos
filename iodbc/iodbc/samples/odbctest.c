/*
 *  odbctest.c
 *
 *  $Id: odbctest.c,v 1.2 2002/07/10 20:50:53 miner Exp $
 *
 *  Sample ODBC program
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

#include <stdio.h>
#include <string.h>

#if defined(__APPLE__) && !defined(NO_FRAMEWORKS)
#include <iODBC/sql.h>
#include <iODBC/sqlext.h>
#else
#include <sql.h>
#include <sqlext.h>
#endif

#define MAXCOLS		32

SQLHENV henv;
SQLHDBC hdbc;
SQLHSTMT hstmt;
int connected;


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
 *	HOST=star;SVT=Informix 5;UID=demo;PWD=demo;DATABASE=stores5
 *
 *	DSN=stores5_informix;PWD=demo
 */
int
ODBC_Connect (char *connStr)
{
  short buflen;
  char buf[257];
  SQLCHAR dataSource[120];
  SQLCHAR dsn[33];
  SQLCHAR desc[255];
  SQLCHAR driverInfo[255];
  SWORD len1, len2;
  int status;

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
	dataSource[strlen (dataSource) - 1] = '\0';

	/*
	 * Check if the user wants to quit
	 */
	if (!strcmp (dataSource, "quit") || !strcmp (dataSource, "exit"))
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
	fprintf (stderr, "\n%-30s | %-30s\n", "DSN", "Description");
	fprintf (stderr, "---------------------------------------------------------------\n");

	/*
	 *  Goto the first record
	 */
	if (SQLDataSources (henv, SQL_FETCH_FIRST,
		dsn, 33, &len1, desc, 255, &len2) != SQL_SUCCESS)
	  continue;

	/*
	 *  Show all records
	 */
	do
	  {
	    fprintf (stderr, "%-30s | %-30s\n", dsn, desc);
	  }
	while (SQLDataSources (henv, SQL_FETCH_NEXT,
		dsn, 33, &len1, desc, 255, &len2) == SQL_SUCCESS);
      }

  status = SQLDriverConnect (hdbc, 0, (UCHAR *) dataSource, SQL_NTS,
      (UCHAR *) buf, sizeof (buf), &buflen, SQL_DRIVER_COMPLETE);

  if (status != SQL_SUCCESS && status != SQL_SUCCESS_WITH_INFO)
    return -1;

  SQLSetConnectOption (hdbc, SQL_OPT_TRACEFILE, (UDWORD) "\\SQL.LOG");

  connected = 1;

  status = SQLGetInfo (hdbc, SQL_DRIVER_VER, 
	driverInfo, sizeof (driverInfo), &len1);
  if (status == SQL_SUCCESS)
    printf ("Driver: %s\n", driverInfo);

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
       int sts;
       sts = SQLCloseCursor (hstmt);
       if (sts != SQL_ERROR)
	   ODBC_Errors ("CloseCursor");
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
 *  This is the message handler for the communications layer.
 *
 *  The messages received here are not passed through SQLError,
 *  because they might occur when no connection is established.
 *
 *  Typically, Rejections from oplrqb are trapped here, and
 *  also RPC errors.
 *
 *  When no message handler is installed, the messages are output to stderr
 */
void
ODBC_MesgHandler (char *reason)
{
  fprintf (stderr, "ODBC_MesgHandler: %s\n", reason);
}


/*
 *  Show all the error information that is available
 */
int
ODBC_Errors (char *where)
{
  unsigned char buf[250];
  unsigned char sqlstate[15];

  /*
   *  Get statement errors
   */
  while (SQLError (henv, hdbc, hstmt, sqlstate, NULL,
      buf, sizeof(buf), NULL) == SQL_SUCCESS)
    {
      fprintf (stderr, "%s, SQLSTATE=%s\n", buf, sqlstate);
    }

  /*
   *  Get connection errors
   */
  while (SQLError (henv, hdbc, SQL_NULL_HSTMT, sqlstate, NULL,
      buf, sizeof(buf), NULL) == SQL_SUCCESS)
    {
      fprintf (stderr, "%s, SQLSTATE=%s\n", buf, sqlstate);
    }

  /*
   *  Get environmental errors
   */
  while (SQLError (henv, SQL_NULL_HDBC, SQL_NULL_HSTMT, sqlstate, NULL,
      buf, sizeof(buf), NULL) == SQL_SUCCESS)
    {
      fprintf (stderr, "%s, SQLSTATE=%s\n", buf, sqlstate);
    }

  return -1;
}


/*
 *  Test program to run on the connected database
 */
int
ODBC_Test ()
{
  char request[512];
  char fetchBuffer[1000];
  short displayWidths[MAXCOLS];
  short displayWidth;
  short numCols;
  short colNum;
  char colName[50];
  short colType;
  UDWORD colPrecision;
  SDWORD colIndicator;
  short colScale;
  short colNullable;
  UDWORD totalRows;
  UDWORD totalSets;
  int i;

  while (1)
    {
      /*
       *  Ask the user for a dynamic SQL statement
       */
      printf ("\nSQL> ");
      if (fgets (request, sizeof (request), stdin) == NULL)
	break;

      request[strlen (request) - 1] = '\0';
      if (request[0] == '\0')
	continue;

      /*
       *  If the user just types tables, give him a list
       */
      if (!strcmp (request, "tables"))
	{
	  if (SQLTables (hstmt, NULL, SQL_NTS, NULL, SQL_NTS, NULL, SQL_NTS,
		  NULL, SQL_NTS) != SQL_SUCCESS)
	    {
	      ODBC_Errors ("SQLTables");
	      continue;
	    }
	}
      else if (!strcmp (request, "quit") || !strcmp (request, "exit"))
	break;			/* If you want to quit, just say so */
      else
	{
	  /*
	   *  Prepare & Execute the statement
	   */
	  if (SQLPrepare (hstmt, (UCHAR *) request, SQL_NTS) != SQL_SUCCESS)
	    {
	      ODBC_Errors ("SQLPrepare");
	      continue;
	    }
	  if (SQLExecute (hstmt) != SQL_SUCCESS)
	    {
	      ODBC_Errors ("SQLExec");
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
	      printf ("Statement executed.\n");
	      goto endCursor;
	    }

	  if (numCols > MAXCOLS)
	    numCols = MAXCOLS;

	  /*
	   *  Get the names for the columns
	   */
	  putchar ('\n');
	  for (colNum = 1; colNum <= numCols; colNum++)
	    {
	      /*
	       *  Get the name and other type information
	       */
	      if (SQLDescribeCol (hstmt, colNum, (UCHAR *) colName,
		      sizeof (colName), NULL, &colType, &colPrecision,
		      &colScale, &colNullable) != SQL_SUCCESS)
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
		  displayWidth = (short) colPrecision;
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
		case SQL_DATE:
		  displayWidth = 10;
		  break;
		case SQL_TIME:
		  displayWidth = 8;
		  break;
		case SQL_TIMESTAMP:
		  displayWidth = 19;
		  break;
		default:
		  displayWidths[colNum - 1] = 0;     /* skip other data types */
		  continue;
		}

	      if (displayWidth < strlen (colName))
		displayWidth = strlen (colName);
	      if (displayWidth > sizeof (fetchBuffer) - 1)
		displayWidth = sizeof (fetchBuffer) - 1;

	      displayWidths[colNum - 1] = displayWidth;

	      /*
	       *  Print header field
	       */
	      printf ("%-*.*s", displayWidth, displayWidth, colName);
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
	      int sts = SQLFetch (hstmt);

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
		  if (SQLGetData (hstmt, colNum, SQL_CHAR, fetchBuffer,
			  sizeof (fetchBuffer), &colIndicator) != SQL_SUCCESS)
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
			fetchBuffer[i] = '*';
		      fetchBuffer[i] = '\0';
		    }

		  printf ("%-*.*s", displayWidths[colNum - 1],
		      displayWidths[colNum - 1], fetchBuffer);
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
      while (SQLMoreResults (hstmt) == SQL_SUCCESS);

    endCursor:
      SQLFreeStmt (hstmt, SQL_CLOSE);
    }

  return 0;
}


int
main (int argc, char **argv)
{
  printf ("iODBC Demonstration program\n");
  printf ("This program shows an interactive SQL processor\n");

  /*
   *  Show a usage string when the user asks for this
   */
  if (argc > 2 || (argc == 2 && argv[1][0] == '-'))
    {
      fprintf (stderr, "\nUsage:\n  odbctest [\"DSN=xxxx;UID=xxxx;PWD=xxxx\"]\n");
      exit(0);
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

  printf ("\nHave a nice day.\n");

  return 0;
}
