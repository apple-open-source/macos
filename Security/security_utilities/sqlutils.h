//
//  sqlutils.h
//  Security
//
//  Created by Fabrice Gautier on 8/26/11.
//  Copyright (c) 2011 Apple, Inc. All rights reserved.
//

/*
 * sqlutils.h - some wrapper for sql3lite
 */
#ifndef _SECURITY_UTILITIES_SQLUTILS_H_
#define _SECURITY_UTILITIES_SQLUTILS_H_

#include <sqlite3.h>

/* Those are just wrapper around the sqlite3 functions, but they have size_t for some len parameters,
   and checks for overflow before casting to int */
static inline int sqlite3_bind_blob_wrapper(sqlite3_stmt* pStmt, int i, const void* zData, size_t n, void(*xDel)(void*))
{
    if(n>INT_MAX) return SQLITE_TOOBIG;
    return sqlite3_bind_blob(pStmt, i, zData, (int)n, xDel);
}

static inline int sqlite3_bind_text_wrapper(sqlite3_stmt* pStmt, int i, const void* zData, size_t n, void(*xDel)(void*))
{
    if(n>INT_MAX) return SQLITE_TOOBIG;
    return sqlite3_bind_text(pStmt, i, zData, (int)n, xDel);
}

static inline int sqlite3_prepare_wrapper(sqlite3 *db, const char *zSql, size_t nByte, sqlite3_stmt **ppStmt, const char **pzTail)
{
    if(nByte>INT_MAX) return SQLITE_TOOBIG;
    return sqlite3_prepare(db, zSql, (int)nByte, ppStmt, pzTail);
}

#endif
