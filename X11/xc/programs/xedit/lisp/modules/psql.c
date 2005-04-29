/*
 * Copyright (c) 2001 by The XFree86 Project, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *  
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the XFree86 Project shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from the
 * XFree86 Project.
 *
 * Author: Paulo César Pereira de Andrade
 */

/* $XFree86: xc/programs/xedit/lisp/modules/psql.c,v 1.13 2003/04/27 18:17:37 tsi Exp $ */

#include <stdlib.h>
#include <libpq-fe.h>
#undef USE_SSL		/* cannot get it to compile... */
#include <postgres.h>
#include <utils/geo_decls.h>
#include "lisp/internal.h"
#include "lisp/private.h"

/*
 * Prototypes
 */
int psqlLoadModule(void);

LispObj *Lisp_PQbackendPID(LispBuiltin*);
LispObj *Lisp_PQclear(LispBuiltin*);
LispObj *Lisp_PQconsumeInput(LispBuiltin*);
LispObj *Lisp_PQdb(LispBuiltin*);
LispObj *Lisp_PQerrorMessage(LispBuiltin*);
LispObj *Lisp_PQexec(LispBuiltin*);
LispObj *Lisp_PQfinish(LispBuiltin*);
LispObj *Lisp_PQfname(LispBuiltin*);
LispObj *Lisp_PQfnumber(LispBuiltin*);
LispObj *Lisp_PQfsize(LispBuiltin*);
LispObj *Lisp_PQftype(LispBuiltin*);
LispObj *Lisp_PQgetlength(LispBuiltin*);
LispObj *Lisp_PQgetvalue(LispBuiltin*);
LispObj *Lisp_PQhost(LispBuiltin*);
LispObj *Lisp_PQnfields(LispBuiltin*);
LispObj *Lisp_PQnotifies(LispBuiltin*);
LispObj *Lisp_PQntuples(LispBuiltin*);
LispObj *Lisp_PQoptions(LispBuiltin*);
LispObj *Lisp_PQpass(LispBuiltin*);
LispObj *Lisp_PQport(LispBuiltin*);
LispObj *Lisp_PQresultStatus(LispBuiltin*);
LispObj *Lisp_PQsetdb(LispBuiltin*);
LispObj *Lisp_PQsetdbLogin(LispBuiltin*);
LispObj *Lisp_PQsocket(LispBuiltin*);
LispObj *Lisp_PQstatus(LispBuiltin*);
LispObj *Lisp_PQtty(LispBuiltin*);
LispObj *Lisp_PQuser(LispBuiltin*);

/*
 * Initialization
 */
static LispBuiltin lispbuiltins[] = {
    {LispFunction, Lisp_PQbackendPID, "pq-backend-pid connection"},
    {LispFunction, Lisp_PQclear, "pq-clear result"},
    {LispFunction, Lisp_PQconsumeInput, "pq-consume-input connection"},
    {LispFunction, Lisp_PQdb, "pq-db connection"},
    {LispFunction, Lisp_PQerrorMessage, "pq-error-message connection"},
    {LispFunction, Lisp_PQexec, "pq-exec connection query"},
    {LispFunction, Lisp_PQfinish, "pq-finish connection"},
    {LispFunction, Lisp_PQfname, "pq-fname result field-number"},
    {LispFunction, Lisp_PQfnumber, "pq-fnumber result field-name"},
    {LispFunction, Lisp_PQfsize, "pq-fsize result field-number"},
    {LispFunction, Lisp_PQftype, "pq-ftype result field-number"},
    {LispFunction, Lisp_PQgetlength, "pq-getlength result tupple field-number"},
    {LispFunction, Lisp_PQgetvalue, "pq-getvalue result tupple field-number &optional type"},
    {LispFunction, Lisp_PQhost, "pq-host connection"},
    {LispFunction, Lisp_PQnfields, "pq-nfields result"},
    {LispFunction, Lisp_PQnotifies, "pq-notifies connection"},
    {LispFunction, Lisp_PQntuples, "pq-ntuples result"},
    {LispFunction, Lisp_PQoptions, "pq-options connection"},
    {LispFunction, Lisp_PQpass, "pq-pass connection"},
    {LispFunction, Lisp_PQport, "pq-port connection"},
    {LispFunction, Lisp_PQresultStatus, "pq-result-status result"},
    {LispFunction, Lisp_PQsetdb, "pq-setdb host port options tty dbname"},
    {LispFunction, Lisp_PQsetdbLogin, "pq-setdb-login host port options tty dbname login password"},
    {LispFunction, Lisp_PQsocket, "pq-socket connection"},
    {LispFunction, Lisp_PQstatus, "pq-status connection"},
    {LispFunction, Lisp_PQtty, "pq-tty connection"},
    {LispFunction, Lisp_PQuser, "pq-user connection"},
};

