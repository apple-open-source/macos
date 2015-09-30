/* Copyright (c) 2012 Apple Inc. All Rights Reserved. */

#include "mechanism.h"
#include "authdb.h"
#include "authutilities.h"
#include "crc.h"
#include "debugging.h"
#include "server.h"
#include "authitems.h"

#define MECHANISM_ID "id"
#define MECHANISM_PLUGIN "plugin"
#define MECHANISM_PARAM "param"
#define MECHANISM_PRIVILEGED "privileged"

static const char SystemPlugins[] = "/System/Library/CoreServices/SecurityAgentPlugins";
static const char LibraryPlugins[] = "/Library/Security/SecurityAgentPlugins";
static const char BuiltinMechanismPrefix[] = "builtin";

typedef struct _mechTypeItem
{
    const char * name;
    uint64_t type;
} mechTypeItem;

static mechTypeItem mechTypeMap[] =
{
    { "entitled", kMechanismTypeEntitled }
};

struct _mechanism_s {
    __AUTH_BASE_STRUCT_HEADER__;
    
    auth_items_t data;

    bool valid;
    char * string;
    
    uint64_t type;
};

static void
_mechanism_finalize(CFTypeRef value)
{
    mechanism_t mech = (mechanism_t)value;
    
    CFReleaseSafe(mech->data);
    free_safe(mech->string);
}

static Boolean
_mechanism_equal(CFTypeRef value1, CFTypeRef value2)
{
    mechanism_t mech1 = (mechanism_t)value1;
    mechanism_t mech2 = (mechanism_t)value2;
    
    if (mech1 == mech2) {
        return true;
    }
    
    if (!_compare_string(mechanism_get_plugin(mech1), mechanism_get_plugin(mech2))) {
        return false;
    }
    
    if (!_compare_string(mechanism_get_param(mech1), mechanism_get_param(mech2))) {
        return false;
    }
    
    return mechanism_is_privileged(mech1) == mechanism_is_privileged(mech2);
}

static CFStringRef
_mechanism_copy_description(CFTypeRef value)
{
    mechanism_t mech = (mechanism_t)value;
    return CFCopyDescription(mech->data);
}

static CFHashCode
_mechanism_hash(CFTypeRef value)
{
    uint64_t crc = crc64_init();
    mechanism_t mech = (mechanism_t)value;
    
    const char * str = mechanism_get_plugin(mech);
    crc = crc64_update(crc, str, strlen(str));
    str = mechanism_get_plugin(mech);
    crc = crc64_update(crc, str, strlen(str));
    bool priv = mechanism_is_privileged(mech);
    crc = crc64_update(crc, &priv, sizeof(priv));
    crc = crc64_final(crc);
    
    return (CFHashCode)crc;
}

AUTH_TYPE_INSTANCE(mechanism,
                   .init = NULL,
                   .copy = NULL,
                   .finalize = _mechanism_finalize,
                   .equal = _mechanism_equal,
                   .hash = _mechanism_hash,
                   .copyFormattingDesc = NULL,
                   .copyDebugDesc = _mechanism_copy_description
                   );

static CFTypeID mechanism_get_type_id() {
    static CFTypeID type_id = _kCFRuntimeNotATypeID;
    static dispatch_once_t onceToken;
    
    dispatch_once(&onceToken, ^{
        type_id = _CFRuntimeRegisterClass(&_auth_type_mechanism);
    });
    
    return type_id;
}

static mechanism_t
_mechanism_create()
{
    mechanism_t mech = (mechanism_t)_CFRuntimeCreateInstance(kCFAllocatorDefault, mechanism_get_type_id(), AUTH_CLASS_SIZE(mechanism), NULL);
    require(mech != NULL, done);
    
    mech->data = auth_items_create();
    
done:
    return mech;
}

static void _mechanism_set_type(mechanism_t mech)
{
    const char * plugin = mechanism_get_plugin(mech);
    const char * param = mechanism_get_param(mech);
    if (strncasecmp(plugin, BuiltinMechanismPrefix, sizeof(BuiltinMechanismPrefix)) == 0) {
        size_t n = sizeof(mechTypeMap)/sizeof(mechTypeItem);
        for (size_t i = 0; i < n; i++) {
            if (strcasecmp(mechTypeMap[i].name, param) == 0) {
                mech->type = mechTypeMap[i].type;
                break;
            }
        }
    }
}

mechanism_t
mechanism_create_with_sql(auth_items_t sql)
{
    mechanism_t mech = NULL;
    require(sql != NULL, done);
    require(auth_items_get_int64(sql, MECHANISM_ID) != 0, done);
    
    mech = _mechanism_create();
    require(mech != NULL, done);
    
    auth_items_copy(mech->data, sql);
    
    _mechanism_set_type(mech);
    
done:
    return mech;
}

