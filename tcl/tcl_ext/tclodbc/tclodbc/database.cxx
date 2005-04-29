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
// DATABASE OBJECT IMPLEMENTATION
//

#include "tclodbc.hxx"

void TclCmdObject::Destroy(ClientData o) {
    // virtual destructor call
    delete (TclCmdObject*) o;
}

TclCmdObject::~TclCmdObject() {
    RemoveFromList();
}

int TclCmdObject::Dispatch (ClientData clientData, Tcl_Interp *interp, 
                  int objc, TCL_CMDARGS) {
    TclCmdObject& o = *(TclCmdObject*) clientData;
    return o.Dispatch(interp, objc, objv);
}

void TclCmdObject::AddToMyList(TclCmdObject *p) {
    if (pNext) 
        pNext->pPrev = p;        
    pNext = p;
    pNext->pPrev = this;
}

void TclCmdObject::RemoveFromList() {
    if (pPrev)
        pPrev->pNext = pNext;
    if (pNext)
        pNext->pPrev = pPrev;
}

TclDatabase::TclDatabase(TclObj db, TclObj uid, TclObj passwd) 
    : TclCmdObject(), dbc (SQL_NULL_HDBC), encoding(NULL), infoExtensions(0),
      useMultipleResultSets(false) {

    RETCODE rc;
    SWORD dummy;

    // allocate connection handle
    rc = SQLAllocConnect(env, &dbc);
    if (rc == SQL_ERROR) 
        THROWOBJ(SqlErr(env, SQL_NULL_HDBC, SQL_NULL_HSTMT))

    // encode strings
    db.Encode(NULL);
    uid.Encode(NULL);
    passwd.Encode(NULL);

    // make connection
    rc = SQLConnect(dbc, 
		    (UCHAR*) db.EncodedValue(), (SWORD) db.EncodedLenght(), 
		    (UCHAR*) uid.EncodedValue(), (SWORD) uid.EncodedLenght(), 
		    (UCHAR*) passwd.EncodedValue(),
		    (SWORD) passwd.EncodedLenght());
    if (rc == SQL_ERROR) {
        TclObj error = SqlErr(env, dbc, SQL_NULL_HSTMT);
        SQLFreeConnect(dbc);
        THROWOBJ(error);
    }

    // get some info of driver's capabilities
    SQLGetInfo(dbc, SQL_GETDATA_EXTENSIONS, &infoExtensions, sizeof(infoExtensions), &dummy);
}

TclDatabase::TclDatabase(TclObj connectionstring) 
    : TclCmdObject(), dbc (SQL_NULL_HDBC), encoding(NULL), infoExtensions(0),
      useMultipleResultSets(false) {
#ifdef INCLUDE_EXTENSIONS
    RETCODE rc;
    unsigned char szConnStrOut[256];
    SWORD pcbConnStrOut;
    SWORD dummy;

    // allocate connection handle
    rc = SQLAllocConnect(env, &dbc);
    if (rc == SQL_ERROR) {
        THROWOBJ(SqlErr(env, SQL_NULL_HDBC, SQL_NULL_HSTMT));
    }

    // make connection
    connectionstring.Encode(NULL);
    rc = SQLDriverConnect(dbc, NULL, 
			  (UCHAR*) connectionstring.EncodedValue(), 
			  (SWORD) connectionstring.EncodedLenght(), 
			  szConnStrOut, 255, &pcbConnStrOut, SQL_DRIVER_NOPROMPT);
    if (rc == SQL_ERROR) {
        TclObj error = SqlErr(env, dbc, SQL_NULL_HSTMT);
        SQLFreeConnect(dbc);
        THROWOBJ(error);
    }

    // get some info of driver's capabilities
    SQLGetInfo(dbc, SQL_GETDATA_EXTENSIONS, &infoExtensions, sizeof(infoExtensions), &dummy);

#else
    THROWSTR(strCmdNotAvailable);
#endif
}

TclDatabase::~TclDatabase() {
    // free all existing statement handles. Commands are still 
    // left in the interpreter, but using them raises an error.
    TclCmdObject* p;
    p = this;
    while ((p = p->Next()) != NULL) {
        ((TclStatement*)p)->FreeStmt();
    }
    if (dbc != SQL_NULL_HDBC) {
        SQLDisconnect(dbc);
        SQLFreeConnect(dbc);
    }
#if TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION >= 1
    if (encoding) 
        Tcl_FreeEncoding(encoding);
#endif
}