LispModuleData psqlLispModuleData = {
    LISP_MODULE_VERSION,
    psqlLoadModule
};

static int PGconn_t, PGresult_t;

/*
 * Implementation
 */
int
psqlLoadModule(void)
{
    int i;
    char *fname = "PSQL-LOAD-MODULE";

    PGconn_t = LispRegisterOpaqueType("PGconn*");
    PGresult_t = LispRegisterOpaqueType("PGresult*");

    GCDisable();
    /* NOTE: Implemented just enough to make programming examples
     * (and my needs) work.
     * Completing this is an exercise to the reader, or may be implemented
     * when/if required.
     */
    LispExecute("(DEFSTRUCT PG-NOTIFY RELNAME BE-PID)\n"
		"(DEFSTRUCT PG-POINT X Y)\n"
		"(DEFSTRUCT PG-BOX HIGH LOW)\n"
		"(DEFSTRUCT PG-POLYGON SIZE NUM-POINTS BOUNDBOX POINTS)\n");

    /* enum ConnStatusType */
    (void)LispSetVariable(ATOM2("PG-CONNECTION-OK"),
			  REAL(CONNECTION_OK), fname, 0);
    (void)LispSetVariable(ATOM2("PG-CONNECTION-BAD"),
			  REAL(CONNECTION_BAD), fname, 0);
    (void)LispSetVariable(ATOM2("PG-CONNECTION-STARTED"),
			  REAL(CONNECTION_STARTED), fname, 0);
    (void)LispSetVariable(ATOM2("PG-CONNECTION-MADE"),
			  REAL(CONNECTION_MADE), fname, 0);
    (void)LispSetVariable(ATOM2("PG-CONNECTION-AWAITING-RESPONSE"),
			  REAL(CONNECTION_AWAITING_RESPONSE), fname, 0);
    (void)LispSetVariable(ATOM2("PG-CONNECTION-AUTH-OK"),
			  REAL(CONNECTION_AUTH_OK), fname, 0);
    (void)LispSetVariable(ATOM2("PG-CONNECTION-SETENV"),
			  REAL(CONNECTION_SETENV), fname, 0);


    /* enum ExecStatusType */
    (void)LispSetVariable(ATOM2("PGRES-EMPTY-QUERY"),
			  REAL(PGRES_EMPTY_QUERY), fname, 0);
    (void)LispSetVariable(ATOM2("PGRES-COMMAND-OK"),
			  REAL(PGRES_COMMAND_OK), fname, 0);
    (void)LispSetVariable(ATOM2("PGRES-TUPLES-OK"),
			  REAL(PGRES_TUPLES_OK), fname, 0);
    (void)LispSetVariable(ATOM2("PGRES-COPY-OUT"),
			  REAL(PGRES_COPY_OUT), fname, 0);
    (void)LispSetVariable(ATOM2("PGRES-COPY-IN"),
			  REAL(PGRES_COPY_IN), fname, 0);
    (void)LispSetVariable(ATOM2("PGRES-BAD-RESPONSE"),
			  REAL(PGRES_BAD_RESPONSE), fname, 0);
    (void)LispSetVariable(ATOM2("PGRES-NONFATAL-ERROR"),
			  REAL(PGRES_NONFATAL_ERROR), fname, 0);
    (void)LispSetVariable(ATOM2("PGRES-FATAL-ERROR"),
			  REAL(PGRES_FATAL_ERROR), fname, 0);
    GCEnable();

    for (i = 0; i < sizeof(lispbuiltins) / sizeof(lispbuiltins[0]); i++)
	LispAddBuiltinFunction(&lispbuiltins[i]);

    return (1);
}

