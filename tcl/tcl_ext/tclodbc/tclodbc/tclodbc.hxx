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
// 2.1 New version, added database typeinfo and statement rowcount      //
//                                                                      //
// 2.2 New version, new binary column handling and some bug fixes       //
//                  some contributions from Robert Black                //
//                                                                      //
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
// STANDARD INCLUDES
//

extern "C" {
#include <string.h>
#include <stdlib.h>

#include <tcl.h>
}

#ifdef WIN32
#include <windows.h>
#define INCLUDE_EXTENSIONS
#else
#define _declspec(x) int
#ifdef HAVE_UNIXODBC
#define INCLUDE_EXTENSIONS // Uncomment this if odbc extensions available
#endif
#ifdef HAVE_IODBC
#define INCLUDE_EXTENSIONS // Uncomment this if odbc extensions available
#endif
#endif

#include "tclobj.hxx"

//////////////////////////////////////////////////////////////////////////
// ODBC INCLUDES 
//

/* set _WCHAR_T so sqltypes.h is happy (until 3779905 is fixed) */
#define       _WCHAR_T
extern "C" {
#ifdef HAVE_IODBC                       // Using the free IODBC driver
/* I am not sure what version this is...
#include <iodbc.h>
#include <isql.h>
#include <isqlext.h>*/
/* iODBC 2.12 */
#include <isql.h>
#include <isqlext.h>
#include <iodbcinst.h>
#else
#include <sql.h>
#include <odbcinst.h>
#include <sqlext.h>
#endif
}

// We need these definitions if we're not using VC++
#ifndef _MSC_VER
typedef unsigned char BOOLEAN;
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

//////////////////////////////////////////////////////////////////////////
// DEFINES
//

#define SQL_MULTIPLE_RESULTSETS 12345

// environment handle
extern HENV env;

// Default lenght for blob buffer. If the database does not return
// the display lenght of a column, this value is used as default.
// However, if more data is available after fetch, more space
// is allocated when necessary.

#define MAX_BLOB_COLUMN_LENGTH  1024

// Maximum number of accepted sql arguments

#define MAX_SQL_ARGUMENTS 32


// A struct for storing strings with integer constants
struct NumStr 
{
    short numeric;
    char* string;
};

#ifndef BOOL
#define BOOL int
#endif

#ifndef FALSE
#define FALSE               0
#endif

#ifndef TRUE
#define TRUE                1
#endif

#define TCLODBC_OPTIONS     1000 
#define TCLODBC_ENCODING    1000 

// A struct for storing necessary data for retrieving sql results
struct ResultBuffer
{
    SDWORD cbValue;
    SDWORD cbValueMax;
    SDWORD fSqlType;
    BOOL   boundColumn;
    char*  strResult;
    SWORD  fTargetType;
};

struct ArgDefBuffer
{
    SWORD   fSqlType;
    UDWORD  cbColDef;
    SWORD   ibScale;
    SWORD   fNullable;
    SDWORD  cbValue;
    SWORD   fSourceType;
};


//////////////////////////////////////////////////////////////////////////
// DATABASE OBJECT CLASS HEADERS
//

// Common TclCmdObject ancestor to all database objects. This defines
// common destructing and command dispatching methods.

class TclCmdObject {
public:
    TclCmdObject() : tclCommand(NULL), pNext(NULL), pPrev(NULL) {};

    virtual ~TclCmdObject();
    // Virtual destructor for all database objects

    static void Destroy(ClientData);
    // Static function for destroying database objects

    static int Dispatch(ClientData clientData, Tcl_Interp *interp,
		int objc, TCL_CMDARGS);
    // Static function for dispatching commands to objects

    Tcl_Command tclCommand;
    // token for tcl command in the interpreter

    void AddToMyList (TclCmdObject *);
    // add an object to list

    void RemoveFromList ();
    // add an object to list

    TclCmdObject* Next () {return pNext;};
    TclCmdObject* Prev () {return pPrev;};
    // list navigation

private:
    virtual int Dispatch(Tcl_Interp *interp, int objc, TCL_CMDARGS) = 0;
    // Virtual function for class dependent implementation of
    // static Dispatch().

    TclCmdObject *pNext;
    TclCmdObject *pPrev;
    // links for linking certain objects to each other

};


// Class TclDatabase provides a general database object, with
// connecting and transaction handling methods. Dispatch interface
// is defined for handling database object commands.

