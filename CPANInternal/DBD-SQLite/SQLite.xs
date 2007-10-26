/* $Id: SQLite.xs,v 1.2 2007/01/30 07:00:52 wkakes Exp $ */

#include "SQLiteXS.h"

DBISTATE_DECLARE;

MODULE = DBD::SQLite          PACKAGE = DBD::SQLite::db

PROTOTYPES: DISABLE

BOOT:
    sv_setpv(get_sv("DBD::SQLite::sqlite_version", TRUE), SQLITE_VERSION);
      
AV *
list_tables(dbh)
    SV *dbh
    CODE:
    {
        RETVAL = newAV();
    }
    OUTPUT:
        RETVAL

int
last_insert_rowid(dbh)
    SV *dbh
    CODE:
    {
        D_imp_dbh(dbh);
        RETVAL = sqlite3_last_insert_rowid(imp_dbh->db);
    }
    OUTPUT:
        RETVAL

void
create_function(dbh, name, argc, func)
    SV *dbh
    char *name
    int argc
    SV *func
    CODE:
    {
        sqlite3_db_create_function( dbh, name, argc, func );
    }

void
create_aggregate(dbh, name, argc, aggr)
    SV *dbh
    char *name
    int argc
    SV *aggr
    CODE:
    {
        sqlite3_db_create_aggregate( dbh, name, argc, aggr );
    }

int
busy_timeout(dbh, timeout=0)
  SV *dbh
  int timeout
  CODE:
    RETVAL = dbd_set_sqlite3_busy_timeout( dbh, timeout );
  OUTPUT:
    RETVAL

MODULE = DBD::SQLite          PACKAGE = DBD::SQLite::st

PROTOTYPES: DISABLE

void
reset(sth)
    SV *sth
    CODE:
    {
        sqlite_st_reset(sth);
    }

MODULE = DBD::SQLite          PACKAGE = DBD::SQLite

INCLUDE: SQLite.xsi