int TclDatabase::Dispatch (Tcl_Interp *interp, int objc, TCL_CMDARGS) {
    TclObj stmtName;
    TclObj stmtIniter;
    TclStatement *pStmt = NULL;

    try {
        // there should be at least one argument
        if (objc == 1) {
            THROWSTR(strWrongArgs);
	}

        // switch on command
        switch (StrToNum (TclObj(objv[1]), databaseCmds, NULL, FALSE)) {
        case STATEMENT:
        case S:
            if (objc < 4 || objc > 5) {
                THROWSTR("wrong # args, should be statement name initer [argtypelist]");
	    }
            stmtName = TclObj(objv[2]);
            stmtIniter = TclObj(objv[3]);

            if (!strcmp(stmtIniter, strTables))
                pStmt = new TclTableQuery(*this);
            else if (!strcmp(stmtIniter, strColumns))
                pStmt = new TclColumnQuery(*this);
            else if (!strcmp(stmtIniter, strIndexes))
                pStmt = new TclIndexQuery(*this);
            else if (!strcmp(stmtIniter, strTypeinfo))
                pStmt = new TclTypeInfoQuery(*this);
            else if (!strcmp(stmtIniter, strPrimarykeys))
                pStmt = new TclPrimaryKeysQuery(*this);
            else {
                pStmt = new TclSqlStatement(*this, stmtIniter, useMultipleResultSets);
                if (pStmt && objc == 5)
                    ((TclSqlStatement*)pStmt)->SetArgDefs(interp, TclObj(objv[4]));
            }

            if (!pStmt) {
                THROWSTR(strMemoryAllocationFailed);
	    }
            pStmt->tclCommand = Tcl_CreateObjCommand(interp, stmtName, 
						     &TclCmdObject::Dispatch, (ClientData) pStmt, TclCmdObject::Destroy);
            AddToMyList(pStmt); // add to list, so we can keep track of all statements
            Tcl_SetObjResult (interp, TclObj(objv[2]));
            break;

        case DISCONNECT:
            if (objc != 2) {
                THROWSTR(strWrongArgs);
	    }
            Tcl_DeleteCommandFromToken(interp, tclCommand);
            Tcl_SetResult(interp, strOK, TCL_STATIC);
            break;

        case TABLES:
            if (objc < 2 || objc > 3) {
                THROWSTR("wrong # args, should be tables [pattern]");
	    }
            Tcl_SetObjResult (interp, TclTableQuery(*this).Value(interp, objc-2, objv+2));
            break;

        case COLUMNS:
            if (objc < 2 || objc > 3) {
                THROWSTR("wrong # args, should be columns [tablename]");
	    }
            Tcl_SetObjResult (interp, TclColumnQuery(*this).Value(interp, objc-2, objv+2));
            break;

        case INDEXES:
            if (objc != 3) {
                THROWSTR("wrong # args, should be indexes tablename");
	    }
            Tcl_SetObjResult (interp, TclIndexQuery(*this).Value(interp, objc-2, objv+2));
            break;

        case TYPEINFO:
            if (objc != 3) {
                THROWSTR("wrong # args, should be typeinfo typeid");
	    }
            Tcl_SetObjResult (interp, TclTypeInfoQuery(*this).Value(interp, objc-2, objv+2));
            break;

        case PRIMARYKEYS:
            if (objc != 3) {
                THROWSTR("wrong # args, should be primarykeys tablename");
	    }
            Tcl_SetObjResult (interp, TclPrimaryKeysQuery(*this).Value(interp, objc-2, objv+2));
            break;

        case AUTOCOMMIT:
            THROWSTR(strOldSyntax);

        case COMMIT:
	    if (objc != 2) {
                THROWSTR(strWrongArgs);
	    }
            Transact(SQL_COMMIT);
            Tcl_SetResult(interp, strOK, TCL_STATIC);
            break;

        case ROLLBACK:
            if (objc != 2) {
                THROWSTR(strWrongArgs);
	    }
            Transact(SQL_ROLLBACK);
            Tcl_SetResult(interp, strOK, TCL_STATIC);
            break;

        case SET:
            if (objc != 4) {
                THROWSTR(strWrongArgs);
	    }
            SetOption(interp,
                      Tcl_GetStringFromObj(objv[2], NULL), 
			          Tcl_GetString(objv[3]));
            Tcl_SetResult(interp, strOK, TCL_STATIC);
            break;

        case GET:
            if (objc != 3) {
                THROWSTR(strWrongArgs);
	    }
            Tcl_SetObjResult(interp, GetOption(Tcl_GetString(objv[2])));
            break;

        case EVAL:
            if (objc < 4 || objc > 6) {
                THROWSTR("wrong # args, should be eval proc sql [typedefs] [args]");
	    } else {
		TclSqlStatement stmt(*this, Tcl_GetString(objv[3]),
				     useMultipleResultSets);
                TclObj proc (objv[2]);

                if (objc == 6) {
                    stmt.SetArgDefs(interp, objv[4]);
                    --objc;
                    ++objv;
                } 

                stmt.Eval(interp, proc, objc-4, objv+4);
                Tcl_SetResult(interp, strOK, TCL_STATIC);
            }
            break;

        case READ:
            if (objc < 4 || objc > 6) {
                THROWSTR("wrong # args, should be read array sql [typedefs] [args]");
            } else {
		TclSqlStatement stmt(*this, Tcl_GetString(objv[3]),
				     useMultipleResultSets);
                TclObj arraySpec (objv[2]);

                if (objc == 6) {
                    stmt.SetArgDefs(interp, objv[4]);
                    --objc;
                    ++objv;
                } 

                stmt.Read(interp, arraySpec, objc-4, objv+4);
                Tcl_SetResult(interp, strOK, TCL_STATIC);
            }
            break;

        default:
            if (objc < 2 || objc > 4) {
                THROWSTR("wrong # args, should be sql [typedefs] [args]");
	    } else {
		TclSqlStatement stmt(*this, Tcl_GetString(objv[1]),
				     useMultipleResultSets);
                if (objc == 4) {
                    stmt.SetArgDefs(interp, objv[2]);
                    --objc;
                    ++objv;
                } 
                Tcl_SetObjResult (interp, stmt.Value(interp, objc-2, objv+2));
	    }
            break;
        }

        // command successful
        return TCL_OK;
    }
    catch (TclObj obj) {
        if (pStmt)
            delete pStmt;
        Tcl_SetObjResult (interp, obj);
        return TCL_ERROR;
    }
}