LispObj *
Lisp_PQbackendPID(LispBuiltin *builtin)
/*
 pq-backend-pid connection
 */
{
    int pid;
    PGconn *conn;

    LispObj *connection;

    connection = ARGUMENT(0);

    if (!CHECKO(connection, PGconn_t))
	LispDestroy("%s: cannot convert %s to PGconn*",
		    STRFUN(builtin), STROBJ(connection));
    conn = (PGconn*)(connection->data.opaque.data);

    pid = PQbackendPID(conn);

    return (INTEGER(pid));
}

LispObj *
Lisp_PQclear(LispBuiltin *builtin)
/*
 pq-clear result
 */
{
    PGresult *res;

    LispObj *result;

    result = ARGUMENT(0);

    if (!CHECKO(result, PGresult_t))
	LispDestroy("%s: cannot convert %s to PGresult*",
		    STRFUN(builtin), STROBJ(result));
    res = (PGresult*)(result->data.opaque.data);

    PQclear(res);

    return (NIL);
}

LispObj *
Lisp_PQconsumeInput(LispBuiltin *builtin)
/*
 pq-consume-input connection
 */
{
    int result;
    PGconn *conn;

    LispObj *connection;

    connection = ARGUMENT(0);

    if (!CHECKO(connection, PGconn_t))
	LispDestroy("%s: cannot convert %s to PGconn*",
		    STRFUN(builtin), STROBJ(connection));
    conn = (PGconn*)(connection->data.opaque.data);

    result = PQconsumeInput(conn);

    return (INTEGER(result));
}

LispObj *
Lisp_PQdb(LispBuiltin *builtin)
/*
 pq-db connection
 */
{
    char *string;
    PGconn *conn;

    LispObj *connection;

    connection = ARGUMENT(0);

    if (!CHECKO(connection, PGconn_t))
	LispDestroy("%s: cannot convert %s to PGconn*",
		    STRFUN(builtin), STROBJ(connection));
    conn = (PGconn*)(connection->data.opaque.data);

    string = PQdb(conn);

    return (string ? STRING(string) : NIL);
}

LispObj *
Lisp_PQerrorMessage(LispBuiltin *builtin)
{
    char *string;
    PGconn *conn;

    LispObj *connection;

    connection = ARGUMENT(0);

    if (!CHECKO(connection, PGconn_t))
	LispDestroy("%s: cannot convert %s to PGconn*",
		    STRFUN(builtin), STROBJ(connection));
    conn = (PGconn*)(connection->data.opaque.data);

    string = PQerrorMessage(conn);

    return (string ? STRING(string) : NIL);
}

LispObj *
Lisp_PQexec(LispBuiltin *builtin)
/*
 pq-exec connection query
 */
{
    PGconn *conn;
    PGresult *res;

    LispObj *connection, *query;

    query = ARGUMENT(1);
    connection = ARGUMENT(0);

    if (!CHECKO(connection, PGconn_t))
	LispDestroy("%s: cannot convert %s to PGconn*",
		    STRFUN(builtin), STROBJ(connection));
    conn = (PGconn*)(connection->data.opaque.data);

    CHECK_STRING(query);
    res = PQexec(conn, THESTR(query));

    return (res ? OPAQUE(res, PGresult_t) : NIL);
}

LispObj *
Lisp_PQfinish(LispBuiltin *builtin)
/*
 pq-finish connection
 */
{
    PGconn *conn;

    LispObj *connection;

    connection = ARGUMENT(0);

    if (!CHECKO(connection, PGconn_t))
	LispDestroy("%s: cannot convert %s to PGconn*",
		    STRFUN(builtin), STROBJ(connection));
    conn = (PGconn*)(connection->data.opaque.data);

    PQfinish(conn);

    return (NIL);
}

