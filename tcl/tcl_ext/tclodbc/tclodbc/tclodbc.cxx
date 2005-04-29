/**********************************************************************

                  - TCLODBC COPYRIGHT NOTICE - 

This software is copyrighted by Roy Nurmi, contact address by email at
Roy.Nurmi@iki.fi. The following terms apply to all files associated with 
the software unless explicitly disclaimed in individual files.

The author hereby grant permission to use, copy, modify, distribute,
and license this software and its documentation for any purpose, provided
that existing copyright notices are retained in all copies and that this
notice is included verbatim in any distributions. No written agreement,
license, or royalty fee is required for any of the authorized uses.
Modifications to this software may be copyrighted by their authors
and need not follow the licensing terms described here, provided that
the new terms are clearly indicated on the first page of each file where
they apply.

IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
MODIFICATIONS.

GOVERNMENT USE: If you are acquiring this software on behalf of the
U.S. government, the Government shall have only "Restricted Rights"
in the software and related documentation as defined in the Federal 
Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
are acquiring the software on behalf of the Department of Defense, the
software shall be classified as "Commercial Computer Software" and the
Government shall have only "Restricted Rights" as defined in Clause
252.227-7013 (c) (1) of DFARs.  Notwithstanding the foregoing, the
authors grant the U.S. Government and others acting in its behalf
permission to use and distribute the software in accordance with the
terms specified in this license. 

***********************************************************************/

//////////////////////////////////////////////////////////////////////////
//                                                                      //
//        ODBC-interface extension to tcl language by Roy Nurmi         //
//                             Version 2.1                              //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// VERSION HISTORY                                                      //
//                                                                      //
// 1.0 Initial version. Requires tcl 8.0 alpha 2 release                //
//                                                                      //
// 1.1 New version, compatible with tcl 8.0 beta 1 release              //
//                                                                      //
// 1.2 New version, enhanced functionality of statement columns command //
//                                                                      //
// 1.3 New version, added database configure option, index queries,     //
//                  new error object definition                         //
//                                                                      //
// 1.4 New version, added database datasources and drivers options      //
//                  and connecting with odbc connection string          //
//                                                                      //
// 1.4 patch 1      Bug corrections, enhanced portability               //
//                                                                      //
// 1.5 New version, added set and get commands for setting some         //
//                  useful connection and statement options             //
//                                                                      //
// 1.6 New version, argument type definition moved to statement         //
//                  creation.                                           //
//                                                                      //
// 1.7 New version, compilable also under tcl 7.6                       //
//                                                                      //
// 2.0 New version, cursor types & rowset_size also configurable        //
//                  contributed by Ric Klaren (klaren@trc.nl).          //
//                  optional array argument to statement fetch command  //
//                  thread safety provided                              //
//                  Unicode support with tcl 8.1                        //
//                  Emprovements in large columns support               //
//                                                                      //
// 2.1 New version, added database typeinfo command                     //
//                                                                      //
// 2.2 New version, new binary column handling and some bug fixes       //
//                  some contributions from Robert Black                //
//                                                                      //
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
// INCLUDES
//

#include "tclodbc.hxx"

//////////////////////////////////////////////////////////////////////////
// GLOBAL VARIABLES
//

// ODBC environment handle management

HENV env = 0;                // environment handle
int envRefCounter = 0;       // enables use of one common handle

// we have to have TCL_THREADS defined, 
// if we want to compile multithreaded version

#ifdef TCL_THREADS
TCL_DECLARE_MUTEX(envMutex); // ensures thread-safe allocation/freeing
#endif

//////////////////////////////////////////////////////////////////////////
// MISANCELLOUS HELPER FUNCTIONS
//

TclObj SqlErr (HENV env, HDBC dbc, HSTMT stmt) {
    char SqlMessage[SQL_MAX_MESSAGE_LENGTH];
    char SqlState[6];
    SDWORD NativeError;
    SWORD Available;
    RETCODE rc;
    TclObj errObj;

    rc = SQLError(env, dbc, stmt, 
            (UCHAR*) SqlState, &NativeError, (UCHAR*) SqlMessage, 
            SQL_MAX_MESSAGE_LENGTH-1, &Available);

    // sql error object is a triple:
    // {standard error code} {native error code} {error message}

    if (rc != SQL_ERROR) {
        errObj.appendElement(TclObj(SqlState));
        errObj.appendElement(TclObj(NativeError));
        errObj.appendElement(TclObj(SqlMessage,Available));
    } else {
        errObj.appendElement("FATAL ERROR: Failed to receive error message");
    }

    return errObj;
}

