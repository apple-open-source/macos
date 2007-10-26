/*
    File:  WorkstationService.c
    
    Version:  1.0

    (c) Copyright 2006 Apple Computer, Inc. All rights reserved.

    IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
    ("Apple") in consideration of your agreement to the following terms, and
    your use, installation, modification or redistribution of this Apple
    software constitutes acceptance of these terms.  If you do not agree with
    these terms, please do not use, install, modify or redistribute this Apple
    software.

    In consideration of your agreement to abide by the following terms, and
    subject to these terms, Apple grants you a personal, non-exclusive license,
    under Apple's copyrights in this original Apple software (the "Apple
    Software"), to use, reproduce, modify and redistribute the Apple Software,
    with or without modifications, in source and/or binary forms; provided that
    if you redistribute the Apple Software in its entirety and without
    modifications, you must retain this notice and the following text and
    disclaimers in all such redistributions of the Apple Software. Neither the
    name, trademarks, service marks or logos of Apple Computer, Inc. may be used
    to endorse or promote products derived from the Apple Software without
    specific prior written permission from Apple.  Except as expressly stated in
    this notice, no other rights or licenses, express or implied, are granted by
    Apple herein, including but not limited to any patent rights that may be
    infringed by your derivative works or by other works in which the Apple
    Software may be incorporated.

    The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
    WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
    WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
    COMBINATION WITH YOUR PRODUCTS.

    IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION
    AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER
    THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR
    OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
      
    Change History (most recent first):
        1.0     October 20, 2006
*/

#include <dns_sd.h>  // DNSServiceDiscovery
#include <unistd.h>  // usleep
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IOEthernetController.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include "CLog.h"
#include "WorkstationService.h"

#define kWorkstationType   "_workstation._tcp."
#define kWorkstationPort   9
#define kMACAddressLengh   (sizeof(" [00:00:00:00:00:00]") - 1)
#define kMaxComputerName   63-kMACAddressLengh


typedef struct MyDNSServiceState {
    DNSServiceRef service;
    CFSocketRef socket;
} MyDNSServiceState;

DNSServiceErrorType RegisterWorkstationService(MyDNSServiceState *ref, CFStringRef serviceName);
CFStringRef CopyNextWorkstationName(SCDynamicStoreRef store, CFStringRef currentName);


CFStringRef gCurrentWorkstationName = NULL;
MyDNSServiceState *gServiceRegistration = NULL;
CFRunLoopSourceRef gNameMonitorSource = NULL;

static MyDNSServiceState *
MyDNSServiceAlloc(void)
{
    MyDNSServiceState *ref = (MyDNSServiceState *) malloc(sizeof(MyDNSServiceState));
    assert(ref);
    ref->service = NULL;
    ref->socket  = NULL;
    return ref;
}


static void
MyDNSServiceFree(MyDNSServiceState *ref)
{
    assert(ref);
    
    if (ref->socket) {
        //Invalidate the CFSocket.
        CFSocketInvalidate(ref->socket);
        CFRelease(ref->socket);

        // Workaround that gives time to CFSocket's select thread so it can remove the socket from its
        // FD set before we close the socket by calling DNSServiceRefDeallocate. <rdar://problem/3585273>
        usleep(1000);
    }
    
    if (ref->service) {
        // Terminate the connection with the mDNSResponder daemon, which cancels the operation.
        DNSServiceRefDeallocate(ref->service);
    }
    
    free(ref);
}


static void
MyDNSServiceSocketCallBack(CFSocketRef s, CFSocketCallBackType type, CFDataRef address, const void *data, void *info)
{
    #pragma unused(s, type, address, data)
    DNSServiceErrorType error;
    MyDNSServiceState *ref = (MyDNSServiceState *)info;  // context passed in to CFSocketCreateWithNative().
    assert(ref);
    
    // Read a reply from the mDNSResponder, which will call the appropriate callback.
    error = DNSServiceProcessResult(ref->service);
    if (kDNSServiceErr_NoError != error) {
        fprintf(stderr, "DNSServiceProcessResult returned %d\n", error);
        
        // Terminate the discovery operation and release everything.
        MyDNSServiceFree(ref);
    }
}


