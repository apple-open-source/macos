/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please
 * obtain a copy of the License at http://www.apple.com/publicsource and
 * read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please
 * see the License for the specific language governing rights and
 * limitations under the License.
 */

/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : dyn_macosx.c
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 3/15/00
            License: Copyright (C) 2000 David Corcoran
                     <corcoran@linuxnet.com>
            Purpose: This abstracts dynamic library loading 
                     functions and timing. 

********************************************************************/

#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>

#include "config.h"
#include "wintypes.h"
#include "pcsclite.h"
#include "dyn_generic.h"
#include "debuglog.h"

/*
 * / Load a module (if needed)
 */
int DYN_LoadLibrary(void **pvLHandle, char *pcLibrary)
{

	CFStringRef bundlePath;
	CFURLRef bundleURL;
	CFBundleRef bundle;

	*pvLHandle = 0;

	/*
	 * @@@ kCFStringEncodingMacRoman might be wrong on non US systems.
	 */

	bundlePath = CFStringCreateWithCString(NULL, pcLibrary,
		kCFStringEncodingMacRoman);
	if (bundlePath == NULL)
		return SCARD_E_NO_MEMORY;

	bundleURL = CFURLCreateWithFileSystemPath(NULL, bundlePath,
		kCFURLPOSIXPathStyle, TRUE);
	CFRelease(bundlePath);
	if (bundleURL == NULL)
		return SCARD_E_NO_MEMORY;

	bundle = CFBundleCreate(NULL, bundleURL);
	CFRelease(bundleURL);
	if (bundle == NULL)
	{
		Log1(PCSC_LOG_ERROR, "CFBundleCreate");
		return SCARD_F_UNKNOWN_ERROR;
	}

	if (!CFBundleLoadExecutable(bundle))
	{
		Log1(PCSC_LOG_ERROR, "CFBundleLoadExecutable");
		CFRelease(bundle);
		return SCARD_F_UNKNOWN_ERROR;
	}

	*pvLHandle = (void *) bundle;

	return SCARD_S_SUCCESS;
}

int DYN_CloseLibrary(void **pvLHandle)
{

	CFBundleRef bundle = (CFBundleRef) * pvLHandle;

	if (CFBundleIsExecutableLoaded(bundle) == TRUE)
	{
		CFBundleUnloadExecutable(bundle);
		CFRelease(bundle);
	}
	else
		Log1(PCSC_LOG_ERROR, "Cannot unload library.");

	*pvLHandle = 0;
	return SCARD_S_SUCCESS;
}

int DYN_GetAddress(void *pvLHandle, void **pvFHandle, const char *pcFunction)
{

	CFBundleRef bundle = (CFBundleRef) pvLHandle;
	CFStringRef cfName = CFStringCreateWithCString(NULL, pcFunction,
		kCFStringEncodingMacRoman);
	if (cfName == NULL)
		return SCARD_E_NO_MEMORY;

	*pvFHandle = CFBundleGetFunctionPointerForName(bundle, cfName);
	CFRelease(cfName);
	if (*pvFHandle == NULL)
		return SCARD_F_UNKNOWN_ERROR;

	return SCARD_S_SUCCESS;
}
