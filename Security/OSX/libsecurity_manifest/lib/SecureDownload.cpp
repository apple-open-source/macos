/*
 * Copyright (c) 2004,2011,2013-2014 Apple Inc. All Rights Reserved.
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
#include <security_utilities/security_utilities.h>
#include <security_cdsa_utilities/cssmbridge.h>
#include <../sec/Security/SecBase.h>
#include "Download.h"
#include "SecureDownload.h"


#define API_BEGIN \
	try {

#define API_END \
	} \
	catch (const MacOSError &err) { return err.osStatus(); } \
	catch (const std::bad_alloc &) { return errSecAllocate; } \
	catch (...) { return errSecInternalComponent; } \
    return errSecSuccess;

#define API_END_GENERIC_CATCH		} catch (...) { return; }

#define API_END_ERROR_CATCH(bad)	} catch (...) { return bad; }



OSStatus SecureDownloadCreateWithTicket (CFDataRef ticket,
										 SecureDownloadTrustSetupCallback setup,
										 void* setupContext,
										 SecureDownloadTrustEvaluateCallback evaluate,
										 void* evaluateContext,
										 SecureDownloadRef* downloadRef)
{
	API_BEGIN
	
	Download* sd = new Download ();
	sd->Initialize (ticket, setup, setupContext, evaluate, evaluateContext);
	Required (downloadRef) = sd;

	API_END
}



OSStatus SecureDownloadCopyURLs (SecureDownloadRef downloadRef, CFArrayRef* urls)
{
	API_BEGIN

	Required (downloadRef);
	Download* d = (Download*) downloadRef;
	Required (urls) = d->CopyURLs ();

	API_END
}



OSStatus SecureDownloadCopyName (SecureDownloadRef downloadRef, CFStringRef* name)
{
	API_BEGIN

	Required (downloadRef);
	Download* d = (Download*) downloadRef;
	Required (name) = d->CopyName ();

	API_END
}



OSStatus SecureDownloadCopyCreationDate (SecureDownloadRef downloadRef, CFDateRef* date)
{
	API_BEGIN

	Required (downloadRef);
	Download* d = (Download*) downloadRef;
	Required (date) = d->CopyDate ();

	API_END
}



OSStatus SecureDownloadGetDownloadSize (SecureDownloadRef downloadRef, SInt64* size)
{
	API_BEGIN

	Required (downloadRef);
	Download* d = (Download*) downloadRef;
	Required (size) = d->GetDownloadSize ();

	API_END
}



OSStatus SecureDownloadUpdateWithData (SecureDownloadRef downloadRef, CFDataRef data)
{
	API_BEGIN
	Required (downloadRef);
	Required (data);
	Download* d = (Download*) downloadRef;
	d->UpdateWithData (data);
	API_END
}



OSStatus SecureDownloadFinished (SecureDownloadRef downloadRef)
{
	API_BEGIN
	Required (downloadRef);
	Download* d = (Download*) downloadRef;
	d->Finalize ();
	API_END
}



OSStatus SecureDownloadRelease (SecureDownloadRef downloadRef)
{
	API_BEGIN
		Required (downloadRef);
		delete (Download*) downloadRef;
	API_END
}



OSStatus SecureDownloadCopyTicketLocation (CFURLRef url, CFURLRef *ticketLocation)
{
	API_BEGIN
		Required (ticketLocation);
		Required (url);
		
		// copy the resource specifier
		CFStringRef resourceSpecifier = CFURLCopyResourceSpecifier (url);
		if (resourceSpecifier == NULL)
		{
			CFError::throwMe ();
		}

		// make a new URL from the resource specifier
		*ticketLocation = CFURLCreateWithString (NULL, resourceSpecifier, NULL);
		if (*ticketLocation == NULL)
		{
			CFError::throwMe ();
		}
		
		// check the scheme to make sure that it isn't a file url
		CFStringRef scheme = CFURLCopyScheme (*ticketLocation);
		if (scheme != NULL)
		{
			CFComparisonResult equal = CFStringCompare (scheme, CFSTR("file"), kCFCompareCaseInsensitive);
			CFRelease (scheme);
			
			if (equal == kCFCompareEqualTo)
			{
				CFRelease (*ticketLocation);
				*ticketLocation = NULL;
				MacOSError::throwMe (errSecureDownloadInvalidDownload);
			}
		}
		
		CFRelease (resourceSpecifier);
	API_END
}
