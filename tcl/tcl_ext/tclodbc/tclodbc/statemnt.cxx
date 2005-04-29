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
// STATEMENT OBJECT IMPLEMENTATION
//

#include "tclodbc.hxx"

TclStatement::TclStatement(TclDatabase& d) 
    : TclCmdObject(), stmt(SQL_NULL_HSTMT), resultBuffer(NULL),
      colCount(-1), colLabels(), pDb(&d), useMultipleResultSets(false) {
    RETCODE rc;

    rc = SQLAllocStmt(d.DBC(), &stmt);
    if (rc == SQL_ERROR) {
        THROWOBJ(SqlErr(env, d.DBC(), stmt));
    } else if (rc == SQL_INVALID_HANDLE) {
        THROWSTR(strInvalidHandle);
    }
}

TclStatement::~TclStatement() {
    FreeResultBuffer();
    FreeStmt();
}

void TclStatement::FreeStmt() {
    if (stmt != SQL_NULL_HSTMT)
        SQLFreeStmt(stmt, SQL_DROP);
    stmt = SQL_NULL_HSTMT;
    pDb = NULL;
};

int TclStatement::Dispatch(Tcl_Interp *interp, int objc, TCL_CMDARGS) {
    SDWORD count;

    if (!pDb) {
        THROWSTR("Connection closed. Cannot execute.");
    }

    try {
        // switch on command
        int command;
	if (objc < 2) {
	    command = RUN;
	} else {
	    command = StrToNum (TclObj(objv[1]), statementCmds, NULL, FALSE);
	}
        switch (command) {
        case DROP:
            if (objc != 2) {
                THROWSTR(strWrongArgs);
	    }
            Tcl_DeleteCommandFromToken(interp, tclCommand);
            Tcl_SetResult(interp, strOK, TCL_STATIC);
            break;

        case COLUMNS:
            if (objc < 2) {
                THROWSTR(strWrongArgs);
	    }

	    Tcl_SetObjResult (interp, Columns(objc-2, objv+2));
            break;

        case EXECUTE:
            if (objc < 2 || objc > 4) {
                THROWSTR("wrong # args, should be execute [args]");
	    }
            Execute(interp, objc-2, objv+2);
            Tcl_SetResult(interp, strOK, TCL_STATIC);
            break;

        case MORERESULTS: {
	    RETCODE rc;
	    bool more;
	    while ((rc = SQLMoreResults(stmt)) == SQL_STILL_EXECUTING) {
		SqlWait(0);
	    }
	    if (rc == SQL_ERROR) {
		THROWOBJ(SqlErr(env, pDb->DBC(), stmt));
		more = false;
	    }
	    more = (rc == SQL_NO_DATA ? false : true);

	    FreeResultBuffer();
	    resultBuffer = 0;

	    Tcl_SetObjResult (interp, TclObj(more));
	    break;
	}

        case FETCH:
            if (objc < 2 && objc > 4) {
                THROWSTR(strWrongArgs);
	    }
            {
                TclObj row;
                Fetch(row);

                if (objc == 2) {
                    // ordinary fetch, just return the row
                    Tcl_SetObjResult (interp, row);
                } else {
                    // fetch values to a buffer (named tcl array)
                    int colCount (ColumnCount());
                    TclObj colLabels;
                    TclObj label, value;
                    BOOL success = FALSE;

                    if (objc == 4) {
                        colLabels = TclObj(objv[3]);
                        if (colLabels.llenght() != colCount) {
                            THROWSTR("Invalid number of column labels");
			}
                    } else {
                        colLabels = ColumnLabels();
                    }

                    if (!row.isNull()) {
                        for (int i = 0; i < colCount; ++i) {
                            label = colLabels.lindex(i, interp);
                            value = row.lindex(i, interp);
                            if (!Tcl_SetVar2(interp, TclObj(objv[2]), 
                                label, value, TCL_LEAVE_ERR_MSG)) 
                                return TCL_ERROR;
                        }
                        // fetch ok, return boolean true
                        success = TRUE;
                    }

                    // return success flag
                    Tcl_SetObjResult(interp, TclObj(success));
                }
            }
            break;

        case SET:
            if (objc != 4) {
                THROWSTR(strWrongArgs);
	    }
            SetOption(Tcl_GetStringFromObj(objv[2], NULL), 
			          Tcl_GetStringFromObj(objv[3], NULL));
            Tcl_SetResult(interp, strOK, TCL_STATIC);
            break;

        case GET:
            if (objc != 3)
                THROWSTR(strWrongArgs)
            Tcl_SetObjResult(interp, GetOption(Tcl_GetStringFromObj(objv[2], NULL)));
            break;

        case EVAL:
            if (objc < 3 || objc > 4)
                THROWSTR("wrong # args, should be eval proc [args]")
            Eval(interp, TclObj(objv[2]), objc-3, objv+3);
            Tcl_SetResult(interp, strOK, TCL_STATIC);
            break;

        case READ:
            if (objc < 3 || objc > 4)
                THROWSTR("wrong # args, should be read array [args]")
            Read(interp, TclObj(objv[2]), objc-3, objv+3);
            Tcl_SetResult(interp, strOK, TCL_STATIC);
            break;

        case ROWCOUNT:
            if (SQLRowCount(stmt, &count) == SQL_ERROR)
                THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
            Tcl_SetObjResult(interp, TclObj(count));
            break;

        case RUN:
	    if (objc >= 2) {
		--objc;
		++objv;
	    }
	    // fall through

        default:
            Tcl_SetObjResult (interp, Value(interp, objc-1, objv+1));
            break;
        }

        // command successful
        return TCL_OK;
    }
    catch (TclObj obj) {
        Tcl_SetObjResult (interp, obj);
        return TCL_ERROR;
    }
}

