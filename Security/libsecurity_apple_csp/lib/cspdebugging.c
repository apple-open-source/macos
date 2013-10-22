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


/*
	File:		cspdebugging.c

	Contains:	Debugging support.

	Written by:	Doug Mitchell

	Copyright:	(c) 1998 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first): 

		03/10/98	dpm		Created.

*/

#include "cspdebugging.h"

#if		!LOG_VIA_PRINTF

#include <string.h>
#include <TextUtils.h>

/* common log macros */

/* this one needs a writable string */
static void logCom(unsigned char *str) {
	c2pstr((char *)str);
	DebugStr(str);
}

/* remaining ones can take constant strings */
void dblog0(char *str)	{
	Str255	outStr;
	strcpy((char *)outStr, str);
	logCom(outStr);
}

void dblog1(char *str, void *arg1)	{
	Str255	outStr;
	sprintf((char *)outStr, str, arg1);
	logCom(outStr);
}

void dblog2(char *str, void * arg1, void * arg2)	{
	Str255	outStr;
	sprintf((char *)outStr, str, arg1, arg2);
	logCom(outStr);
}

void dblog3(char *str, void * arg1, void * arg2, void * arg3)	{
	Str255	outStr;
	sprintf((char *)outStr, str, arg1, arg2, arg3);
	logCom(outStr);
}

void dblog4(char *str, void * arg1, void * arg2, void * arg3, void * arg4)	{
	Str255	outStr;
	sprintf((char *)outStr, str, arg1, arg2, arg3, arg4);
	logCom(outStr);
}

#endif	/* !LOG_VIA_PRINTF */

//int foobarSymbol;
