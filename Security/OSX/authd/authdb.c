/* Copyright (c) 2012-2014 Apple Inc. All Rights Reserved. */

#include "authdb.h"
#include "mechanism.h"
#include "rule.h"
#include "debugging.h"
#include "authitems.h"
#include "server.h"

#include <sqlite3.h>
#include <sqlite3_private.h>
#include <CoreFoundation/CoreFoundation.h>
#include "rule.h"
#include "authutilities.h"
#include <libgen.h>
#include <sys/stat.h>
#include "PreloginUserDb.h"

AUTHD_DEFINE_LOG

#define AUTHDB "/var/db/auth.db"
#define AUTHDB_DATA "/System/Library/Security/authorization.plist"

#define AUTH_STR(x) #x
#define AUTH_STRINGIFY(x) AUTH_STR(x)

#define AUTHDB_VERSION 2
#define AUTHDB_VERSION_STRING AUTH_STRINGIFY(AUTHDB_VERSION)

#define AUTHDB_BUSY_DELAY 1
#define AUTHDB_MAX_HANDLES 3

struct _authdb_connection_s {
    __AUTH_BASE_STRUCT_HEADER__;
    
    authdb_t db;
    sqlite3 * handle;
};

struct _authdb_s {
    __AUTH_BASE_STRUCT_HEADER__;
    
    char * db_path;
    dispatch_queue_t queue;
    CFMutableArrayRef connections;
};

static const char * const authdb_upgrade_sql[] = {
    /* 0 */
    /* current scheme */
    "CREATE TABLE delegates_map ("
        "r_id INTEGER NOT NULL REFERENCES rules(id) ON DELETE CASCADE,"
        "d_id INTEGER NOT NULL REFERENCES rules(id) ON DELETE CASCADE,"
        "ord INTEGER NOT NULL"
        ");"
    "CREATE INDEX d_map_d_id ON delegates_map(d_id);"
    "CREATE INDEX d_map_r_id ON delegates_map(r_id);"
    "CREATE INDEX d_map_r_id_ord ON delegates_map (r_id, ord);"
    "CREATE TABLE mechanisms ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT UNIQUE,"
        "plugin TEXT NOT NULL,"
        "param TEXT NOT NULL,"
        "privileged INTEGER CHECK (privileged = 0 OR privileged = 1) NOT NULL DEFAULT (0)"
        ");"
    "CREATE UNIQUE INDEX mechanisms_lookup ON mechanisms (plugin,param,privileged);"
    "CREATE TABLE mechanisms_map ("
        "r_id INTEGER NOT NULL REFERENCES rules(id) ON DELETE CASCADE,"
        "m_id INTEGER NOT NULL REFERENCES mechanisms(id) ON DELETE CASCADE,"
        "ord INTEGER NOT NULL"
        ");"
    "CREATE INDEX m_map_m_id ON mechanisms_map (m_id);"
    "CREATE INDEX m_map_r_id ON mechanisms_map (r_id);"
    "CREATE INDEX m_map_r_id_ord ON mechanisms_map (r_id, ord);"
    "CREATE TABLE rules ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT UNIQUE,"
        "name TEXT NOT NULL UNIQUE,"
        "type INTEGER CHECK (type = 1 OR type = 2) NOT NULL,"
        "class INTEGER CHECK (class > 0),"
        "'group' TEXT,"
        "kofn INTEGER,"
        "timeout INTEGER,"
        "flags INTEGER,"
        "tries INTEGER,"
        "version INTEGER NOT NULL DEFAULT (0),"
        "created REAL NOT NULL DEFAULT (0),"
        "modified REAL NOT NULL DEFAULT (0),"
        "hash BLOB,"
        "identifier TEXT,"
        "requirement BLOB,"
        "comment TEXT"
        ");"
    "CREATE INDEX a_type ON rules (type);"
    "CREATE TABLE config ("
        "'key' TEXT PRIMARY KEY NOT NULL UNIQUE,"
        "value"
        ");"
    "CREATE TABLE prompts ("
        "r_id INTEGER NOT NULL REFERENCES rules(id) ON DELETE CASCADE,"
        "lang TEXT NOT NULL,"
        "value TEXT NOT NULL"
        ");"
    "CREATE INDEX p_r_id ON prompts(r_id);"
    "CREATE TABLE buttons ("
        "r_id INTEGER NOT NULL REFERENCES rules(id) ON DELETE CASCADE,"
        "lang TEXT NOT NULL,"
        "value TEXT NOT NULL"
        ");"
    "CREATE INDEX b_r_id ON buttons(r_id);"
    "INSERT INTO config VALUES('version', '1');" ,
    
    // version 2 of the database
    "CREATE TABLE rules_history ("
        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "rule TEXT NOT NULL,"
        "version INTEGER NOT NULL,"
        "source TEXT NOT NULL,"
        "operation INTEGER NOT NULL"
        ");"
};

static sqlite3 * _create_handle(authdb_t db);

static int32_t
_sqlite3_exec(sqlite3 * handle, const char * query)
{
    int32_t rc = SQLITE_ERROR;
    require(query != NULL, done);
    
    char * errmsg = NULL;
    rc = sqlite3_exec(handle, query, NULL, NULL, &errmsg);
    if (errmsg) {
        os_log_error(AUTHD_LOG, "authdb: exec, (%i) %{public}s", rc, errmsg);
        sqlite3_free(errmsg);
    }
    
done:
    return rc;
}

struct _db_upgrade_stages {
    int pre;
    int main;
    int post;
};

static struct _db_upgrade_stages auth_upgrade_script[] = {
    { .pre = -1, .main = 0, .post = -1 }, // Create version AUTHDB_VERSION databse.
    { .pre = -1, .main = 1, .post = -1}
};