void TclStatement::ReserveResultBuffer() {
    RETCODE rc;
    UWORD i;
    BOOL unbound_cols (FALSE);

    // allocate space for column array
    resultBuffer = (ResultBuffer*) Tcl_Alloc(ColumnCount()*sizeof(ResultBuffer));
    if (!resultBuffer)
        THROWSTR(strMemoryAllocationFailed)
    memset (resultBuffer, 0, ColumnCount()*sizeof(ResultBuffer));

    // allocate space for columns and bind them
    for (i=0; i<ColumnCount(); ++i) {
        // find out the max column length
        while ((rc = SQLColAttributes(stmt, (UWORD)(i+1), SQL_COLUMN_DISPLAY_SIZE, 
            NULL, 0, NULL, &resultBuffer[i].cbValueMax)) == SQL_STILL_EXECUTING) Tcl_Sleep(0);
        if (rc == SQL_ERROR) {
            THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
        }

        // column type
        while ((rc = SQLColAttributes(stmt, (UWORD)(i+1), SQL_COLUMN_TYPE, 
            NULL, 0, NULL, &resultBuffer[i].fSqlType)) == SQL_STILL_EXECUTING) Tcl_Sleep(0);
        if (rc == SQL_ERROR) {
            THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
        }
        
        // target type
        resultBuffer[i].fTargetType = MapSqlType (resultBuffer[i].fSqlType);

        // determine amount for allocated space
        // variable lenght column types are always allocated after fetch
        // some drivers (e.g. MS SqlServer 6.5) require, that you
        // may hava unbound columns only after all bound columns

        if (resultBuffer[i].cbValueMax == SQL_NO_TOTAL ||
            resultBuffer[i].fSqlType == SQL_LONGVARBINARY || 
            resultBuffer[i].fSqlType == SQL_LONGVARCHAR ||
            resultBuffer[i].cbValueMax > MAX_BLOB_COLUMN_LENGTH) {
            // column is not bound
            resultBuffer[i].strResult = NULL;
            resultBuffer[i].cbValueMax = 0;
            resultBuffer[i].boundColumn = FALSE;
            if (!(pDb->DriverInfo() & SQL_GD_ANY_COLUMN)) 
                unbound_cols = TRUE;
        } else {
            // reserve buffer
            resultBuffer[i].strResult = (char*) 
                Tcl_Alloc((resultBuffer[i].cbValueMax+1)*sizeof(char));

            if (!resultBuffer[i].strResult)
                THROWSTR(strMemoryAllocationFailed)

            memset (resultBuffer[i].strResult, 0, resultBuffer[i].cbValueMax*sizeof(char)+1);

            // bind
            if (!unbound_cols) {
                while ((rc = SQLBindCol(stmt, (UWORD)(i+1), resultBuffer[i].fTargetType, 
		            resultBuffer[i].strResult, resultBuffer[i].cbValueMax+1, 
			        &(resultBuffer[i].cbValue))) == SQL_STILL_EXECUTING) Tcl_Sleep(0);
                if (rc == SQL_ERROR) {
                    THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
                }
                resultBuffer[i].boundColumn = TRUE;
            } else {
                resultBuffer[i].boundColumn = FALSE;
            }
        }
    }
}

