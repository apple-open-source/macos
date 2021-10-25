/*
 * Copyright (c) 2021 Apple Inc. All Rights Reserved.
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

#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach/mach.h>
#include <os/overflow.h>
#include <os/cleanup.h>

#include "codesign_alloc.h"

#define CODE_SIGN_ALLOC_DEBUG 0

OS_FORMAT_PRINTF(2, 3)
static void log_error(char*& errorMessage, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    vasprintf((char**)&errorMessage, format, ap);
    va_end(ap);
#if CODE_SIGN_ALLOC_DEBUG
    printf("%s\n",errorMessage);
#endif
}

static bool vm_alloc(void*& addr, vm_size_t size, char*& errorMessage)
{
    vm_address_t allocAddr = 0;
    kern_return_t r = ::vm_allocate(mach_task_self(), &allocAddr, size, VM_FLAGS_ANYWHERE);
    if (r != KERN_SUCCESS) {
        log_error(errorMessage, "failed to allocate memory\n");
        return false;
    }
    addr = (void*)allocAddr;
    return true;
}

static bool vm_dealloc(void*& addr, vm_size_t size, char*& errorMessage)
{
    vm_address_t deallocAddr = (vm_address_t)addr;
    kern_return_t r = ::vm_deallocate(mach_task_self(), deallocAddr, size);
    if (r != KERN_SUCCESS) {
        log_error(errorMessage,"failed to deallocate memory\n");
        return false;
    }
    addr = NULL;
    return true;
}

// memory maps a file
static bool mapFile(const char* path, const void*& mappedAddr, uint64_t& fileLen, char*& errorMessage)
{
    struct stat stat_buf;
    int fd = ::open(path, O_RDONLY, 0);
    if ( fd == -1 ) {
        log_error(errorMessage, "cannot open file %s, errno=%d\n", path, errno);
        return false;
    }
    if ( ::fstat(fd, &stat_buf) != 0 )  {
        log_error(errorMessage, "fstat(%s) failed, errno=%d\n", path, errno);
        return false;
    }
    uint8_t* p = (uint8_t*)::mmap(NULL, stat_buf.st_size, PROT_READ, MAP_FILE | MAP_PRIVATE | MAP_RESILIENT_CODESIGN, fd, 0);
    if ( p == ((uint8_t*)(-1)) ) {
        log_error(errorMessage, "cannot mmap file %s\n", path);
        return false;
    }
    ::close(fd);
    mappedAddr = p;
    fileLen = stat_buf.st_size;
    return true;
}

static ssize_t write64(int fildes, const void *buf, size_t nbyte)
{
    unsigned char* uchars = (unsigned char*)buf;
    ssize_t total = 0;
    
    while (nbyte) {
        /*
         * If we were writing socket- or stream-safe code we'd chuck the
         * entire buf to write(2) and then gracefully re-request bytes that
         * didn't get written. But write(2) will return EINVAL if you ask it to
         * write more than 2^31-1 bytes. So instead we actually need to throttle
         * the input to write.
         *
         * Historically code using write(2) to write to disk will assert that
         * that all of the requested bytes were written. It seems harmless to
         * re-request bytes as one does when writing to streams, with the
         * compromise that we will return immediately when write(2) returns 0
         * bytes written.
         */
        size_t limit = 0x7FFFFFFF;
        size_t towrite = nbyte < limit ? nbyte : limit;
        ssize_t wrote = write(fildes, uchars, towrite);
        if (-1 == wrote) {
            return -1;
        } else if (0 == wrote) {
            break;
        } else {
            nbyte -= wrote;
            uchars += wrote;
            total += wrote;
        }
    }
    
    return total;
}

// write a memory buffer to a file
static bool writeFile(const char* path, const void* buffer, unsigned int len, char*& errorMessage)
{
    int fd = ::open(path, O_CREAT | O_WRONLY | O_TRUNC, 0777);
    if (fd == -1) {
        log_error(errorMessage, "can't open output file for writing: %s, errno=%d\n", path, errno);
        return false;
    }
    if (write64(fd, buffer, len) != len) {
        log_error(errorMessage, "can't write to output file (len: %d): %s, errno=%d\n", len, path, errno);
        return false;
    }
    ::close(fd);
    return true;
}