short StrToNum (char *str, NumStr array[], char* errMsg, BOOLEAN allowNumeric) {

    short num;
    int strFirst (1);
    int strLast (array[0].numeric);
    int i;
    int strcmpResult;

    if (allowNumeric && (num = atoi(str))) 
        return num;

    // binary search for matching string
    while (strFirst <= strLast) {
        i = (strFirst + strLast) / 2;
        strcmpResult = strcmp(str, array[i].string);
        if (strcmpResult == 0) 
            return array[i].numeric;
        else if (strcmpResult > 0) 
            strFirst = i + 1;
        else
            strLast = i - 1;
    }

/*
  for (int i = 0; array[i].string; ++i) {
  if (!strcmp(str, array[i].string)) 
  return array[i].numeric;
  }
  }
*/
    // No match found. Throw error, if error message != NULL.
    if (errMsg) {
        TclObj errObj (errMsg);
        errObj.append(str);
	THROWOBJ(errObj);
    }

    // Otherways just return -1
    return -1;  
}

TclObj NumToStr (short num, NumStr array[]) {
    for (int i = 1; i <= array[0].numeric; ++i) {
	if (array[i].numeric == num) 
	    return TclObj(array[i].string);
    }
    TclObj errObj ("Invalid numeric value: ");
    errObj.append(TclObj(num));
    THROWOBJ(errObj);
    return errObj; // to keep compiler happy
}

// this routine determines which sql types should be encoded and decoded
BOOL EncodedType (int i) 
{
    switch (i) {
	case SQL_CHAR: 
	case SQL_VARCHAR:
	case SQL_LONGVARCHAR:
	    return TRUE;
	default:
	    return FALSE;
    }
}

// type mapping function sql / c
SWORD MapSqlType (SDWORD colType) {
    switch (colType) {
	case SQL_VARBINARY:
	case SQL_LONGVARBINARY:
	    return SQL_C_BINARY;
	default:
	    return SQL_C_CHAR;
    }
}


//////////////////////////////////////////////////////////////////////////
// TCL COMMAND INTERFACE
//

int tcl_database (ClientData clientData, Tcl_Interp *interp, int objc,
		  TCL_CMDARGS)
{
    TclObj name, db, uid, password;
    char *p;
    TclObj attributes;
    TclDatabase* pDataBase = NULL;

    try {
        if (objc == 1) {
            // return usage
            Tcl_SetResult(interp, strUsage, TCL_STATIC);
            return TCL_OK;
        }

        switch (StrToNum (TclObj(objv[1]), databaseOptions, NULL, FALSE)) {
	    case TclDatabase::CONFIGURE:
		if (objc != 5) {
		    THROWSTR("wrong # args, should be configure operation driver attributes");
		}
		return TclDatabase::Configure(interp, objc-2, objv+2);

	    case TclDatabase::DATASOURCES:
		// generate list of all databases
		Tcl_SetObjResult (interp, TclDatabase::Datasources());
		return TCL_OK;

	    case TclDatabase::DRIVERS:
		// generate list of all drivers available
		Tcl_SetObjResult (interp, TclDatabase::Drivers());
		return TCL_OK;

	    case TclDatabase::VERSION:
		// return version information
		Tcl_SetResult(interp, strVersion, TCL_STATIC);
		return TCL_OK;

	    case TclDatabase::CONNECT:
		--objc;
		++objv;
		// fall through

	    default:
		if (objc < 3 || objc > 5) {
		    THROWSTR("wrong # args, should be database name connectionstring | (db [uid] [password])");
		}
		name = TclObj(objv[1]);

		db = TclObj(objv[2]);

		// search for '=' in dbname, indicating a odbc connection
		// string
		for (p = (char*) db; *p != '\0' && *p != '='; ++p); 

		// connect using a connection string or a datasource name,
		if (objc == 3 && *p == '=') {
		    pDataBase = new TclDatabase(db);
		} else {
		    uid = objc > 3 ? TclObj(objv[3]) : TclObj();
		    password = objc > 4 ? TclObj(objv[4]) : TclObj();
		    pDataBase = new TclDatabase(db, uid, password);
		}

		if (!pDataBase) {
		    THROWSTR(strMemoryAllocationFailed);
		}

		pDataBase->tclCommand = Tcl_CreateObjCommand(interp, name, 
			&TclCmdObject::Dispatch, pDataBase,
			TclCmdObject::Destroy);

		Tcl_SetObjResult(interp, TclObj(objv[1]));
		return TCL_OK;
        }
    }
    catch( TclObj obj ) {
        if (pDataBase) {
            delete pDataBase;
	}
        Tcl_SetObjResult(interp, obj);
        return TCL_ERROR;
    }
}

