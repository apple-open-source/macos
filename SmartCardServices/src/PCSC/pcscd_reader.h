/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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

/*
 *  reader.h
 *  SmartCardServices
 */

#ifndef _H_PCSCD_READER
#define _H_PCSCD_READER

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>
#include "wintypes.h"
#include "pcsclite.h"
#include "readerfactory.h"
#include <security_utilities/refcount.h>
#include <security_cdsa_utilities/handleobject.h>
#include <map>

#if 0
	struct ReaderContext
	{
		char lpcReader[MAX_READERNAME];	/* Reader Name */
		char lpcLibrary[MAX_LIBNAME];	/* Library Path */
		PCSCLITE_THREAD_T pthThread;	/* Event polling thread */
		PCSCLITE_MUTEX_T mMutex;	/* Mutex for this connection */
		RDR_CAPABILITIES psCapabilites;	/* Structure of reader
						   capabilities */
		PROT_OPTIONS psProtOptions;	/* Structure of protocol options */
		RDR_CLIHANDLES psHandles[PCSCLITE_MAX_CONTEXTS];	
                                         /* Structure of connected handles */
		FCT_MAP psFunctions;	/* Structure of function pointers */
		UCHAR ucAtr[MAX_ATR_SIZE];	/* Atr for inserted card */
		DWORD dwAtrLen;			/* Size of the ATR */
		LPVOID vHandle;			/* Dlopen handle */
		DWORD dwVersion;		/* IFD Handler version number */
		DWORD dwPort;			/* Port ID */
		DWORD dwProtocol;		/* Currently used protocol */
		DWORD dwSlot;			/* Current Reader Slot */
		DWORD dwBlockStatus;	/* Current blocking status */
		DWORD dwStatus;			/* Current Status Mask */
		DWORD dwLockId;			/* Lock Id */
		DWORD dwIdentity;		/* Shared ID High Nibble */
		DWORD dwContexts;		/* Number of open contexts */
		DWORD dwPublicID;		/* Public id of public state struct */
		PDWORD dwFeeds;			/* Number of shared client to lib */
	};
#endif

#if defined(__cplusplus)

namespace PCSCD {

//
// The server object itself. This is the "go to" object for anyone who wants
// to access the server's global state. It runs the show.
// There is only one Server, and its name is Server::active().
//

//
// A PODWrapper for the PCSC READER_CONTEXT structure
//
class XReaderContext : public PodWrapper<XReaderContext, READER_CONTEXT>
{
public:
	void set(const char *name, unsigned long known = SCARD_STATE_UNAWARE);
	
	const char *name() const	{ return lpcReader; }
//	void name(const char *s)	{ szReader = s; }

//	unsigned long lastKnown() const { return dwStatus; }
	void lastKnown(unsigned long s);

	unsigned long state() const { return 0; }	//fix
	bool state(unsigned long it) const { return state() & it; }
	bool changed() const		{ return state(SCARD_STATE_CHANGED); }
	
//	template <class T>
//	T * &userData() { return reinterpret_cast<T * &>(pvUserData); }
	
	// DataOid access to the ATR data
//	const void *data() const { return ucAtr; }
//	size_t length() const { return dwAtrLen; }
	void setATR(const void *atr, size_t size);
	
	IFDUMP(void dump());
};


class Reader : public HandleObject, public RefCount
{
public:
	Reader(const char *bootstrapName);
	~Reader();
private:
	// mach bootstrap registration name
	std::string mBootstrapName;
	mutable Mutex mLock;	
};

class Readers
{
public:
	Readers();
	~Readers();

	typedef std::map<uint32_t, RefPointer<PCSCD::Reader> > ReaderMap;
	ReaderMap mReaders;

	bool find(uint32_t id, XReaderContext &rc) const;
	bool find(const char *name, XReaderContext &rc) const;
	bool find(uint32_t port, const char *name, XReaderContext &rc) const;
	
	mutable Mutex mReaderMapLock;

	void insert(pair<uint32_t, RefPointer<PCSCD::Reader> > readerpair) { StLock<Mutex> _(mReaderMapLock); mReaders.insert(readerpair); }
	void remove(ReaderMap::iterator it) { StLock<Mutex> _(mReaderMapLock); mReaders.erase(it); }

private:
	mutable Mutex mLock;	
};

} // end namespace PCSCD

#endif /* __cplusplus__ */

#endif //_H_PCSCD_READER

