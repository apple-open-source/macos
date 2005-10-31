/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *	File: $Id: IOI2CBus.h,v 1.3 2005/07/01 16:09:52 bwpang Exp $
 *
 *  DRI: Joseph Lehrer
 *
 *		$Log: IOI2CBus.h,v $
 *		Revision 1.3  2005/07/01 16:09:52  bwpang
 *		[4086434] added APSL headers
 *		
 *		Revision 1.2  2004/09/17 20:30:17  jlehrer
 *		Removed ASPL headers.
 *		
 *		Revision 1.1  2004/06/07 21:53:41  jlehrer
 *		Initial Checkin
 *		
 *
 */

#ifndef _IOI2CBus_H
#define _IOI2CBus_H


#include <IOKit/IOService.h>

class IOI2CBus : public IOService
{
	OSDeclareDefaultStructors(IOI2CBus)

public:
	virtual bool start ( IOService *provider );
    virtual void stop ( IOService *provider );
	virtual void free ( void );

	using IOService::callPlatformFunction;
	virtual IOReturn callPlatformFunction (
		const OSSymbol *functionName,
		bool waitForFunction,
		void *param1, void *param2,
		void *param3, void *param4 );

protected:
	UInt32		fI2CBus;
	IOService	*fProvider;

	const OSSymbol	*symReadI2CBus;
	const OSSymbol	*symWriteI2CBus;
	const OSSymbol	*symLockI2CBus;
	const OSSymbol	*symUnlockI2CBus;

protected:
	// Space reserved for future expansion.
    OSMetaClassDeclareReservedUnused ( IOI2CBus,  0 );
    OSMetaClassDeclareReservedUnused ( IOI2CBus,  1 );
    OSMetaClassDeclareReservedUnused ( IOI2CBus,  2 );
    OSMetaClassDeclareReservedUnused ( IOI2CBus,  3 );
    OSMetaClassDeclareReservedUnused ( IOI2CBus,  4 );
    OSMetaClassDeclareReservedUnused ( IOI2CBus,  5 );
    OSMetaClassDeclareReservedUnused ( IOI2CBus,  6 );
    OSMetaClassDeclareReservedUnused ( IOI2CBus,  7 );
};

#endif // _IOI2CBus_H
