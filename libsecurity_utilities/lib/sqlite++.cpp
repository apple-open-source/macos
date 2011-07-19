/*
 * Copyright (c) 2008 Apple Computer, Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
//
// sqlite++ - C++ interface to SQLite3
//
#include "sqlite++.h"
#include <stdexcept>
#include <security_utilities/cfutilities.h>


//@@@
// From cssmapple.h - layering break
// Where should this go?
//@@@
#define errSecErrnoBase 100000
#define errSecErrnoLimit 100255


namespace Security {
namespace SQLite3 {


//
// Our exception object
//
void Error::check(int err)
{
	if (err != SQLITE_OK)
		throw Error(err);
}

Error::Error(Database &db)
	: error(db.errcode()), message(db.errmsg())
{
	SECURITY_EXCEPTION_THROW_SQLITE(this, error, (char*)message.c_str());
}
	
void Error::throwMe(int err)
{
	throw Error(err);
}

OSStatus Error::osStatus() const
{
	return unixError() + errSecErrnoBase;
}

int Error::unixError() const
{
	switch (error) {
	case SQLITE_PERM:
	case SQLITE_READONLY:
	case SQLITE_AUTH:
		return EACCES;
	case SQLITE_BUSY:
		return EAGAIN;
	case SQLITE_NOMEM:
		return ENOMEM;
	case SQLITE_IOERR:
		return EIO;
	case SQLITE_FULL:
		return ENOSPC;
	case SQLITE_TOOBIG:
		return EFBIG;
	case SQLITE_MISMATCH:
	case SQLITE_MISUSE:
		return EINVAL;
	case SQLITE_NOLFS:
		return ENOTSUP;
	case SQLITE_RANGE:
		return EDOM;
	default:
		return -1;
	}
}


//
// Database objects
//
Database::Database(const char *path, int flags)
	: mMutex(Mutex::recursive)
{
	try {
		check(::sqlite3_open_v2(path, &mDb, flags, NULL));
		check(::sqlite3_extended_result_codes(mDb, true));
	} catch (...) {
		sqlite3_close(mDb);		// allocated even if open fails(!)
		throw;
	}
}

Database::~Database()
{
	this->close();
}

void Database::close()
{
	if (mDb)
		check(::sqlite3_close(mDb));
}


int Database::execute(const char *text, bool strict /* = true */)
{
	StLock<Mutex> _(mMutex);

	int rc = ::sqlite3_exec(mDb, text, NULL, NULL, NULL);
	if (strict)
		check(rc);
	return rc;
}


void Database::busyDelay(int ms)
{
	StLock<Mutex> _(mMutex);

	check(::sqlite3_busy_timeout(mDb, ms));
}


void Database::check(int err)
{
	if (err)
		throw Error(*this);
}


bool Database::empty()
{
	return value("select count(*) from sqlite_master;", 0) == 0;
}


int Database::errcode()
{
	StLock<Mutex> _(mMutex);

	return sqlite3_errcode(mDb);
}



const char *Database::errmsg()
{
	StLock<Mutex> _(mMutex);

	return sqlite3_errmsg(mDb);
}



bool Database::inTransaction()
{
	StLock<Mutex> _(mMutex);

	return !::sqlite3_get_autocommit(mDb);
}



int64 Database::lastInsert()
{
	StLock<Mutex> _(mMutex);

	return ::sqlite3_last_insert_rowid(mDb);
}

void Database::interrupt()
{
	StLock<Mutex> _(mMutex);

	::sqlite3_interrupt(mDb);
}

//
// Transaction managers
//
Transaction::Transaction(Database &db, Type type, const char *name)
	: database(db), mName(name ? name : "")
{
	switch (type) {
	case deferred:	xactCommand("BEGIN DEFERRED"); break;
	case immediate:	xactCommand("BEGIN IMMEDIATE"); break;
	case exclusive:	xactCommand("BEGIN EXCLUSIVE"); break;
	}
}

