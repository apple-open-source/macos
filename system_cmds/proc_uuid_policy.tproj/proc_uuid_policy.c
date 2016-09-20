/*
 * Copyright (c) 2016 Apple Inc. All rights reserved.
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

/* Header Declarations */
#include <errno.h>
#include <fcntl.h>
#include <libkern/OSByteOrder.h>
#include <libproc.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <System/sys/proc_uuid_policy.h>

/* Constant Declarations */
#define SUCCESS                     0
#define FAILURE                     -1
#define MAX_CHUNK_SIZE              1024 * 1024 * 16

#ifndef PROC_UUID_ALT_DYLD_POLICY
#define PROC_UUID_ALT_DYLD_POLICY   0x00000004
#endif

/* UUID bucket */
struct uuid_bucket
{
    unsigned int    num_uuids;
    uuid_t          *binary_uuids;
};

/* Static Function Definitions */
static
void
usage();

static
int
parse_macho_uuids(
    const char *path,
    struct uuid_bucket *uuid_bucket);

static
int
parse_macho_slice(
    const void *mapped,
    const unsigned int offset,
    const unsigned int slice_index,
    struct uuid_bucket *uuid_bucket);

/* Function Definitions */
int
main(
    int argc,
    char **argv)
{
    int                 exit_status = EXIT_FAILURE;
    const char          *verb_string;
    const char          *policy_string;
    const char          *uuid_path_string;
    int                 operation = 0;
    const char          *operation_string = NULL;
    int                 policy = 0;
    uuid_t              uuid;
    struct stat         sb;
    struct uuid_bucket  uuid_bucket = {0, NULL};
    unsigned int        i;
    uuid_string_t       uuid_string = "";

    /*
     * Parse the arguments.
     */

    if (argc != 4) {

        usage();
        goto BAIL;
    }

    verb_string = argv[1];
    policy_string = argv[2];
    uuid_path_string = argv[3];

    if (strcmp(verb_string, "clear") == 0) {

        operation = PROC_UUID_POLICY_OPERATION_CLEAR;
        operation_string = "Clearing";
    } else if (strcmp(verb_string, "add") == 0) {

        operation = PROC_UUID_POLICY_OPERATION_ADD;
        operation_string = "Adding";
    } else if (strcmp(verb_string, "remove") == 0) {

        operation = PROC_UUID_POLICY_OPERATION_REMOVE;
        operation_string = "Removing";
    } else {

        fprintf(stderr, "Unknown verb: %s\n", verb_string);
        usage();
        goto BAIL;
    }

    if (strcmp(policy_string, "none") == 0) {

        policy = PROC_UUID_POLICY_FLAGS_NONE;
    } else if (strcmp(policy_string, "no_cellular") == 0) {

        policy = PROC_UUID_NO_CELLULAR;
    } else if (strcmp(policy_string, "necp") == 0) {

        policy = PROC_UUID_NECP_APP_POLICY;
    } else if (strcmp(policy_string, "alt-dyld") == 0) {

        policy = PROC_UUID_ALT_DYLD_POLICY;
    } else {

        fprintf(stderr, "Unknown policy: %s\n", policy_string);
        usage();
        goto BAIL;
    }

    if (uuid_parse(uuid_path_string, uuid) == -1) {

        /* Is this a path to a macho file? */
        if (stat(uuid_path_string, &sb) == -1) {

            fprintf(stderr, "%s is not a UUID nor path: %s\n", uuid_path_string, strerror(errno));
            goto BAIL;
        } else {

            /* Parse the UUID from the macho file. */
            if (parse_macho_uuids(uuid_path_string, &uuid_bucket)) {

                fprintf(stderr, "Could not parse %s for its UUID\n", uuid_path_string);
                goto BAIL;
            }
        }
    } else {

        uuid_bucket.num_uuids = 1;
        uuid_bucket.binary_uuids = calloc(1, sizeof(uuid_t));
        if (uuid_bucket.binary_uuids == NULL) {

            fprintf(stderr, "Could not allocate single UUID bucket\n");
            goto BAIL;
        }

        memcpy(uuid_bucket.binary_uuids[0], uuid, sizeof(uuid_t));
    }

    for (i = 0; i < uuid_bucket.num_uuids; i++) {

        uuid_unparse(uuid_bucket.binary_uuids[i], uuid_string);
        printf("%s the %s policy for %s\n", operation_string, policy_string, uuid_string);

        if (proc_uuid_policy(operation, uuid_bucket.binary_uuids[i], sizeof(uuid_t), policy) == -1) {

            fprintf(stderr, "Could not enable the UUID policy: %s\n", strerror(errno));
            goto BAIL;
        }
    }

    /* Set the exit status to success. */
    exit_status = EXIT_SUCCESS;

BAIL:

    /*
     * Clean up.
     */

    if (uuid_bucket.binary_uuids != NULL) {

        free(uuid_bucket.binary_uuids);
    }

    return exit_status;
}

