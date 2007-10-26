/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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

#ifndef	_OCSPD_SERVER_H_
#define _OCSPD_SERVER_H_

#include <security_utilities/machserver.h>
#include <Security/cssmtype.h>
#include <security_ocspd/ocspd.h>						/* created by MIG */

#define MAX_OCSPD_THREADS		6

void ServerActivity();


class OcspdServer : public MachPlusPlus::MachServer
{
	NOCOPY(OcspdServer)
public:
	OcspdServer(const char *bootstrapName);
	~OcspdServer();

	Allocator		&alloc()	{ return mAlloc; }
	static OcspdServer &active() 
								{ return safer_cast<OcspdServer &>(MachServer::active()); }
	
protected:
    // implementation methods of MachServer
	boolean_t handle(mach_msg_header_t *in, mach_msg_header_t *out);
	
	/* 
	 * Timer subclass to handle periodic flushes of DB caches.
	 */
	class OcspdTimer : public MachServer::Timer
	{
		NOCOPY(OcspdTimer)
	public:
		/* TImer(false) --> !longTerm --> avoid spawning a thread for this */
		OcspdTimer(OcspdServer &server) : Timer(true), mServer(server) {}
		virtual ~OcspdTimer() {}
		virtual void action();
	private:
		OcspdServer &mServer;
	};

	/* we're not handling dead port notification for now */
private:
	Allocator		&mAlloc;
	OcspdTimer		mTimer;
};

/*
 * Given a CSSM_DATA which was allocated in our server's alloc space, 
 * pass referent data back to caller and schedule a dealloc after the RPC
 * completes with MachServer.
 */
extern void passDataToCaller(
	CSSM_DATA		&srcData,		// allocd in our server's alloc space
	Data			*outData,
	mach_msg_type_number_t *outDataCnt);

#endif	/* _OCSPD_SERVER_H_ */


