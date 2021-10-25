/*
 * Copyright (c) 2004-2021 Apple Inc. All Rights Reserved.
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

#include <Security/Security.h>
#include <Security/SecBase.h>
#include "SecureDownload.h"


OSStatus SecureDownloadCreateWithTicket (CFDataRef ticket,
										 SecureDownloadTrustSetupCallback setup,
										 void* setupContext,
										 SecureDownloadTrustEvaluateCallback evaluate,
										 void* evaluateContext,
										 SecureDownloadRef* downloadRef)
{
    return errSecUnimplemented;
}



OSStatus SecureDownloadCopyURLs (SecureDownloadRef downloadRef, CFArrayRef* urls)
{
    return errSecUnimplemented;
}



OSStatus SecureDownloadCopyName (SecureDownloadRef downloadRef, CFStringRef* name)
{
    return errSecUnimplemented;
}



OSStatus SecureDownloadCopyCreationDate (SecureDownloadRef downloadRef, CFDateRef* date)
{
    return errSecUnimplemented;
}



OSStatus SecureDownloadGetDownloadSize (SecureDownloadRef downloadRef, SInt64* size)
{
    return errSecUnimplemented;
}



OSStatus SecureDownloadUpdateWithData (SecureDownloadRef downloadRef, CFDataRef data)
{
    return errSecUnimplemented;
}



OSStatus SecureDownloadFinished (SecureDownloadRef downloadRef)
{
    return errSecUnimplemented;
}



OSStatus SecureDownloadRelease (SecureDownloadRef downloadRef)
{
    return errSecUnimplemented;
}



OSStatus SecureDownloadCopyTicketLocation (CFURLRef url, CFURLRef *ticketLocation)
{
    return errSecUnimplemented;
}