class TclStatement; // forward declaration
class TclDatabase : public TclCmdObject {
public:
    enum COMMANDS {STATEMENT, S, RECORDSET, DISCONNECT, TABLES, 
    COLUMNS, INDEXES, AUTOCOMMIT, COMMIT, ROLLBACK, SET, GET, 
    TYPEINFO, PRIMARYKEYS, EVAL, READ};

    enum OPTIONS {CONFIGURE, DATASOURCES, DRIVERS, VERSION, CONNECT};

    TclDatabase(TclObj db, TclObj uid, TclObj passwd);
    // Constructor creates a database object with connection
    // to a named datasource.

    TclDatabase(TclObj connectionstring);
    // Constructor creates a database object with a given connection
    // string.

    virtual ~TclDatabase();
    // Virtual destructor, disconnects from the database and destroys
    // the object.

    void Autocommit(BOOL on);
    // Autocommit sets the autocommit property on or off.

    void Transact(UWORD operation);
    // Transact is used for transaction handling. It is called with 
    // constant SQL_COMMIT or SQL_ROLLBACK.

    void SetOption(Tcl_Interp *interp, char* option, char* value);
	// SetOption is used for setting various connection object
	// otions.

    TclObj GetOption(char* option);
	// GetOption is used for querying object options current values.
	
    const Tcl_Encoding Encoding() {return encoding;};
	// GetOption is used for querying object options current values.

    static TclObj Datasources();
    // List of all registered data source names

    static TclObj Drivers();
    // List of all registered drivers

    static int Configure(Tcl_Interp *interp, int objc, TCL_CMDARGS);
    // List of all registered drivers

    HDBC DBC() {return dbc;};
    // return dbc handle

    UDWORD DriverInfo() {return infoExtensions;};
    // return info from driver
private:
    virtual int Dispatch(Tcl_Interp *interp, int objc, TCL_CMDARGS);
    void AddStatement(TclStatement *);

    bool useMultipleResultSets;

    HDBC dbc;
    Tcl_Encoding encoding;
    UDWORD infoExtensions;
};


// Class TclStatement provides general statement handling methods, 
// including statement handle creation and destroying, executing 
// statement, and handling result set and result buffer.

class TclStatement : public TclCmdObject {
public:
    enum COMMANDS {EXECUTE, MORERESULTS, FETCH, SET, GET, EVAL, RUN, READ, DROP, COLUMNS, ROWCOUNT, CURSORNAME};

    virtual ~TclStatement();
    // Destructor drops the statement handle and frees dynamic structures

    virtual void Execute(Tcl_Interp *interp, int objc, TCL_CMDARGS) = 0;
    // executes the statement with given arguments

    int ColumnCount();
    // returns the count of columns in current result set.

    TclObj ColumnLabels();
    // returns the labels of the columns

    TclObj Result();
    // The result set of an executed statement in a single TclObj.

    TclObj Value(Tcl_Interp *interp, int objc = 0, TCL_CMDARGS = NULL);
    // This first executes the statement, and then returns the result set

    TclObj Columns(int objc, TCL_CMDARGS);
    // returns requested information of the columns in a tcl list.

    TclObj ColumnInfo(int col, UWORD attr);
    // returns requested information of a columns

    BOOL Fetch(TclObj&);
    // Retches and returns the next row in the result set. Returns
    // false if no more data

    void SetOption(char* option, char* value);
	// SetOption is used for setting various connection object
	// otions.

    TclObj GetOption(char* option);
	// GetOption is used for querying object options current values.

    void Eval(Tcl_Interp *interp, TclObj proc, int objc, TCL_CMDARGS);
    // execute statement and eval tcl procedure for each result row

    void Read(Tcl_Interp *interp, TclObj array, int objc, TCL_CMDARGS);
    // execute statement and read result rows to tcl array

    void FreeStmt();
    // statement cleanup

    void SqlWait(int delay);
    // do something while waiting asynchronous sql statement finalize

protected:
    TclStatement(TclDatabase& db);
    // Protected constructor creates a statement connected to a given data 
    // source. This is called from descendents constructors.

    HSTMT stmt;
    TclDatabase* pDb;

    virtual int Dispatch(Tcl_Interp *interp, int objc, TCL_CMDARGS);
protected:
    bool useMultipleResultSets;
private:
    int colCount;
    TclObj colLabels;
    ResultBuffer *resultBuffer;