static uint32_t get32(bool swap, uint32_t value)
{
    return (swap ? _OSSwapInt32(value) : value);
}

static uint64_t get64(bool swap, uint64_t value)
{
    return (swap ? _OSSwapInt64(value) : value);
}

// make sure mach-o binary has LC_CODE_SIGNATURE load command with sigSpace space
// assumes LINKEDIT region pointed to by LC_CODE_SIGNATURE is always at end of file
static bool assure_signature_space(void* machoFile, unsigned int sigSpace, uint32_t inFileSize,
                                   uint32_t& newFileSize, char*& errorMessage)
{
    // find existing LC_CODE_SIGNATURE
    mach_header* mh = (mach_header*)machoFile;
    const bool swap = ((mh->magic == MH_CIGAM) || (mh->magic == MH_CIGAM_64));
    const bool is64 = ((mh->magic == MH_MAGIC_64) || (mh->magic == MH_CIGAM_64));
    const uint32_t filetype = get32(swap, mh->filetype);
    if ((filetype != MH_EXECUTE) &&
        (filetype != MH_DYLIB) &&
        (filetype != MH_BUNDLE) &&
        (filetype != MH_KEXT_BUNDLE) &&
        (filetype != MH_DYLINKER)) {
        log_error(errorMessage, "mach-o filetype (%d) does not support code signing\n", filetype);
        return false;
    }
    load_command* const cmds = (load_command*)((char*)mh + (is64 ? sizeof(mach_header_64) : sizeof(mach_header)));
    const uint32_t cmdCount = get32(swap, mh->ncmds);
    const uint32_t cmdsSize = get32(swap, mh->sizeofcmds);
    const load_command* const cmdsEnd = (load_command*)((char*)cmds + cmdsSize);
    linkedit_data_command* sigCmd = NULL;
    segment_command* leSegCmd = NULL;
    segment_command_64* leSegCmd64 = NULL;
    const load_command* cmd = cmds;
    for (uint32_t i = 0; i < cmdCount; ++i) {
        const uint32_t cmdKind = get32(swap, cmd->cmd);
        const uint32_t cmdSize = get32(swap, cmd->cmdsize);
        switch ( cmdKind ) {
            case LC_CODE_SIGNATURE:
                sigCmd = (linkedit_data_command*)cmd;
                break;
            case LC_SEGMENT:
                if (strcmp(((segment_command*)cmd)->segname, "__LINKEDIT") == 0) {
                    leSegCmd = (segment_command*)cmd;
                }
                break;
            case LC_SEGMENT_64:
                if (strcmp(((segment_command_64*)cmd)->segname, "__LINKEDIT") == 0) {
                    leSegCmd64 = (segment_command_64*)cmd;
                }
                break;
        }
        cmd = (load_command*)((char*)cmd + cmdSize);
        if (cmd > cmdsEnd) {
            log_error(errorMessage, "malformed mach-o file, load command #%d is outside size of load commands\n", i);
            return false;
        }
    }
    // make sure LINKEDIT extends to end of file
    if (leSegCmd != NULL) {
        uint32_t leFileOffsetEnd = 0;
        if (os_add_overflow(get32(swap, leSegCmd->fileoff), get32(swap, leSegCmd->filesize), &leFileOffsetEnd) ||
            (inFileSize < leFileOffsetEnd)) {
            log_error(errorMessage, "malformed mach-o file, __LINKEDIT segment extends past end of file\n");
            return false;
        }
    } else if ( leSegCmd64 != NULL ) {
        uint64_t leFileOffsetEnd = 0;
        if (os_add_overflow(get64(swap, leSegCmd64->fileoff), get64(swap, leSegCmd64->filesize), &leFileOffsetEnd) ||
            (inFileSize < leFileOffsetEnd)) {
            log_error(errorMessage, "malformed mach-o file, __LINKEDIT segment extends past end of file\n");
            return false;
        }
    } else {
        log_error(errorMessage, "malformed mach-o file, no __LINKEDIT segment\n");
        return false;
    }

    if (sigCmd != NULL) {
        // make sure existing load command points to data at end of LINKEDIT
        unsigned int leSigOffsetEnd = 0;
        if (os_add_overflow(get32(swap, sigCmd->dataoff), get32(swap, sigCmd->datasize), &leSigOffsetEnd)) {
            log_error(errorMessage, "malformed mach-o file, LC_CODE_SIGNATURE offset + size overflows\n");
            return false;
        }
        if (leSigOffsetEnd < (inFileSize-7)) {
            log_error(errorMessage, "malformed mach-o file, LC_CODE_SIGNATURE does not point to end of file\n");
            return false;
        }
        // update load command with new sigSize
        sigCmd->datasize = get32(swap, sigSpace);
        uint32_t oldFileSize = newFileSize;
        if (os_add_overflow(get32(swap, sigCmd->dataoff), sigSpace, &newFileSize)) {
            log_error(errorMessage, "mew sigSpace causes overflow\n");
            return false;
        }
        if (newFileSize < oldFileSize) {
            uint8_t* cursor = (uint8_t*) machoFile;
            memset((cursor + newFileSize), 0, oldFileSize - newFileSize);
        }
    } else {
        // no existing LC_CODE_SIGNATURE need to add one
        // find start of first section to make sure there is room to add new load command
        uint32_t firstSectionFileOffset = 0;
        bool firstSectionFound = false;
        const load_command* cmd = cmds;
        for (uint32_t i = 0; i < cmdCount; ++i) {
            const uint32_t cmdKind = get32(swap, cmd->cmd);
            const uint32_t cmdSize = get32(swap, cmd->cmdsize);
            if (cmdKind == LC_SEGMENT) {
                const segment_command* segCmd = (segment_command*)cmd;
                if ((segCmd->fileoff == 0) && (segCmd->filesize != 0) && (segCmd->nsects != 0)) {
                    const section* firstSection = (section*)((char*)segCmd + sizeof(segment_command));
                    firstSectionFileOffset = get32(swap, firstSection->offset);
                    firstSectionFound = true;
                    break;
                }
            } else if (cmdKind == LC_SEGMENT_64) {
                const segment_command_64* segCmd = (segment_command_64*)cmd;
                if ( (segCmd->fileoff == 0) && (segCmd->filesize != 0) && (segCmd->nsects != 0) ) {
                    const section_64* firstSection = (section_64*)((char*)segCmd + sizeof(segment_command_64));
                    firstSectionFileOffset = get32(swap, firstSection->offset);
                    firstSectionFound = true;
                    break;
                }
            }
            cmd = (load_command*)((char*)cmd + cmdSize);
        }
        // If the first section offset wasn't found, we always attempt to write the LC_CODE_SIGNATURE command. This is
        // for compatibility with some old tools (bitcode_strip) that can cause the first section offset to not be
        // set properly, which would then break signing + archiving workflows with older toolchains.
        if (firstSectionFound && ((cmdsSize + sizeof(linkedit_data_command)) > firstSectionFileOffset)) {
            log_error(errorMessage, "not enough room in load commands to add LC_CODE_SIGNATURE\n");
            return false;
        }
        // is enough padding, append new load command
        sigCmd = (linkedit_data_command*)cmdsEnd;
        mh->sizeofcmds = get32(swap, cmdsSize + sizeof(linkedit_data_command));
        mh->ncmds = get32(swap, cmdCount + 1);
        sigCmd->cmd = get32(swap, LC_CODE_SIGNATURE);
        sigCmd->cmdsize = get32(swap, sizeof(linkedit_data_command));
        uint32_t appendOffset = (inFileSize + 15) & (-16); // 16-byte align start
        sigCmd->dataoff = get32(swap, appendOffset);
        sigCmd->datasize = get32(swap, sigSpace);
        if (os_add_overflow(appendOffset, sigSpace, &newFileSize)) {
            log_error(errorMessage, "sigSpace + appendOffset overflows\n");
            return false;
        }
    }

    // update LINKEDIT size
    if (leSegCmd != NULL) {
        uint32_t size = newFileSize - get32(swap, leSegCmd->fileoff);
        uint32_t vmsize = (size + 0x3FFF) & (-0x4000);
        leSegCmd->filesize = get32(swap, size);
        leSegCmd->vmsize   = get32(swap, vmsize);
    } else {
        uint64_t size = newFileSize - get64(swap, leSegCmd64->fileoff);
        uint64_t vmsize = (size + 0x3FFF) & (-0x4000);
        leSegCmd64->filesize = get64(swap, size);
        leSegCmd64->vmsize   = get64(swap, vmsize);
    }

    return true;
}