Transaction::~Transaction()
{
	if (database.inTransaction())
		abort();
}

void Transaction::commit()
{
	xactCommand("COMMIT");
}

void Transaction::abort()
{
	xactCommand("ROLLBACK");
}

void Transaction::xactCommand(const string &cmd)
{
	database.execute(cmd + " TRANSACTION " + mName + ";");
}


//
// Statement objects
//
Statement::Statement(Database &db, const char *text)
	: StLock<Mutex>(db.mMutex), database(db)
{
	const char *tail;
	check(::sqlite3_prepare_v2(db.sql(), text, -1, &mStmt, &tail));
	if (*tail)
		throw std::logic_error("multiple statements");
}

Statement::~Statement()
{
	// Sqlite3_finalize will return an error if the Statement (executed and) failed.
	// So we eat any error code here, since we can't tell "genuine" errors apart from
	// errors inherited from the Statement execution.
	::sqlite3_finalize(mStmt);
}


void Statement::unbind()
{
	check(::sqlite3_clear_bindings(mStmt));
}

void Statement::reset()
{
	check(::sqlite3_reset(mStmt));
}


int Statement::step()
{
	return ::sqlite3_step(mStmt);
}

void Statement::execute()
{
	switch (int rc = this->step()) {
	case SQLITE_DONE:
	case SQLITE_OK:
		break;
	default:
		check(rc);
	}
}

bool Statement::nextRow()
{
	switch (int rc = this->step()) {
	case SQLITE_ROW:
		return true;
	case SQLITE_DONE:
		return false;
	default:
		check(rc);
		return false;
	}
}


//
// Binding gluons.
//
Statement::Binding Statement::bind(const char *name) const
{
	if (int ix = ::sqlite3_bind_parameter_index(mStmt, name))
		return Binding(*this, ix);
	else
		throw std::logic_error("unknown parameter name");
}

void Statement::Binding::operator = (const Value &value)
{
	statement.check(::sqlite3_bind_value(statement.sql(), index, value.sql()));
}

void Statement::Binding::operator = (int value)
{
	statement.check(::sqlite3_bind_int(statement.sql(), index, value));
}

void Statement::Binding::operator = (sqlite3_int64 value)
{
	statement.check(::sqlite3_bind_int64(statement.sql(), index, value));
}

void Statement::Binding::operator = (double value)
{
	statement.check(::sqlite3_bind_double(statement.sql(), index, value));
}

void Statement::Binding::operator = (const char *value)
{
	statement.check(::sqlite3_bind_text(statement.sql(), index,
		::strdup(value), -1, ::free));
}

void Statement::Binding::blob(const void *data, size_t length, bool shared /* = false */)
{
	if (data == NULL)
		statement.check(::sqlite3_bind_null(statement.sql(), index));
	else if (shared) {
		statement.check(::sqlite3_bind_blob(statement.sql(), index, data, length, NULL));
	} else if (void *copy = ::malloc(length)) {
		::memcpy(copy, data, length);
		statement.check(::sqlite3_bind_blob(statement.sql(), index,
		copy, length, ::free));
	} else
		throw std::bad_alloc();
}

void Statement::Binding::operator = (CFDataRef data)
{
	this->blob(CFDataGetBytePtr(data), CFDataGetLength(data));
}

const char *Statement::Binding::name() const
{
	return sqlite3_bind_parameter_name(statement.sql(), index);
}


//
// Row/column results
//
const char *Statement::Result::name() const
{
	return sqlite3_column_name(statement.sql(), index);
}

CFDataRef Statement::Result::data() const
{
	switch (this->type()) {
	case SQLITE_NULL:
		return NULL;
	case SQLITE_BLOB:
		return makeCFData(this->blob(), this->length());
	default:
		throw Error(SQLITE_MISMATCH, "Retrieving data() of non-Blob");
	}
}


}	// SQLite3
}	// Security
