/*
 * Copyright (c) 2003-2004,2006 Apple Computer, Inc. All Rights Reserved.
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
#ifndef _H_DBMPP
#define _H_DBMPP

#include <security_utilities/utilities.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <security_utilities/unix++.h>
#include <string>
#include <db.h>


namespace Security {
namespace UnixPlusPlus {


class UnixDb : public FileDesc {
public:
	UnixDb();
	UnixDb(const char *path, int flags = O_RDWR, int mode = 0666, DBTYPE type = DB_HASH);
	UnixDb(const std::string &path, int flags = O_RDWR, int mode = 0666, DBTYPE type = DB_HASH);
	
	virtual ~UnixDb();
		
	void open(const char *path, int flags = O_RDWR, int mode = 0666, DBTYPE type = DB_HASH);
	void open(const std::string &path, int flags = O_RDWR, int mode = 0666, DBTYPE type = DB_HASH);
	void close();

	bool get(const CssmData &key, CssmData &value, int flags = 0) const;
	bool get(const CssmData &key, CssmOwnedData &value, int flags = 0) const;
	bool put(const CssmData &key, const CssmData &value, int flags = 0);
	void erase(const CssmData &key, int flags = 0);
	void flush(int flags = 0);
	
	bool next(CssmData &key, CssmData &value, int flags = R_NEXT) const;
	bool first(CssmData &key, CssmData &value) const
		{ return next(key, value, R_FIRST); }
	
	operator bool () const
		{ return mDb; }
	
public:
	struct Data : public PodWrapper<Data, DBT> {
		template <class T>
		Data(const T &src)		{ DBT::data = src.data(); DBT::size = src.length(); }
		
		Data() { }
		Data(void *data, size_t length) { DBT::data = data; DBT::size = length; }
		Data(const DBT &dat)	{ DBT::data = dat.data; DBT::size = dat.size; }
		
		void *data() const		{ return DBT::data; }
		size_t length() const	{ return size; }
		operator bool () const	{ return DBT::data != NULL; }
		operator CssmData () const { return CssmData(data(), length()); }
	};

private:
	DB *mDb;
};


}	// end namespace UnixPlusPlus
}	// end namespace Security


#endif //_H_DBMPP