static void
MyDNSServiceAddToRunLoop(MyDNSServiceState *ref)
{
    CFSocketNativeHandle    sock;
    CFOptionFlags           sockFlags;
    CFRunLoopSourceRef      source;
    CFSocketContext         context = { 0, ref, NULL, NULL, NULL };  // Use MyDNSServiceState as context data.

    assert(ref);

    // Access the underlying Unix domain socket to communicate with the mDNSResponder daemon.
    sock = DNSServiceRefSockFD(ref->service);
    assert(sock >= 0);

    // Create a CFSocket using the Unix domain socket.
    ref->socket = CFSocketCreateWithNative(NULL, sock, kCFSocketReadCallBack, MyDNSServiceSocketCallBack, &context);
    assert(ref->socket);

    // Prevent CFSocketInvalidate from closing DNSServiceRef's socket.
    sockFlags = CFSocketGetSocketFlags(ref->socket);
    CFSocketSetSocketFlags(ref->socket, sockFlags & (~kCFSocketCloseOnInvalidate));

    // Create a CFRunLoopSource from the CFSocket.
    source = CFSocketCreateRunLoopSource(NULL, ref->socket, 0);
    assert(source);

    // Add the CFRunLoopSource to the current runloop.
    CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
    
    // We no longer need our reference to source.  The runloop continues to 
    // hold a reference to it, but we don't care about that.  When we invalidate 
    // the socket, the runloop will drop its reference and the source will get 
    // destroyed.
    
    CFRelease(source);
}


static void
CFStringTruncateToUTF8Length(CFMutableStringRef str, ssize_t utf8LengthLimit)
    // Truncates a CFString such that it's UTF-8 representation will have 
    // utf8LengthLimit or less characters (not including the terminating 
    // null character).  Handles UTF-16 surrogates and trims whitespace 
    // from the end of the resulting string.
{
    CFIndex             shortLen;
    CFIndex             convertedLen;
    CFIndex             originalLen;
    CFIndex             utf8Length;
    CFCharacterSetRef   whiteCharSet;
    UniChar             thisChar;
    CFIndex             trailingCharsToDelete;
    
    // Keep converting successively smaller strings until the UTF-8 string is suitably 
    // short.  Note that utf8LengthLimit must be non-negative, so this loop will 
    // always terminate before we run off the front of the string.
    
    originalLen = CFStringGetLength(str);
    shortLen = originalLen;
    do {
        // Because the buffer parameter is NULL, CFStringGetBytes returns the size of 
        // buffer required for the range of characters.  This doesn't include the 
        // trailing null byte traditionally associated with UTF-8 C strings, which 
        // is cool because that's what our caller is expecting.
        
        convertedLen = CFStringGetBytes(str, CFRangeMake(0, shortLen), kCFStringEncodingUTF8, 0, false, NULL, 0, &utf8Length);
        assert( (convertedLen == shortLen) || (convertedLen == (shortLen - 1)) );
        shortLen = convertedLen;
        
        if (utf8Length <= utf8LengthLimit) {
            break;
        }
        shortLen -= 1;
    } while (true);
    
    whiteCharSet = CFCharacterSetGetPredefined(kCFCharacterSetWhitespaceAndNewline);
    assert(whiteCharSet != NULL);
    
    do {
        if ( shortLen == 0 ) {
            break;
        }
        thisChar = CFStringGetCharacterAtIndex(str, shortLen - 1);
        if ( ! CFCharacterSetIsCharacterMember(whiteCharSet, thisChar) ) {
            break;
        }
        shortLen -= 1;
    } while (true);    
    
    trailingCharsToDelete = originalLen - shortLen;
    CFStringDelete(str, CFRangeMake(originalLen - trailingCharsToDelete, trailingCharsToDelete));
}


static kern_return_t
FindEthernetInterfaces(io_iterator_t *matchingServices)
{
    kern_return_t    kernResult; 
    CFMutableDictionaryRef  matchingDict;
    CFMutableDictionaryRef  propertyMatchDict;
    
    // Ethernet interfaces are instances of class kIOEthernetInterfaceClass. 
    // IOServiceMatching is a convenience function to create a dictionary with the key kIOProviderClassKey and 
    // the specified value.
    matchingDict = IOServiceMatching(kIOEthernetInterfaceClass);

    // Note that another option here would be:
    // matchingDict = IOBSDMatching("en0");
        
    if (NULL == matchingDict) {
        printf("IOServiceMatching returned a NULL dictionary.\n");
    } else {
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
            
        propertyMatchDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                            &kCFTypeDictionaryKeyCallBacks,
                            &kCFTypeDictionaryValueCallBacks);
    
        if (NULL == propertyMatchDict) {
            printf("CFDictionaryCreateMutable returned a NULL dictionary.\n");
        } else {
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
    kernResult = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, matchingServices);    
    if (KERN_SUCCESS != kernResult) {
        printf("IOServiceGetMatchingServices returned 0x%08x\n", kernResult);
    }
        
    return kernResult;
}


