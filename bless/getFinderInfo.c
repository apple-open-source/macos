/*
 *  getFinderInfo.c
 *  bless
 *
 *  Created by ssen on Thu Apr 19 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/attr.h>

#include "bless.h"

int getFinderInfo(unsigned char mountpoint[], UInt32 words[]) {
    int err, i;
    struct volinfobuf vinfo;
    struct attrlist alist;


    alist.bitmapcount = 5;
    alist.reserved = 0;
    alist.commonattr = ATTR_CMN_FNDRINFO;
    alist.volattr = ATTR_VOL_INFO;
    alist.dirattr = 0;
    alist.fileattr = 0;
    alist.forkattr = 0;
    
    if(err = getattrlist(mountpoint, &alist, &vinfo, sizeof(vinfo), 0)) {
      errorprintf("Can't get volume information for %s\n", mountpoint);
      return 1;
    }

    for(i=0; i<8; i++) {
        words[i] = vinfo.finderinfo[i];
    }

    return 0;
}

int setFinderInfo(unsigned char mountpoint[], UInt32 words[]) {

    struct attrlist alist;
    struct volinfobuf vinfo;
    int err, i;

    alist.bitmapcount = 5;
    alist.reserved = 0;
    alist.commonattr = ATTR_CMN_FNDRINFO;
    alist.volattr = ATTR_VOL_INFO;
    alist.dirattr = 0;
    alist.fileattr = 0;
    alist.forkattr = 0;

    if(err = getattrlist(mountpoint, &alist, &vinfo, sizeof(vinfo), 0)) {
        errorprintf("Can't volume information for %s\n", mountpoint);
        return 1;
    }

    for(i=0; i<8; i++) {
        vinfo.finderinfo[i] = words[i];
    }


    if(err = setattrlist(mountpoint, &alist, &vinfo.finderinfo, sizeof(vinfo.finderinfo), 0)) {
        errorprintf("Error while setting volume information for %s\n", mountpoint);
        return 2;
    }

    return 0;
}