static int32_t _db_run_script(authdb_connection_t dbconn, int number)
{
    int32_t s3e;
    
    /* Script -1 == skip this step. */
    if (number < 0)
        return SQLITE_OK;
    
    /* If we are attempting to run a script we don't have, fail. */
    if ((size_t)number >= sizeof(authdb_upgrade_sql) / sizeof(char*))
        return SQLITE_CORRUPT;
    
    s3e = _sqlite3_exec(dbconn->handle, authdb_upgrade_sql[number]);
    
    return s3e;
}

static int32_t _db_upgrade_from_version(authdb_connection_t dbconn, int32_t version)
{
    int32_t s3e;
    
    /* If we are attempting to upgrade to a version greater than what we have
     an upgrade script for, fail. */
    if (version < 0 ||
        (size_t)version >= sizeof(auth_upgrade_script) / sizeof(struct _db_upgrade_stages))
        return SQLITE_CORRUPT;
    
    struct _db_upgrade_stages *script = &auth_upgrade_script[version];
    s3e = _db_run_script(dbconn, script->pre);
    if (s3e == SQLITE_OK)
        s3e = _db_run_script(dbconn, script->main);
    if (s3e == SQLITE_OK)
        s3e = _db_run_script(dbconn, script->post);
    
    return s3e;
}

static CFDictionaryRef _copy_plist(auth_items_t config, CFAbsoluteTime *outTs)
{
    CFDateRef mDate = NULL;
    CFErrorRef error = NULL;
    CFAbsoluteTime ts = 0;
    CFAbsoluteTime old_ts = 0;
    CFDictionaryRef plist = NULL;
    
    struct stat attr;
    int err = stat(AUTHDB_DATA, &attr);
    require_action(err == 0, done, os_log_error(AUTHD_LOG, "authdb: file mdate not available %{public}s, err %d", AUTHDB_DATA, err));
    mDate = dateFromUnixTimestamp(attr.st_mtime);

    ts = CFDateGetAbsoluteTime(mDate);
    if (outTs) {
        *outTs = ts;
    }
    
    if (config) {
        old_ts = auth_items_get_double(config, "data_ts");
    }
    
    // <rdar://problem/17484375> SEED: BUG: Fast User Switching Not Working
    // After Mavericks => Yosemite upgrade install, the new Yosemite rule "system.login.fus" was missing.
    // Somehow (probably during install) ts < old_ts, even though that should never happen.
    // Solution: always import plist and update db when time stamps don't match.
    // After a successful import, old_ts = ts below.
    if (!config || (ts != old_ts))
    {
        os_log_debug(AUTHD_LOG, "authdb: %{public}s modified old=%f, new=%f", AUTHDB_DATA, old_ts, ts);
        plist = readDatabasePlist(CFSTR(AUTHDB_DATA), &error);
        require_action(error == NULL, done, os_log_error(AUTHD_LOG, "authdb: failed to read plist: %{public}@", error));
    }

done:
    CFReleaseSafe(error);
    CFReleaseSafe(mDate);

    return plist;
}

static void _repair_broken_kofn_right(authdb_connection_t dbconn, const char *right, CFDictionaryRef plist)
{
    if (!right || !dbconn) {
        return;
    }
    
    CFDictionaryRef localPlist = plist;
    if (!localPlist) {
        localPlist = _copy_plist(NULL, NULL);
    }
    
    // import the broken right
    os_log(AUTHD_LOG, "Repairing broken right %{public}s", right);
    authdb_import_plist(dbconn, localPlist, FALSE, right);
    
    if (!plist && localPlist) {
        CFRelease(localPlist);
    }
}

static bool _check_for_db_update(authdb_connection_t dbconn)
{
    // these are the most reliable indicators of a database corruption made during update/migration
    // these rights are either completely missing or they are present but in a previous version
    // if any of these rights is missing or the last one is old, it means that database needs
    // to be reset
    const char *rightNames[] = {
            "com.apple.installassistant.requestpassword",
            "com.apple.system-migration.launch-password",
            "com.apple.trust-settings.admin",
            NULL };
    const char **rightName = rightNames;
    bool fault = false;
    while (!fault && *rightName) {
        rule_t right = rule_create_with_string(*rightName, dbconn);
        if (!right || !rule_sql_fetch(right, dbconn) ) {
            os_log_fault(AUTHD_LOG, "Old and not updated database found: no %{public}s right is defined", *rightName);
            fault = true;
        } else if (strcmp(*rightName, "com.apple.trust-settings.admin") == 0) {
            if (rule_get_class(right) != RC_RULE) {
                os_log_fault(AUTHD_LOG, "Old and not updated database found: old %{public}s right is defined", *rightName);
                fault = true;
            }
        }

        CFReleaseNull(right);
        ++rightName;
    }
    if (!fault) {
        os_log(AUTHD_LOG, "Database check OK");
    }
    return fault;
}

static bool _truncate_db(authdb_connection_t dbconn)
{
    int32_t rc = SQLITE_ERROR;
    int32_t flags = SQLITE_TRUNCATE_JOURNALMODE_WAL | SQLITE_TRUNCATE_AUTOVACUUM_FULL;
    rc = sqlite3_file_control(dbconn->handle, NULL, SQLITE_TRUNCATE_DATABASE, &flags);
    if (rc != SQLITE_OK) {
        os_log_debug(AUTHD_LOG, "Failed to delete db handle! SQLite error %i.", rc);
        if (rc == SQLITE_IOERR) {
            // Unable to recover successfully if we can't truncate
            abort();
        }
    }
    
    return rc == SQLITE_OK;
}

