/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


#ifndef _MDSSESSION_H_
#define _MDSSESSION_H_  1

#include <Security/DatabaseSession.h>
#include <Security/handleobject.h>
#include <Security/mds.h>
#include <map>
#include <sys/stat.h>
#include <list>

typedef list<class PluginInfo *> PluginInfoList;

class MDSSession: public DatabaseSession, public HandleObject
{
    NOCOPY(MDSSession)
public:
    MDSSession (const Guid *inCallerGuid,
                const CSSM_MEMORY_FUNCS &inMemoryFunctions);
    virtual ~MDSSession ();

    void terminate ();
    void install ();
    void uninstall ();

    void GetDbNames(CSSM_NAME_LIST_PTR &outNameList);
    void FreeNameList(CSSM_NAME_LIST &inNameList);

    // implement CssmHeap::Allocator
    void *malloc(size_t size) { return mCssmMemoryFunctions.malloc(size); };
    void free(void *addr) { mCssmMemoryFunctions.free(addr); }
	void *realloc(void *addr, size_t size) { return mCssmMemoryFunctions.realloc(addr, size); }

private:
	bool obtainLock(int timeout = 0);
	void releaseLock();

	void initializeDatabases();
	void updateDatabases();

	void scanPluginDirectory();
	void removeOutdatedPlugins(const PluginInfoList &pluginList);
	void insertNewPlugins(const PluginInfoList &pluginList);

    const CssmMemoryFunctions mCssmMemoryFunctions;
    Guid mCallerGuid;
    bool mCallerGuidPresent;
	
	struct timespec mLastScanTime;
	int mLockFd;
};

#endif //_MDSSESSION_H_