static kern_return_t
GetMACAddress(io_iterator_t intfIterator, UInt8 *MACAddress, UInt8 bufferSize)
{
    io_object_t    intfService;
    io_object_t    controllerService;
    kern_return_t  kernResult = KERN_FAILURE;
    
    // Make sure the caller provided enough buffer space. Protect against buffer overflow problems.
	if (bufferSize < kIOEthernetAddressSize) {
		return kernResult;
	}
  
	// Initialize the returned address
    bzero(MACAddress, bufferSize);
    
    // IOIteratorNext retains the returned object, so release it when we're done with it.
    while ((intfService = IOIteratorNext(intfIterator))) {
        CFTypeRef  MACAddressAsCFData;        

        // IONetworkControllers can't be found directly by the IOServiceGetMatchingServices call, 
        // since they are hardware nubs and do not participate in driver matching. In other words,
        // registerService() is never called on them. So we've found the IONetworkInterface and will 
        // get its parent controller by asking for it specifically.
        
        // IORegistryEntryGetParentEntry retains the returned object, so release it when we're done with it.
        kernResult = IORegistryEntryGetParentEntry(intfService, kIOServicePlane, &controllerService);
    
        if (KERN_SUCCESS != kernResult) {
            printf("IORegistryEntryGetParentEntry returned 0x%08x\n", kernResult);
        } else {
            // Retrieve the MAC address property from the I/O Registry in the form of a CFData
            MACAddressAsCFData = IORegistryEntryCreateCFProperty(controllerService, CFSTR(kIOMACAddress), kCFAllocatorDefault, 0);
            if (MACAddressAsCFData) {
                //CFShow(MACAddressAsCFData); // for display purposes only; output goes to stderr
                
                // Get the raw bytes of the MAC address from the CFData
                CFDataGetBytes((CFDataRef)MACAddressAsCFData, CFRangeMake(0, kIOEthernetAddressSize), MACAddress);
                CFRelease(MACAddressAsCFData);
            }
                
            // Done with the parent Ethernet controller object so we release it.
            (void)IOObjectRelease(controllerService);
        }
        
        // Done with the Ethernet interface object so we release it.
        (void)IOObjectRelease(intfService);
    }
        
    return kernResult;
}


static CFStringRef
CopyPrimaryMacAddress(void)
{
	io_iterator_t intfIterator;
    UInt8 MAC[kIOEthernetAddressSize];
	CFStringRef macAddress = NULL;
 
    kern_return_t kernResult = FindEthernetInterfaces(&intfIterator);
    if (KERN_SUCCESS != kernResult) {
        printf("FindEthernetInterfaces returned 0x%08x\n", kernResult);
    } else {
        kernResult = GetMACAddress(intfIterator, MAC, sizeof(MAC));
        
        if (KERN_SUCCESS != kernResult) {
            printf("GetMACAddress returned 0x%08x\n", kernResult);
        } else {
			macAddress = CFStringCreateWithFormat(NULL, NULL, CFSTR("[%02x:%02x:%02x:%02x:%02x:%02x]"), MAC[0], MAC[1], MAC[2], MAC[3], MAC[4], MAC[5]);
		}
    }
    
    (void)IOObjectRelease(intfIterator);

	return macAddress;
}


static void
RegisterWorkstationCallBack(DNSServiceRef service, DNSServiceFlags flags, DNSServiceErrorType errorCode,
    const char *name, const char *type, const char *domain, void *context)
{
    #pragma unused(flags, service, context)
       
    if (errorCode == kDNSServiceErr_NoError) {
        printf("Registered Workstation service %s.%s%s\n", name, type, domain);
    } else if (errorCode == kDNSServiceErr_NameConflict) {
        fprintf(stderr, "ERROR: Workstation name conflict occured so something weird is happening\n");

		if (gServiceRegistration) {
			MyDNSServiceFree(gServiceRegistration);
			gServiceRegistration = NULL;
		}
	
		CFStringRef newWorkstationName = CopyNextWorkstationName(NULL, gCurrentWorkstationName);
		
		if (gCurrentWorkstationName) CFRelease(gCurrentWorkstationName);
		gCurrentWorkstationName = newWorkstationName;

		if (gCurrentWorkstationName) {
			gServiceRegistration = MyDNSServiceAlloc();
			RegisterWorkstationService(gServiceRegistration, gCurrentWorkstationName);
		}
    } else {
        fprintf(stderr, "ERROR: Workstation name registration failed with error (%d)\n", errorCode);

		if (gServiceRegistration) {
			MyDNSServiceFree(gServiceRegistration);
			gServiceRegistration = NULL;
		}
	}
}


