/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <string.h>

#include "compatCarbon.h"
#include "bless.h"

#define SYSTEM "\pSystem"

static int getBootBlocksFromFSSpec(FSSpec systemFile, unsigned char bootBlocks[]) {
    SInt16                          fRefNum;
    Handle                          bbH;
    int err = 0;

    bzero(bootBlocks, 1024);
        
    fRefNum = _BLFSpOpenResFile(&systemFile, fsRdPerm);
    if (fRefNum == -1) {
        return 3;
    }

    bbH = _BLGet1Resource('boot', 1);
    if (!bbH) {
        return 4;
    }

    _BLDetachResource(bbH);
    memcpy(bootBlocks, *bbH, 1024);
    _BLDisposeHandle(bbH);
    _BLCloseResFile(fRefNum);
 
    return err;
}

int BLGetBootBlocksFromFolder(BLContext context, unsigned char mountpoint[], u_int32_t dir9, unsigned char bootBlocks[]) {

    FSSpec                          spec, systemFile;
    int err;

	err = _BLNativePathNameToFSSpec(mountpoint, &spec, 0);
     if(err) {
       return 1;
    }
    
    err = _BLFSMakeFSSpec(spec.vRefNum, dir9,  SYSTEM , &systemFile);
    if(err) {
        return 2;
    }

    err = getBootBlocksFromFSSpec(systemFile, bootBlocks);
    if(err) {
        return err;
    }
    
    return 0;
}

int BLGetBootBlocksFromFile(BLContext context, unsigned char file[], unsigned char bootBlocks[]) {

    FSSpec                          spec;
    int err;

	err = _BLNativePathNameToFSSpec(file, &spec, 0);
     if(err) {
       return 1;
    }
    
    err = getBootBlocksFromFSSpec(spec, bootBlocks);
    if(err) {
        return err;
    }
    
    return 0;
}

int BLGetBootBlocksFromDataForkFile(BLContext context, unsigned char file[], unsigned char bootBlocks[]) {

    int fd;
    int err;

    fd = open(file, O_RDONLY, 0);

     if(fd == -1) {
       return 1;
    }
    
    err = read(fd, bootBlocks, 1024);
    
    if(err != 1024) {
      close(fd);
        return 2;
    }
    
    close(fd);

    return 0;
}