static void _handle_corrupt_db(authdb_connection_t dbconn)
{
    int32_t rc = SQLITE_ERROR;
    char buf[PATH_MAX+1];
    sqlite3 *corrupt_db = NULL;
    
    snprintf(buf, sizeof(buf), "%s-corrupt", dbconn->db->db_path);
    if (sqlite3_open(buf, &corrupt_db) == SQLITE_OK) {
        
        int on = 1;
        sqlite3_file_control(corrupt_db, 0, SQLITE_FCNTL_PERSIST_WAL, &on);
        
        rc = sqlite3_file_control(corrupt_db, NULL, SQLITE_REPLACE_DATABASE, (void *)dbconn->handle);
        if (SQLITE_OK == rc) {
            os_log_error(AUTHD_LOG, "Database at path %{public}s is corrupt. Copying it to %{public}s for further investigation.", dbconn->db->db_path, buf);
        } else {
            os_log_error(AUTHD_LOG, "Tried to copy corrupt database at path %{public}s, but we failed with SQLite error %i.", dbconn->db->db_path, rc);
        }
    }

	// SQLite documentation says:
	// Whether or not an error occurs when it is opened, resources associated with the database connection handle should be released by passing it to sqlite3_close() when it is no longer required.
	if (corrupt_db)
		sqlite3_close(corrupt_db);

    _truncate_db(dbconn);
}

static int32_t _db_maintenance(authdb_connection_t dbconn)
{
    __block int32_t s3e = SQLITE_OK;
    __block auth_items_t config = NULL;
    
    authdb_transaction(dbconn, AuthDBTransactionNormal, ^bool(void) {
        
        authdb_get_key_value(dbconn, "config", true, &config);
        
        // We don't have a config table
        if (NULL == config) {
            os_log_debug(AUTHD_LOG, "authdb: initializing database");
            s3e = _db_upgrade_from_version(dbconn, 0);
            require_noerr_action(s3e, done, os_log_error(AUTHD_LOG, "authdb: failed to initialize database %i", s3e));
            
            s3e = authdb_get_key_value(dbconn, "config", true, &config);
            require_noerr_action(s3e, done, os_log_error(AUTHD_LOG, "authdb: failed to get config %i", s3e));
        }
        
        int64_t currentVersion = auth_items_get_int64(config, "version");
        os_log_debug(AUTHD_LOG, "authdb: current db ver=%lli", currentVersion);
        if (currentVersion < AUTHDB_VERSION) {
            os_log(AUTHD_LOG, "authdb: upgrading schema from version %lld to version %d", currentVersion, AUTHDB_VERSION);
            s3e = _db_upgrade_from_version(dbconn, (int32_t)currentVersion);
            if (s3e != SQLITE_OK) {
                os_log_error(AUTHD_LOG, "authdb: failed to upgrade DB schema %i", s3e);
            } else {
                auth_items_set_int64(config, "version", AUTHDB_VERSION);
                authdb_set_key_value(dbconn, "config", config);
            }
        }

    done:
        return true;
    });
    
    CFReleaseSafe(config);
    return s3e;
}

static void _db_load_data(authdb_connection_t dbconn, auth_items_t config)
{
    CFAbsoluteTime ts = 0;
    CFDictionaryRef plist = _copy_plist(config, &ts);
    
    if (plist) {
        if (authdb_import_plist(dbconn, plist, true, NULL)) {
            os_log_debug(AUTHD_LOG, "authdb: updating data_ts");
            auth_items_t update = auth_items_create();
            auth_items_set_double(update, "data_ts", ts);
            authdb_set_key_value(dbconn, "config", update);
            CFReleaseSafe(update);
            int32_t rc = sqlite3_db_cacheflush(dbconn->handle);
            os_log_debug(AUTHD_LOG, "Flush result: %d", rc);
            _sqlite3_exec(dbconn->handle, "VACUUM");
        }
    }
    
    CFReleaseSafe(plist);
}

//static void unlock_notify_cb(void **apArg, int nArg AUTH_UNUSED){
//    dispatch_semaphore_t semaphore = (dispatch_semaphore_t)apArg[0];
//    dispatch_semaphore_signal(semaphore);
//}
//
//static int32_t _wait_for_unlock_notify(authdb_connection_t dbconn, sqlite3_stmt * stmt)
//{
//    int32_t rc;
//    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
//    
//    rc = sqlite3_unlock_notify(dbconn->handle, unlock_notify_cb, semaphore);
//    require(!rc, done);
//    
//    if (dispatch_semaphore_wait(semaphore, 5*NSEC_PER_SEC) != 0) {
//        os_log_debug(AUTHD_LOG, "authdb: timeout occurred!");
//        sqlite3_unlock_notify(dbconn->handle, NULL, NULL);
//        rc = SQLITE_LOCKED;
//    } else if (stmt){
//        sqlite3_reset(stmt);
//    }
//
//done:
//    dispatch_release(semaphore);
//    return rc;
//}

static void _repair_all_kofns(authdb_connection_t dbconn, auth_items_t config)
{
    // we want plist to be returned always and not only when never
    CFDictionaryRef plist = _copy_plist(NULL, NULL);
    
    if (!plist) {
        os_log_error(AUTHD_LOG, "authdb: unable to repair kofns");
        return;
    }
    
    authdb_step(dbconn, "SELECT name FROM rules WHERE (rules.kofn > 0) AND (id NOT IN (SELECT r_id FROM delegates_map WHERE r_id = id))",
                NULL, ^bool(auth_items_t data) {
                    const char *name = auth_items_get_string(data, "name");
                    if (name) {
                        _repair_broken_kofn_right(dbconn, name, plist);
                    }
                    return true;
                });
    
    __block bool brokenDb = false;
    authdb_step(dbconn, "SELECT name FROM rules WHERE (class = 2) AND (id NOT IN (SELECT r_id FROM delegates_map WHERE r_id = id))",
                NULL, ^bool(auth_items_t data) {
                    brokenDb = true;
                    return false;
                });
    CFRelease(plist);

    if (brokenDb) {
        os_log_error(AUTHD_LOG, "authdb: broken delegates, marking db as corrupt");
        _handle_corrupt_db(dbconn);
        authdb_maintenance(dbconn);
    }
    
}

