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
 *  BLIsOpenFirmwarePresent.c
 *  bless
 *
 *  Created by Shantonu Sen on Tue Jul 22 2003.
 *  Copyright (c) 2003-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLIsOpenFirmwarePresent.c,v 1.10 2005/07/29 18:28:25 ssen Exp $
 *
 *  $Log: BLIsOpenFirmwarePresent.c,v $
 *  Revision 1.10  2005/07/29 18:28:25  ssen
 *  use new BLGetPreBootEnvironmentType()
 *
 *  Revision 1.9  2005/02/03 00:42:29  ssen
 *  Update copyrights to 2005
 *
 *  Revision 1.8  2005/01/16 02:11:53  ssen
 *  misc changes to support updating booters
 *
 *  Revision 1.7  2004/04/20 21:40:45  ssen
 *  Update copyrights to 2004
 *
 *  Revision 1.6  2003/10/24 22:46:36  ssen
 *  don't try to compare IOKit objects to NULL, since they are really mach port integers
 *
 *  Revision 1.5  2003/08/26 00:39:09  ssen
 *  new minibless target
 *
 *  Revision 1.4  2003/07/25 05:22:21  ssen
 *  add newline to end of file
 *
 *  Revision 1.3  2003/07/25 00:19:10  ssen
 *  add more apsl 2.0 license
 *
 *  Revision 1.2  2003/07/23 18:24:53  ssen
 *  Per Josh, Apple OF will always have IODeviceTree:/openprom
 *
 *  Revision 1.1  2003/07/23 07:29:33  ssen
 *  Add and export BLIsOpenFirmwarePresent. Only try to set OF
 *  via nvram(8) if the machine actually uses Open Firmware
 *
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