//
// Reads from (possibly fat) mach-o file at path 'existingFilePath'
// and writes a new file at path 'newOutputFilePath'.
// Returns true on success, otherwise returns false and the errorMessage
// parameter is set to a malloced failure messsage.
// For each mach-o slice in the file, the block 'getSigSpaceNeeded' is called
// and expected to return the amount of space need for the code signature.
//
bool code_sign_allocate(const char* existingFilePath,
                        const char* newOutputFilePath,
                        unsigned int (^getSigSpaceNeeded)(cpu_type_t cputype, cpu_subtype_t cpusubtype),
                        char*& errorMessage)
{
    errorMessage = NULL;

    // map in inputfile
    const void* mappedInputFile = NULL;
    uint64_t inputFileLen64 = 0;
    uint32_t inputFileLen = 0;
    void* output = NULL;
    vm_size_t worstCaseOutputSize = 0;
    unsigned int outputSize = 0;
    
    bool success = mapFile(existingFilePath, mappedInputFile, inputFileLen64, errorMessage);
    if (!success) {
        return false;
    }

    // check if fat file
    const mach_header* mh = (mach_header*)mappedInputFile;
    
    if (inputFileLen64 > UINT32_MAX) {
        log_error(errorMessage, "input file too large: %lld bytes\n", inputFileLen64);
        success = false;
        goto lb_unmap;
    }
    inputFileLen = (uint32_t)inputFileLen64;
    
    if (mh->magic == OSSwapBigToHostInt32(FAT_MAGIC)) {
        // gather signature space needed for each slice
        const struct fat_header* inFatHeader = (struct fat_header*)mh;
        const struct fat_arch* inArches = (struct fat_arch*)((char*)mh + sizeof(struct fat_header));
        const int fatCount = OSSwapBigToHostInt32(inFatHeader->nfat_arch);
        unsigned int *__os_free sigSpace = (unsigned int*)calloc(fatCount, sizeof(unsigned int));
        unsigned int totalSigSize = 0;
        for (unsigned int i=0; i < fatCount; ++i) {
            unsigned int offset = OSSwapBigToHostInt32(inArches[i].offset);
            unsigned int size = OSSwapBigToHostInt32(inArches[i].size);
            if ((size + offset) > inputFileLen) {
                log_error(errorMessage, "malformed fat file, slice %d extends past end of file\n", i);
                success = false;
                goto lb_unmap;
            }
            unsigned int sigSize = getSigSpaceNeeded(get32(true,inArches[i].cputype),
                                                     get32(true, inArches[i].cpusubtype));
            if (sigSize == UINT32_MAX) {
                log_error(errorMessage, "requested signature size is too long for slice: %d\n", i);
                success = false;
                goto lb_unmap;
            }
            if ((sigSize & 0xF) != 0) {
                log_error(errorMessage, "signature size not a multiple of 16 in slice %d\n", i);
                success = false;
                goto lb_unmap;
            }
            sigSpace[i] = sigSize;
            totalSigSize += sigSize;
        }
        // allocate output buffer, worst case size
        if (os_add3_overflow(inputFileLen, totalSigSize, (15+0x4000)*fatCount, &worstCaseOutputSize)) {
            log_error(errorMessage, "worstCaseOutputsize overflows: inputFileLen (%d) totalSigSize (%d), fatCount(%d)\n", inputFileLen, totalSigSize, fatCount);
            success = false;
            goto lb_unmap;
        }
        if (!vm_alloc(output, worstCaseOutputSize, errorMessage)) {
            success = false;
            goto lb_unmap;
        }
        struct fat_header* outFatHeader = (struct fat_header*)output;
        struct fat_arch* outArches = (struct fat_arch*)((char*)output + sizeof(struct fat_header));
        // copy fat header
        *outFatHeader = *inFatHeader;
        // copy each slice
        void* outputSlice = ((char*)output + 0x4000); // 16KB align first slice
        for (unsigned int i=0; i < fatCount; ++i) {
            outArches[i] = inArches[i];
            const void* inputSlice = (void*)((char*)mh + OSSwapBigToHostInt32(inArches[i].offset));
            unsigned int inputSliceSize = OSSwapBigToHostInt32(inArches[i].size);
            memcpy(outputSlice, inputSlice, inputSliceSize);
            unsigned int newSliceSize = inputSliceSize+sigSpace[i];
            success = assure_signature_space(outputSlice, sigSpace[i], inputSliceSize, newSliceSize, errorMessage);
            if (!success) {
                success = false;
                goto lb_dealloc;
            }

            // update fat header with new info
            uintptr_t tempOffset = 0;
            if (os_sub_overflow((uintptr_t)outputSlice, (uintptr_t)output, &tempOffset)) {
                log_error(errorMessage, "new architecture offset underflows");
                success = false;
                goto lb_dealloc;
            }

            if (tempOffset > UINT32_MAX) {
                log_error(errorMessage, "new architecture offset is too large");
                success = false;
                goto lb_dealloc;
            }
            unsigned int newOffset = (unsigned int)tempOffset;
            outArches[i].offset = OSSwapHostToBigInt32(newOffset);
            outArches[i].size = OSSwapHostToBigInt32(newSliceSize);
            outArches[i].align = OSSwapHostToBigInt32(14);
            outputSlice = (char*)outputSlice + ((newSliceSize + 0x3FFF) & (-0x4000)); // 16KB align next slice
            if (os_add_overflow(newOffset, newSliceSize, &outputSize)) {
                log_error(errorMessage, "new outputsize overflows: newOffset(%d) newSliceSize(%d)\n", newOffset, newSliceSize);
                success = false;
                goto lb_dealloc;
            }
        }
    } else if ((mh->magic == MH_MAGIC) ||
               (mh->magic == MH_CIGAM) ||
               (mh->magic == MH_MAGIC_64) ||
               (mh->magic == MH_CIGAM_64)) {
        const bool swap = ((mh->magic == MH_CIGAM) || (mh->magic == MH_CIGAM_64));
        // handle non-fat mach-o file
        unsigned int sigSize = getSigSpaceNeeded(get32(swap, mh->cputype),
                                                 get32(swap, mh->cpusubtype));
        if (sigSize == UINT32_MAX) {
            log_error(errorMessage, "requested signature size is too long for slice");
            success = false;
            goto lb_unmap;
        }
        if ((sigSize & 0xF) == 0) {
            // allocate output buffer, worst case size
            if (os_add3_overflow(inputFileLen, sigSize, 15, &outputSize)) {
                log_error(errorMessage, "overflow calculating output size (%u + %d + 15)", inputFileLen, sigSize);
                success = false;
                goto lb_unmap;
            }
            worstCaseOutputSize = outputSize;
            if (!vm_alloc(output, worstCaseOutputSize, errorMessage)) {
                success = false;
                goto lb_unmap;
            }
            // copy to output buffer
            memcpy(output, mh, inputFileLen);
            success = assure_signature_space(output, sigSize, inputFileLen, outputSize, errorMessage);
        } else {
            log_error(errorMessage, "signature size not a multiple of 16\n");
            success = false;
        }
    }

    // write buffer to output file
    if (success) {
        success = writeFile(newOutputFilePath, output, outputSize, errorMessage);
    }

lb_dealloc:
    if (!vm_dealloc(output, worstCaseOutputSize, errorMessage)) {
        success = false;
    }
lb_unmap:
    ::munmap((void*)mappedInputFile, inputFileLen);

    return success;
}