static bool _is_busy(int32_t rc)
{
    return SQLITE_BUSY == rc || SQLITE_LOCKED == rc;
}

static void _checkResult(authdb_connection_t dbconn, int32_t rc, const char * fn_name, sqlite3_stmt * stmt, const bool skip_maintenance)
{
    bool isCorrupt = (SQLITE_CORRUPT == rc) || (SQLITE_NOTADB == rc) || (SQLITE_IOERR == rc);
    
    if (isCorrupt) {
		if (skip_maintenance) {
			os_log_debug(AUTHD_LOG, "authdb: corrupted db, skipping maintenance %{public}s %{public}s", fn_name, sqlite3_errmsg(dbconn->handle));
		} else {
        _handle_corrupt_db(dbconn);
        authdb_maintenance(dbconn);
		}
    } else if (SQLITE_CONSTRAINT == rc || SQLITE_READONLY == rc) {
        if (stmt) {
            os_log_debug(AUTHD_LOG, "authdb: %{public}s %{public}s for %{public}s", fn_name, sqlite3_errmsg(dbconn->handle), sqlite3_sql(stmt));
        } else {
            os_log_debug(AUTHD_LOG, "authdb: %{public}s %{public}s", fn_name, sqlite3_errmsg(dbconn->handle));
        }
    }
}

char * authdb_copy_sql_string(sqlite3_stmt * sql,int32_t col)
{
    char * result = NULL;
    const char * sql_str = (const char *)sqlite3_column_text(sql, col);
    if (sql_str) {
        size_t len = strlen(sql_str) + 1;
        result = (char*)calloc(1u, len);
        check(result != NULL);
        
        strlcpy(result, sql_str, len);
    }
    return result;
}

#pragma mark -
#pragma mark authdb_t

static void
_authdb_finalize(CFTypeRef value)
{
    authdb_t db = (authdb_t)value;
    
    CFReleaseNull(db->connections);
    dispatch_release(db->queue);
    free_safe(db->db_path);
}

AUTH_TYPE_INSTANCE(authdb,
                   .init = NULL,
                   .copy = NULL,
                   .finalize = _authdb_finalize,
                   .equal = NULL,
                   .hash = NULL,
                   .copyFormattingDesc = NULL,
                   .copyDebugDesc = NULL
                   );

static CFTypeID authdb_get_type_id(void) {
    static CFTypeID type_id = _kCFRuntimeNotATypeID;
    static dispatch_once_t onceToken;
    
    dispatch_once(&onceToken, ^{
        type_id = _CFRuntimeRegisterClass(&_auth_type_authdb);
    });
    
    return type_id;
}

authdb_t
authdb_create(bool force_memory)
{
    authdb_t db = NULL;
    
    db = (authdb_t)_CFRuntimeCreateInstance(kCFAllocatorDefault, authdb_get_type_id(), AUTH_CLASS_SIZE(authdb), NULL);
    require(db != NULL, done);
    
    db->queue = dispatch_queue_create("Database queue", DISPATCH_QUEUE_SERIAL);
    db->connections = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);
    
    if (force_memory || getenv("__OSINSTALL_ENVIRONMENT") != NULL) {
        os_log_debug(AUTHD_LOG, "authdb: running from memory");
        db->db_path = _copy_string("file::memory:?cache=shared");
    } else {
        db->db_path = _copy_string(AUTHDB);
    }
    
done:
    return db;
}

authdb_connection_t authdb_connection_acquire(authdb_t db)
{
    __block authdb_connection_t dbconn = NULL;
#if DEBUG
    static int32_t total = 0;
#endif
    dispatch_sync(db->queue, ^{
        CFIndex count = CFArrayGetCount(db->connections);
        if (count) {
            dbconn = (authdb_connection_t)CFArrayGetValueAtIndex(db->connections, 0);
            CFArrayRemoveValueAtIndex(db->connections, 0);
        } else {
            dbconn = authdb_connection_create(db);
#if DEBUG
            total++;
            os_log_debug(AUTHD_LOG, "authdb: handles count: %i", total);
#endif
        }
    });

    return dbconn;
}

void authdb_connection_release(authdb_connection_t * dbconn)
{
    if (!(*dbconn))
        return;

    authdb_connection_t tmp = *dbconn;
    *dbconn = NULL;
    
    dispatch_async(tmp->db->queue, ^{
        CFIndex count = CFArrayGetCount(tmp->db->connections);
        if (count <= AUTHDB_MAX_HANDLES) {
            CFArrayAppendValue(tmp->db->connections, tmp);
        } else {
            os_log_debug(AUTHD_LOG, "authdb: freeing extra connection");
            CFRelease(tmp);
        }
    });
}

static bool _db_check_corrupted(authdb_connection_t dbconn)
{
    bool isCorrupted = true;
    sqlite3_stmt *stmt = NULL;
    int32_t rc;

	if (!dbconn->handle)
		return true;
    
    rc = sqlite3_prepare_v2(dbconn->handle, "PRAGMA integrity_check;", -1, &stmt, NULL);
    if (rc == SQLITE_LOCKED || rc == SQLITE_BUSY) {
        os_log_debug(AUTHD_LOG, "authdb: warning error %i when running integrity check", rc);
        isCorrupted = false;
        
    } else if (rc == SQLITE_OK) {
        rc = sqlite3_step(stmt);
        
        if (rc == SQLITE_LOCKED || rc == SQLITE_BUSY) {
            os_log_debug(AUTHD_LOG, "authdb: warning error %i when running integrity check", rc);
            isCorrupted = false;
        } else if (rc == SQLITE_ROW) {
            const char * result = (const char*)sqlite3_column_text(stmt, 0);
            os_log_debug(AUTHD_LOG, "authdb: integrity check result: %{public}s", result);

            if (result && strncasecmp(result, "ok", 3) == 0) {
                isCorrupted = false;
            } else {
                os_log_error(AUTHD_LOG, "authdb: integrity check result %{public}s", result);
            }
        }
    } else {
        os_log_error(AUTHD_LOG, "authdb: integrity check failed %d", rc);
    }
    
    sqlite3_finalize(stmt);
    return isCorrupted;
}