void TclStatement::FreeResultBuffer() {
    if (resultBuffer) {
        for (int i=0; i<ColumnCount(); ++i)
            if (resultBuffer[i].strResult)
                Tcl_Free(resultBuffer[i].strResult);
        Tcl_Free((char*)resultBuffer);
        resultBuffer = NULL;
        colCount = -1;
    }
}


int TclStatement::ColumnCount() {
    RETCODE rc;

    // the column count of a single statement is always the same,
    // and it is enough to check it once
    if (colCount == -1) {
        SWORD tmp = 0;
        while ((rc = SQLNumResultCols(stmt, &tmp)) == SQL_STILL_EXECUTING) Tcl_Sleep(0);
        if (rc == SQL_ERROR)
            THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
        else if (rc == SQL_INVALID_HANDLE)
            THROWSTR(strInvalidHandle)

        colCount = tmp;
    }
    return colCount;
}

TclObj TclStatement::ColumnLabels() {
    // the column labels are always the same,
    // and it is enough to check them once
    if (colLabels.isNull()) {
        colLabels = Columns(0, NULL);
    }
    return colLabels;
}

TclObj TclStatement::Result() {
    if (useMultipleResultSets == false) {
	TclObj result;

	// if sql has result set, return it, otherways simply OK
	if (ColumnCount() > 0) {
	    //fetch rows
	    TclObj row;

	    // remember to reset object before each fetch
	    while (Fetch(row.set(NULL))) {
		result.appendElement(row);
	    }
	} else {
	    long i;
	    SQLRowCount(stmt, (SQLINTEGER *) &i);
	    result = i;
	}
	return result;
    } else {
	TclObj sets;

	// foreach results sets
	RETCODE rc;
	bool more = false;
	do {
	    FreeResultBuffer();
	    resultBuffer = 0;
			
	    if (ColumnCount() > 0) {
		TclObj result;
		//fetch rows
		TclObj row;
		// remember to reset object before each fetch
		while (Fetch(row.set(NULL))) {
		    result.appendElement(row);
		}

		// add to list of sets
		sets.appendElement(result);				
				
		// look if more results are waiting
		while ((rc = SQLMoreResults(stmt)) == SQL_STILL_EXECUTING) {
		    SqlWait(0);
		}
		if (rc == SQL_ERROR) {
		    THROWOBJ(SqlErr(env, pDb->DBC(), stmt));
		    more = false;
		}

		more = (rc == SQL_NO_DATA ? false : true);


	    } else {
		long i;
		SQLRowCount(stmt, (SQLINTEGER *) &i);
		return i;
	    }

	} while(more);

	return sets;
    }
}

TclObj TclStatement::Value(Tcl_Interp *interp, int objc, TCL_CMDARGS) {
    // first execute
    Execute(interp, objc, objv);

    // return result in result
    return Result();
}

void TclStatement::Eval(Tcl_Interp *interp, TclObj proc, int objc, TCL_CMDARGS) {
    // first execute
    Execute(interp, objc, objv);

    // if sql has result set, return it, otherways simply OK
    if (ColumnCount() > 0) {
        TclObj row;
        //fetch rows, always initialize row to the procedure name
        while (Fetch(row.set(proc))) {
            row.eval(interp);
        }
    } else {
        THROWSTR("Cannot evaluate, no data.")
    }
}

