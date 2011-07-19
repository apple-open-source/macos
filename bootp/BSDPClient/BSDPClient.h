/*
 * Copyright (c) 2002 - 2004 Apple Inc. All rights reserved.
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
 * BSDPClient.h
 * - BSDP client library functions
 */
/* 
 * Modification History
 *
 * February 25, 2002	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 * April 29, 2002	Dieter Siegmund (dieter@apple.com)
 * - added image kind and made the whole 32-bit identifier the
 *   unique id, rather than just the 16-bit index
 * - deprecated IsInstall and Index values, which are still supplied
 *   but should not be used
 */

#ifndef _S_BSDPCLIENT_H
#define _S_BSDPCLIENT_H

#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>

struct BSDPClient_s;
typedef struct BSDPClient_s * BSDPClientRef;

typedef enum {
    kBSDPClientStatusOK = 0,
    kBSDPClientStatusUnsupportedFirmware = 1,
    kBSDPClientStatusNoSuchInterface = 2,
    kBSDPClientStatusInterfaceNotConfigured = 3,
    kBSDPClientStatusInvalidArgument = 4,
    kBSDPClientStatusAllocationError = 5,
    kBSDPClientStatusPermissionDenied = 6,
    kBSDPClientStatusOperationTimedOut = 7,
    kBSDPClientStatusTransmitFailed = 8,
    kBSDPClientStatusServerSentFailure = 9,
    kBSDPClientStatusLast = 9
} BSDPClientStatus;

typedef enum {
    kBSDPImageKindMacOS9 = 0,
    kBSDPImageKindMacOSX = 1,
    kBSDPImageKindMacOSXServer = 2,
} BSDPImageKind;

static __inline__ const char *
BSDPClientStatusString(BSDPClientStatus status)
{
    static const char * msg[] = {
	"OK",
	"Unsupported firmware",
	"No such interface",
	"Interface not configured",
	"Invalid argument",
	"Allocation error",
	"Permission denied",
	"Operation timed out",
	"Transmit failed",
	"Server sent failure",
    };

    if (status >= kBSDPClientStatusOK && status <= kBSDPClientStatusLast) {
	return (msg[status]);
    }
    return ("<unknown>");
}

/* 
 * BSDP Image Description Dictionary
 *
 * Each BSDP image description dictionary contains the keys
 * "Name", "Identifier", and "Index".  An image description may 
 * optionally contain any or all of the keys "IsDefault", 
 * "IsSelected", and "IsInstall".
 *
 * Notes:
 * 1) If multiple servers report an image whose "Identifier" is not local to the
 *    server, and the "Identifier" values are equal, they can be treated as the
 *    same image.  See BSDPImageDescriptionIdentifierIsServerLocal()
 *    defined below.
 * 2) "IsDefault" means the image is the default for this server.
 * 3) "IsSelected" means the image is the image that the client currently
 *    has a binding for with this server. Usually, just a single server
 *    will report a single image with this property, but if a server that
 *    had been off the network for awhile is suddenly placed on the 
 *    network again, more than one server may report this.
 */
/* mandatory keys, will be present: */
#define kBSDPImageDescriptionName	CFSTR("Name") /* CFString */
#define kBSDPImageDescriptionIdentifier	CFSTR("Identifier") /* CFNumber (32-bit) */
/* optional keys, may or may not be present: */
#define kBSDPImageDescriptionIsDefault	CFSTR("IsDefault") /* CFBoolean */
#define kBSDPImageDescriptionIsSelected	CFSTR("IsSelected") /* CFBoolean */

/* deprecated: use BSDPImageDescriptionImageIsInstall() instead */
#define kBSDPImageDescriptionIsInstall	CFSTR("IsInstall") /* CFBoolean */

/* deprecated: use "Identifier" instead */
#define kBSDPImageDescriptionIndex	CFSTR("Index") /* CFNumber (16-bit) */

/* 
 * Function: BSDPImageDescriptionIndexIsServerLocal 
 * Purpose:
 *    NOTE: This function is deprecated, use
 *    BSDPImageDescriptionIdentifierIsServerLocal() instead.
 */
Boolean
BSDPImageDescriptionIndexIsServerLocal(CFNumberRef index);

/*
 * Function: BSDPImageDescriptionIdentifierIsServerLocal
 * 
 * Purpose:
 *   Given the CFNumberRef corresponding to the "Identifier" property
 *   from the image description dictionary, returns whether it is a
 *   server local image.
 *
 *   If the "Identifier" for an image is server local, the image must be
 *   presented as a unique item in the list presented to the user.  
 *   If the "Identifier" for an image is not server local, and more than
 *   one servers supply an image with same "Identifier", they may be
 *   presented as a single choice in the list presented to the user.
 */
Boolean
BSDPImageDescriptionIdentifierIsServerLocal(CFNumberRef identifier);

/*
 * Function: BSDPImageDescriptionIdentifierImageKind 
 * Purpose:
 *   Returns the BSDPImageKind for the given identifier.
  */
