#define PERL_NO_GET_CONTEXT

#include "SQLiteXS.h"

DBISTATE_DECLARE;

MODULE = DBD::SQLite          PACKAGE = DBD::SQLite::db

PROTOTYPES: DISABLE

BOOT:
    sv_setpv(get_sv("DBD::SQLite::sqlite_version", TRUE|GV_ADDMULTI), SQLITE_VERSION);

IV
last_insert_rowid(dbh)
    SV *dbh
    ALIAS:
        DBD::SQLite::db::sqlite_last_insert_rowid = 1
    CODE:
    {
        D_imp_dbh(dbh);
        RETVAL = sqlite3_last_insert_rowid(imp_dbh->db);
    }
    OUTPUT:
        RETVAL

static int
create_function(dbh, name, argc, func)
    SV *dbh
    char *name
    int argc
    SV *func
    ALIAS:
        DBD::SQLite::db::sqlite_create_function = 1
    CODE:
    {
        RETVAL = sqlite_db_create_function(aTHX_ dbh, name, argc, func );
    }
    OUTPUT:
        RETVAL

static int
enable_load_extension(dbh, onoff)
    SV *dbh
    int onoff
    ALIAS:
        DBD::SQLite::db::sqlite_enable_load_extension = 1
    CODE:
    {
        RETVAL = sqlite_db_enable_load_extension(aTHX_ dbh, onoff );
    }
    OUTPUT:
        RETVAL

static int
create_aggregate(dbh, name, argc, aggr)
    SV *dbh
    char *name
    int argc
    SV *aggr
    ALIAS:
        DBD::SQLite::db::sqlite_create_aggregate = 1
    CODE:
    {
        RETVAL = sqlite_db_create_aggregate(aTHX_ dbh, name, argc, aggr );
    }
    OUTPUT:
        RETVAL

static int
create_collation(dbh, name, func)
    SV *dbh
    char *name
    SV *func
    ALIAS:
        DBD::SQLite::db::sqlite_create_collation = 1
    CODE:
    {
        RETVAL = sqlite_db_create_collation(aTHX_ dbh, name, func );
    }
    OUTPUT:
        RETVAL


static void
collation_needed(dbh, callback)
    SV *dbh
    SV *callback
    ALIAS:
        DBD::SQLite::db::sqlite_collation_needed = 1
    CODE:
    {
        sqlite_db_collation_needed(aTHX_ dbh, callback );
    }


static int
progress_handler(dbh, n_opcodes, handler)
    SV *dbh
    int n_opcodes
    SV *handler
    ALIAS:
        DBD::SQLite::db::sqlite_progress_handler = 1
    CODE:
    {
        RETVAL = sqlite_db_progress_handler(aTHX_ dbh, n_opcodes, handler );
    }
    OUTPUT:
        RETVAL

SV*
commit_hook(dbh, hook)
    SV *dbh
    SV *hook
    ALIAS:
        DBD::SQLite::db::sqlite_commit_hook = 1
    CODE:
    {
        RETVAL = (SV*) sqlite_db_commit_hook( aTHX_ dbh, hook );
    }
    OUTPUT:
        RETVAL

SV*
rollback_hook(dbh, hook)
    SV *dbh
    SV *hook
    ALIAS:
        DBD::SQLite::db::sqlite_rollback_hook = 1
    CODE:
    {
        RETVAL = (SV*) sqlite_db_rollback_hook( aTHX_ dbh, hook );
    }
    OUTPUT:
        RETVAL

SV*
update_hook(dbh, hook)
    SV *dbh
    SV *hook
    ALIAS:
        DBD::SQLite::db::sqlite_update_hook = 1
    CODE:
    {
        RETVAL = (SV*) sqlite_db_update_hook( aTHX_ dbh, hook );
    }
    OUTPUT:
        RETVAL


SV*
set_authorizer(dbh, authorizer)
    SV *dbh
    SV *authorizer
    ALIAS:
        DBD::SQLite::db::sqlite_set_authorizer = 1
    CODE:
    {
        RETVAL = (SV*) sqlite_db_set_authorizer( aTHX_ dbh, authorizer );
    }
    OUTPUT:
        RETVAL


int
busy_timeout(dbh, timeout=0)
    SV *dbh
    int timeout
    ALIAS:
        DBD::SQLite::db::sqlite_busy_timeout = 1
    CODE:
        RETVAL = sqlite_db_busy_timeout(aTHX_ dbh, timeout );
    OUTPUT:
        RETVAL

static int
backup_from_file(dbh, filename)
    SV *dbh
    char *filename
    ALIAS:
        DBD::SQLite::db::sqlite_backup_from_file = 1
    CODE:
        RETVAL = sqlite_db_backup_from_file(aTHX_ dbh, filename);
    OUTPUT:
        RETVAL

static int
backup_to_file(dbh, filename)
    SV *dbh
    char *filename
    ALIAS:
        DBD::SQLite::db::sqlite_backup_to_file = 1
    CODE:
        RETVAL = sqlite_db_backup_to_file(aTHX_ dbh, filename);
    OUTPUT:
        RETVAL

MODULE = DBD::SQLite          PACKAGE = DBD::SQLite::st

PROTOTYPES: DISABLE

MODULE = DBD::SQLite          PACKAGE = DBD::SQLite