void TclStatement::Read(Tcl_Interp *interp, TclObj arraySpec, int objc, TCL_CMDARGS) {
    TclObj row, columnLabels, arrayName;
    int dataColumns;
    BOOL createSubIndex;

    // solve the proper mode type
    dataColumns = ColumnCount() - 1;
    if (dataColumns == arraySpec.llenght()) {
        createSubIndex = FALSE;
    } else if (arraySpec.llenght() == 1) {
        createSubIndex = TRUE;
        columnLabels = ColumnLabels();
        arrayName = arraySpec;
    } else {
        TclObj message ("Invalid array specification: ");
        message.append(arraySpec);

        THROWOBJ(message);
    }

    // first execute
    Execute(interp, objc, objv);

    //fetch rows, always clear row before fetch
    while (Fetch(row.set(NULL))) {
        for (int i = 0; i < dataColumns; ++i) {
            TclObj index ((char*) row.lindex(0));
            TclObj value (row.lindex(i+1));

            if (createSubIndex) {
                index.append(",");
                index.append(columnLabels.lindex(i+1));
            } else {
                arrayName = arraySpec.lindex(i);
            }

            if (!Tcl_SetVar2(interp, arrayName, index, value, TCL_LEAVE_ERR_MSG)) 
                #if TCL_MAJOR_VERSION == 8
                THROWOBJ(Tcl_GetObjResult(interp))
                #else
                THROWSTR(interp->result)
                #endif
        }
    }
}

TclObj TclStatement::Columns(int objc, TCL_CMDARGS) {
    int i, arg;
    TclObj result;

	for (i = 1; i <= ColumnCount(); ++i) {
		// read column data for all specified argument
		TclObj element;
        if (objc > 0) {
	        for (arg = 0; arg < objc; ++ arg) {
                UWORD attr = AttrDef(Tcl_GetStringFromObj(objv[arg],NULL));
                element.appendElement(ColumnInfo (i, attr));
		    }
        } else {
            // default argument, if none given
            element.appendElement(ColumnInfo (i, SQL_COLUMN_LABEL));
        }
        result.appendElement (element);
	}
    return result;
}

TclObj TclStatement::ColumnInfo(int col, UWORD attr) {
    char   strData [256];
	SDWORD wordData;

	switch (attr) {
	case SQL_COLUMN_LABEL:
	case SQL_COLUMN_TYPE_NAME:
	case SQL_COLUMN_TABLE_NAME:
	case SQL_COLUMN_NAME:
	case SQL_COLUMN_OWNER_NAME:
	case SQL_COLUMN_QUALIFIER_NAME:
		if (SQLColAttributes(stmt, col, attr, 
			(UCHAR*)strData, 256, NULL, NULL) == SQL_ERROR)
			THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
		return TclObj(strData, pDb->Encoding());
		break;

	case SQL_COLUMN_DISPLAY_SIZE:
	case SQL_COLUMN_TYPE:
	case SQL_COLUMN_PRECISION:
	case SQL_COLUMN_SCALE:
    case SQL_COLUMN_NULLABLE:
    case SQL_COLUMN_UPDATABLE:
		if (SQLColAttributes(stmt, col, attr, 
			NULL, 0, NULL, &wordData) == SQL_ERROR)
			THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
        return TclObj(wordData);
		break;

	default: 
		THROWSTR("Invalid column data definition")
        return TclObj(); // to keep compiler happy
		break;
	};
}