// remove LC_CODE_SIGNATURE and any corresponding LINKEDIT space
static bool remove_signature_space(void* machoFile,
                                   uint32_t inFileSize,
                                   uint32_t& newFileSize,  // on input orginal file size, on output new file size
                                   char*& errorMessage)
{
    // find existing LC_CODE_SIGNATURE
    mach_header* mh = (mach_header*)machoFile;
    const bool swap = ((mh->magic == MH_CIGAM) || (mh->magic == MH_CIGAM_64));
    const bool is64 = ((mh->magic == MH_MAGIC_64) || (mh->magic == MH_CIGAM_64));
    const uint32_t filetype = get32(swap, mh->filetype);
    if ((filetype != MH_EXECUTE) &&
        (filetype != MH_DYLIB) &&
        (filetype != MH_BUNDLE) &&
        (filetype != MH_KEXT_BUNDLE) &&
        (filetype != MH_DYLINKER)) {
        log_error(errorMessage, "mach-o filetype (%d) does not support code signing", filetype);
        return false;
    }
    load_command* const cmds = (load_command*)((char*)mh + (is64 ? sizeof(mach_header_64) : sizeof(mach_header)));
    const uint32_t cmdCount = get32(swap, mh->ncmds);
    const uint32_t cmdsSize = get32(swap, mh->sizeofcmds);
    const load_command* const cmdsEnd = (load_command*)((char*)cmds + cmdsSize);
    linkedit_data_command* sigCmd = NULL;
    segment_command* leSegCmd = NULL;
    segment_command_64* leSegCmd64 = NULL;
    const load_command* cmd = cmds;
    for (uint32_t i = 0; i < cmdCount; ++i) {
        const uint32_t cmdKind = get32(swap, cmd->cmd);
        const uint32_t cmdSize = get32(swap, cmd->cmdsize);
        switch (cmdKind) {
            case LC_CODE_SIGNATURE:
                sigCmd = (linkedit_data_command*)cmd;
                break;
            case LC_SEGMENT:
                if (strcmp(((segment_command*)cmd)->segname, "__LINKEDIT") == 0) {
                    leSegCmd = (segment_command*)cmd;
                }
                break;
            case LC_SEGMENT_64:
                if (strcmp(((segment_command_64*)cmd)->segname, "__LINKEDIT") == 0) {
                    leSegCmd64 = (segment_command_64*)cmd;
                }
                break;
        }
        cmd = (load_command*)((char*)cmd + cmdSize);
        if (cmd > cmdsEnd) {
            log_error(errorMessage, "malformed mach-o file, load command #%d is outside size of load commands\n", i);
            return false;
        }
    }
    // make sure LINKEDIT extends to end of file
    if (leSegCmd != NULL) {
        uint32_t leFileOffsetEnd = 0;
        if (os_add_overflow(get32(swap, leSegCmd->fileoff), get32(swap, leSegCmd->filesize), &leFileOffsetEnd) ||
            inFileSize < leFileOffsetEnd) {
            log_error(errorMessage, "malformed mach-o file, __LINKEDIT segment extends past end of file\n");
            return false;
        }
    } else if (leSegCmd64 != NULL) {
        uint64_t leFileOffsetEnd = 0;
        if (os_add_overflow(get64(swap, leSegCmd64->fileoff), get64(swap, leSegCmd64->filesize), &leFileOffsetEnd) ||
            inFileSize < leFileOffsetEnd) {
            log_error(errorMessage, "malformed mach-o file, __LINKEDIT segment extends past end of file\n");
            return false;
        }
    } else {
        log_error(errorMessage, "malformed mach-o file, no __LINKEDIT segment\n");
        return false;
    }

    // if no existing LC_CODE_SIGNATURE, then nothing to do
    if (sigCmd != NULL) {
        uint32_t sigEndOffset = 0;
        if (os_add_overflow(get32(swap, sigCmd->dataoff), get32(swap, sigCmd->datasize), &sigEndOffset)) {
            log_error(errorMessage, "malformed mach-o file, LC_CODE_SIGNATURE wraps around\n");
            return false;
        }
        // make sure existing load command points to data at end of LINKEDIT
        if (sigEndOffset < (inFileSize-7)) {
            log_error(errorMessage, "malformed mach-o file, LC_CODE_SIGNATURE does not point to end of file\n");
            return false;
        }
        // make sure signature does not overflow existing size
        if (sigEndOffset > newFileSize) {
            log_error(errorMessage, "malformed mach-o file, LC_CODE_SIGNATURE points past the end of the mach-o\n");
            return false;
        }
        // overwrite code signature with zeros
        uint32_t newSize = get32(swap, sigCmd->dataoff);
        bzero((uint8_t*)machoFile + get32(swap, sigCmd->dataoff), get32(swap, sigCmd->datasize));
        // shrink file size
        newFileSize = newSize;
        // zero out load command
        sigCmd->cmd = 0;
        sigCmd->cmdsize = 0;
        sigCmd->dataoff = 0;
        sigCmd->datasize = 0;
        // update mach_header
        mh->ncmds = get32(swap, cmdCount-1);
        mh->sizeofcmds = get32(swap, cmdsSize-sizeof(linkedit_data_command));
        // if not last load command, move ones afterwards
        load_command* const nextCmd = (load_command*)((char*)sigCmd + sizeof(linkedit_data_command));
        if (nextCmd < cmdsEnd) {
            memmove(sigCmd, nextCmd, (char*)cmdsEnd - (char*)nextCmd);
            bzero((char*)cmdsEnd - sizeof(linkedit_data_command), sizeof(linkedit_data_command));
        }
        // update LINKEDIT size
        if (leSegCmd != NULL) {
            leSegCmd->filesize = get32(swap, newFileSize - get32(swap, leSegCmd->fileoff));
        } else {
            leSegCmd64->filesize = get64(swap, newFileSize - get64(swap, leSegCmd64->fileoff));
        }
    }

    return true;
}



