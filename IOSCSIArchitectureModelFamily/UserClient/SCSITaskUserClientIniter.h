/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOKIT_SCSI_TASK_USER_CLIENT_INITER_H_
#define _IOKIT_SCSI_TASK_USER_CLIENT_INITER_H_

#if defined(KERNEL) && defined(__cplusplus)

#include <IOKit/IOLib.h>
#include <IOKit/IOUserClient.h>

#include <libkern/c++/OSDictionary.h>

#include <IOKit/IOService.h>

class SCSITaskUserClientIniter : public IOService 
{
	
	OSDeclareDefaultStructors ( SCSITaskUserClientIniter );
	
private:
	
    virtual bool start ( IOService * provider );
	
};

#endif	/* defined(KERNEL) && defined(__cplusplus) */

#endif /* ! _IOKIT_SCSI_TASK_USER_CLIENT_INITER_H_ */