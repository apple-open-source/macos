/*
 *  getNewWorld.c
 *  bless
 *
 *  Created by ssen on Thu Apr 19 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#include <sys/param.h>	// for sysctl
#include <sys/sysctl.h>	// for sysctl

int getNewWorld()
{
    int mib[2], nw;
    size_t len;

    // do the lookup
    mib[0] = CTL_HW;
    mib[1] = HW_EPOCH;
    len = sizeof(nw);
    sysctl(mib, 2, &nw, &len, NULL, 0);

    return nw;
}
