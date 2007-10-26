/*
 * Copyright (c) 2006-2007 Apple Inc. All Rights Reserved.
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
// cskernel - Kernel implementation of the Code Signing Host Interface
//
#include "cskernel.h"
#include "csprocess.h"
#include "kerneldiskrep.h"
#include <libproc.h>
#include <sys/codesign.h>
#include <sys/param.h>	// MAXPATHLEN

namespace Security {
namespace CodeSigning {


//
// The running-kernel singletons
//
ModuleNexus<KernelCode::Globals> KernelCode::globals;

KernelCode::Globals::Globals()
{
	code = new KernelCode;
	staticCode = new KernelStaticCode;
}

KernelCode::KernelCode()
	: SecCode(NULL)
{
}

KernelStaticCode::KernelStaticCode()
	: SecStaticCode(new KernelDiskRep())
{
}


//
// We locate a guest (process) by invoking a kernel service.
// The only attributes supported are ("pid", pid_t).
// (We could also support task ports if we liked, but those can be translated
// to pids by the caller without trouble.)
//
SecCode *KernelCode::locateGuest(CFDictionaryRef attributes)
{
	if (CFTypeRef attr = CFDictionaryGetValue(attributes, kSecGuestAttributePid)) {
		if (CFDictionaryGetCount(attributes) != 1)
			MacOSError::throwMe(errSecCSUnsupportedGuestAttributes); // had more
		if (CFGetTypeID(attr) == CFNumberGetTypeID())
			return (new ProcessCode(cfNumber<pid_t>(CFNumberRef(attr))))->retain();
		MacOSError::throwMe(errSecCSInvalidAttributeValues);
	} else
		MacOSError::throwMe(errSecCSUnsupportedGuestAttributes);
}


//
// We map guests to disk by calling a kernel service.
//
SecStaticCode *KernelCode::mapGuestToStatic(SecCode *iguest)
{
	if (ProcessCode *guest = dynamic_cast<ProcessCode *>(iguest)) {
		char path[2 * MAXPATHLEN];	// reasonable upper limit
		if (::proc_pidpath(guest->pid(), path, sizeof(path)))
			return (new ProcessStaticCode(DiskRep::bestGuess(path)))->retain();
		else
			UnixError::throwMe();
	}
	MacOSError::throwMe(errSecCSNoSuchCode);
}


//
// We obtain the guest's status by asking the kernel
//
uint32_t KernelCode::getGuestStatus(SecCode *iguest)
{
	if (ProcessCode *guest = dynamic_cast<ProcessCode *>(iguest)) {
		uint32_t pFlags;
		if (::csops(guest->pid(), CS_OPS_STATUS, &pFlags, 0) == -1) {
			secdebug("kcode", "cannot get guest status of %p(%d) errno=%d",
				guest, guest->pid(), errno);
			switch (errno) {
			case ESRCH:
				MacOSError::throwMe(errSecCSNoSuchCode);
			default:
				UnixError::throwMe();
			}
		}
		secdebug("kcode", "guest %p(%d) kernel status 0x%x", guest, guest->pid(), pFlags);
		
#if defined(USERSPACE_VALIDATION)
		// Former static substitute for dynamic kernel validation of executable pages.
		// This is now done in the kernel's page-in path.
		guest->staticCode()->validateExecutable();
#endif //USERSPACE_VALIDATION
		
		return pFlags;
	} else
		MacOSError::throwMe(errSecCSNoSuchCode);
}


//
// The StaticCode for the running kernel is explicit.
// We can't ask our own host for it, naturally.
//
SecStaticCode *KernelCode::getStaticCode()
{
	return globals().staticCode->retain();
}


} // CodeSigning
} // Security