LispObj *
Lisp_PQfname(LispBuiltin *builtin)
/*
 pq-fname result field-number
 */
{
    char *string;
    int field;
    PGresult *res;

    LispObj *result, *field_number;

    field_number = ARGUMENT(1);
    result = ARGUMENT(0);

    if (!CHECKO(result, PGresult_t))
	LispDestroy("%s: cannot convert %s to PGresult*",
		    STRFUN(builtin), STROBJ(result));
    res = (PGresult*)(result->data.opaque.data);

    CHECK_INDEX(field_number);
    field = FIXNUM_VALUE(field_number);

    string = PQfname(res, field);

    return (string ? STRING(string) : NIL);
}

LispObj *
Lisp_PQfnumber(LispBuiltin *builtin)
/*
 pq-fnumber result field-name
 */
{
    int number;
    int field;
    PGresult *res;

    LispObj *result, *field_name;

    field_name = ARGUMENT(1);
    result = ARGUMENT(0);

    if (!CHECKO(result, PGresult_t))
	LispDestroy("%s: cannot convert %s to PGresult*",
		    STRFUN(builtin), STROBJ(result));
    res = (PGresult*)(result->data.opaque.data);

    CHECK_STRING(field_name);
    number = PQfnumber(res, THESTR(field_name));

    return (INTEGER(number));
}

LispObj *
Lisp_PQfsize(LispBuiltin *builtin)
/*
 pq-fsize result field-number
 */
{
    int size, field;
    PGresult *res;

    LispObj *result, *field_number;

    field_number = ARGUMENT(1);
    result = ARGUMENT(0);

    if (!CHECKO(result, PGresult_t))
	LispDestroy("%s: cannot convert %s to PGresult*",
		    STRFUN(builtin), STROBJ(result));
    res = (PGresult*)(result->data.opaque.data);

    CHECK_INDEX(field_number);
    field = FIXNUM_VALUE(field_number);

    size = PQfsize(res, field);

    return (INTEGER(size));
}

LispObj *
Lisp_PQftype(LispBuiltin *builtin)
{
    Oid oid;
    int field;
    PGresult *res;

    LispObj *result, *field_number;

    field_number = ARGUMENT(1);
    result = ARGUMENT(0);

    if (!CHECKO(result, PGresult_t))
	LispDestroy("%s: cannot convert %s to PGresult*",
		    STRFUN(builtin), STROBJ(result));
    res = (PGresult*)(result->data.opaque.data);

    CHECK_INDEX(field_number);
    field = FIXNUM_VALUE(field_number);

    oid = PQftype(res, field);

    return (INTEGER(oid));
}

LispObj *
Lisp_PQgetlength(LispBuiltin *builtin)
/*
 pq-getlength result tupple field-number
 */
{
    PGresult *res;
    int tuple, field, length;

    LispObj *result, *otupple, *field_number;

    field_number = ARGUMENT(2);
    otupple = ARGUMENT(1);
    result = ARGUMENT(0);

    if (!CHECKO(result, PGresult_t))
	LispDestroy("%s: cannot convert %s to PGresult*",
		    STRFUN(builtin), STROBJ(result));
    res = (PGresult*)(result->data.opaque.data);

    CHECK_INDEX(otupple);
    tuple = FIXNUM_VALUE(otupple);

    CHECK_INDEX(field_number);
    field = FIXNUM_VALUE(field_number);

    length = PQgetlength(res, tuple, field);

    return (INTEGER(length));
}