/* Static Function Definitions */
static
void
usage(void)
{
    fprintf(stderr, "usage: %s <verb> <policy> <uuid>\n", getprogname());
    fprintf(stderr, "Verbs:\n");
    fprintf(stderr, "\tclear\tClear all policies for a given UUID\n");
    fprintf(stderr, "\tadd\tAdd a specific policy\n");
    fprintf(stderr, "\tremove\tRemove a specific policy\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Policies:\n");
    fprintf(stderr, "\tnone\t\tPROC_UUID_POLICY_FLAGS_NONE\n");
    fprintf(stderr, "\tno_cellular\tPROC_UUID_NO_CELLULAR\n");
    fprintf(stderr, "\tnecp\t\tPROC_UUID_NECP_APP_POLICY\n");
    fprintf(stderr, "\talt-dyld\tPROC_UUID_ALT_DYLD_POLICY\n");
}

static
int
parse_macho_uuids(
    const char *path,
    struct uuid_bucket *uuid_bucket)
{
    int                 result = FAILURE;
    int                 fd = -1;
    struct stat         sb;
    void                *mapped = MAP_FAILED;

    struct fat_header   *fat_header_pointer;
    struct fat_arch     *fat_arch_pointer;
    bool                swapped = false;

    uint32_t            nfat_arch = 0;
    unsigned int        i;
    uint32_t            arch_offset;
    uint32_t            arch_size;

    /* Open the file and determine its size. */
    fd = open(path, O_RDONLY);
    if (fd == -1) {

        fprintf(stderr, "Could not open %s: %s\n", path, strerror(errno));
        goto BAIL;
    }

    if (fstat(fd, &sb) == -1) {

        fprintf(stderr, "Could not fstat %s: %s\n", path, strerror(errno));
        goto BAIL;
    }

    /* Memory map the file. */
    mapped = mmap (0, (size_t)sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {

        fprintf(stderr, "Could not memory map %s: %s\n", path, strerror(errno));
        goto BAIL;
    }

    /*
     * Determine the file type.
     */

    fat_header_pointer = (struct fat_header *) mapped;

    switch (fat_header_pointer->magic) {

        case FAT_MAGIC: {

            nfat_arch = fat_header_pointer->nfat_arch;
        }break;

        case FAT_CIGAM: {

            swapped = true;
            nfat_arch = OSSwapInt32(fat_header_pointer->nfat_arch);
        }break;

        case MH_MAGIC:
        case MH_CIGAM:
        case MH_MAGIC_64:
        case MH_CIGAM_64: {

            uuid_bucket->num_uuids = 1;

            uuid_bucket->binary_uuids = calloc(1, sizeof(uuid_t));
            if (uuid_bucket->binary_uuids == NULL) {

                fprintf(stderr, "Could not allocate a UUID\n");
                goto BAIL;
            }

            if (parse_macho_slice(mapped, 0, 0, uuid_bucket)) {

                fprintf(stderr, "Could not parse slice\n");
                goto BAIL;
            }
        }break;

        default: {

            fprintf(stderr, "Unknown magic: %d\n", fat_header_pointer->magic);
            goto BAIL;
        }
    }

    if (nfat_arch > 0) {

        uuid_bucket->num_uuids = nfat_arch;

        uuid_bucket->binary_uuids = calloc(nfat_arch, sizeof(uuid_t));
        if (uuid_bucket->binary_uuids == NULL) {

            fprintf(stderr, "Could not allocate %d UUIDs\n", nfat_arch);
            goto BAIL;
        }

        for (i = 0; i < nfat_arch; i++) {

            fat_arch_pointer = (struct fat_arch *)(mapped + sizeof(struct fat_header) + (sizeof(struct fat_arch) * i));

            if (swapped) {

                arch_offset = OSSwapInt32(fat_arch_pointer->offset);
                arch_size = OSSwapInt32(fat_arch_pointer->size);
            } else {

                arch_offset = fat_arch_pointer->offset;
                arch_size = fat_arch_pointer->size;
            }

            if (parse_macho_slice(mapped, arch_offset, i, uuid_bucket)) {

                fprintf(stderr, "Could not parse slice %d of %d\n", i, nfat_arch);
                goto BAIL;
            }
        }
    }

    /* Set the result to success. */
    result = SUCCESS;

BAIL:

    /*
     * Clean up.
     */

    if (mapped != MAP_FAILED) {

        (void) munmap(mapped, (size_t)sb.st_size);
        mapped = MAP_FAILED;
    }

    if (fd != -1) {

        (void) close(fd);
        fd = -1;
    }

    return result;
}

static
int
parse_macho_slice(
    const void *mapped,
    const unsigned int offset,
    const unsigned int slice_index,
    struct uuid_bucket *uuid_bucket)
{
    int                     result = FAILURE;

