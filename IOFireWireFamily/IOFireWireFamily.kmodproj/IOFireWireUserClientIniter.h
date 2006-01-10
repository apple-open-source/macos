/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
 *  IOFireWireUserClientIniter.h
 *  IOFireWireFamily
 *
 *  Created by NWG on Wed Jan 24 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#ifndef __IOKIT_IOFireWireUserClientIniter_H__
#define __IOKIT_IOFireWireUserClientIniter_H__

#include <IOKit/IOService.h>

/*! @class IOFireWireUserClientIniter
*/
class IOFireWireUserClientIniter : public IOService 
{
    OSDeclareDefaultStructors(IOFireWireUserClientIniter);

private:
	/*! @struct ExpansionData
		@discussion This structure will be used to expand the capablilties of the class in the future.
		*/    
	struct ExpansionData { };
	
	/*! @var reserved
		Reserved for future use.  (Internal use only)  */
	ExpansionData *reserved;

	IOService*						fProvider;

	static IORecursiveLock *		sIniterLock;

public:
	virtual bool					start(IOService* provider);
	virtual bool					init(OSDictionary* propTable);
	virtual void					free();
	virtual void					stop(IOService* provider);
	
protected:
	void				mergeProperties( IORegistryEntry * dest, OSDictionary * src );
	void				mergeDictionaries( OSDictionary * dest, OSDictionary * src );
	OSDictionary*		dictionaryDeepCopy(OSDictionary* srcDictionary);

private:
    OSMetaClassDeclareReservedUnused(IOFireWireUserClientIniter, 0);
    OSMetaClassDeclareReservedUnused(IOFireWireUserClientIniter, 1);
    OSMetaClassDeclareReservedUnused(IOFireWireUserClientIniter, 2);
    OSMetaClassDeclareReservedUnused(IOFireWireUserClientIniter, 3);
};

#endif//#ifndef __IOKIT_IOFireWireUserClientIniter_H__