void TclDatabase::Autocommit(BOOL on) {
    RETCODE rc;
    UDWORD commit;

    commit = on ? SQL_AUTOCOMMIT_ON : SQL_AUTOCOMMIT_OFF;

    rc = SQLSetConnectOption(dbc, SQL_AUTOCOMMIT, commit);
    if (rc == SQL_ERROR) {
        THROWOBJ(SqlErr(env, dbc, SQL_NULL_HSTMT));
    }
}

void TclDatabase::SetOption(Tcl_Interp *interp, char* option, char* value) {
    RETCODE rc;
    UWORD op;
    UDWORD val = 0;

    if (strcmp(option, "multisets") == 0) {
	int boolval;
	if (Tcl_GetBoolean(interp, value, &boolval) != TCL_OK) {
            THROWOBJ(TclObj(Tcl_GetObjResult(interp)));
	}
	useMultipleResultSets = boolval;
	return;
    }

    op = StrToNum (option, connectOp);

    switch (op) {
    case SQL_AUTOCOMMIT:
    case SQL_NOSCAN:
    case SQL_ASYNC_ENABLE:
	val = StrToNum (value, booleanOp);
	break;

    case SQL_CONCURRENCY:
	val = StrToNum (value, concurrencyOp);
	break;

    case SQL_QUERY_TIMEOUT:
    case SQL_MAX_ROWS:
    case SQL_MAX_LENGTH:
    case SQL_ROWSET_SIZE:
	val = atoi(value);
	break;

    case SQL_CURSOR_TYPE:
	val = StrToNum (value, cursorOp);
	break;

	// special case, encoding
    case TCLODBC_ENCODING:
#if TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION >= 1
        if (encoding)
            Tcl_FreeEncoding(encoding);
        encoding = Tcl_GetEncoding(interp, value);
        if (!encoding) {
            THROWOBJ(TclObj(Tcl_GetObjResult(interp)));
	}
#endif
	return; 
    }

    rc = SQLSetConnectOption(dbc, op, val);
    if (rc == SQL_ERROR) {
        THROWOBJ(SqlErr(env, dbc, SQL_NULL_HSTMT));
    }
}

TclObj TclDatabase::GetOption(char* option) {
    RETCODE rc;
    UWORD op;
    UDWORD val;

    if (strcmp(option, "multisets") == 0) {
	return NumToStr ((short) useMultipleResultSets, booleanOp);
    }

    op = StrToNum (option, connectOp);

    // determine value
    if (op < TCLODBC_OPTIONS) {
        rc = SQLGetConnectOption(dbc, op, &val);
        if (rc == SQL_ERROR) 
            THROWOBJ(SqlErr(env, dbc, SQL_NULL_HSTMT))
    }

    // interprete value
    switch (op) {
    case SQL_NOSCAN:
    case SQL_AUTOCOMMIT:
    case SQL_ASYNC_ENABLE:
	return NumToStr ((short) val, booleanOp);

    case SQL_CONCURRENCY:
	return NumToStr ((short) val, concurrencyOp);

    case SQL_QUERY_TIMEOUT:
    case SQL_MAX_ROWS:
    case SQL_MAX_LENGTH:
    case SQL_ROWSET_SIZE:
	return TclObj(val);

    case SQL_CURSOR_TYPE:
	return NumToStr((short) val, cursorOp);

    case TCLODBC_ENCODING:
#if TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION >= 1
	return TclObj(Tcl_GetEncodingName(encoding));
#else
        return TclObj();
#endif

    default:
	return TclObj(); // to keep compiler happy

    }
}