    struct mach_header      *mach_header_pointer;
    struct mach_header_64   *mach_header_64_pointer;
    struct load_command     *load_command_pointer;

    bool                    swapped = false;

    unsigned int            number_load_commands = 0;
    unsigned int            i;

    bool                    found_uuid_load_command = false;
    struct uuid_command     *uuid_load_command_pointer = NULL;

    mach_header_pointer = (struct mach_header *)(mapped + offset);

    switch (mach_header_pointer->magic) {

        case FAT_MAGIC: {

            fprintf(stderr, "FAT_MAGIC\n");
            goto BAIL;
        }break;

        case FAT_CIGAM: {

            fprintf(stderr, "FAT_CIGAM\n");
            goto BAIL;
        }break;

        case MH_MAGIC: {

            number_load_commands = mach_header_pointer->ncmds;
            load_command_pointer = (struct load_command *)(void *)(mach_header_pointer + 1);
        }break;

        case MH_CIGAM: {

            swapped = true;

            number_load_commands = OSSwapInt32(mach_header_pointer->ncmds);
            load_command_pointer = (struct load_command *)(void *)(mach_header_pointer + 1);
        }break;

        case MH_MAGIC_64: {

            mach_header_64_pointer = (struct mach_header_64 *)(mapped + offset);
            number_load_commands = mach_header_64_pointer->ncmds;

            load_command_pointer = (struct load_command *)(void *)(mach_header_64_pointer + 1);
        }break;

        case MH_CIGAM_64: {

            swapped = true;

            mach_header_64_pointer = (struct mach_header_64 *)(mapped + offset);
            number_load_commands = OSSwapInt32(mach_header_64_pointer->ncmds);

            load_command_pointer = (struct load_command *)(void *)(mach_header_64_pointer + 1);
        }break;

        default: {

            fprintf(stderr, "Unknown magic: %d\n", mach_header_pointer->magic);
            goto BAIL;
        }
    }

    /* Walk the load commands looking for LC_UUID. */
    for (i = 0; i < number_load_commands; i++) {

        if (load_command_pointer->cmd == LC_UUID) {

            found_uuid_load_command = true;
            uuid_load_command_pointer = (struct uuid_command *)load_command_pointer;
            memcpy(uuid_bucket->binary_uuids[slice_index], uuid_load_command_pointer->uuid, sizeof(uuid_t));
        }

        load_command_pointer = (struct load_command *)((uintptr_t)load_command_pointer + load_command_pointer->cmdsize);
    }

    if (found_uuid_load_command == false) {

        fprintf(stderr, "Could not find LC_UUID\n");
        goto BAIL;
    }

    /* Set the result to success. */
    result = SUCCESS;

BAIL:

    return result;
}
