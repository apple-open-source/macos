/*
 *  createBootX.c
 *  bless
 *
 *  Created by ssen on Tue Apr 17 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>


#include "bless.h"
#define READBUFSIZE 512

int createBootX(unsigned char bootXsrc[],  unsigned char folder[]) {

    unsigned char bootXpath[MAXPATHLEN];
    unsigned char buffer[READBUFSIZE];

    int err;
    int fdw, fdr;
    int bytesread;

    if(*bootXsrc == '\0') {
        /* No BootX creation asked for */
        return 0;
    }

    snprintf(bootXpath, MAXPATHLEN-1, "%s/%s", folder, BOOTX);
    bootXpath[MAXPATHLEN-1] = '\0';
    
    /* Read in the BootX file and write it out */

    fdr = open(bootXsrc, O_RDONLY, 0);

    if(fdr == -1) {
        errorprintf("Error while opening %s for reading\n", bootXsrc);
        return 1;
    } else {
        verboseprintf("BootX source %s opened for reading\n", bootXsrc);
    }
    
    fdw = open(bootXpath, O_WRONLY|O_CREAT|O_TRUNC,
                S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

    if(fdw == -1) {
        errorprintf("Error while opening %s for writing\n", bootXpath);
        close(fdr);
        return 2;
    } else {
        verboseprintf("BootX at %s opened for writing\n", bootXpath);
    }

    verboseprintf("Beginning to copy data:\n");

    while((bytesread = read(fdr, buffer, READBUFSIZE)) > 0) {
        verboseprintf(".");
        if(write(fdw, buffer, bytesread) == -1) {
            errorprintf("Error while writing to %s\n", bootXpath);
            close(fdr);
            close(fdw);
            return 3;
        }
    }
    verboseprintf("\n");

    close(fdr);
    close(fdw);
    
    if(err = setTypeCreator(bootXpath, 'tbxi', 'chrp')) {
        errorprintf("Error while setting type/creator for %s\n", bootXpath);
        return 4;
    } else {
        verboseprintf("Type/creator set for %s\n", bootXpath);
    }

    return 0;
}
