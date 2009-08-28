/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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
 *  EncodedSourceID.c
 *  SourceIDValidation
 *
 *  (Uses FindPrimaryEthernetInterfaces() and GetPrimaryMACAddress() from developer.apple.com.)
 *  Copyright (c) 2005 Apple. All rights reserved.
 */

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IOEthernetController.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <openssl/md5.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

#include "EncodedSourceID.h"

#define ACCOUNT_NAME_MAX_LEN	255
#define ENCODED_ID_MAX_LEN	 31

#define SUCCESS 1
#define FAILURE 0

static int GenerateIDString(char *idBuffer);
static int PrimaryMACAddressFromSystem(char *addressBuffer);
static kern_return_t GetPrimaryMACAddress(UInt8 *MACAddress);
static kern_return_t FindPrimaryEthernetInterfaces(io_iterator_t *matchingServices);

/* Returns a Base-64 encoded MD5 hash of 'username:primary-mac-address' */
int GetEncodedSourceID(char encodedIdBuffer[32]) {

    char idStr[ACCOUNT_NAME_MAX_LEN + (kIOEthernetAddressSize * 2) + 2];
    unsigned char MD5Value[MD5_DIGEST_LENGTH];
    BIO *mbio,*b64bio,*bio;
    int b64Len = 0, writeLen = 0, status = FAILURE;
    BUF_MEM *bptr = NULL;
    int result;
    
    if (encodedIdBuffer == NULL) return FAILURE;
    encodedIdBuffer[0] = '\0';
	
    if (GenerateIDString(idStr)) {

        /* Convert idStr to MD5 */
        MD5((const unsigned char *)idStr, strlen(idStr), MD5Value);
            
        /* Convert MD5 to Base-64 */
        mbio=BIO_new(BIO_s_mem());
        b64bio=BIO_new(BIO_f_base64());
        bio=BIO_push(b64bio,mbio);
        writeLen = BIO_write(bio,MD5Value,MD5_DIGEST_LENGTH);
        
        if (writeLen > 0) {
        
            result = BIO_flush(bio);
            BIO_get_mem_ptr(mbio, &bptr);
            if ( (bptr != NULL) && ((b64Len = bptr->length) > 0) ) {
                memcpy(encodedIdBuffer, bptr->data, b64Len);
                
                if (b64Len > ENCODED_ID_MAX_LEN) {
                    encodedIdBuffer[ENCODED_ID_MAX_LEN] = '\0';
                } else {
                    encodedIdBuffer[b64Len-1] = '\0'; /* Overwrites the newline char */
                }
                status = SUCCESS;
            }
        }
        BIO_free_all(bio);
    }
    
    return status;
}

static int GenerateIDString(char *idBuffer) {

    char addressString[(kIOEthernetAddressSize * 2) + 1];
    const char *accountName = NULL;

    if (idBuffer == NULL) return FAILURE;
    idBuffer[0] = '\0';
    
    accountName = getlogin();
                            
    if ( (accountName==NULL) || (strlen(accountName)<=0) ) {
    
        return FAILURE;
    }
	
    if (PrimaryMACAddressFromSystem(addressString) == SUCCESS) {
         
        sprintf(idBuffer, "%s:%s", accountName, addressString);
        return SUCCESS;
    }
    else {
        return FAILURE;
    }
}

/* Writes the MAC address of the primary interface to the addressBuffer argument as a 
 * human-readable hex string.
  */
static int PrimaryMACAddressFromSystem(char *addressBuffer) {

    int status = FAILURE, len = kIOEthernetAddressSize;
    UInt8 MACAddress[ kIOEthernetAddressSize ];
    kern_return_t kernResult = KERN_FAILURE;
    
    if (addressBuffer == NULL) return status;
	
    kernResult = GetPrimaryMACAddress(MACAddress);
                    
    if (KERN_SUCCESS == kernResult) {
    
            char *bufPtr = addressBuffer;
            const UInt8 *addrPtr = &MACAddress[0];
            
            while ( len-- ) {
                bufPtr += sprintf( bufPtr, "%2.2x", *addrPtr++ );
            }
    
            if (strlen(addressBuffer) > 0) {
              
                status = SUCCESS;
            }
    }

    return status;
}

