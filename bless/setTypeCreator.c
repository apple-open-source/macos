/*
 *  setTypeAndCreate.c
 *  bless
 *
 *  Created by ssen on Tue Apr 17 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/attr.h>
#include <sys/stat.h>

#include "bless.h"

int setTypeCreator(unsigned char path[], UInt32 type, UInt32 creator) {
    struct attrlist		alist;
    UInt32			finderInfo[8] = {0};
    int err;

    finderInfo[0] = type;
    finderInfo[1] = creator;
	
    alist.bitmapcount = 5;
    alist.reserved = 0;
    alist.commonattr = ATTR_CMN_FNDRINFO;
    alist.volattr = 0;
    alist.dirattr = 0;
    alist.fileattr = 0;
    alist.forkattr = 0;

    err = setattrlist(path, &alist, &finderInfo, sizeof(finderInfo), 0);
    return err;
}

