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
 *  BLMiscUtilities.c
 *  bless
 *
 *  Created by Shantonu Sen on Sat Apr 19 2003.
 *  Copyright (c) 2003-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLMiscUtilities.c,v 1.5 2005/02/03 00:42:27 ssen Exp $
 *
 *  $Log: BLMiscUtilities.c,v $
 *  Revision 1.5  2005/02/03 00:42:27  ssen
 *  Update copyrights to 2005
 *
 *  Revision 1.4  2004/05/28 03:42:52  ssen
 *  Add newline to end of file for gcc 3.5
 *
 *  Revision 1.3  2004/04/20 21:40:44  ssen
 *  Update copyrights to 2004
 *
 *  Revision 1.2  2003/07/22 15:58:34  ssen
 *  APSL 2.0
 *
 *  Revision 1.1  2003/04/23 00:08:30  ssen
 *  misc functions, including blostype2string
 *
 */

#include "bless.h"
#include "bless_private.h"

char * blostype2string(uint32_t type, char buf[5])
{
    bzero(buf, sizeof(buf));
    if(type == 0) return buf;

    sprintf(buf, "%c%c%c%c",
	    (type >> 24)&0xFF,
	    (type >> 16)&0xFF,
	    (type >> 8)&0xFF,
	    (type >> 0)&0xFF);

    return buf;    
}
