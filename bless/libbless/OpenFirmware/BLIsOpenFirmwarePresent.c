/*
 * Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
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
 *  BLIsOpenFirmwarePresent.c
 *  bless
 *
 *  Created by Shantonu Sen on Tue Jul 22 2003.
 *  Copyright (c) 2003-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: BLIsOpenFirmwarePresent.c,v 1.11 2006/02/20 22:49:57 ssen Exp $
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>

#include <mach/mach_error.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitKeys.h>

#include <CoreFoundation/CoreFoundation.h>

#include "bless.h"
#include "bless_private.h"

#define kBootRomPath "/openprom"

int BLIsOpenFirmwarePresent(BLContextPtr context) {

  BLPreBootEnvType preboot;
  int ret;

  ret = BLGetPreBootEnvironmentType(context, &preboot);

  // on error, assume no OF
  if(ret) return 0;

  if(preboot == kBLPreBootEnvType_OpenFirmware)
    return 1;
  else
    return 0;

}
