/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved.
 *
 */

#ifndef _I2CUSERCLIENTPRIVATE_H
#define _I2CUSERCLIENTPRIVATE_H

#include <IOKit/IOUserClient.h>
#include "PPCI2CInterface.h"
#include "I2CUserClient.h"

#ifdef DLOG
#undef DLOG
#endif

// Uncomment to enable debug output
// #define I2CUSERCLIENT_DEBUG 1

#ifdef I2CUSERCLIENT_DEBUG
#define DLOG(fmt, args...)	IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

class I2CUserClient : public IOUserClient
{
	OSDeclareDefaultStructors(I2CUserClient)

	private:
		task_t			fOwningTask;
		PPCI2CInterface	*fProvider;
		IOLock			*fIsOpenLock;
		bool			fIsOpen;
		
	public:
		virtual bool start(IOService *provider);
		virtual void stop(IOService *provider);

		virtual IOExternalMethod *getTargetAndMethodForIndex(
				IOService **target, UInt32 Index);

		virtual IOReturn clientClose(void);

		virtual bool init(OSDictionary *dict);
		virtual bool initWithTask(task_t owningTask, void *security_id,
				UInt32 type);
		virtual void free(void);
		virtual bool attach(IOService *provider);
		virtual void detach(IOService *provider);

		// Externally accessible methods
		IOReturn userClientOpen(void);
		IOReturn userClientClose(void);
		IOReturn read( void *inStruct, void *outStruct,
				void *inCount, void *outCount, void *p5, void *p6 );
		IOReturn write( void *inStruct, void *outStruct,
				void *inCount, void *outCount, void *p5, void *p6 );
		IOReturn rmw( void *inStruct, void *inCount,
				void *p3, void *p4, void *p5, void *p6 );
};

#endif