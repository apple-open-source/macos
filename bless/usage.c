/*
 *  usage.c
 *  bless
 *
 *  Created by ssen on Sun Apr 22 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#include "bless.h"

void usage() {
    int i;
    regularprintf("Usage: "PROGRAM" [options]\n");
    
    for(i=0; i < klast; i++) {
        if(commandlineopts[i].takesarg) {
            regularprintf("   -%-9sarg", commandlineopts[i].flag);
        } else {
            regularprintf("   -%-12s", commandlineopts[i].flag);
        }
        regularprintf("\t%s\n", commandlineopts[i].description);
    }
    exit(1);
}


