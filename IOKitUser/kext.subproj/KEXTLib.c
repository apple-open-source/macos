/*
 * Copyright (c) 1999-2000 Apple Computer, Inc. All rights reserved.
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
/*
 * HISTORY
 *
 */

#include "KEXTPrivate.h"
#include <unistd.h>
#include <sys/wait.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitServer.h>
#include <IOKit/IOCFSerialize.h>


#define kKModLoadUtil 	"/sbin/kmodload"


KEXTReturn KERN2KEXTReturn(kern_return_t kr)
{
    KEXTReturn error;

    switch ( kr ) {
        case KERN_SUCCESS:
            error = kKEXTReturnSuccess;
            break;
        case KERN_INVALID_SECURITY:
        case KERN_NO_ACCESS:
            error = kKEXTReturnPermissionError;
            break;
        case KERN_RESOURCE_SHORTAGE:
            error = kKEXTReturnNoMemory;
        default:
            error = kKEXTReturnError;
            break;
    }

    return error;
}

void KEXTError(KEXTReturn error, CFStringRef text)
{
    CFStringRef str;
    CFStringRef message;

    if ( !text ) {
        return;
    }
    
    switch ( error ) {
        case kKEXTReturnSuccess:
            str = CFSTR("success");
            break;
        case kKEXTReturnBadArgument:
            str = CFSTR("bad argument");
            break;
        case kKEXTReturnNoMemory:
            str = CFSTR("no memory");
            break;
        case kKEXTReturnNotKext:
            str = CFSTR("not kext bundle");
            break;
        case kKEXTReturnResourceNotFound:
            str = CFSTR("resource not found");
            break;
        case kKEXTReturnModuleNotFound:
            str = CFSTR("module not found");
            break;
        case kKEXTReturnPersonalityNotFound:
            str = CFSTR("personality not found");
            break;
        case kKEXTReturnPropertyNotFound:
            str = CFSTR("property not found");
            break;
        case kKEXTReturnModuleFileNotFound:
            str = CFSTR("module file not found");
            break;
        case kKEXTReturnBundleNotFound:
            str = CFSTR("kernel extension bundle not found");
            break;
        case kKEXTReturnPermissionError:
            str = CFSTR("permission error");
            break;
        case kKEXTReturnMissingDependency:
            str = CFSTR("missing dependency");
            break;
        case kKEXTReturnDependencyVersionMismatch:
            str = CFSTR("one or more dependencies lack compatible versions");
            break;
        case kKEXTReturnSerializationError:
            str = CFSTR("serialization error");
            break;
        case kKEXTReturnKModLoadError:
            str = CFSTR("kmod load error");
            break;
        case kKEXTReturnModuleAlreadyLoaded:
            str = CFSTR("module already loaded");
            break;
        case kKEXTReturnModuleDisabled:
            str = CFSTR("module disabled");
            break;
        case kKEXTReturnError:
        default:
            str = CFSTR("general error");
            break;
    }
    message = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@: %@."), text, str);
    CFShow(message);
    CFRelease(message);
}

KEXTReturn KEXTSendDataToCatalog(mach_port_t port, int flag, CFTypeRef obj)
{
    kern_return_t kr;
    KEXTReturn error;
    CFDataRef data;
    CFIndex len;
    void * ptr;

    error = kKEXTReturnSuccess;
    
    data = IOCFSerialize(obj, kNilOptions);
    if ( !data ) {
        return kKEXTReturnSerializationError;
    }

    len = CFDataGetLength(data) + 1;
    ptr = (void *)CFDataGetBytePtr(data);
    kr = IOCatalogueSendData(port, flag, ptr, len);
    error = KERN2KEXTReturn(kr);
    CFRelease(data);

    return error;
}