LispObj *
Lisp_PQgetvalue(LispBuiltin *builtin)
/*
 pq-getvalue result tuple field &optional type-specifier
 */
{
    char *string;
    double real = 0.0;
    PGresult *res;
    int tuple, field, isint = 0, isreal = 0, integer;

    LispObj *result, *otupple, *field_number, *type;

    type = ARGUMENT(3);
    field_number = ARGUMENT(2);
    otupple = ARGUMENT(1);
    result = ARGUMENT(0);

    if (!CHECKO(result, PGresult_t))
	LispDestroy("%s: cannot convert %s to PGresult*",
		    STRFUN(builtin), STROBJ(result));
    res = (PGresult*)(result->data.opaque.data);

    CHECK_INDEX(otupple);
    tuple = FIXNUM_VALUE(otupple);

    CHECK_INDEX(field_number);
    field = FIXNUM_VALUE(field_number);

    string = PQgetvalue(res, tuple, field);

    if (type != UNSPEC) {
	char *typestring;

	CHECK_SYMBOL(type);
	typestring = ATOMID(type);

	if (strcmp(typestring, "INT16") == 0) {
	    integer = *(short*)string;
	    isint = 1;
	    goto simple_type;
	}
	else if (strcmp(typestring, "INT32") == 0) {
	    integer = *(int*)string;
	    isint = 1;
	    goto simple_type;
	}
	else if (strcmp(typestring, "FLOAT") == 0) {
	    real = *(float*)string;
	    isreal = 1;
	    goto simple_type;
	}
	else if (strcmp(typestring, "REAL") == 0) {
	    real = *(double*)string;
	    isreal = 1;
	    goto simple_type;
	}
	else if (strcmp(typestring, "PG-POLYGON") == 0)
	    goto polygon_type;
	else if (strcmp(typestring, "STRING") != 0)
	    LispDestroy("%s: unknown type %s",
			STRFUN(builtin), typestring);
    }

simple_type:
    return (isint ? INTEGER(integer) : isreal ? DFLOAT(real) :
	    (string ? STRING(string) : NIL));

polygon_type:
  {
    LispObj *poly, *box, *p = NIL, *cdr, *obj;
    POLYGON *polygon;
    int i, size;

    size = PQgetlength(res, tuple, field);
    polygon = (POLYGON*)(string - sizeof(int));

    GCDisable();
    /* get polygon->boundbox */
    cdr = EVAL(CONS(ATOM("MAKE-PG-POINT"),
		    CONS(KEYWORD("X"),
			 CONS(REAL(polygon->boundbox.high.x),
			      CONS(KEYWORD("Y"),
				   CONS(REAL(polygon->boundbox.high.y), NIL))))));
    obj = EVAL(CONS(ATOM("MAKE-PG-POINT"),
		    CONS(KEYWORD("X"),
			 CONS(REAL(polygon->boundbox.low.x),
			      CONS(KEYWORD("Y"),
				   CONS(REAL(polygon->boundbox.low.y), NIL))))));
    box = EVAL(CONS(ATOM("MAKE-PG-BOX"),
		    CONS(KEYWORD("HIGH"),
			 CONS(cdr,
			      CONS(KEYWORD("LOW"),
				   CONS(obj, NIL))))));
    /* get polygon->p values */
    for (i = 0; i < polygon->npts; i++) {
	obj = EVAL(CONS(ATOM("MAKE-PG-POINT"),
			CONS(KEYWORD("X"),
			     CONS(REAL(polygon->p[i].x),
			      CONS(KEYWORD("Y"),
				   CONS(REAL(polygon->p[i].y), NIL))))));
	if (i == 0)
	    p = cdr = CONS(obj, NIL);
	else {
	    RPLACD(cdr, CONS(obj, NIL));
	    cdr = CDR(cdr);
	}
    }

    /* make result */
    poly = EVAL(CONS(ATOM("MAKE-PG-POLYGON"),
		     CONS(KEYWORD("SIZE"),
			  CONS(REAL(size),
			       CONS(KEYWORD("NUM-POINTS"),
				    CONS(REAL(polygon->npts),
					 CONS(KEYWORD("BOUNDBOX"),
					      CONS(box,
						   CONS(KEYWORD("POINTS"),
							CONS(QUOTE(p), NIL))))))))));
    GCEnable();

    return (poly);
  }
}

LispObj *
Lisp_PQhost(LispBuiltin *builtin)
/*
 pq-host connection
 */
{
    char *string;
    PGconn *conn;

    LispObj *connection;

    connection = ARGUMENT(0);

    if (!CHECKO(connection, PGconn_t))
	LispDestroy("%s: cannot convert %s to PGconn*",
		    STRFUN(builtin), STROBJ(connection));
    conn = (PGconn*)(connection->data.opaque.data);

    string = PQhost(conn);

    return (string ? STRING(string) : NIL);
}

LispObj *
Lisp_PQnfields(LispBuiltin *builtin)
/*
 pq-nfields result
 */
{
    int nfields;
    PGresult *res;

    LispObj *result;

    result = ARGUMENT(0);

    if (!CHECKO(result, PGresult_t))
	LispDestroy("%s: cannot convert %s to PGresult*",
		    STRFUN(builtin), STROBJ(result));
    res = (PGresult*)(result->data.opaque.data);

    nfields = PQnfields(res);

    return (INTEGER(nfields));
}

