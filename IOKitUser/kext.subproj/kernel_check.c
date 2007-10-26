/*
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 2000-2007 Apple Inc.  All Rights Reserved.
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

#include "kernel_check.h"
#include <mach/kmod.h>
#include <libc.h>

#include "fat_util.h"
#include "macho_util.h"

struct _uuid_stuff {
    unsigned int uuid_size;
    char * uuid;
};

macho_seek_result uuid_callback(
    struct load_command * load_command,
    const void * file_end,
    uint8_t swap __unused,
    void * user_data)
{
    struct _uuid_stuff * uuid_stuff = (struct _uuid_stuff *)user_data;
    if (load_command->cmd == LC_UUID) {
        struct uuid_command * uuid_command = (struct uuid_command *)load_command;
        if (((void *)load_command + load_command->cmdsize) > file_end) {
            return macho_seek_result_error;
        }
        uuid_stuff->uuid_size = sizeof(uuid_command->uuid);
        uuid_stuff->uuid = (char *)uuid_command->uuid;
        return macho_seek_result_found;
    }
    return macho_seek_result_not_found;
}

int copyKextUUID(
    mach_port_t host_port,
    const char * filename,
    const char * bundle_id,
    char ** uuid,
    unsigned int * uuid_size)
{
    int result = -1;
    fat_iterator iter = NULL;

    if (!uuid || !uuid_size) {
        goto finish;
    }

    *uuid = NULL;
    *uuid_size = 0;

    if (!filename) {
        kern_return_t kern_result;
        char * kern_uuid = 0;
        unsigned int kern_uuid_size = 0;

        if (!MACH_PORT_VALID(host_port)) {
            goto finish;
        }

       /* Stuff the ID into the in-uuid parameters. This vm_allocated buffer
        * gets consumed by the call to kmod_control(), but we get back a
        * new one that we have to vm_deallocate().
        */
        if (bundle_id) {
            kern_uuid_size = 1 + strlen(bundle_id);
            if (KERN_SUCCESS != vm_allocate(mach_task_self(), (vm_address_t *)&kern_uuid,
                kern_uuid_size, VM_FLAGS_ANYWHERE)) {

                goto finish;
            }
            bzero(kern_uuid, kern_uuid_size);
            memcpy(kern_uuid, bundle_id, kern_uuid_size);
        }

        kern_result = kmod_control(host_port, 0, KMOD_CNTL_GET_UUID,
            (kmod_args_t *)&kern_uuid, &kern_uuid_size);
        // KERN_INVALID_ARGUMENT: the requested kmod isn't even loaded
        // KERN_FAILURE: the loaded kmod has no UUID
        // any other: the call failed and we cannot confirm the UUID
        if (kern_result == KERN_INVALID_ARGUMENT || kern_result == KERN_FAILURE) {
            result = 0;
            goto finish;
        } else if (kern_result != KERN_SUCCESS) {
            goto finish;
        }
        
        *uuid = (char *)malloc(kern_uuid_size);
        if (!*uuid) {
            goto finish;
        }

        memcpy(*uuid, kern_uuid, kern_uuid_size);
        *uuid_size = kern_uuid_size;
        vm_deallocate(mach_task_self(), (vm_address_t)kern_uuid, kern_uuid_size);
        
        result = 1;

    } else {
        struct mach_header * file = NULL;
        void * file_end = NULL;

        iter = fat_iterator_open(filename, 1);
        if (!iter) {
            goto finish;
        }
        file = (struct mach_header *)fat_iterator_find_host_arch(
            iter, &file_end);
        if (!file) {
            goto finish;
        }
        result = copyMachoUUIDFromMemory(file, file_end,
            uuid, uuid_size);
    }

finish:
    if (iter) fat_iterator_close(iter);
    return result;
}

int copyMachoUUIDFromMemory(
    struct mach_header * file,
    void * file_end,
    char ** uuid,
    unsigned int * uuid_size)
{
    int result = -1;
    macho_seek_result seek_result;
    struct _uuid_stuff seek_uuid;

    if (!uuid || !uuid_size) {
        goto finish;
    }
    
    *uuid = NULL;
    *uuid_size = 0;

    seek_result = macho_scan_load_commands(
        file, file_end,
        uuid_callback, (const void **)&seek_uuid);
    if (seek_result != macho_seek_result_found) {
        result = 0;  // ok for there to not be a uuid
        goto finish;
    }

    *uuid = (char *)malloc(seek_uuid.uuid_size);
    if (!*uuid) {
        goto finish;
    }

    memcpy(*uuid, seek_uuid.uuid, seek_uuid.uuid_size);
    *uuid_size = seek_uuid.uuid_size;
    result = 1;
finish:
    return result;
}

int machoUUIDsMatch(
    mach_port_t host_port,
    const char * file_1,
    const char * file_2)
{
    int result = -1;
    char * uuid1;
    char * uuid2;
    unsigned int uuid1_length, uuid2_length;
    
    if (!copyKextUUID(host_port, file_1, NULL, &uuid1, &uuid1_length)) {
        goto finish;
    }
    if (!copyKextUUID(host_port, file_2, NULL, &uuid2, &uuid2_length)) {
        goto finish;
    }

    result = 0;

    if (uuid1_length == uuid2_length &&
        0 == memcmp(uuid1, uuid2, uuid1_length)) {

        result = 1;
    }
    
finish:
    if (uuid1) free(uuid1);
    if (uuid2) free(uuid2);
    return result;
}


int machoFileMatchesUUID(
    const char * file,
    char * running_uuid,
    unsigned int running_uuid_size)
{
    int result = -1;
    char * file_uuid = NULL;     // must free
    unsigned int file_uuid_size = 0;

    if (1 == copyKextUUID(MACH_PORT_NULL, file, NULL,
        &file_uuid, &file_uuid_size)) {

        result = 0;
        if (file_uuid_size == running_uuid_size &&
            0 == memcmp(running_uuid, file_uuid, file_uuid_size)) {

            result = 1;
        }
    }

    if (file_uuid) free(file_uuid);
    return result;
}
