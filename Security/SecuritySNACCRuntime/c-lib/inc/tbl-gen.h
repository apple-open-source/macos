/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


#ifndef TBL_GEN_H
#define TBL_GEN_H

#define USE_GEN_BUF 1
#include "tbl-incl.h"

typedef enum {TDEINFO, TDEEOC=TDEINFO, TDEPEEKTAG, TDEPUSHTAG,
	TDEWARNING, TDEUNEXPECTED=TDEWARNING, TDENONOPTIONAL, TDEMANDATORY,
	TDECONSTRAINT, TDENOMATCH,
	TDEERROR} TdeExceptionCode;

typedef int (*TdeTypeProc) PROTO ((TBLType* type, AVal* val, int begin));
typedef int (*TdeSimpleProc) PROTO ((AsnTag tag, AsnOcts* val, int begin));
typedef int (*TdeExcProc) PROTO ((TdeExceptionCode code, void* p1, void* p2, void* p3));

int
TdeDecode PROTO ((TBL* tbl, BUF_TYPE b, unsigned long int* bytesDecoded,
    	TdeTypeProc typeproc, TdeSimpleProc simpleproc, TdeExcProc excproc));

int
TdeDecodeSpecific PROTO ((TBL* tbl, BUF_TYPE b, TBLType* type,
	unsigned long int* bytesDecoded,
    	TdeTypeProc typeproc, TdeSimpleProc simpleproc, TdeExcProc excproc));
#endif