static KEXTReturn _KEXTModuleLoad(char ** arglist)
{
    pid_t pid;
    KEXTReturn error;

    error = kKEXTReturnSuccess;

    do {
        pid = fork();
        if ( pid < 0 ) {
            error = kKEXTReturnError;
            break;
        }

        if ( pid ) {
            SInt32 err;
            int status;

            if ( wait4(pid, &status, WUNTRACED, 0) < 0 ) {
                error = kKEXTReturnError;
                break;
            }
            err = (signed char)WEXITSTATUS(status);
            if ( !WIFEXITED(status) || (err != 0) ) {
                error = kKEXTReturnKModLoadError;
            }
        }
        else {
            exit(execv(arglist[0], arglist));
        }
    } while ( 0 );


    return error;
}

KEXTReturn KEXTLoadModule(CFStringRef path, CFArrayRef dependencies)
{
    char ** argList;
    char * string;
    KEXTReturn error;
    CFStringRef str;
    CFIndex depCount;
    CFIndex len;
    CFIndex j, notfree;

    error = kKEXTReturnSuccess;

    argList = malloc(sizeof(char *) * 512);
    notfree = 0;
    argList[notfree++] = (char *) kKModLoadUtil;

    depCount = 0;
    j = notfree;
    if ( dependencies ) {
        CFIndex i;
        depCount = CFArrayGetCount(dependencies);
        for ( i = 0; i < depCount; i++ ) {
            str = (CFStringRef)CFArrayGetValueAtIndex(dependencies, i);
            if ( !str )
                continue;

            string = (char *)malloc(sizeof(char) * 3);
            strcpy(string, "-d");
            argList[j++] = string;

            len = CFStringGetLength(str) + 1;
            string = (char *)malloc(sizeof(char) * len);
            if ( !CFStringGetCString(str, string, len, kCFStringEncodingMacRoman) ) {
                free(string);
                continue;
            }

            argList[j++] = string;
        }
    }
    len = CFStringGetLength(path) + 1;
    string = (char *)malloc(sizeof(char) * len);
    if ( CFStringGetCString(path, string, len, kCFStringEncodingMacRoman) ) {
        argList[j++] = string;
        argList[j++] = NULL;
        error = _KEXTModuleLoad(argList);
    }
    else {
        error = kKEXTReturnError;
    }

    j--;	// Don't free the NULL!
    while (j-- > notfree)
        free(argList[j]);
    free(argList);

    return error;
}

const void * _KEXTBundleRetainCB(CFAllocatorRef a, const void * ptr)
{
    return (const void *)KEXTBundleRetain((KEXTBundleRef)ptr);
}

void _KEXTBundleReleaseCB(CFAllocatorRef a, const void * ptr)
{
    KEXTBundleRelease((KEXTBundleRef)ptr);
}

Boolean _KEXTBundleEqualCB(const void * ptr1, const void * ptr2)
{
    return KEXTBundleEqual((KEXTBundleRef)ptr1, (KEXTBundleRef)ptr2);
}

const void * _KEXTModuleRetainCB(CFAllocatorRef allocator, const void *ptr)
{
    return (const void *)KEXTModuleRetain((KEXTModuleRef)ptr);
}

void _KEXTModuleReleaseCB(CFAllocatorRef allocator, const void *ptr)
{
    KEXTModuleRelease((KEXTModuleRef)ptr);
}

Boolean _KEXTModuleEqualCB(const void *ptr1, const void *ptr2)
{
    return KEXTModuleEqual((KEXTModuleRef)ptr1, (KEXTModuleRef)ptr2);
}

const void * _KEXTPersonalityRetainCB(CFAllocatorRef allocator, const void *ptr)
{
    return (const void *)KEXTPersonalityRetain((KEXTPersonalityRef)ptr);
}

void _KEXTPersonalityReleaseCB(CFAllocatorRef allocator, const void *ptr)
{
    KEXTPersonalityRelease((KEXTPersonalityRef)ptr);
}

Boolean _KEXTPersonalityEqualCB(const void *ptr1, const void *ptr2)
{
    return KEXTPersonalityEqual((KEXTPersonalityRef)ptr1, (KEXTPersonalityRef)ptr2);
}