//
// Reads from (possibly fat) mach-o file at path 'existingFilePath'
// and writes a new file at path 'newOutputFilePath'.  For each
// mach-o slice in the file, if there is an existing code signature,
// the code signature data in the LINKEDIT is removed along with
// the LC_CODE_SIGNATURE load command.  Returns true on success,
// otherwise returns false and the errorMessage parameter is set
// to a malloced failure messsage.
//
bool code_sign_deallocate(const char* existingFilePath,
                          const char* newOutputFilePath,
                          char*& errorMessage)
{
    errorMessage = NULL;

    // map in inputfile
    const void* mappedInputFile = NULL;
    uint64_t inputFileLen64 = 0;
    uint32_t inputFileLen = 0;
    void* output = NULL;
    vm_size_t worstCaseOutputSize = 0;
    unsigned int outputSize = 0;
    
    bool success = mapFile(existingFilePath, mappedInputFile, inputFileLen64, errorMessage);
    if (!success) {
        return false;
    }
    
    const mach_header* mh = (mach_header*)mappedInputFile;
    
    if (inputFileLen64 > UINT32_MAX) {
        log_error(errorMessage, "input file is too big: %lld\n", inputFileLen64);
        success = false;
        goto lb_unmap;
    }
    inputFileLen = (uint32_t)inputFileLen64;

    // check if fat file
    if (mh->magic == OSSwapBigToHostInt32(FAT_MAGIC)) {
        // gather signature space needed for each slice
        const struct fat_header* inFatHeader = (struct fat_header*)mh;
        const struct fat_arch* inArches = (struct fat_arch*)((char*)mh + sizeof(struct fat_header));
        const int fatCount = OSSwapBigToHostInt32(inFatHeader->nfat_arch);
        // allocate output buffer, worst case size
        worstCaseOutputSize = inputFileLen + (fatCount * 0x4000);
        if (!vm_alloc(output, worstCaseOutputSize, errorMessage)) {
            success = false;
            goto lb_unmap;
        }
        struct fat_header* outFatHeader = (struct fat_header*)output;
        struct fat_arch* outArches = (struct fat_arch*)((char*)output + sizeof(struct fat_header));
        // copy fat header
        *outFatHeader = *inFatHeader;
        // copy each slice
        void* outputSlice = ((char*)output + 0x4000); // 16KB align first slice
        for (unsigned int i = 0; i < fatCount; ++i) {
            outArches[i] = inArches[i];
            const void* inputSlice = (void*)((char*)mh + OSSwapBigToHostInt32(inArches[i].offset));
            unsigned int inputSliceSize = OSSwapBigToHostInt32(inArches[i].size);
            memcpy(outputSlice, inputSlice, inputSliceSize);
            unsigned int newSliceSize = inputSliceSize;
            if (!remove_signature_space(outputSlice, inputSliceSize, newSliceSize, errorMessage)) {
                success = false;
                break;
            }
            // update fat header with new info
            // update fat header with new info
            uintptr_t tempOffset = 0;
            if (os_sub_overflow((uintptr_t)outputSlice, (uintptr_t)output, &tempOffset)) {
                log_error(errorMessage, "new architecture offset underflows");
                success = false;
                break;
            }

            if (tempOffset > UINT32_MAX) {
                log_error(errorMessage, "new architecture offset is too large");
                success = false;
                break;
            }
            unsigned int newOffset = (unsigned int)tempOffset;
            outArches[i].offset = OSSwapHostToBigInt32(newOffset);
            outArches[i].size = OSSwapHostToBigInt32(newSliceSize);
            outArches[i].align = OSSwapHostToBigInt32(14);
            // round up to even page size (16KB align all slices)
            unsigned int newSliceAlignedSize = ((newSliceSize + 0x3FFF) & (-0x4000));
            outputSlice = (char*)outputSlice + newSliceAlignedSize;
            outputSize = newOffset + newSliceSize;
        }
    } else if ((mh->magic == MH_MAGIC) ||
               (mh->magic == MH_CIGAM) ||
               (mh->magic == MH_MAGIC_64) ||
               (mh->magic == MH_CIGAM_64)) {
        // handle non-fat mach-o file
        worstCaseOutputSize = outputSize = inputFileLen;
        if(!vm_alloc(output, worstCaseOutputSize, errorMessage)) {
            success = false;
            goto lb_unmap;
        }
        // copy to output buffer
        memcpy(output, mh, inputFileLen);
        success = remove_signature_space(output, inputFileLen, outputSize, errorMessage);
    }

    // write buffer to output file
    if (success) {
        success = writeFile(newOutputFilePath, output, outputSize, errorMessage);
    }
    if (!vm_dealloc(output, worstCaseOutputSize, errorMessage)) {
        success = false;
    }
lb_unmap:
    ::munmap((void*)mappedInputFile, inputFileLen);

    return success;
}


#if 0
// example usage
int main (int argc, const char* argv[])
{
    char* errMsg = NULL;
    int blobSize = atoi(argv[2]);
    bool success = false;
    if (blobSize <= 0) {
        printf("removing signature space\n");
        success = code_sign_deallocate(argv[1], argv[3], errMsg);
    } else {
        printf("adding signature space\n");
        success = code_sign_allocate(argv[1], argv[3], ^unsigned int(cpu_type_t cputype, cpu_subtype_t cpusubtype) {
            printf ("cputype: %x cpusubtype: %x\n", cputype, cpusubtype);
            return (unsigned int)blobSize;
        }, errMsg);
    }
    printf("codesign_allocate() success=%d, errMsg=%s\n", success, errMsg);
    return 0;
}
#endif

