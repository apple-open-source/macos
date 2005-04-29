/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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
 *  BLUpdateBooter.c
 *  bless
 *
 *  Created by Shantonu Sen on 2/2/05.
 *  Copyright 2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLUpdateBooter.c,v 1.4 2005/02/23 17:19:32 ssen Exp $
 *
 *  $Log: BLUpdateBooter.c,v $
 *  Revision 1.4  2005/02/23 17:19:32  ssen
 *  initialize variables to 0
 *
 *  Revision 1.3  2005/02/04 13:11:46  ssen
 *  Convert OF label code to using generic booter updating code.
 *
 *  Revision 1.2  2005/02/04 01:43:55  ssen
 *  Move RAID plist code over to common booter update code. Hopefully
 *  this is the last time this code has to be copied.
 *
 *  Revision 1.1  2005/02/03 00:34:09  ssen
 *  start work on generic booter update code
 *
 */

#include <stdlib.h>
#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>

#include "bless.h"
#include "bless_private.h"

int BLUpdateBooter(BLContextPtr context, const unsigned char device[],
				   BLUpdateBooterFileSpec *specs,
				   int32_t specCount)
{
	return 1;	
}