mechanism_t
mechanism_create_with_string(const char * str, authdb_connection_t dbconn)
{
    mechanism_t mech = NULL;
    require(str != NULL, done);
    require(strchr(str,':') != NULL, done);
    
    mech = _mechanism_create();
    require(mech != NULL, done);
    
    const char delimiters[] = ":,";
    size_t buf_len = strlen(str)+1;
    char * buf = (char*)calloc(1u, buf_len);
    strlcpy(buf, str, buf_len);
    
    char * tok = strtok(buf, delimiters);
    if (tok) {
        auth_items_set_string(mech->data, MECHANISM_PLUGIN, tok);
    }
    tok = strtok(NULL, delimiters);
    if (tok) {
        auth_items_set_string(mech->data, MECHANISM_PARAM, tok);
    }
    tok = strtok(NULL, delimiters);
    if (tok) {
        auth_items_set_int64(mech->data, MECHANISM_PRIVILEGED, strcasecmp("privileged", tok) == 0);
    }
    free(buf);
    
    if (dbconn) {
        mechanism_sql_fetch(mech, dbconn);
    }
    
    _mechanism_set_type(mech);
    
done:
    return mech;
}

static
bool _pluginExists(const char * plugin, const char * base)
{
    bool result = false;
    
    require(plugin != NULL, done);
    require(base != NULL, done);

    char filePath[PATH_MAX];
    char realPath[PATH_MAX+1];
    snprintf(filePath, sizeof(filePath), "%s/%s.bundle", base, plugin);

    require(realpath(filePath, realPath) != NULL, done);
    require(strncmp(realPath, base, strlen(base)) == 0, done);
    
    if (access(filePath, F_OK) == 0) {
        result = true;
    }
    
done:
    return result;
}

bool
mechanism_exists(mechanism_t mech)
{
    if (mech->valid) {
        return true;
    }

    const char * plugin = mechanism_get_plugin(mech);
    if (plugin == NULL) {
        return false;
    }
    
    if (strncasecmp(plugin, BuiltinMechanismPrefix, sizeof(BuiltinMechanismPrefix)) == 0) {
        mech->valid = true;
        return true;
    }

    if (_pluginExists(plugin, SystemPlugins)) {
        mech->valid = true;
        return true;
    }
    
    if (_pluginExists(plugin,LibraryPlugins)) {
        mech->valid = true;
        return true;
    }
    
    return false;
}

bool
mechanism_sql_fetch(mechanism_t mech, authdb_connection_t dbconn)
{
    __block bool result = false;
    
    authdb_step(dbconn, "SELECT id FROM mechanisms WHERE plugin = ? AND param = ? AND privileged = ? LIMIT 1", ^(sqlite3_stmt * stmt) {
        sqlite3_bind_text(stmt, 1, mechanism_get_plugin(mech), -1, NULL);
        sqlite3_bind_text(stmt, 2, mechanism_get_param(mech), -1, NULL);
        sqlite3_bind_int(stmt, 3, mechanism_is_privileged(mech));
    }, ^bool(auth_items_t data) {
        result = true;
        auth_items_copy(mech->data, data);
        return true;
    });
    
    return result;
}

bool
mechanism_sql_commit(mechanism_t mech, authdb_connection_t dbconn)
{
    bool result = false;

    result = authdb_step(dbconn, "INSERT INTO mechanisms VALUES (NULL,?,?,?)", ^(sqlite3_stmt *stmt) {
        sqlite3_bind_text(stmt, 1, mechanism_get_plugin(mech), -1, NULL);
        sqlite3_bind_text(stmt, 2, mechanism_get_param(mech), -1, NULL);
        sqlite3_bind_int(stmt, 3, mechanism_is_privileged(mech));
    }, NULL);
    
    return result;
}

const char *
mechanism_get_string(mechanism_t mech)
{
    if (!mech->string) {
        asprintf(&mech->string, "%s:%s%s", mechanism_get_plugin(mech), mechanism_get_param(mech), mechanism_is_privileged(mech) ? ",privileged" : "");
    }
    
    return mech->string;
}

int64_t
mechanism_get_id(mechanism_t mech)
{
    return auth_items_get_int64(mech->data, MECHANISM_ID);
}

const char *
mechanism_get_plugin(mechanism_t mech)
{
    return auth_items_get_string(mech->data, MECHANISM_PLUGIN);
}

const char *
mechanism_get_param(mechanism_t mech)
{
    return auth_items_get_string(mech->data, MECHANISM_PARAM);
}

bool
mechanism_is_privileged(mechanism_t mech)
{
    return auth_items_get_int64(mech->data, MECHANISM_PRIVILEGED);
}

uint64_t
mechanism_get_type(mechanism_t mech)
{
    return mech->type;
}
