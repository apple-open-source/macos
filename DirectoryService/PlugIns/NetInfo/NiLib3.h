/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

/*!
 * @header NiLib3
 */

#ifndef __NiLib3_h__
#define __NiLib3_h__

#include <netinfo/ni.h>

class NiLib3 {
public:
	static void			CreateProp		( ni_proplist *l, const ni_name n, const ni_name v );
	static void			AppendProp		( ni_proplist *l, const ni_name n, const ni_name v );
	static int			MergeProp		( ni_proplist *l, const ni_name n, const ni_name v );
	static int			DestroyProp		( ni_proplist *l, const ni_name n );
	static int			DestroyVal		( ni_proplist *l, const ni_name n, const ni_name v );
	static char*		Name			( ni_proplist l );
	static ni_namelist*	FindPropVals	( ni_proplist l, const ni_name n );
	static char*		FindPropVal		( ni_proplist l, const ni_name n, char *out );
};

#endif