/* -----------------------------------------------------------------------------
Get the mac address of the primary interface
----------------------------------------------------------------------------- */
static kern_return_t GetPrimaryMACAddress(UInt8 *MACAddress)
{
    io_object_t  intfService;
    io_object_t  controllerService;
    kern_return_t kernResult = KERN_FAILURE;
    io_iterator_t intfIterator;

    kernResult = FindPrimaryEthernetInterfaces(&intfIterator);
    if (kernResult != KERN_SUCCESS)
        return kernResult;

    // Initialize the returned address
    bzero(MACAddress, kIOEthernetAddressSize);
    
    // IOIteratorNext retains the returned object, so release it when we're done with it.
    while ((intfService = IOIteratorNext(intfIterator))) {
    
        CFTypeRef MACAddressAsCFData;        

        // IONetworkControllers can't be found directly by the IOServiceGetMatchingServices call, 
        // since they are hardware nubs and do not participate in driver matching. In other words,
        // registerService() is never called on them. So we've found the IONetworkInterface and will 
        // get its parent controller by asking for it specifically.
        
        // IORegistryEntryGetParentEntry retains the returned object, so release it when we're done with it.
        kernResult = IORegistryEntryGetParentEntry( intfService,
                                                    kIOServicePlane,
                                                    &controllerService );

        if (kernResult == KERN_SUCCESS) {
        
            // Retrieve the MAC address property from the I/O Registry in the form of a CFData
            MACAddressAsCFData = IORegistryEntryCreateCFProperty( controllerService,
                                                                  CFSTR(kIOMACAddress),
                                                                  kCFAllocatorDefault, 0);
            if (MACAddressAsCFData) {
                // Get the raw bytes of the MAC address from the CFData
                CFDataGetBytes(MACAddressAsCFData, CFRangeMake(0, kIOEthernetAddressSize), MACAddress);
                CFRelease(MACAddressAsCFData);
            }
                
            // Done with the parent Ethernet controller object so we release it.
            IOObjectRelease(controllerService);
        }
        
        // Done with the Ethernet interface object so we release it.
        IOObjectRelease(intfService);
    }
    
    IOObjectRelease(intfIterator);

    return kernResult;
}

/* -----------------------------------------------------------------------------
Returns an iterator containing the primary (built-in) Ethernet interface. 
The caller is responsible for releasing the iterator after the caller is done with it.
 ----------------------------------------------------------------------------- */
static kern_return_t FindPrimaryEthernetInterfaces(io_iterator_t *matchingServices)
{
    kern_return_t  kernResult; 
    mach_port_t   masterPort;
    CFMutableDictionaryRef matchingDict;
    CFMutableDictionaryRef propertyMatchDict;
    
    // Retrieve the Mach port used to initiate communication with I/O Kit
    kernResult = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if (kernResult != KERN_SUCCESS) {
        //error("Can't create IOKit MasterPort (%d)\n", kernResult);
        return kernResult;
    }
    
    // Ethernet interfaces are instances of class kIOEthernetInterfaceClass. 
    // IOServiceMatching is a convenience function to create a dictionary with the key kIOProviderClassKey and 
    // the specified value.
    matchingDict = IOServiceMatching(kIOEthernetInterfaceClass);
    if (matchingDict) {
    
        // Each IONetworkInterface object has a Boolean property with the key kIOPrimaryInterface. Only the
        // primary (built-in) interface has this property set to TRUE.
        
        // IOServiceGetMatchingServices uses the default matching criteria defined by IOService. This considers
        // only the following properties plus any family-specific matching in this order of precedence 
        // (see IOService::passiveMatch):
        //
        // kIOProviderClassKey (IOServiceMatching)
        // kIONameMatchKey (IOServiceNameMatching)
        // kIOPropertyMatchKey
        // kIOPathMatchKey
        // kIOMatchedServiceCountKey
        // family-specific matching
        // kIOBSDNameKey (IOBSDNameMatching)
        // kIOLocationMatchKey
        
        // The IONetworkingFamily does not define any family-specific matching. This means that in            
        // order to have IOServiceGetMatchingServices consider the kIOPrimaryInterface property, we must
        // add that property to a separate dictionary and then add that to our matching dictionary
        // specifying kIOPropertyMatchKey.
            
        propertyMatchDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
                                                       &kCFTypeDictionaryKeyCallBacks,
                                                       &kCFTypeDictionaryValueCallBacks);
        if (propertyMatchDict) {
        
            // Set the value in the dictionary of the property with the given key, or add the key 
            // to the dictionary if it doesn't exist. This call retains the value object passed in.
            CFDictionarySetValue(propertyMatchDict, CFSTR(kIOPrimaryInterface), kCFBooleanTrue); 
            
            // Now add the dictionary containing the matching value for kIOPrimaryInterface to our main
            // matching dictionary. This call will retain propertyMatchDict, so we can release our reference 
            // on propertyMatchDict after adding it to matchingDict.
            CFDictionarySetValue(matchingDict, CFSTR(kIOPropertyMatchKey), propertyMatchDict);
            CFRelease(propertyMatchDict);
        }
    }
    
    // IOServiceGetMatchingServices retains the returned iterator, so release the iterator when we're done with it.
    // IOServiceGetMatchingServices also consumes a reference on the matching dictionary so we don't need to release
    // the dictionary explicitly.
    kernResult = IOServiceGetMatchingServices(masterPort, matchingDict, matchingServices);    
        
    return kernResult;
}