BOOL TclStatement::Fetch(TclObj& row) {
    RETCODE rc;
    UWORD i;

    //result buffer is reserved here, always only once per statement
    if (!resultBuffer && ColumnCount() > 0)
        ReserveResultBuffer();

    //fetch row
    rc = Fetch1();
    switch (rc) {
    case SQL_NO_DATA_FOUND:
        return FALSE; 
    case SQL_ERROR:
        THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
    case SQL_INVALID_HANDLE:
        THROWSTR(strInvalidHandle)
    }

	for (i=0; i<ColumnCount(); ++i) {
        if (!resultBuffer[i].strResult) {
            // Variable lenght columns are read with SQLGetData.
            // First get the length with the first call by setting bufsize = 1
            // (1 for string null terminator, obligatory with some drivers)
            char dummy;
            while ((rc = SQLGetData(stmt, (UWORD) (i+1), resultBuffer[i].fTargetType, 
                &dummy, resultBuffer[i].fTargetType == SQL_C_CHAR ? 1 : 0,
                &(resultBuffer[i].cbValue))) == SQL_STILL_EXECUTING) Tcl_Sleep(0); 
            if (rc == SQL_ERROR) {
                THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
            }

        } else if (!resultBuffer[i].boundColumn) {
            // if column has reserved buffer, but it is not bound, get data here
            while ((rc = SQLGetData(stmt, i+1, 
                resultBuffer[i].fTargetType, 
                resultBuffer[i].strResult, 
                resultBuffer[i].cbValueMax+1, &(resultBuffer[i].cbValue))) == SQL_STILL_EXECUTING) Tcl_Sleep(0); 
            if (rc == SQL_ERROR) {
                THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
            }
        }

        if (resultBuffer[i].cbValue == SQL_NULL_DATA) {
            row.appendElement(TclObj());
        } else {
            TclObj element;

            if (resultBuffer[i].strResult) {
                // assing element from bind buffer
                element = TclObj (resultBuffer[i].strResult, 
                    min(resultBuffer[i].cbValue,resultBuffer[i].cbValueMax));
            } else if (resultBuffer[i].cbValue == SQL_NO_TOTAL) {
                // this is not polite of the driver, 
                // now we have to get the data in pieces
                #define BUFSIZE 65536
                TclObj buffer;
                buffer.setLength(BUFSIZE);
                while (resultBuffer[i].cbValue == SQL_NO_TOTAL || resultBuffer[i].cbValue > BUFSIZE) {
                    // get buffer full of data
                    while ((rc = SQLGetData(stmt, i+1, resultBuffer[i].fTargetType, 
                        (char*) buffer, 
                        BUFSIZE + (resultBuffer[i].fTargetType == SQL_C_CHAR ? 1 : 0), 
                        &(resultBuffer[i].cbValue))) == SQL_STILL_EXECUTING) Tcl_Sleep(0); 
                    if (rc == SQL_ERROR) {
                        THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
                    }

                    element.append(buffer, resultBuffer[i].cbValue == SQL_NO_TOTAL ? BUFSIZE : min(BUFSIZE,resultBuffer[i].cbValue));
                }
                #undef BUFSIZE
            } else {
			    // allocate space for variable length data
                element.setLength(resultBuffer[i].cbValue);

                // finally, get the actual data
                while ((rc = SQLGetData(stmt, i+1, resultBuffer[i].fTargetType, 
                    (char*) element, 
                    resultBuffer[i].cbValue + 1, 
                    &(resultBuffer[i].cbValue))) == SQL_STILL_EXECUTING) Tcl_Sleep(0); 

				// set element length again. Some drivers return originally too long value
                element.setLength(resultBuffer[i].cbValue);

                if (rc == SQL_ERROR) {
                    THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
                }
            }

            if (EncodedType(resultBuffer[i].fSqlType))
                element.Decode(pDb->Encoding());
            row.appendElement(element);
        }
    }

    // success
    return TRUE;
}

void TclStatement::SetOption(char* option, char* value) {
    RETCODE rc;
    UWORD op;
    UDWORD val = 0;

    op = StrToNum (option, stmtOp);
	
    switch (op) {
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
    }

    rc = SQLSetStmtOption(stmt, op, val);
    if (rc == SQL_ERROR) {
        THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt));
    }
}

