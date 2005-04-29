/*
 * Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
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
/*
 *  BLIsNewWorld.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Apr 19 2001.
 *  Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLIsNewWorld.c,v 1.8 2005/02/03 00:42:27 ssen Exp $
 *
 *  $Log: BLIsNewWorld.c,v $
 *  Revision 1.8  2005/02/03 00:42:27  ssen
 *  Update copyrights to 2005
 *
 *  Revision 1.7  2004/04/20 21:40:44  ssen
 *  Update copyrights to 2004
 *
 *  Revision 1.6  2003/07/22 15:58:34  ssen
 *  APSL 2.0
 *
 *  Revision 1.5  2003/04/19 00:11:12  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.4  2003/04/16 23:57:33  ssen
 *  Update Copyrights
 *
 *  Revision 1.3  2002/06/11 00:50:49  ssen
 *  All function prototypes need to use BLContextPtr. This is really
 *  a minor change in all of the files.
 *
 *  Revision 1.2  2002/02/23 04:13:06  ssen
 *  Update to context-based API
 *
 *  Revision 1.1  2001/11/16 05:36:47  ssen
 *  Add libbless files
 *
 *  Revision 1.6  2001/11/11 06:20:59  ssen
 *  readding files
 *
 *  Revision 1.4  2001/10/26 04:19:41  ssen
 *  Add dollar Id and dollar Log
 *
 *
 */

#include <sys/param.h>	// for sysctl
#include <sys/sysctl.h>	// for sysctl

#include "bless.h"
#include "bless_private.h"

int BLIsNewWorld(BLContextPtr context)
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