BSDPImageKind
BSDPImageDescriptionIdentifierImageKind(CFNumberRef identifier);

/*
 * Function: BSDPImageDescriptionIdentifierIsInstall
 * Purpose:
 *   Returns whether the identifier refers to an install image or not.
 */
Boolean
BSDPImageDescriptionIdentifierIsInstall(CFNumberRef identifier);


/*
 * Type: BSDPClientListCallBack
 *
 * Purpose:
 *   The prototype for the List operation callback function.
 *   The function is called when new image information is
 *   available, and is provided with an array of image description
 *   dictionaries.  This array must NOT be released.
 *   See the "BSDP Image Description Dictionary" discussion above.
 *
 * Parameters:
 *   client	the BSDPClientRef allocated using Create
 *   status	the status of the List operation
 *   list	an array of image description dictionaries,
 *              must NOT be CFRelease()'d
 *   info	the caller-supplied opaque handle supplied to BSDPClientList()
 *
 * Note:
 *   It's possible that duplicate responses may be received due to loops
 *   in the network.  It is the caller's responsibility to update
 *   its accumulated list appropriately in this case.  One simple strategy
 *   is to remove any existing image description dictionaries with the
 *   given ServerAddress before accumulating any new ones.
 */    
typedef void (*BSDPClientListCallBack)(BSDPClientRef client, 
				       BSDPClientStatus status,
				       CFStringRef ServerAddress,
				       CFNumberRef ServerPriority,
				       CFArrayRef list,
				       void *info);

/*
 * Type: BSDPClientSelectCallBack
 *
 * Purpose:
 *   The prototype for the Select operation callback.
 *   This function is called when the Select operation either
 *   succeeds or fails.
 *
 * Parameters:
 *   client	the BSDPClientRef created using BSDPClientCreate()
 *   status	the status of the Select operation
 *   info	the caller-supplied opaque handle supplied to BSDPClientSelect()
 */    
typedef void (*BSDPClientSelectCallBack)(BSDPClientRef client,
					 BSDPClientStatus status, 
					 void *info);

/*
 * Function: BSDPClientCreate
 *
 * Purpose:
 *   Allocate and initialize a BSDPClientRef for the default interface.
 *   The BSDPClientRef is used with BSDPClientList() and BSDPClientSelect().
 *
 * Returns:
 *   A non-NULL BSDPClientRef if successful, NULL otherwise.
 *   The status is returned in *status_p.
 */
BSDPClientRef
BSDPClientCreate(BSDPClientStatus * status_p);

/*
 * Function: BSDPClientCreateWithInterface
 *
 * Purpose:
 *   Allocate and initialize a BSDPClientRef for the specified interface.
 *   The BSDPClientRef is used with BSDPClientList() and BSDPClientSelect().
 *
 * Returns:
 *   A non-NULL BSDPClientRef if successful, NULL otherwise.
 *   The status is returned in *status_p.
 */
BSDPClientRef
BSDPClientCreateWithInterface(BSDPClientStatus * status_p,
			      const char * ifname);
/*
 * Function: BSDPClientFree
 * Purpose:
 *   Release resources allocated during the use of the BSDPClientRef.
 */
void
BSDPClientFree(BSDPClientRef * client_p);

/*
 * Function: BSDPClientList
 *
 * Purpose:
 *   Send a BSDP List request, and wait for responses.  If no responses
 *   appear, retry the request, doubling the timeout.
 *
 *   When a response arrives, decode it and build an array of BSDP image
 *   description dictionaries (see discussion above), and pass it to the
 *   callback.
 *
 *   If no responses arrive after 1 minute of trying, or some other
 *   error occurs, the callback is called with the appropriate status.
 *
 * Parameters:
 *   client		The client handle created using BSDPClientCreate().
 *   callback		The function to call when image information is
 *  			available, or when an error occurs.
 *   info		The caller-supplied argument to pass to the callback.
 *
 * Returns:
 *   kBSDPClientStatusOK if List process started successfully.
 *   If an error occurred, some other status is returned.
 */
BSDPClientStatus
BSDPClientList(BSDPClientRef client, BSDPClientListCallBack callback, 
	       void * info);

/*
 * Function: BSDPClientSelect
 *
 * Purpose:
 *   Once the user has made a selection, this function is called to
 *   create a binding with a particular server/image.
 *
 * Parameters:
 *   client		The client handle created using BSDPClientCreate().
 *   ServerAddress	The ServerAddress value suppled to the list 
 *                      callback function.
 *   Identifier		The value of the kBSDPImageDescriptionIdentifier property
 *			from the selected BSDP Image Description dictionary.
 *   callback		The function to call when status is available.
 *   info		The caller-supplied argument to pass to the callback.
 */
BSDPClientStatus
BSDPClientSelect(BSDPClientRef client, 
		 CFStringRef ServerAddress,
		 CFNumberRef Identifier,
		 BSDPClientSelectCallBack callback, void * info);
		      
#endif _S_BSDPCLIENT_H