LispObj *
Lisp_PQnotifies(LispBuiltin *builtin)
/*
 pq-notifies connection
 */
{
    LispObj *result, *code, *cod = COD;
    PGconn *conn;
    PGnotify *notifies;

    LispObj *connection;

    connection = ARGUMENT(0);

    if (!CHECKO(connection, PGconn_t))
	LispDestroy("%s: cannot convert %s to PGconn*",
		    STRFUN(builtin), STROBJ(connection));
    conn = (PGconn*)(connection->data.opaque.data);

    if ((notifies = PQnotifies(conn)) == NULL)
	return (NIL);

    GCDisable();
    code = CONS(ATOM("MAKE-PG-NOTIFY"),
		  CONS(KEYWORD("RELNAME"),
		       CONS(STRING(notifies->relname),
			    CONS(KEYWORD("BE-PID"),
				 CONS(REAL(notifies->be_pid), NIL)))));
    COD = CONS(code, COD);
    GCEnable();
    result = EVAL(code);
    COD = cod;

    free(notifies);

    return (result);
}

LispObj *
Lisp_PQntuples(LispBuiltin *builtin)
/*
 pq-ntuples result
 */
{
    int ntuples;
    PGresult *res;

    LispObj *result;

    result = ARGUMENT(0);

    if (!CHECKO(result, PGresult_t))
	LispDestroy("%s: cannot convert %s to PGresult*",
		    STRFUN(builtin), STROBJ(result));
    res = (PGresult*)(result->data.opaque.data);

    ntuples = PQntuples(res);

    return (INTEGER(ntuples));
}

LispObj *
Lisp_PQoptions(LispBuiltin *builtin)
/*
 pq-options connection
 */
{
    char *string;
    PGconn *conn;

    LispObj *connection;

    connection = ARGUMENT(0);

    if (!CHECKO(connection, PGconn_t))
	LispDestroy("%s: cannot convert %s to PGconn*",
		    STRFUN(builtin), STROBJ(connection));
    conn = (PGconn*)(connection->data.opaque.data);

    string = PQoptions(conn);

    return (string ? STRING(string) : NIL);
}

LispObj *
Lisp_PQpass(LispBuiltin *builtin)
/*
 pq-pass connection
 */
{
    char *string;
    PGconn *conn;

    LispObj *connection;

    connection = ARGUMENT(0);

    if (!CHECKO(connection, PGconn_t))
	LispDestroy("%s: cannot convert %s to PGconn*",
		    STRFUN(builtin), STROBJ(connection));
    conn = (PGconn*)(connection->data.opaque.data);

    string = PQpass(conn);

    return (string ? STRING(string) : NIL);
}

LispObj *
Lisp_PQport(LispBuiltin *builtin)
/*
 pq-port connection
 */
{
    char *string;
    PGconn *conn;

    LispObj *connection;

    connection = ARGUMENT(0);

    if (!CHECKO(connection, PGconn_t))
	LispDestroy("%s: cannot convert %s to PGconn*",
		    STRFUN(builtin), STROBJ(connection));
    conn = (PGconn*)(connection->data.opaque.data);

    string = PQport(conn);

    return (string ? STRING(string) : NIL);
}

LispObj *
Lisp_PQresultStatus(LispBuiltin *builtin)
/*
 pq-result-status result
 */
{
    int status;
    PGresult *res;

    LispObj *result;

    result = ARGUMENT(0);

    if (!CHECKO(result, PGresult_t))
	LispDestroy("%s: cannot convert %s to PGresult*",
		    STRFUN(builtin), STROBJ(result));
    res = (PGresult*)(result->data.opaque.data);

    status = PQresultStatus(res);

    return (INTEGER(status));
}