bool authdb_maintenance(authdb_connection_t dbconn)
{
    os_log_debug(AUTHD_LOG, "authdb: starting maintenance");
    int32_t rc = SQLITE_ERROR;
    auth_items_t config = NULL;
    
    bool isCorrupted = _db_check_corrupted(dbconn);
    os_log_debug(AUTHD_LOG, "authdb: integrity check=%{public}s", isCorrupted ? "fail" : "pass");
    
    if (isCorrupted) {
        _handle_corrupt_db(dbconn);
    }

	if (dbconn->handle == NULL) {
		dbconn->handle = _create_handle(dbconn->db);
	}

	require_action(dbconn->handle, done, os_log_error(AUTHD_LOG, "authdb: maintenance cannot open database"));

	_db_maintenance(dbconn);

	rc = authdb_get_key_value(dbconn, "config", true, &config);
	require_noerr_action(rc, done, os_log_debug(AUTHD_LOG, "authdb: maintenance failed %i", rc));

    _db_load_data(dbconn, config);
    
    _repair_all_kofns(dbconn, config);
done:
    CFReleaseSafe(config);
    os_log_debug(AUTHD_LOG, "authdb: finished maintenance");
    return rc == SQLITE_OK;
}

void authdb_check_for_mandatory_rights(authdb_connection_t dbconn)
{
    // check the database if it contains the up to date rights
    if (_check_for_db_update(dbconn)) {
        // update is needed
        os_log_error(AUTHD_LOG, "Database is in a bad shape, recreating it");
        _handle_corrupt_db(dbconn);
        authdb_maintenance(dbconn);
        
        // do the final check again
        bool recheck = _check_for_db_update(dbconn);
        os_log(AUTHD_LOG, "Database recheck result: %d", recheck);
    }
}


int32_t
authdb_exec(authdb_connection_t dbconn, const char * query)
{
    int32_t rc = SQLITE_ERROR;
    require(query != NULL, done);
    
    rc = _sqlite3_exec(dbconn->handle, query);
    _checkResult(dbconn, rc, __FUNCTION__, NULL, false);
    
done:
    return rc;
}

static int32_t _prepare(authdb_connection_t dbconn, const char * sql, const bool skip_maintenance, sqlite3_stmt ** out_stmt)
{
    int32_t rc;
    sqlite3_stmt * stmt = NULL; 
    
    require_action(sql != NULL, done, rc = SQLITE_ERROR);
    require_action(out_stmt != NULL, done, rc = SQLITE_ERROR);
    
    rc = sqlite3_prepare_v2(dbconn->handle, sql, -1, &stmt, NULL);
    require_noerr_action(rc, done, os_log_debug(AUTHD_LOG, "authdb: prepare (%i) %{public}s", rc, sqlite3_errmsg(dbconn->handle)));
    
    *out_stmt = stmt;
    
done:
    _checkResult(dbconn, rc, __FUNCTION__, stmt, skip_maintenance);
    return rc;
}

static void _parseItemsAtIndex(sqlite3_stmt * stmt, int32_t col, auth_items_t items, const char * key)
{
    switch (sqlite3_column_type(stmt, col)) {
        case SQLITE_FLOAT:
            auth_items_set_double(items, key, sqlite3_column_double(stmt, col));
            break;
        case SQLITE_INTEGER:
            auth_items_set_int64(items, key, sqlite3_column_int64(stmt, col));
            break;
        case SQLITE_BLOB:
            auth_items_set_data(items,
                                key,
                                sqlite3_column_blob(stmt, col),
                                (size_t)sqlite3_column_bytes(stmt, col));
            break;
        case SQLITE_NULL:
            break;
        case SQLITE_TEXT:
        default:
            auth_items_set_string(items, key, (const char *)sqlite3_column_text(stmt, col));
            break;
    }

//    LOGD("authdb: col=%s, val=%s, type=%i", sqlite3_column_name(stmt, col), sqlite3_column_text(stmt, col), sqlite3_column_type(stmt,col));
}

static int32_t _bindItemsAtIndex(sqlite3_stmt * stmt, int col, auth_items_t items, const char * key)
{
    int32_t rc;
    switch (auth_items_get_type(items, key)) {
        case AI_TYPE_INT:
            rc = sqlite3_bind_int64(stmt, col, auth_items_get_int(items, key));
            break;
        case AI_TYPE_UINT:
            rc = sqlite3_bind_int64(stmt, col, auth_items_get_uint(items, key));
            break;
        case AI_TYPE_INT64:
            rc = sqlite3_bind_int64(stmt, col, auth_items_get_int64(items, key));
            break;
        case AI_TYPE_UINT64:
            rc = sqlite3_bind_int64(stmt, col, (int64_t)auth_items_get_uint64(items, key));
            break;
        case AI_TYPE_DOUBLE:
            rc = sqlite3_bind_double(stmt, col, auth_items_get_double(items, key));
            break;
        case AI_TYPE_BOOL:
            rc = sqlite3_bind_int64(stmt, col, auth_items_get_bool(items, key));
            break;
        case AI_TYPE_DATA:
        {
            size_t blobLen = 0;
            const void * blob = auth_items_get_data(items, key, &blobLen);
            rc = sqlite3_bind_blob(stmt, col, blob, (int32_t)blobLen, NULL);
        }
            break;
        case AI_TYPE_STRING:
            rc = sqlite3_bind_text(stmt, col, auth_items_get_string(items, key), -1, NULL);
            break;
        default:
            rc = sqlite3_bind_null(stmt, col);
            break;
    }
    if (rc != SQLITE_OK) {
        os_log_debug(AUTHD_LOG, "authdb: auth_items bind failed (%i)", rc);
    }
    return rc;
}

