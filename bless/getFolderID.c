/*
 *  getFolderID.c
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


int getFolderID(unsigned char path[], UInt32 *folderID) {

    int err;

    struct attrlist blist;
    struct objectinfobuf {
        UInt32 info_length;
        fsobj_id_t dirid;
    } attrbuf;


    // Get System Folder dirID
    blist.bitmapcount = 5;
    blist.reserved = 0;
    blist.commonattr = ATTR_CMN_OBJID;
    blist.volattr = 0;
    blist.dirattr = 0;
    blist.fileattr = 0;
    blist.forkattr = 0;

    if (err = getattrlist(path, &blist, &attrbuf, sizeof(attrbuf), 0)) {
        return 1;
    };

    
    *folderID =(UInt32)attrbuf.dirid.fid_objno;
    return 0;
}