# a couple of constants exported from sqlite3.h

PROTOTYPES: ENABLE

static int
OK()
    CODE:
        RETVAL = SQLITE_OK;
    OUTPUT:
        RETVAL

static int
DENY()
    CODE:
        RETVAL = SQLITE_DENY;
    OUTPUT:
        RETVAL

static int
IGNORE()
    CODE:
        RETVAL = SQLITE_IGNORE;
    OUTPUT:
        RETVAL

static int
CREATE_INDEX()
    CODE:
        RETVAL = SQLITE_CREATE_INDEX;
    OUTPUT:
        RETVAL

static int
CREATE_TABLE()
    CODE:
        RETVAL = SQLITE_CREATE_TABLE;
    OUTPUT:
        RETVAL

static int
CREATE_TEMP_INDEX()
    CODE:
        RETVAL = SQLITE_CREATE_TEMP_INDEX;
    OUTPUT:
        RETVAL

static int
CREATE_TEMP_TABLE()
    CODE:
        RETVAL = SQLITE_CREATE_TEMP_TABLE;
    OUTPUT:
        RETVAL

static int
CREATE_TEMP_TRIGGER()
    CODE:
        RETVAL = SQLITE_CREATE_TEMP_TRIGGER;
    OUTPUT:
        RETVAL

static int
CREATE_TEMP_VIEW()
    CODE:
        RETVAL = SQLITE_CREATE_TEMP_VIEW;
    OUTPUT:
        RETVAL

static int
CREATE_TRIGGER()
    CODE:
        RETVAL = SQLITE_CREATE_TRIGGER;
    OUTPUT:
        RETVAL

static int
CREATE_VIEW()
    CODE:
        RETVAL = SQLITE_CREATE_VIEW;
    OUTPUT:
        RETVAL

static int
DELETE()
    CODE:
        RETVAL = SQLITE_DELETE;
    OUTPUT:
        RETVAL

static int
DROP_INDEX()
    CODE:
        RETVAL = SQLITE_DROP_INDEX;
    OUTPUT:
        RETVAL

static int
DROP_TABLE()
    CODE:
        RETVAL = SQLITE_DROP_TABLE;
    OUTPUT:
        RETVAL

static int
DROP_TEMP_INDEX()
    CODE:
        RETVAL = SQLITE_DROP_TEMP_INDEX;
    OUTPUT:
        RETVAL

static int
DROP_TEMP_TABLE()
    CODE:
        RETVAL = SQLITE_DROP_TEMP_TABLE;
    OUTPUT:
        RETVAL

static int
DROP_TEMP_TRIGGER()
    CODE:
        RETVAL = SQLITE_DROP_TEMP_TRIGGER;
    OUTPUT:
        RETVAL

static int
DROP_TEMP_VIEW()
    CODE:
        RETVAL = SQLITE_DROP_TEMP_VIEW;
    OUTPUT:
        RETVAL

static int
DROP_TRIGGER()
    CODE:
        RETVAL = SQLITE_DROP_TRIGGER;
    OUTPUT:
        RETVAL

static int
DROP_VIEW()
    CODE:
        RETVAL = SQLITE_DROP_VIEW;
    OUTPUT:
        RETVAL

static int
INSERT()
    CODE:
        RETVAL = SQLITE_INSERT;
    OUTPUT:
        RETVAL

static int
PRAGMA()
    CODE:
        RETVAL = SQLITE_PRAGMA;
    OUTPUT:
        RETVAL

static int
READ()
    CODE:
        RETVAL = SQLITE_READ;
    OUTPUT:
        RETVAL

static int
SELECT()
    CODE:
        RETVAL = SQLITE_SELECT;
    OUTPUT:
        RETVAL

static int
TRANSACTION()
    CODE:
        RETVAL = SQLITE_TRANSACTION;
    OUTPUT:
        RETVAL

static int
UPDATE()
    CODE:
        RETVAL = SQLITE_UPDATE;
    OUTPUT:
        RETVAL

static int
ATTACH()
    CODE:
        RETVAL = SQLITE_ATTACH;
    OUTPUT:
        RETVAL

static int
DETACH()
    CODE:
        RETVAL = SQLITE_DETACH;
    OUTPUT:
        RETVAL

static int
ALTER_TABLE()
    CODE:
        RETVAL = SQLITE_ALTER_TABLE;
    OUTPUT:
        RETVAL

static int
REINDEX()
    CODE:
        RETVAL = SQLITE_REINDEX;
    OUTPUT:
        RETVAL

static int
ANALYZE()
    CODE:
        RETVAL = SQLITE_ANALYZE;
    OUTPUT:
        RETVAL

static int
CREATE_VTABLE()
    CODE:
        RETVAL = SQLITE_CREATE_VTABLE;
    OUTPUT:
        RETVAL

static int
DROP_VTABLE()
    CODE:
        RETVAL = SQLITE_DROP_VTABLE;
    OUTPUT:
        RETVAL

static int
FUNCTION()
    CODE:
        RETVAL = SQLITE_FUNCTION;
    OUTPUT:
        RETVAL

static int
SAVEPOINT()
    CODE:
        RETVAL = SQLITE_SAVEPOINT;
    OUTPUT:
        RETVAL



INCLUDE: SQLite.xsi