LispObj *
LispPQsetdb(LispBuiltin *builtin, int loginp)
/*
 pq-setdb host port options tty dbname
 pq-setdb-login host port options tty dbname login password
 */
{
    PGconn *conn;
    char *host, *port, *options, *tty, *dbname, *login, *password;

    LispObj *ohost, *oport, *ooptions, *otty, *odbname, *ologin, *opassword;

    if (loginp) {
	opassword = ARGUMENT(6);
	ologin = ARGUMENT(5);
    }
    else
	opassword = ologin = NIL;
    odbname = ARGUMENT(4);
    otty = ARGUMENT(3);
    ooptions = ARGUMENT(2);
    oport = ARGUMENT(1);
    ohost = ARGUMENT(0);

    if (ohost != NIL) {
	CHECK_STRING(ohost);
	host = THESTR(ohost);
    }
    else
	host = NULL;

    if (oport != NIL) {
	CHECK_STRING(oport);
	port = THESTR(oport);
    }
    else
	port = NULL;

    if (ooptions != NIL) {
	CHECK_STRING(ooptions);
	options = THESTR(ooptions);
    }
    else
	options = NULL;

    if (otty != NIL) {
	CHECK_STRING(otty);
	tty = THESTR(otty);
    }
    else
	tty = NULL;

    if (odbname != NIL) {
	CHECK_STRING(odbname);
	dbname = THESTR(odbname);
    }
    else
	dbname = NULL;

    if (ologin != NIL) {
	CHECK_STRING(ologin);
	login = THESTR(ologin);
    }
    else
	login = NULL;

    if (opassword != NIL) {
	CHECK_STRING(opassword);
	password = THESTR(opassword);
    }
    else
	password = NULL;

    conn = PQsetdbLogin(host, port, options, tty, dbname, login, password);

    return (conn ? OPAQUE(conn, PGconn_t) : NIL);
}

LispObj *
Lisp_PQsetdb(LispBuiltin *builtin)
/*
 pq-setdb host port options tty dbname
 */
{
    return (LispPQsetdb(builtin, 0));
}

LispObj *
Lisp_PQsetdbLogin(LispBuiltin *builtin)
/*
 pq-setdb-login host port options tty dbname login password
 */
{
    return (LispPQsetdb(builtin, 1));
}

LispObj *
Lisp_PQsocket(LispBuiltin *builtin)
/*
 pq-socket connection
 */
{
    int sock;
    PGconn *conn;

    LispObj *connection;

    connection = ARGUMENT(0);

    if (!CHECKO(connection, PGconn_t))
	LispDestroy("%s: cannot convert %s to PGconn*",
		    STRFUN(builtin), STROBJ(connection));
    conn = (PGconn*)(connection->data.opaque.data);

    sock = PQsocket(conn);

    return (INTEGER(sock));
}

LispObj *
Lisp_PQstatus(LispBuiltin *builtin)
/*
 pq-status connection
 */
{
    int status;
    PGconn *conn;

    LispObj *connection;

    connection = ARGUMENT(0);

    if (!CHECKO(connection, PGconn_t))
	LispDestroy("%s: cannot convert %s to PGconn*",
		    STRFUN(builtin), STROBJ(connection));
    conn = (PGconn*)(connection->data.opaque.data);

    status = PQstatus(conn);

    return (INTEGER(status));
}

LispObj *
Lisp_PQtty(LispBuiltin *builtin)
/*
 pq-tty connection
 */
{
    char *string;
    PGconn *conn;

    LispObj *connection;

    connection = ARGUMENT(0);

    if (!CHECKO(connection, PGconn_t))
	LispDestroy("%s: cannot convert %s to PGconn*",
		    STRFUN(builtin), STROBJ(connection));
    conn = (PGconn*)(connection->data.opaque.data);

    string = PQtty(conn);

    return (string ? STRING(string) : NIL);
}

LispObj *
Lisp_PQuser(LispBuiltin *builtin)
/*
 pq-user connection
 */
{
    char *string;
    PGconn *conn;

    LispObj *connection;

    connection = ARGUMENT(0);

    if (!CHECKO(connection, PGconn_t))
	LispDestroy("%s: cannot convert %s to PGconn*",
		    STRFUN(builtin), STROBJ(connection));
    conn = (PGconn*)(connection->data.opaque.data);

    string = PQuser(conn);

    return (string ? STRING(string) : NIL);
}