DNSServiceErrorType
RegisterWorkstationService(MyDNSServiceState *ref, CFStringRef serviceName)
{
	char name[64];
	DNSServiceErrorType error = kDNSServiceErr_BadParam;
	
	if (CFStringGetCString(serviceName, name, sizeof(name), kCFStringEncodingUTF8)) {

		error = DNSServiceRegister(&ref->service,
					kDNSServiceFlagsNoAutoRename,
					kDNSServiceInterfaceIndexAny,
											name,
								kWorkstationType,
											NULL,
											NULL,
						 htons(kWorkstationPort),
											   0,
											NULL,
                     RegisterWorkstationCallBack,
									(void *)ref);
	}

    if (kDNSServiceErr_NoError == error) {
        MyDNSServiceAddToRunLoop(ref);
	}
	
    return error;
}


CFStringRef
CopyNextWorkstationName(SCDynamicStoreRef store, CFStringRef currentName)
{
	CFMutableStringRef computerName = NULL;
	CFStringRef macAddress = NULL;
		
	if (currentName) {
	
		/* Sanity check to make sure the current Workstation name is longer than the length of a MAC address string */
		CFIndex totalLengh = CFStringGetLength(currentName);
		if (totalLengh >= (CFIndex)kMACAddressLengh) {
		
			/* Create a substring that chops off the MAC addres, giving us the Computer Name */
			CFRange range = CFRangeMake(0, totalLengh - kMACAddressLengh);
			CFStringRef oldComputerName = CFStringCreateWithSubstring(NULL, currentName, range);
			
			/* If the Computer Name contains a trailing close paren it means that this name may have
			already experienced a name conflict which means it could have a trailing digit to increment */
			if (CFStringHasSuffix(oldComputerName, CFSTR(")"))) {
				CFRange result;
				
				/* Search for the first open paren, starting the search from the end of the Computer Name */
				if (CFStringFindWithOptions(oldComputerName, CFSTR("("), range, kCFCompareBackwards, &result)) {
				
					/* Create a substring which contains the contents between the open and close paren */
					range = CFRangeMake(result.location + 1, CFStringGetLength(oldComputerName) - result.location - 2);
					CFStringRef countString = CFStringCreateWithSubstring(NULL, currentName, range);
					
					/* Try converting the substring to an integer */
					SInt32 conflictCount = CFStringGetIntValue(countString);
					if (conflictCount) {
					
						/* Create a substring of just the Computer Name without the trailing open paren, conflict integer, close paren */
						range = CFRangeMake(0, result.location);
						CFStringRef tempComputerName = CFStringCreateWithSubstring(NULL, oldComputerName, range);
						
						/* Create a mutable copy of the Computer Name base substring */
						computerName = CFStringCreateMutableCopy(NULL, 0, tempComputerName);
						
						/* Create a string containing a space, open paren, previous conflict digit incremented by one, close paren */
						CFStringRef numberString = CFStringCreateWithFormat(NULL, NULL, CFSTR(" (%d)"), ++conflictCount);

						/* Truncate the Computer Name base as neccessary to ensure we can append the conflict digits */
						CFStringTruncateToUTF8Length(computerName, kMaxComputerName - CFStringGetLength(numberString));
						
						/* Append the incremented conflict digit to the Computer Name base string */
						CFStringAppend(computerName, numberString);
					}
				}
			}
			
			/* If computerName is NULL it means that the previous Computer Name didn't contain any conflict digits so append a " (2)" */
			if (!computerName) {
			
				/* Create mutable copy of previous Computer Name */
				computerName = CFStringCreateMutableCopy(NULL, 0, oldComputerName);
				
				/* Make sure we have enough room to append 4 characters to the name by truncating the Computer Name if neccessary */
				CFStringTruncateToUTF8Length(computerName, kMaxComputerName - 4);
				
				/* Append the name conflict digits */
				CFStringAppend(computerName, CFSTR(" (2)"));
			}
			
			CFRelease(oldComputerName);
		} else {
			fprintf(stderr, "ERROR: Workstation name is shorter than a MAC address which shouldn't be possible\n");
		}
	
	} else {
		
		/* There's currently no registered Workstation name so get the Computer Name from the dynamic store */
		CFStringRef tempName = SCDynamicStoreCopyComputerName(store, NULL);
		if (tempName) {
			/* Create a mutable copy of the Computer Name */
			computerName = CFStringCreateMutableCopy(NULL, 0, tempName);
			CFRelease(tempName);
			
			/* Truncate the Computer Name to ensure we can append the MAC address */
			CFStringTruncateToUTF8Length(computerName, kMaxComputerName);
		} else {
			return NULL;
		}
	}
	
	/* Copy the primary MAC address string */
	macAddress = CopyPrimaryMacAddress();
	if (!macAddress) {
		if (computerName) {
			CFRelease(computerName);
		}
		return NULL;
	}
	
	/* Append a space */
	CFStringAppend(computerName, CFSTR(" "));
	
	/* Append the MAC address string */
	CFStringAppend(computerName, macAddress);
	CFRelease(macAddress);

	return computerName;
}


