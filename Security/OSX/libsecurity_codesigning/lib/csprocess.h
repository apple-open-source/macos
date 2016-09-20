/*
 * Copyright (c) 2006,2011,2013-2014 Apple Inc. All Rights Reserved.
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
// csprocess - UNIX process implementation of the Code Signing Host Interface
//
#ifndef _H_CSPROCESS
#define _H_CSPROCESS

#include "csgeneric.h"
#include "StaticCode.h"
#include "PidDiskRep.h"
#include <security_utilities/utilities.h>

namespace Security {
namespace CodeSigning {


//
// A SecCode that represents a running UNIX process.
// Processes are identified by pid.
//
// ProcessCode inherits GenericCode's access to the cshosting Mach protocol to
// deal with guests.
//
class ProcessCode : public GenericCode {
public:
	ProcessCode(pid_t pid, const audit_token_t* token, PidDiskRep *pidDiskRep = NULL);
	~ProcessCode() throw () { delete mAudit; }
	
	pid_t pid() const { return mPid; }
	const audit_token_t* audit() const { return mAudit; }
	
	PidDiskRep *pidBased() const { return mPidBased; }
	
	int csops(unsigned int ops, void *addr, size_t size);

	mach_port_t getHostingPort();

private:
	pid_t mPid;
	audit_token_t* mAudit;
	RefPointer<PidDiskRep> mPidBased;
};


//
// We don't need a GenericCode variant of ProcessCode
//
typedef SecStaticCode ProcessStaticCode;
        
class ProcessDynamicCode : public SecStaticCode {
public:
	ProcessDynamicCode(ProcessCode *diskRep);

        CFDataRef component(CodeDirectory::SpecialSlot slot, OSStatus fail = errSecCSSignatureFailed);
        
        CFDictionaryRef infoDictionary();
        
        void validateComponent(CodeDirectory::SpecialSlot slot, OSStatus fail = errSecCSSignatureFailed);
private:
        ProcessCode *mGuest;

};

} // end namespace CodeSigning
} // end namespace Security

#endif // !_H_CSPROCESS
