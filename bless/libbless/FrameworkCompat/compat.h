/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  compat.h
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Tue Apr 17 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: compat.h,v 1.1 2001/11/16 05:36:46 ssen Exp $
 *
 *  $Log: compat.h,v $
 *  Revision 1.1  2001/11/16 05:36:46  ssen
 *  Add libbless files
 *
 *  Revision 1.22  2001/11/11 06:19:08  ssen
 *  revert to -pre-libbless
 *
 *  Revision 1.19  2001/10/26 04:19:40  ssen
 *  Add dollar Id and dollar Log
 *
 *
 */

#ifndef NSLOOKUPSYMBOLINIMAGE_OPTION_BIND
#define _BUILD_CHEETAH /* building on Cheetah */
#endif

#ifdef _BUILD_CHEETAH
#define GETFUNCTIONPTR(lib, name) ( NSIsSymbolNameDefinedWithHint(name, lib) \
 ? NSAddressOfSymbol(NSLookupAndBindSymbolWithHint(name, lib)) : NULL)
#else
#define GETFUNCTIONPTR(lib, name) NSAddressOfSymbol(NSLookupSymbolInImage(lib, name, \
    NSLOOKUPSYMBOLINIMAGE_OPTION_BIND|NSLOOKUPSYMBOLINIMAGE_OPTION_RETURN_ON_ERROR))
#endif

