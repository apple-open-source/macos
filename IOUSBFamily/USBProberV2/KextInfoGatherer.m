/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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


#import "KextInfoGatherer.h"


@implementation KextInfoGatherer

static int kmod_compare(const void * a, const void * b)
{
    kmod_info_t * k1 = (kmod_info_t *)a;
    kmod_info_t * k2 = (kmod_info_t *)b;
    // these are load indices, not CFBundleIdentifiers
//    return strcasecmp(k1->name, k2->name);
    return (k1->id - k2->id);
}

+ (NSMutableArray *)loadedExtensions {
    NSMutableArray *returnArray = [[NSMutableArray alloc] init];
    
    kern_return_t mach_result = KERN_SUCCESS;
    mach_port_t host_port = MACH_PORT_NULL;
    kmod_info_t * kmod_list;
    mach_msg_type_number_t kmod_bytecount;  // not really used
    int kmod_count;
    kmod_info_t * this_kmod;
    int i;
    
    /* Get the list of loaded kmods from the kernel.
        */
    host_port = mach_host_self();
    mach_result = kmod_get_info(host_port, (void *)&kmod_list,
                                &kmod_bytecount);
    if (mach_result != KERN_SUCCESS) {

        // ERROR
        
        goto finish;
    }
    
    /* kmod_get_info() doesn't return a proper count so we have
        * to scan the array checking for a NULL next pointer.
        */
    this_kmod = kmod_list;
    kmod_count = 0;
    while (this_kmod) {
        kmod_count++;
        this_kmod = (this_kmod->next) ? (this_kmod + 1) : 0;
    }
    
    
    if (!kmod_count) {
        goto finish;
    }
    
    qsort(kmod_list, kmod_count, sizeof(kmod_info_t), kmod_compare);
    
    /* Now print out what was found. */
    this_kmod = kmod_list;
    for (i=0; i < kmod_count; i++, this_kmod++) {
        NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];
        [dict setObject:[NSString stringWithFormat:@"%s",this_kmod->name] forKey:@"Name"];
        [dict setObject:[NSString stringWithFormat:@"%s",this_kmod->version] forKey:@"Version"];
        if (this_kmod->size != 0) {
            if (this_kmod->size > 1024) {
                [dict setObject:[NSString stringWithFormat:@"%U KB",this_kmod->size/1024] forKey:@"Size"];
            } else {
                [dict setObject:[NSString stringWithFormat:@"%U Bytes",this_kmod->size] forKey:@"Size"];
            }
                
        } else {
            [dict setObject:@"n/a" forKey:@"Size"];
        }
        if (this_kmod->size - this_kmod->hdr_size) {
            if ((this_kmod->size - this_kmod->hdr_size) > 1024) {
                [dict setObject:[NSString stringWithFormat:@"%U KB",(this_kmod->size - this_kmod->hdr_size)/1024] forKey:@"Wired"];
            } else {
                [dict setObject:[NSString stringWithFormat:@"%U bytes",this_kmod->size - this_kmod->hdr_size] forKey:@"Wired"];
            }
        } else {
            [dict setObject:@"n/a" forKey:@"Wired"];
        }
        [dict setObject:[NSString stringWithFormat:@"%-10p",this_kmod->address] forKey:@"Address"];
        
        [returnArray addObject:dict];
        [dict release];
    }
    
finish:
        
        /* Dispose of the host port to prevent security breaches and port
        * leaks. We don't care about the kern_return_t value of this
        * call for now as there's nothing we can do if it fails.
        */
        if (MACH_PORT_NULL != host_port) {
            mach_port_deallocate(mach_task_self(), host_port);
        }
    
    if (kmod_list) {
        vm_deallocate(mach_task_self(), (vm_address_t)kmod_list, kmod_bytecount);
    }
    
    return [returnArray autorelease];
}

+ (NSMutableArray *)loadedExtensionsContainingString:(NSString *)string {
    NSMutableArray *returnArray = [NSMutableArray arrayWithArray:[KextInfoGatherer loadedExtensions]];
    if (returnArray != nil) {
		NSArray * arrayCopy = [ returnArray copy ];
        NSEnumerator *enumerator = [arrayCopy objectEnumerator];
        NSDictionary *thisKext = NULL;
        
        while (thisKext = [enumerator nextObject]) {
            if ([[thisKext objectForKey:@"Name"] rangeOfString:string options:NSCaseInsensitiveSearch].location == NSNotFound) {
                [returnArray removeObject:thisKext];
            }
        }
		[arrayCopy release];
    }
     
        
    return returnArray;
}

@end