void TclDatabase::Transact(UWORD op) {
    RETCODE rc;

    rc = SQLTransact(env, dbc, op);
    if (rc == SQL_ERROR) {
        THROWOBJ(SqlErr(env, dbc, SQL_NULL_HSTMT));
    } else if (rc == SQL_INVALID_HANDLE) {
        THROWSTR(strInvalidHandle);
    }
}

TclObj TclDatabase::Datasources() {
    TclObj list;

#ifdef INCLUDE_EXTENSIONS
    RETCODE rc = 0;
    char dsn[SQL_MAX_DSN_LENGTH+1], descr[256];
    SWORD dsnLen, descrLen; 
    BOOL first = TRUE;

    while ((rc = SQLDataSources(env, 
        first ? SQL_FETCH_FIRST : SQL_FETCH_NEXT, 
        (UCHAR*)dsn, SQL_MAX_DSN_LENGTH+1, &dsnLen, 
        (UCHAR*)descr, 255, &descrLen)) == SQL_SUCCESS) 
    {
        TclObj item;
        TclObj dnsObj (dsn, (Tcl_Encoding) NULL, dsnLen);
        TclObj descrObj (descr, (Tcl_Encoding) NULL, descrLen);
        item.appendElement(dnsObj);
        item.appendElement(descrObj);
        list.appendElement(item);
        first = FALSE;
    }

#else
	THROWSTR(strCmdNotAvailable)
#endif

    // success
    return list;
}

TclObj TclDatabase::Drivers() {
    TclObj list;

#ifdef INCLUDE_EXTENSIONS
    RETCODE rc = 0;
    char driver[256], attrs[1024];
	char *attr;
    SWORD driverLen, attrsLen; 
    BOOL first = TRUE;

    while ((rc = SQLDrivers(env, 
        first ? SQL_FETCH_FIRST : SQL_FETCH_NEXT, 
        (UCHAR*)driver, 255, &driverLen, 
        (UCHAR*)attrs, 1023, &attrsLen)) == SQL_SUCCESS) 
    {
        TclObj item;
        TclObj driverObj (driver, (Tcl_Encoding) NULL, driverLen);
        TclObj attrsObj;

	// loop over all attribute strings, list terminates with
	// double-null.
	for (attr = attrs; *attr; attr += strlen(attr)+1) {
	    TclObj attrObj (attr, (Tcl_Encoding) NULL, -1);
            attrsObj.appendElement(attrObj);
	}

        item.appendElement(driverObj);
        item.appendElement(attrsObj);
        list.appendElement(item);
        first = FALSE;
    }

#else
    THROWSTR(strCmdNotAvailable);
#endif

    // success
    return list;
}

int TclDatabase::Configure(Tcl_Interp *interp, int objc, TCL_CMDARGS) {
#ifdef INCLUDE_EXTENSIONS
    WORD operation;
    char *driver;
    TclObj attributes;
    TclObj attrList;
    int i, attrcount;

    operation = ConfigOp(Tcl_GetStringFromObj(objv[0],NULL));
    if (!operation)
        THROWSTR("invalid operation code");

    driver = Tcl_GetStringFromObj(objv[1], NULL);

    // construct attribute string, values separated by nulls, ending with
    // a double null
    attrList = TclObj(objv[2]);
    attrcount = attrList.llenght(interp);
    for (i = 0; i < attrcount; ++i) {
        attributes.append(attrList.lindex(i, interp));
        attributes.append("\0", 1);
    }
    attributes.append("\0", 1);
    attributes.Encode(NULL);

    if (SQLConfigDataSource(NULL, operation, driver, attributes.EncodedValue())) {
        Tcl_SetResult(interp, strOK, TCL_STATIC);
        return TCL_OK;
    } else {
        THROWSTR("datasource configuration failed");
	return TCL_ERROR; // to make compiler happy
    }

#else
    THROWSTR(strCmdNotAvailable);
    return TCL_ERROR; // to make compiler happy
#endif
}