    void ReserveResultBuffer();
    void FreeResultBuffer();

    virtual RETCODE Fetch1 ();
};

class TclSqlStatement : public TclStatement {
public:
    TclSqlStatement(TclDatabase& db, TclObj sql, bool multiSets /*, BOOL recordset = FALSE*/);
    // creates a sql statement object connected to a given data 
    // source. Sql clause is given as argument.

    void SetArgDefs (Tcl_Interp *interp, TclObj defObj);
    // sets argument definitions based on tcl list object

protected:
    virtual void Execute(Tcl_Interp *interp, int objc, TCL_CMDARGS);

private:
    ArgDefBuffer *argDefBuffer;
    SWORD argCount;
};

class TclTableQuery : public TclStatement {
public:
    TclTableQuery(TclDatabase& db) : TclStatement(db) {};
    // creates a table query object connected to a given 
    // data source. 

private:
    virtual void Execute(Tcl_Interp *interp, int objc, TCL_CMDARGS);
};

class TclColumnQuery : public TclStatement {
public:
    TclColumnQuery(TclDatabase& db) : TclStatement(db) {};
    // creates a column query object connected to a given 
    // data source. 

private:
    virtual void Execute(Tcl_Interp *interp, int objc, TCL_CMDARGS);
};

class TclIndexQuery : public TclStatement {
public:
    TclIndexQuery(TclDatabase& db) : TclStatement(db) {};
    // creates a index query object connected to a given 
    // data source. 

private:
    virtual void Execute(Tcl_Interp *interp, int objc, TCL_CMDARGS);
};

class TclTypeInfoQuery : public TclStatement {
public:
    TclTypeInfoQuery(TclDatabase& db) : TclStatement(db) {};
    // creates a typeinfo query object connected to a given 
    // data source. 

private:
    virtual void Execute(Tcl_Interp *interp, int objc, TCL_CMDARGS);
};

class TclPrimaryKeysQuery : public TclStatement {
public:
    TclPrimaryKeysQuery(TclDatabase& db) : TclStatement(db) {};
    // creates a primary keys query object connected to a given 
    // data source. 

private:
    virtual void Execute(Tcl_Interp *interp, int objc, TCL_CMDARGS);
};

//////////////////////////////////////////////////////////////////////////
// COMMON STRINGS AND STRINGTABLES
//

extern char* strVersion;
extern char* strMemoryAllocationFailed;
extern char* strInvalidHandle;
extern char* strOK;
extern char* strTables;
extern char* strColumns;
extern char* strIndexes;
extern char* strPrimarykeys;
extern char* strTypeinfo;
extern char* strEval;
extern char* strRead;
extern char* strGet;
extern char* strSet;
extern char* strAutocommit;
extern char* strWrongArgs;
extern char* strUsage;
extern char* strCmdNotAvailable;
extern char* strEmpty;

extern char* strConcurrency;
extern char* strMaxrows;
extern char* strTimeout;
extern char* strMaxlenght;
extern char* strRowsetsize;
extern char* strCursortype;

extern char* strInvalidOption;

extern char* strOldSyntax;

extern NumStr sqlType [];
extern NumStr attrDef [];
extern NumStr concurrencyOp [];
extern NumStr cursorOp [];
extern NumStr connectOp [];
extern NumStr booleanOp [];
extern NumStr stmtOp [];
extern NumStr configOp [];
extern NumStr databaseOptions [];
extern NumStr databaseCmds [];
extern NumStr statementCmds [];
extern NumStr globalOptions [];

//////////////////////////////////////////////////////////////////////////
// FUNCTION DECLARATIONS
//

TclObj SqlErr (HENV env, HDBC dbc, HSTMT stmt);
short StrToNum (char *str, NumStr array[], 
                char* errMsg = strInvalidOption,
                BOOLEAN allowNumeric = TRUE);
TclObj NumToStr (short num, NumStr array[]);

inline short SqlType (char *strType) {
    return StrToNum(strType, sqlType, "Invalid sql type: ");
};

inline short AttrDef (char *strDef) {
    return StrToNum(strDef, attrDef, "Invalid attribute: ");
};

inline short ConfigOp (char *strDef) {
    return StrToNum(strDef, configOp);
};

BOOL EncodedType (int i);



SWORD MapSqlType (SDWORD colType);

#ifdef _DEBUG
int tclodbc_validateNumStrArrays();
int tclodbc_validateStrToNumFunction();
#endif

