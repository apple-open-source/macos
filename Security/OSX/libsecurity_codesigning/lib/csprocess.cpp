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
#include "csprocess.h"
#include "cskernel.h"
#include <securityd_client/ssclient.h>
#include <System/sys/codesign.h>
#include "LWCRHelper.h"

namespace Security {
namespace CodeSigning {


//
// Construct a running process representation
//
ProcessCode::ProcessCode(pid_t pid, const audit_token_t* token, PidDiskRep *pidDiskRep /*= NULL */)
	: SecCode(KernelCode::active()), mPid(pid), mPidBased(pidDiskRep)
{
	if (token)
		mAudit = new audit_token_t(*token);
	else
		mAudit = NULL;
}


int ProcessCode::csops(unsigned int ops, void *addr, size_t size)
{
	// pass pid and audit token both if we have it, or just the pid if we don't
	if (mAudit)
		return ::csops_audittoken(mPid, ops, addr, size, mAudit);
	else
		return ::csops(mPid, ops, addr, size);
}

void ProcessCode::codeMatchesLightweightCodeRequirementData(CFDataRef lwcrData)
{
#if !TARGET_OS_SIMULATOR
	if (mAudit) {
		evaluateLightweightCodeRequirementInKernel(*mAudit, lwcrData);
	} else {
		MacOSError::throwMe(errSecParam);
	}
#else
	MacOSError::throwMe(errSecCSUnimplemented)
#endif

}


/*
 *
 */
        
ProcessDynamicCode::ProcessDynamicCode(ProcessCode *guest)
        : SecStaticCode(guest->pidBased()), mGuest(guest)
{
}

CFDataRef ProcessDynamicCode::component(CodeDirectory::SpecialSlot slot, OSStatus fail /* = errSecCSSignatureFailed */)
{
        if (slot == cdInfoSlot && !mGuest->pidBased()->supportInfoPlist())
                return NULL;
        else if (slot == cdResourceDirSlot)
                return NULL;
        return SecStaticCode::component(slot, fail);
}

CFDictionaryRef ProcessDynamicCode::infoDictionary()
{
        if (mGuest->pidBased()->supportInfoPlist())
                return SecStaticCode::infoDictionary();
        if (!mEmptyInfoDict) {
                mEmptyInfoDict.take(makeCFDictionary(0));
        }
        return mEmptyInfoDict;
}

void ProcessDynamicCode::validateComponent(CodeDirectory::SpecialSlot slot, OSStatus fail /* = errSecCSSignatureFailed */)
{
        if (slot == cdInfoSlot && !mGuest->pidBased()->supportInfoPlist())
                return;
        else if (slot == cdResourceDirSlot)
                return;
        SecStaticCode::validateComponent(slot, fail);
}


        
} // CodeSigning
} // Security
