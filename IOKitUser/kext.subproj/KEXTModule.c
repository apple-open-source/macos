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
#include "vers_rsrc.h"

#include <mach/mach_init.h>
#include <mach/mach_types.h>
#include <mach/kmod.h>

extern KEXTReturn KERN2KEXTReturn(kern_return_t kr);

KEXTEntityType KEXTModuleGetEntityType(void)
{
    return (KEXTEntityType)CFSTR("ModuleDescriptor");
}

KEXTModuleRef KEXTModuleCreate(CFStringRef parentKey, CFDictionaryRef properties)
{
    KEXTModuleRef module;

    if ( !parentKey || !properties ) {
        return NULL;
    }

    module = CFDictionaryCreateMutable(
                        kCFAllocatorDefault, 0,
                        &kCFTypeDictionaryKeyCallBacks,
                        &kCFTypeDictionaryValueCallBacks);
    if ( module ) {
        CFStringRef name;
        CFStringRef primaryKey;

        CFDictionarySetValue(module, CFSTR("EntityType"), CFSTR("ModuleDescriptor"));
        CFDictionarySetValue(module, CFSTR("ModuleProperties"), properties);
        CFDictionarySetValue(module, CFSTR("ParentKey"), parentKey);

        name = CFDictionaryGetValue(properties, CFSTR(kNameKey));
        primaryKey = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("KEXTModule?%@"), name);
        CFDictionarySetValue(module, CFSTR("PrimaryKey"), primaryKey);
        CFRelease(primaryKey);
    }

    return module;
}

KEXTModuleRef KEXTModuleRetain(KEXTModuleRef module)
{
    return (KEXTModuleRef)CFRetain(module);
}

void KEXTModuleRelease(KEXTModuleRef module)
{
    CFRelease(module);
}

CFURLRef KEXTModuleCopyURL(KEXTModuleRef module)
{
    CFStringRef string;
    CFURLRef url;

    if ( !module )
        return NULL;

    string = CFDictionaryGetValue(module, CFSTR("ParentKey"));
    if ( !string )
        return NULL;

    url = CFURLCreateWithString(kCFAllocatorDefault, string, NULL);

    return url;
}

CFTypeRef KEXTModuleGetProperty(KEXTModuleRef module, CFStringRef key)
{
    CFDictionaryRef props;
    if ( !module || !key)
        return NULL;

    props = CFDictionaryGetValue(module, CFSTR("ModuleProperties"));
    if ( !props )
        return NULL;
    
    return CFDictionaryGetValue(props, key);
}

Boolean KEXTModuleEqual(KEXTModuleRef module1, KEXTModuleRef module2)
{
    CFDictionaryRef prop1;
    CFDictionaryRef prop2;

    if ( !module1 || !module2 )
        return false;

    prop1 = CFDictionaryGetValue(module1, CFSTR("ModuleProperties"));
    prop2 = CFDictionaryGetValue(module2, CFSTR("ModuleProperties"));

    return CFEqual(prop1, prop2);
}

Boolean KEXTModuleIsLoaded(KEXTModuleRef module,
    KEXTReturn * error)
{
    CFStringRef name;
    mach_port_t kernel_port;
    kmod_info_t * k_info;
    Boolean ret;
    char str[256];
    int cnt;

    ret = false;

    if ( !module )
        return false;

    name = KEXTModuleGetProperty(module, CFSTR(kNameKey));
    if ( !name ) {
        *error = kKEXTReturnPropertyNotFound;
        return ret;
    }

    if ( !CFStringGetCString(name, str, 256, kCFStringEncodingMacRoman) ) {
        *error = kKEXTReturnError;
        return ret;
    }
    
    *error = KERN2KEXTReturn(task_for_pid(mach_task_self(), 0, &kernel_port));
    if ( *error != kKEXTReturnSuccess ) {
        return ret;
    }

    *error = KERN2KEXTReturn(kmod_get_info(kernel_port, (void *)&k_info, (unsigned int *)&cnt));
    if ( *error != kKEXTReturnSuccess ) {
        return ret;
    }

    while ( k_info ) {
        ret = ( strcmp(str, k_info->name) == 0 );
        if ( ret )
            break;

        k_info = (k_info->next) ? (k_info + 1) : 0;
    }

    return ret;
}

Boolean 		KEXTModuleGetLoadedVers(KEXTModuleRef module,
    UInt32 * vers)
{
    KEXTReturn error;
    CFStringRef name;
    mach_port_t kernel_port;
    kmod_info_t * k_info;
    Boolean ret;
    char str[256];
    int cnt;

    ret = false;

    if ( !module || !vers)
        return false;

    name = KEXTModuleGetProperty(module, CFSTR(kNameKey));
    if ( !name ) {
        return ret;
    }

    if ( !CFStringGetCString(name, str, 256, kCFStringEncodingMacRoman) ) {
        return ret;
    }
    
    error = KERN2KEXTReturn(task_for_pid(mach_task_self(), 0, &kernel_port));
    if ( error != kKEXTReturnSuccess ) {
        return ret;
    }

    error = KERN2KEXTReturn(kmod_get_info(kernel_port, (void *)&k_info, (unsigned int *)&cnt));
    if ( error != kKEXTReturnSuccess ) {
        return ret;
    }

    while ( k_info ) {
        ret = ( strcmp(str, k_info->name) == 0 );
        if ( ret )
            break;

        k_info = (k_info->next) ? (k_info + 1) : 0;
    }

    if (ret) {
        if (!VERS_parse_string(k_info->version, vers)) {
             ret = false;
        }
    }

    return ret;
}

CFStringRef KEXTModuleGetPrimaryKey(KEXTModuleRef module)
{
    return CFDictionaryGetValue(module, CFSTR("PrimaryKey"));
}

CFDictionaryRef _KEXTModuleGetProperties(KEXTModuleRef module)
{
    return CFDictionaryGetValue(module, CFSTR("ModuleProperties"));
}


