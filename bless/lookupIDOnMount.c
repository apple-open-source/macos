/*
 *  lookupIDOnMount.c
 *  bless
 *
 *  Created by ssen on Thu Apr 19 2001.
 *  Copyright (c) 2001 Apple Computer, Int. All rights reserved.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include <sys/attr.h>

#include "bless.h"

static int lookupIDOnVolID(unsigned long volid, unsigned long fileID, unsigned char out[]);

int lookupIDOnMount(unsigned char mount[], unsigned long fileID, unsigned char out[]) {
    struct attrlist alist;
    struct cataloginforeturn catinfo;
    int err;

    unsigned long volid;
    unsigned char relpath[MAXPATHLEN];

    if(fileID < 2) {
        out[0] = '\0';
        return 0;
    }

    alist.bitmapcount = 5;
    alist.commonattr = ATTR_CMN_NAME | ATTR_CMN_FSID | ATTR_CMN_OBJID | ATTR_CMN_PAROBJID;
    alist.volattr = 0;
    alist.dirattr = 0;
    alist.fileattr = 0;
    alist.forkattr = 0;
    
    if(err = getattrlist(mount, &alist, &catinfo, sizeof(catinfo), 0)) {
        return 1;
    }

    volid = (unsigned long)catinfo.c.volid.val[0];

    err = lookupIDOnVolID(volid, fileID, relpath);
    if(err) {
        return 2;
    }

    if(strcmp(mount, "/")) {
        /* If the mount point is not '/', prefix by mount */
        snprintf(out, MAXPATHLEN, "%s/%s", mount, relpath);
    } else {
        snprintf(out, MAXPATHLEN, "/%s", relpath);
    }

    return 0;
}

static int lookupIDOnVolID(unsigned long volid, unsigned long fileID, unsigned char out[]) {

    unsigned char *bp;

    unsigned long dirID = fileID; /* to initialize loop */
    unsigned char volpath[MAXPATHLEN];

    struct attrlist alist;
    struct cataloginforeturn catinfo;
    int err;

    out[0] = '\0';

    if(fileID <= 2) {
        return 0;
    }

    /* Now for the recursive step
     * 1. getattr on /.vol/volid/dirID
     * 2. get the name.
     * 3. set dirID = parentID
     * 4. go to 1)
     * 5. exit when dirID == 2
    */


    /* bp will hold our current position. Work from the end
     * of the buffer until the beginning */
    bp = &(out[MAXPATHLEN-1]);
    *bp = '\0';

    while(dirID != 2) {
        char *nameptr;
        int namelen;
        sprintf(volpath, "/.vol/%ld/%ld", volid, dirID);
        alist.bitmapcount = 5;
        alist.commonattr = ATTR_CMN_NAME | ATTR_CMN_FSID | ATTR_CMN_OBJID | ATTR_CMN_PAROBJID;
        alist.volattr = 0;
        alist.dirattr = 0;
        alist.fileattr = 0;
        alist.forkattr = 0;
        
        err = getattrlist(volpath, &alist, &catinfo, sizeof(catinfo), 0);
        if (err) {
            return 3;
        }

        dirID = (unsigned long)catinfo.c.parentid.fid_objno;
        nameptr = (char *)(&catinfo.c.name) + catinfo.c.name.attr_dataoffset;
        namelen = strlen(nameptr); /* move bp by this many and copy */
        bp -= namelen;
        strncpy(bp, nameptr, namelen); /* ignore trailing \0 */
        bp--;
        *bp = '/';
    }

    bp++; /* Don't want a '/' prefix, relative path! */
    memmove(out, bp, strlen(bp)+1);
    return 0;
}






