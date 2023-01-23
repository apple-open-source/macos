/*
 * Copyright (c) 2011-2012,2014 Apple Inc. All Rights Reserved.
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

#ifndef _SSLRECORDINTERNAL_H_
#define _SSLRECORDINTERNAL_H_ 1

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <SecureTransport.h>

#include "sslTypes.h"

/* Internal Record layer creation functions, called from the SSLContext layer */

SSLRecordContextRef
SSLCreateInternalRecordLayer(SSLContextRef ctx);

void
SSLDestroyInternalRecordLayer(SSLRecordContextRef ctx);


extern struct SSLRecordFuncs SSLRecordLayerInternal;

#endif
