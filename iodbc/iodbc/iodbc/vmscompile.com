$ ! To compile iODBC for VMS, just type $ @VMSCOMPILE or $ @VMSCOMPILE DEBUG
$ ! Compile *.c, and remember all object files created.
$ ! Create a shared library from all the created object files.
$ !---------------------------------------------------------------------------
$ debug=0
$ if p1 .eqs. "DEBUG" then debug=1
$ ! Find version number by searching the configure. file
$ search/exact/out=x.tmp configure. "VERSION="
$ version=""
$ open/read fi x.tmp
$ read/end=CLOSEFILE fi version
$ CLOSEFILE:
$ close fi
$ version=version-"VERSION="
$ if version .eqs. ""
$ then
$   version="1.1"
$   write sys$output "Could not find the version number of iODBC in CONFIGURE."
$ endif
$ write sys$output "Creating iODBC version ''version'"
$ stdopts="/INCLUDE=SYS$DISK:[]/PREFIX=ALL/NOLIS"
$ configure="/DEFINE=(""VERSION=""""''version'"""""")"
$ debugopt=""
$ if debug then debugopt="/DEBUG"
$ if debug then stdopts=stdopts+"/NOOPT"
$ objects=""
$ NEXTFILE:
$   file=f$search("*.c")
$   if file .eqs. "" then goto LINKIT
$   filename=f$parse(file,,,"name")
$   objects=objects+filename+","
$   write sys$output "$ cc''debugopt' ''filename'"
$   cc'stdopts' 'configure' 'debugopt' 'filename'
$   goto NEXTFILE
$ LINKIT:
$ write sys$output "$ LINK''debugopt' IODBC.EXE"
$ open/write fo x.opt
$ sversion=version-"."
$ write fo "GSMATCH=LEQ,1,"+sversion
$ close fo
$ link/share=IODBC 'debugopt' 'objects' x/opt,sys$input/opt
SYMBOL_VECTOR=(SQLTables=PROCEDURE)
SYMBOL_VECTOR=(SQLColumns=PROCEDURE)
SYMBOL_VECTOR=(SQLStatistics=PROCEDURE)
SYMBOL_VECTOR=(SQLTablePrivileges=PROCEDURE)
SYMBOL_VECTOR=(SQLColumnPrivileges=PROCEDURE)
SYMBOL_VECTOR=(SQLSpecialColumns=PROCEDURE)
SYMBOL_VECTOR=(SQLPrimaryKeys=PROCEDURE)
SYMBOL_VECTOR=(SQLForeignKeys=PROCEDURE)
SYMBOL_VECTOR=(SQLProcedures=PROCEDURE)
SYMBOL_VECTOR=(SQLProcedureColumns=PROCEDURE)
SYMBOL_VECTOR=(SQLAllocEnv=PROCEDURE)
SYMBOL_VECTOR=(SQLAllocConnect=PROCEDURE)
SYMBOL_VECTOR=(SQLConnect=PROCEDURE)
SYMBOL_VECTOR=(SQLDriverConnect=PROCEDURE)
SYMBOL_VECTOR=(SQLBrowseConnect=PROCEDURE)
SYMBOL_VECTOR=(SQLDisconnect=PROCEDURE)
SYMBOL_VECTOR=(SQLFreeConnect=PROCEDURE)
SYMBOL_VECTOR=(SQLFreeEnv=PROCEDURE)
SYMBOL_VECTOR=(SQLExecute=PROCEDURE)
SYMBOL_VECTOR=(SQLExecDirect=PROCEDURE)
SYMBOL_VECTOR=(SQLNativeSql=PROCEDURE)
SYMBOL_VECTOR=(SQLParamData=PROCEDURE)
SYMBOL_VECTOR=(SQLPutData=PROCEDURE)
SYMBOL_VECTOR=(SQLCancel=PROCEDURE)
SYMBOL_VECTOR=(SQLGetFunctions=PROCEDURE)
SYMBOL_VECTOR=(SQLGetInfo=PROCEDURE)
SYMBOL_VECTOR=(SQLGetTypeInfo=PROCEDURE)
SYMBOL_VECTOR=(SQLSetConnectOption=PROCEDURE)
SYMBOL_VECTOR=(SQLSetStmtOption=PROCEDURE)
SYMBOL_VECTOR=(SQLGetConnectOption=PROCEDURE)
SYMBOL_VECTOR=(SQLGetStmtOption=PROCEDURE)
SYMBOL_VECTOR=(SQLAllocStmt=PROCEDURE)
SYMBOL_VECTOR=(SQLFreeStmt=PROCEDURE)
SYMBOL_VECTOR=(SQLPrepare=PROCEDURE)
SYMBOL_VECTOR=(SQLSetParam=PROCEDURE)
SYMBOL_VECTOR=(SQLBindParameter=PROCEDURE)
SYMBOL_VECTOR=(SQLDescribeParam=PROCEDURE)
SYMBOL_VECTOR=(SQLParamOptions=PROCEDURE)
SYMBOL_VECTOR=(SQLNumParams=PROCEDURE)
SYMBOL_VECTOR=(SQLSetScrollOptions=PROCEDURE)
SYMBOL_VECTOR=(SQLSetCursorName=PROCEDURE)
SYMBOL_VECTOR=(SQLGetCursorName=PROCEDURE)
SYMBOL_VECTOR=(SQLNumResultCols=PROCEDURE)
SYMBOL_VECTOR=(SQLDescribeCol=PROCEDURE)
SYMBOL_VECTOR=(SQLColAttributes=PROCEDURE)
SYMBOL_VECTOR=(SQLBindCol=PROCEDURE)
SYMBOL_VECTOR=(SQLFetch=PROCEDURE)
SYMBOL_VECTOR=(SQLGetData=PROCEDURE)
SYMBOL_VECTOR=(SQLMoreResults=PROCEDURE)
SYMBOL_VECTOR=(SQLRowCount=PROCEDURE)
SYMBOL_VECTOR=(SQLSetPos=PROCEDURE)
SYMBOL_VECTOR=(SQLExtendedFetch=PROCEDURE)
SYMBOL_VECTOR=(SQLError=PROCEDURE)
SYMBOL_VECTOR=(SQLTransact=PROCEDURE)
SYMBOL_VECTOR=(SQLDataSources=PROCEDURE)
SYMBOL_VECTOR=(SQLDrivers=PROCEDURE)
$ delete x.opt.*
$ exit