TclObj TclStatement::GetOption(char* option) {
    RETCODE rc;
    UWORD op;
    UDWORD val;
    TclObj retVal;

    op = StrToNum (option, stmtOp);

    rc = SQLGetStmtOption(stmt, op, &val);
    if (rc == SQL_ERROR) {
        THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt));
    }
	
    switch (op) {
    case SQL_NOSCAN:
    case SQL_ASYNC_ENABLE:
	return NumToStr ((short) val, booleanOp);

    case SQL_CONCURRENCY:
	return TclObj(NumToStr ((short) val, concurrencyOp));

    case SQL_QUERY_TIMEOUT:
    case SQL_MAX_ROWS:
    case SQL_MAX_LENGTH:
    case SQL_ROWSET_SIZE:
	return TclObj(val);
	break;

    case SQL_CURSOR_TYPE:
	return NumToStr((short) val, cursorOp);

    default:
	return TclObj(); // to keep compiler happy
    }
}

// virtual defined function, overridden in recordset object
RETCODE TclStatement::Fetch1 (void) {
    RETCODE rc;

    // Allow running in async mode. 
    while ((rc = SQLFetch(stmt)) == SQL_STILL_EXECUTING) SqlWait(0);
    return rc;
}

TclSqlStatement::TclSqlStatement(TclDatabase& db, TclObj sql, bool multiSets) 
    : TclStatement (db), argDefBuffer (NULL) {
    RETCODE rc;

    useMultipleResultSets = multiSets;

    // encode TclObj to selected character set
    sql.Encode(db.Encoding());

    // prepare statement
    while ((rc = SQLPrepare(stmt, (UCHAR*) sql.EncodedValue(), sql.EncodedLenght())) == SQL_STILL_EXECUTING) Tcl_Sleep(0);
    if (rc == SQL_ERROR) 
        THROWOBJ(SqlErr(env, db.DBC(), stmt))

    while ((rc = SQLNumParams(stmt, &argCount)) == SQL_STILL_EXECUTING) Tcl_Sleep(0);
    if (rc == SQL_ERROR) {
        // All drivers do not support this. This is not fatal, 
        // we just set some upper limit here
        argCount = 32;
    }

    // allocate space for argument definition buffer
    if (argCount > 0) {
        argDefBuffer = (ArgDefBuffer*) Tcl_Alloc(argCount*sizeof(ArgDefBuffer));
        if (!argDefBuffer) {
            THROWSTR(strMemoryAllocationFailed);
	}
    }

    // initialize argument definitions, these can be overridden later
    // by giving explicit type definitions
    for (int i = 0; i < argCount; ++i) {
        // default source type always char
        while ((rc = SQLDescribeParam(stmt, i,
            &argDefBuffer[i].fSqlType,
            &argDefBuffer[i].cbColDef,
            &argDefBuffer[i].ibScale,
            &argDefBuffer[i].fNullable)) == SQL_STILL_EXECUTING) Tcl_Sleep(0);
        if (rc == SQL_ERROR) {
            // if driver does not support this, give some defaults
            argDefBuffer[i].fSqlType = SQL_VARCHAR;
            argDefBuffer[i].cbColDef = 0;
            argDefBuffer[i].ibScale = 0;
            argDefBuffer[i].fNullable = SQL_NULLABLE_UNKNOWN;
        }
        argDefBuffer[i].fSourceType = MapSqlType (argDefBuffer[i].fSqlType);
    }
}

void TclSqlStatement::SetArgDefs(Tcl_Interp *interp, TclObj defObjList) {
    int argc, subc, tmpInt;
    TclObj tmpObj;
    TclObj typeDefObj;

    argc = defObjList.llenght(interp);

    if (argc > argCount) {
        THROWSTR("Too many argument definitions");
    }

    for (int i = 0; i < argc; ++i) {
        // corresponding single type definition
        typeDefObj = defObjList.lindex(i, interp);

        // argument definition list length
        subc = typeDefObj.llenght(interp);

        // type definition parts: {type lenght scale}
        switch (subc) {
        case 3:
            tmpInt = typeDefObj.lindex(2, interp).asInt(interp);
            argDefBuffer[i].ibScale = (SWORD) tmpInt;
        case 2:
            tmpInt = typeDefObj.lindex(1, interp).asInt(interp);
            argDefBuffer[i].cbColDef = tmpInt;
        case 1:
            tmpObj = typeDefObj.lindex(0, interp);
            argDefBuffer[i].fSqlType = SqlType(tmpObj);
            argDefBuffer[i].fSourceType = MapSqlType (argDefBuffer[i].fSqlType);
            break;
        default:
            THROWSTR("Invalid type definition")
        }
    }
}

