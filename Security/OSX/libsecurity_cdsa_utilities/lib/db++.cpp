/*
 * Copyright (c) 2003-2004,2006,2011,2014 Apple Inc. All Rights Reserved.
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
// dbm++ - generic C++ layer interface to [n]dbm
//
#include "db++.h"
#include <security_utilities/debugging.h>


namespace Security {
namespace UnixPlusPlus {

UnixDb::UnixDb() : mDb(NULL)
{
}

UnixDb::UnixDb(const char *path, int flags, int mode, DBTYPE type) : mDb(NULL)
{
	open(path, flags, mode);
}

UnixDb::UnixDb(const std::string &path, int flags, int mode, DBTYPE type) : mDb(NULL)
{
	open(path, flags, mode);
}
	
UnixDb::~UnixDb()
{
	close();
}

void UnixDb::open(const char *path, int flags, int mode, DBTYPE type)
{
	if (DB* newDb = ::dbopen(path, flags, mode, type, NULL)) {
		close();
		mDb = newDb;
		setFd(mDb->fd(mDb));
		secdebug("unixdb", "open(%s,0x%x,0x%x,type=%d)=%p", path, flags, mode, type, mDb);
	} else
		UnixError::throwMe();
}

void UnixDb::open(const std::string &path, int flags, int mode, DBTYPE type)
{
	open(path.c_str(), flags, mode);
}

void UnixDb::close()
{
	if (mDb) {
		secdebug("unixdb", "close(%p)", mDb);
		mDb->close(mDb);
		mDb = NULL;
		setFd(invalidFd);
	}
}

bool UnixDb::get(const CssmData &key, CssmData &value, int flags) const
{
	Data dKey(key);
	Data val;
	int rc = mDb->get(mDb, &dKey, &val, flags);
	secdebug("unixdb", "get(%p,[:%ld],flags=0x%x)=%d[:%ld]",
		mDb, key.length(), flags, rc, value.length());
	checkError(rc);
	if (!rc) {
		value = val;
		return true;
	} else
		return false;
}

bool UnixDb::get(const CssmData &key, CssmOwnedData &value, int flags) const
{
	CssmData val;
	if (get(key, val, flags)) {
		value = val;
		return true;
	} else
		return false;
}

bool UnixDb::put(const CssmData &key, const CssmData &value, int flags)
{
	Data dKey(key);
	Data dValue(value);
	int rc = mDb->put(mDb, &dKey, &dValue, flags);
	secdebug("unixdb", "put(%p,[:%ld],[:%ld],flags=0x%x)=%d",
		mDb, key.length(), value.length(), flags, rc);
	checkError(rc);
	return !rc;
}

void UnixDb::erase(const CssmData &key, int flags)
{
	Data dKey(key);
	secdebug("unixdb", "delete(%p,[:%ld],flags=0x%x)", mDb, key.length(), flags);
	checkError(mDb->del(mDb, &dKey, flags));
}

bool UnixDb::next(CssmData &key, CssmData &value, int flags /* = R_NEXT */) const
{
	Data dKey, dValue;
	int rc = mDb->seq(mDb, &dKey, &dValue, flags);
	checkError(rc);
	if (!rc) {
		key = dKey;
		value = dValue;
		return true;
	} else
		return false;
}


void UnixDb::flush(int flags)
{
	checkError(mDb->sync(mDb, flags));
}


}	// end namespace UnixPlusPlus
}	// end namespace Security