int32_t authdb_get_key_value(authdb_connection_t dbconn, const char * table, const bool skip_maintenance, auth_items_t * out_items)
{
    int32_t rc = SQLITE_ERROR;
    char * query = NULL;
    sqlite3_stmt * stmt = NULL;
    auth_items_t items = NULL;
    
    require(table != NULL, done);
    require(out_items != NULL, done);
    
    asprintf(&query, "SELECT * FROM %s", table);
    
    rc = _prepare(dbconn, query, skip_maintenance, &stmt);
    require_noerr(rc, done);
    
    items = auth_items_create();
    while ((rc = sqlite3_step(stmt)) != SQLITE_DONE) {
        switch (rc) {
            case SQLITE_ROW:
                _parseItemsAtIndex(stmt, 1, items, (const char*)sqlite3_column_text(stmt, 0));
                break;
            default:
                _checkResult(dbconn, rc, __FUNCTION__, stmt, skip_maintenance);
                if (_is_busy(rc)) {
                    sleep(AUTHDB_BUSY_DELAY);
                } else {
                    require_noerr_action(rc, done, os_log_debug(AUTHD_LOG, "authdb: get_key_value (%i) %{public}s", rc, sqlite3_errmsg(dbconn->handle)));
                }
                break;
        }
    }
    
    rc = SQLITE_OK;
    CFRetain(items);
    *out_items = items;
    
done:
    CFReleaseSafe(items);
    free_safe(query);
    sqlite3_finalize(stmt);
    return rc;
}

int32_t authdb_set_key_value(authdb_connection_t dbconn, const char * table, auth_items_t items)
{
    __block int32_t rc = SQLITE_ERROR;
    char * query = NULL;
    sqlite3_stmt * stmt = NULL;
    
    require(table != NULL, done);
    require(items != NULL, done);
    
    asprintf(&query, "INSERT OR REPLACE INTO %s VALUES (?,?)", table);
    
    rc = _prepare(dbconn, query, false, &stmt);
    require_noerr(rc, done);
    
    auth_items_iterate(items, ^bool(const char *key) {
        sqlite3_reset(stmt);
        _checkResult(dbconn, rc, __FUNCTION__, stmt, false);
        
        sqlite3_bind_text(stmt, 1, key, -1, NULL);
        _bindItemsAtIndex(stmt, 2, items, key);
        
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            _checkResult(dbconn, rc, __FUNCTION__, stmt, false);
            os_log_debug(AUTHD_LOG, "authdb: set_key_value, step (%i) %{public}s", rc, sqlite3_errmsg(dbconn->handle));
        }
        
        return true;
    });
    
done:
    free_safe(query);
    sqlite3_finalize(stmt);
    return rc;
}

static int32_t _begin_transaction_type(authdb_connection_t dbconn, AuthDBTransactionType type)
{
    int32_t result = SQLITE_ERROR;
    
    const char * query = NULL;
    switch (type) {
        case AuthDBTransactionImmediate:
            query = "BEGIN IMMEDIATE;";
            break;
        case AuthDBTransactionExclusive:
            query = "BEGIN EXCLUSIVE;";
            break;
        case AuthDBTransactionNormal:
            query = "BEGIN;";
            break;
        default:
            break;
    }
    
    result = SQLITE_OK;
    
    if (query != NULL && sqlite3_get_autocommit(dbconn->handle) != 0) {
        result = _sqlite3_exec(dbconn->handle, query);
    }
    
    return result;
}

static int32_t _end_transaction(authdb_connection_t dbconn, bool commit)
{
    if (commit) {
        return _sqlite3_exec(dbconn->handle, "END;");
    } else {
        return _sqlite3_exec(dbconn->handle, "ROLLBACK;");
    }
}

bool authdb_transaction(authdb_connection_t dbconn, AuthDBTransactionType type, bool (^t)(void))
{
    int32_t result = SQLITE_ERROR;
    bool commit = false;

    result = _begin_transaction_type(dbconn, type);
    require_action(result == SQLITE_OK, done, os_log_debug(AUTHD_LOG, "authdb: transaction begin failed %i", result));

    commit = t();
    
    result = _end_transaction(dbconn, commit);
    require_action(result == SQLITE_OK, done, commit = false; os_log_debug(AUTHD_LOG, "authdb: transaction end failed %i", result));

done:
    return commit;
}