void TclSqlStatement::Execute(Tcl_Interp *interp, int objc, TCL_CMDARGS) {
    RETCODE rc;
    int argc, i, sqlarglen;
    TclObj tmpObj, tmpObj2; 
    TclObj sqlArguments;
    TclObj sqlArg;

    switch (objc) {
    case 2:
        // syntax removed from tclodbc 2.1, use argument definition in
        // statement creation
        THROWSTR(strOldSyntax)
        // set argument definitions in buffer, overriding any previously
        // set or default values
        SetArgDefs(interp, objv[0]);
    case 1:
        // bind optional sql variables
        // sql argument count = count of items in the last argument

        // we duplicate object here, because we want to decode each value
        sqlArguments = TclObj(objv[objc-1]);
        argc = sqlArguments.llenght(interp);

        if (argc > argCount)
            THROWSTR("Too many arguments")

        for (i = 0; i < argc; ++i) {
            sqlArg = sqlArguments.lindex(i, interp); 

            // only defined types are encoded
            if (EncodedType(argDefBuffer[i].fSqlType))
                sqlArg.Encode(pDb->Encoding());

            sqlarglen = sqlArg.EncodedLenght();

            if (sqlarglen == 0) {
                argDefBuffer[i].cbValue = SQL_NULL_DATA;
			} else {
                argDefBuffer[i].cbValue = sqlarglen;
                if (argDefBuffer[i].cbColDef == 0)
                    // this can't be zero, give here the best guess
                    argDefBuffer[i].cbColDef = sqlarglen;  
            }

            rc = SQLBindParameter(stmt, (UWORD)(i+1), SQL_PARAM_INPUT, 
                    argDefBuffer[i].fSourceType, argDefBuffer[i].fSqlType, 
                    argDefBuffer[i].cbColDef, argDefBuffer[i].ibScale, 
                    (void*) sqlArg.EncodedValue(), sqlarglen+1, &argDefBuffer[i].cbValue);

            if (rc == SQL_ERROR) 
                THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
            else if (rc == SQL_INVALID_HANDLE) 
                THROWSTR(strInvalidHandle)
        }
    case 0:
        break;
    default:
        THROWSTR("Invalid arguments, should be stmt [argtypelist] [arglist]")
    }

    // execute sql, always close previous cursor if necessary
    rc = SQLFreeStmt(stmt, SQL_CLOSE);
    if (rc == SQL_ERROR)
        THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
    else if (rc == SQL_INVALID_HANDLE) 
        THROWSTR(strInvalidHandle)

    // Allow running in async mode. 
    while ((rc = SQLExecute(stmt)) == SQL_STILL_EXECUTING) SqlWait(1);
    
    if (rc == SQL_ERROR) 
        THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
    else if (rc == SQL_INVALID_HANDLE) 
        THROWSTR(strInvalidHandle)
}

void TclTableQuery::Execute(Tcl_Interp *interp, int objc, TCL_CMDARGS) {
    RETCODE rc;
    TclObj tableName;

    if (objc) {
        tableName = TclObj(objv[0]);
        tableName.Encode(pDb->Encoding());
    }

    // close previous cursor
    rc = SQLFreeStmt(stmt, SQL_CLOSE);
    if (rc == SQL_ERROR)
        THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
    else if (rc == SQL_INVALID_HANDLE) 
        THROWSTR(strInvalidHandle)

    // read table information
    while ((rc = SQLTables(stmt, NULL, 0, NULL, 0,  
        tableName.isNull() ? NULL : (UCHAR*) tableName.EncodedValue(), 
        tableName.isNull() ? 0 : tableName.EncodedLenght(), 
        NULL, 0)) == SQL_STILL_EXECUTING) SqlWait(1);

    if (rc == SQL_ERROR)
        THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
}