static void
ComputerNameChangedCallBack(SCDynamicStoreRef store, CFArrayRef changedKeys, void *info)
{
    #pragma unused(changedKeys, info)

	if (gServiceRegistration) {
		MyDNSServiceFree(gServiceRegistration);
		gServiceRegistration = NULL;
	}
	
	if (gCurrentWorkstationName) {
		CFRelease(gCurrentWorkstationName);
	}
	
	gCurrentWorkstationName = CopyNextWorkstationName(store, NULL);
	if (gCurrentWorkstationName) {
		gServiceRegistration = MyDNSServiceAlloc();
		RegisterWorkstationService(gServiceRegistration, gCurrentWorkstationName);
	}
}


static CFRunLoopSourceRef
InstallComputerNameMonitor(void)
{
	CFRunLoopSourceRef runLoopSource = NULL;
    SCDynamicStoreContext context = { 0, NULL, NULL, NULL, NULL };
    SCDynamicStoreRef store = SCDynamicStoreCreate(NULL, CFSTR("DirectoryService"), ComputerNameChangedCallBack, &context);
    if (store) {
        CFStringRef computerNameKey = SCDynamicStoreKeyCreateComputerName(NULL);
		CFMutableArrayRef keys = CFArrayCreateMutable(NULL, 1, &kCFTypeArrayCallBacks);
		CFArrayAppendValue(keys, computerNameKey);
		
        if (SCDynamicStoreSetNotificationKeys(store, keys, NULL)) {
            runLoopSource = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
            if (runLoopSource) {
                CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);
            }
        }
		CFRelease(computerNameKey);
		CFRelease(keys);
		CFRelease(store);
    }
	return runLoopSource;
}


static void DisposeRegistration(void);
void DisposeRegistration(void)
{
	if (gNameMonitorSource) {
		CFRunLoopSourceInvalidate(gNameMonitorSource);
		CFRelease(gNameMonitorSource);
		gNameMonitorSource = NULL;
	}

	if (gServiceRegistration) {
		MyDNSServiceFree(gServiceRegistration);
		gServiceRegistration = NULL;
	}
	
	if (gCurrentWorkstationName) {
		CFRelease(gCurrentWorkstationName);
		gCurrentWorkstationName = NULL;
	}
}


int32_t
WorkstationServiceRegister(void)
{
	DNSServiceErrorType error = kDNSServiceErr_NoError;
	CFStringRef workstationName = NULL;
	
	workstationName = CopyNextWorkstationName(NULL, NULL);
	if ( workstationName == NULL )
		return kDNSServiceErr_NoSuchName;
	
	if ( gCurrentWorkstationName != NULL )
	{
		// if this name is registered, there's nothing to do
		if ( CFStringCompare(workstationName, gCurrentWorkstationName, 0) == kCFCompareEqualTo )
		{
			CFRelease( workstationName );
			return kDNSServiceErr_NoError;
		}
		
		// there is a new name to register; remove the old registration
		DisposeRegistration();
	}
	
	// register
	gCurrentWorkstationName = workstationName;
	gNameMonitorSource = InstallComputerNameMonitor();
	gServiceRegistration = MyDNSServiceAlloc();
	error = RegisterWorkstationService(gServiceRegistration, gCurrentWorkstationName);
	if (error != kDNSServiceErr_NoError)
	{
		DbgLog( kLogHandler, "WorkstationServiceRegister(): received error from RegisterWorkstationService: %l", error );
		DisposeRegistration();
	}
	
    return error;
}


int32_t
WorkstationServiceUnregister(void)
{
	DisposeRegistration();
	return 0;
}