bool authdb_step(authdb_connection_t dbconn, const char * sql, void (^bind_stmt)(sqlite3_stmt*), authdb_iterator_t iter)
{
    bool result = false;
    sqlite3_stmt * stmt = NULL;
    int32_t rc = SQLITE_ERROR;
    
    require_action(sql != NULL, done, rc = SQLITE_ERROR);
    
    rc = _prepare(dbconn, sql, false, &stmt);
    require_noerr(rc, done);
    
    if (bind_stmt) {
        bind_stmt(stmt);
    }
    
    int32_t count = sqlite3_column_count(stmt);
    
    auth_items_t items = NULL;
    while ((rc = sqlite3_step(stmt)) != SQLITE_DONE) {
        switch (rc) {
            case SQLITE_ROW:
                {
                    if (iter) {
                        items = auth_items_create();
                        for (int i = 0; i < count; i++) {
                            _parseItemsAtIndex(stmt, i, items, sqlite3_column_name(stmt, i));
                        }
                        result = iter(items);
                        CFReleaseNull(items);
                        if (!result) {
                            goto done;
                        }
                    }
                }
                break;
            default:
                if (_is_busy(rc)) {
                    os_log(AUTHD_LOG, "authdb: %{public}s (will try to recover)", sqlite3_errmsg(dbconn->handle));
                    sleep(AUTHDB_BUSY_DELAY);
                    sqlite3_reset(stmt);
                } else {
                    require_noerr_action(rc, done, os_log_debug(AUTHD_LOG, "authdb: step (%i) %{public}s", rc, sqlite3_errmsg(dbconn->handle)));
                }
                break;
        }
    }
    
done:
    _checkResult(dbconn, rc, __FUNCTION__, stmt, false);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

void authdb_checkpoint(authdb_connection_t dbconn)
{
    int32_t rc = sqlite3_wal_checkpoint(dbconn->handle, NULL);
    if (rc != SQLITE_OK) {
        os_log_debug(AUTHD_LOG, "authdb: checkpoit failed %i", rc);
    }
}

static CFMutableArrayRef
_copy_rules_dict(RuleType type, CFDictionaryRef plist, authdb_connection_t dbconn)
{
    CFMutableArrayRef result = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    require(result != NULL, done);

    _cf_dictionary_iterate(plist, ^bool(CFTypeRef key, CFTypeRef value) {
        if (CFGetTypeID(key) != CFStringGetTypeID()) {
            return true;
        }
        
        if (CFGetTypeID(value) != CFDictionaryGetTypeID()) {
            return true;
        }
        
        rule_t rule = rule_create_with_plist(type, key, value, dbconn);
        if (rule) {
            CFArrayAppendValue(result, rule);
            CFReleaseSafe(rule);
        }
        
        return true;
    });
    
done:
    return result;
}

static void
_import_rules(authdb_connection_t dbconn, CFMutableArrayRef rules, bool version_check, CFAbsoluteTime now, const char *nameFilter)
{
    CFMutableArrayRef notcommited = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    CFIndex count = CFArrayGetCount(rules);
    
    for (CFIndex i = 0; i < count; i++) {
        rule_t rule = (rule_t)CFArrayGetValueAtIndex(rules, i);
        
        if (nameFilter && (strcmp(nameFilter, rule_get_name(rule)) != 0)) {
            // current rule name does not match the requested filter
            continue;
        }
        
        bool update = false;
        if (version_check) {
            if (rule_get_id(rule) != 0) { // rule already exists see if we need to update
                rule_t current = rule_create_with_string(rule_get_name(rule), dbconn);
                int64_t currVer = rule_get_version(current);
                int64_t newVer = rule_get_version(rule);

                if (newVer > currVer) {
                    update = true;
                }
                CFReleaseSafe(current);
                
                if (!update) {
                    continue;
                } else {
                    os_log(AUTHD_LOG, "authdb: right %{public}s new version %lld vs existing version %lld, will update", rule_get_name(rule), newVer, currVer);
                }
            }
        }
        
        __block bool delayCommit = false;
        
        switch (rule_get_type(rule)) {
            case RT_RULE:
                rule_delegates_iterator(rule, ^bool(rule_t delegate) {
                    if (rule_get_id(delegate) == 0) {
                        // fetch the rule from the database if it was previously committed
                        rule_sql_fetch(delegate, dbconn);
                    }
                    if (rule_get_id(delegate) == 0) {
                        os_log_debug(AUTHD_LOG, "authdb: delaying %{public}s waiting for delegate %{public}s", rule_get_name(rule), rule_get_name(delegate));
                        delayCommit = true;
                        return false;
                    }
                    return true;
                });
                break;
            default:
                break;
        }
        
        if (!delayCommit) {
            bool success = rule_sql_commit(rule, dbconn, now, NULL);
            os_log(AUTHD_LOG, "authdb: %{public}s %{public}s %{public}s %{public}s",
                 update ? "updating" : "importing",
                 rule_get_type(rule) == RT_RULE ? "rule" : "right",
                 rule_get_name(rule), success ? "success" : "FAIL");
            if (!success) {
                CFArrayAppendValue(notcommited, rule);
            }
        } else {
            CFArrayAppendValue(notcommited, rule);
        }
    }
    CFArrayRemoveAllValues(rules);
    CFArrayAppendArray(rules, notcommited, CFRangeMake(0, CFArrayGetCount(notcommited)));
    CFReleaseSafe(notcommited);
}

bool
authdb_import_plist(authdb_connection_t dbconn, CFDictionaryRef plist, bool version_check, const char *name)
{
    bool result = false;
    
    os_log_debug(AUTHD_LOG, "authdb: starting import");
    
    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
    CFMutableArrayRef rights = NULL;
    CFMutableArrayRef rules = NULL;
    require(plist != NULL, done);
    
    CFTypeRef rightsDict = CFDictionaryGetValue(plist, CFSTR("rights"));
    if (rightsDict && CFGetTypeID(rightsDict) == CFDictionaryGetTypeID()) {
        rights = _copy_rules_dict(RT_RIGHT, rightsDict, dbconn);
    }
    
    CFTypeRef rulesDict = CFDictionaryGetValue(plist, CFSTR("rules"));
    if (rulesDict && CFGetTypeID(rulesDict) == CFDictionaryGetTypeID()) {
        rules = _copy_rules_dict(RT_RULE, rulesDict, dbconn);
    }

    if (!name) {
        os_log_debug(AUTHD_LOG, "authdb: rights = %li", CFArrayGetCount(rights));
        os_log_debug(AUTHD_LOG, "authdb: rules = %li", CFArrayGetCount(rules));
    }
    
    CFIndex count;
    // first pass import base rules without delegations
    // remaining import rules that delegate to other rules
    // loop upto 3 times to commit dependent rules first
    for (int32_t j = 0; j < 3; j++) {
        count = CFArrayGetCount(rules);
        if (!count)
            break;

        _import_rules(dbconn, rules, version_check, now, name);
    }
    
    _import_rules(dbconn, rights, version_check, now, name);
    
    if (CFArrayGetCount(rights) == 0) {
        result = true;
    }
    
    authdb_checkpoint(dbconn);
    
done:
    CFReleaseSafe(rights);
    CFReleaseSafe(rules);
    
    os_log_debug(AUTHD_LOG, "authdb: finished import, %{public}s", result ? "succeeded" : "failed");
    
    return result;
}

#pragma mark -
#pragma mark authdb_connection_t

static bool _sql_profile_enabled(void)
{
    static bool profile_enabled = false;
    
#if DEBUG
    static dispatch_once_t onceToken;

    //sudo defaults write /Library/Preferences/com.apple.security.auth profile -bool true
    dispatch_once(&onceToken, ^{
		CFTypeRef profile = (CFNumberRef)CFPreferencesCopyValue(CFSTR("profile"), CFSTR(SECURITY_AUTH_NAME), kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
        
        if (profile && CFGetTypeID(profile) == CFBooleanGetTypeID()) {
            profile_enabled = CFBooleanGetValue((CFBooleanRef)profile);
        }
        
        os_log_debug(AUTHD_LOG, "authdb: sql profile: %{public}s", profile_enabled ? "enabled" : "disabled");
        
        CFReleaseSafe(profile);
    });
#endif
    
    return profile_enabled;
}

static void _profile(void *context AUTH_UNUSED, const char *sql, sqlite3_uint64 ns) {
    os_log_debug(AUTHD_LOG, "==\nauthdb: %{private}s\nTime: %llu ms\n", sql, ns >> 20);
}

static sqlite3 * _create_handle(authdb_t db)
{
    bool dbcreated = false;
    sqlite3 * handle = NULL;
    int32_t rc = sqlite3_open_v2(db->db_path, &handle, SQLITE_OPEN_READWRITE, NULL);
    
    if (rc != SQLITE_OK) {
		os_log_error(AUTHD_LOG, "authdb: open %{public}s (%i) %{public}s", db->db_path, rc, handle ? sqlite3_errmsg(handle) : "no memory for handle");
		if (handle) {
			sqlite3_close(handle);
		}
        char * tmp = dirname(db->db_path);
        if (tmp) {
            mkpath_np(tmp, 0700);
		}
		rc = sqlite3_open_v2(db->db_path, &handle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
		dbcreated = true;

		if (rc != SQLITE_OK) {
			os_log_error(AUTHD_LOG, "authdb: create %{public}s (%i) %{public}s", db->db_path, rc, handle ? sqlite3_errmsg(handle) : "no memory for handle");
			if (handle) {
				sqlite3_close(handle);
				handle = NULL;
			}
			goto done;
		}
	}

    if (_sql_profile_enabled()) {
        sqlite3_profile(handle, _profile, NULL);
    }
    
    _sqlite3_exec(handle, "PRAGMA foreign_keys = ON");
    _sqlite3_exec(handle, "PRAGMA temp_store = MEMORY");
    
    if (dbcreated) {
        _sqlite3_exec(handle, "PRAGMA auto_vacuum = FULL");
        _sqlite3_exec(handle, "PRAGMA journal_mode = WAL");
        
        int on = 1;
        sqlite3_file_control(handle, 0, SQLITE_FCNTL_PERSIST_WAL, &on);
        
        chmod(db->db_path, S_IRUSR | S_IWUSR);
    }
    
    // Let SQLite handle timeouts.
    sqlite3_busy_timeout(handle, 5*1000);
done:
    return handle;
}

static void
_authdb_connection_finalize(CFTypeRef value)
{
    authdb_connection_t dbconn = (authdb_connection_t)value;
    
    if (dbconn->handle) {
        sqlite3_close(dbconn->handle);
    }
    CFReleaseNull(dbconn->db);
}

AUTH_TYPE_INSTANCE(authdb_connection,
                   .init = NULL,
                   .copy = NULL,
                   .finalize = _authdb_connection_finalize,
                   .equal = NULL,
                   .hash = NULL,
                   .copyFormattingDesc = NULL,
                   .copyDebugDesc = NULL
                   );

static CFTypeID authdb_connection_get_type_id(void) {
    static CFTypeID type_id = _kCFRuntimeNotATypeID;
    static dispatch_once_t onceToken;
    
    dispatch_once(&onceToken, ^{
        type_id = _CFRuntimeRegisterClass(&_auth_type_authdb_connection);
    });
    
    return type_id;
}

authdb_connection_t
authdb_connection_create(authdb_t db)
{
    authdb_connection_t dbconn = NULL;
    
    dbconn = (authdb_connection_t)_CFRuntimeCreateInstance(kCFAllocatorDefault, authdb_connection_get_type_id(), AUTH_CLASS_SIZE(authdb_connection), NULL);
    require(dbconn != NULL, done);
    
    dbconn->db = (authdb_t)CFRetain(db);
    dbconn->handle = _create_handle(dbconn->db);

done:
    return dbconn;
}

OSStatus authdb_reset(authdb_connection_t dbconn)
{
    os_log(AUTHD_LOG, "Resetting database");
    const char *query = "DROP TABLE buttons; DROP TABLE delegates_map; DROP TABLE mechanisms_map; DROP TABLE rules; DROP TABLE config; DROP TABLE mechanisms; DROP TABLE prompts;";
    int32_t res = _sqlite3_exec(dbconn->handle, query); // error is logged inside exec and we need to continue even when error occured
    authdb_maintenance(dbconn);
    os_log(AUTHD_LOG, "Reset finished");
    return (OSStatus)res;
}