void TclColumnQuery::Execute(Tcl_Interp *interp, int objc, TCL_CMDARGS) {
    RETCODE rc;
    TclObj tableName;

    if (objc) {
        tableName = TclObj(objv[0]);
        tableName.Encode(pDb->Encoding());
    }

    // close previous cursor
    rc = SQLFreeStmt(stmt, SQL_CLOSE);
    if (rc == SQL_ERROR)
        THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
    else if (rc == SQL_INVALID_HANDLE) 
        THROWSTR(strInvalidHandle)

    // read column information
    while ((rc = SQLColumns(stmt, NULL, 0, NULL, 0, 
        tableName.isNull() ? NULL : (UCHAR*) tableName.EncodedValue(), 
        tableName.isNull() ? 0 : tableName.EncodedLenght(), NULL, 0)) == SQL_STILL_EXECUTING) SqlWait(1);

    if (rc == SQL_ERROR) 
        THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
}

void TclIndexQuery::Execute(Tcl_Interp *interp, int objc, TCL_CMDARGS) {
    RETCODE rc;
    TclObj tableName;

    if (objc != 1)
        THROWSTR("wrong # args, tablename is required")

    tableName = TclObj(objv[0]);
    tableName.Encode(pDb->Encoding());

    // close previous cursor
    rc = SQLFreeStmt(stmt, SQL_CLOSE);
    if (rc == SQL_ERROR)
        THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
    else if (rc == SQL_INVALID_HANDLE) 
        THROWSTR(strInvalidHandle)

    // read index information
    while ((rc = SQLStatistics(stmt, NULL, 0, NULL, 0, (UCHAR*) tableName.EncodedValue(),
        tableName.EncodedLenght(), SQL_INDEX_ALL, SQL_ENSURE)) == SQL_STILL_EXECUTING) SqlWait(1);
    if (rc == SQL_ERROR) 
        THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
}

void TclTypeInfoQuery::Execute(Tcl_Interp *interp, int objc, TCL_CMDARGS) {
    RETCODE rc;
    TclObj typeId;

    if (objc != 1)
        THROWSTR("wrong # args, typeid is required")

    typeId = TclObj(objv[0]);

    // close previous cursor
    rc = SQLFreeStmt(stmt, SQL_CLOSE);
    if (rc == SQL_ERROR)
        THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
    else if (rc == SQL_INVALID_HANDLE) 
        THROWSTR(strInvalidHandle)

    // read type information
    while ((rc = SQLGetTypeInfo(stmt, typeId.asInt())) == SQL_STILL_EXECUTING) SqlWait(1);
    if (rc == SQL_ERROR) 
        THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
}

void TclPrimaryKeysQuery::Execute(Tcl_Interp *interp, int objc, TCL_CMDARGS) {
    RETCODE rc;
    TclObj tableName;

    if (objc != 1)
        THROWSTR("wrong # args, tablename is required")

    tableName = TclObj(objv[0]);
    tableName.Encode(pDb->Encoding());

    // close previous cursor
    rc = SQLFreeStmt(stmt, SQL_CLOSE);
    if (rc == SQL_ERROR)
        THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
    else if (rc == SQL_INVALID_HANDLE) 
        THROWSTR(strInvalidHandle)

    // read type information
    while ((rc = SQLPrimaryKeys(stmt, (UCHAR*) strEmpty, 0, (UCHAR*) strEmpty, 0, 
        (UCHAR*) tableName.EncodedValue(), tableName.EncodedLenght())) == SQL_STILL_EXECUTING) SqlWait(1);
    if (rc == SQL_ERROR) 
        THROWOBJ(SqlErr(env, SQL_NULL_HDBC, stmt))
}

void TclStatement::SqlWait (int delay) {
    // Do single event.
    // If there was nothing to do, sleep a bit before possibly looping again 
    // to avoid busy loop.
    if (!Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT) && delay) {
        Tcl_Sleep(delay);  
    }
}