//////////////////////////////////////////////////////////////////////////
// TCL EXTENSION CLEANUP ROUTINE
//

void Tclodbc_Kill(ClientData clientData)
{
#ifdef TCL_THREADS
    // The last call to this function will clean up the ODBC environment
    // handle.
    Tcl_MutexLock(&envMutex);
#endif

    envRefCounter -= 1;

    if (envRefCounter == 0 && env) {
	SQLFreeEnv(env);	
	env = (HENV) NULL;
    }

#ifdef TCL_THREADS
    Tcl_MutexUnlock(&envMutex);
#endif
}


//////////////////////////////////////////////////////////////////////////
// TCL EXTENSION INITIALIZATION ROUTINE
//

extern "C" {
_declspec(dllexport)
Tclodbc_Init(Tcl_Interp *interp) 
{
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, "8.1", 0) == NULL) {
	return TCL_ERROR;
    }
#endif

#ifdef _DEBUG
    // run some validation routines in debug mode
    if (tclodbc_validateNumStrArrays() == TCL_ERROR) {
	Tcl_SetResult(interp, "tclodbc_validateNumStrArrays() failed",
		TCL_STATIC);
	return TCL_ERROR;
    }
    if (tclodbc_validateStrToNumFunction() == TCL_ERROR) {
	Tcl_SetResult(interp, "tclodbc_validateStrToNumFunction() failed",
		TCL_STATIC);
	return TCL_ERROR;
    }
#endif

    // The library IS thread-safe with global environment handle. 
    // An exerpt from ODBC reference:

    /*
     * An environment handle references global information such as valid
     * connection handles and active connection handles. To request an
     * environment handle, an application passes the address of an henv to
     * SQLAllocEnv.  The driver allocates memory for the environment
     * information and stores the value of the associated handle in the
     * henv. On operating systems that support multiple threads, applications
     * can use the same henv on different threads and drivers must therefore
     * support safe, multithreaded access to this information. The application
     * passes the henv value in all subsequent calls that require an henv.
     *
     * There should never be more than one henv allocated at one time and the
     * application should not call SQLAllocEnv when there is a current valid
     * henv.
     *
     * To support multiple interpreters we keep reference count of
     * Tclodbc_init calls. We reserve handle only once, and release it when
     * the last interpreter calls Tclodbc_Kill.
     */

#ifdef TCL_THREADS
    // get mutex to increment local reference counter
    Tcl_MutexLock(&envMutex);
#endif

    // allocate environment handle if not yet allocated
    if (!env && SQLAllocEnv(&env) == SQL_ERROR) {
	if (env == SQL_NULL_HENV) {
	    Tcl_SetResult(interp, strMemoryAllocationFailed, TCL_STATIC);
	} else {
	    Tcl_SetObjResult(interp,
		    SqlErr(env, SQL_NULL_HDBC, SQL_NULL_HSTMT));
	}

#ifdef TCL_THREADS
	Tcl_MutexUnlock(&envMutex);
#endif

	return TCL_ERROR;
    }

    envRefCounter += 1;

    // free mutex after reference counter increment
#ifdef TCL_THREADS
    Tcl_MutexUnlock(&envMutex);
#endif

    // create exit handler
    Tcl_CreateExitHandler(Tclodbc_Kill, (ClientData) 0);

    // create commands
    Tcl_CreateObjCommand(interp, "database", tcl_database, NULL, 
	    (Tcl_CmdDeleteProc *) NULL);

    // provide package information
    Tcl_PkgProvide(interp, "tclodbc", PACKAGE_VERSION);
    return TCL_OK ;
}

} // extern "C"
