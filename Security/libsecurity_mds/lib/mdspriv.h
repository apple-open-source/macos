/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
   File:      mdspriv.h

   Contains:  Module Directory Services Data Types and API, private section.

*/

#ifndef _MDSPRIV_H_
#define _MDSPRIV_H_  1

#include <Security/mds.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
	const char *guid;
	uint32 ssid;
	const char *serial;
	const char *printName;
} MDS_InstallDefaults;


/* MDS Context APIs */

CSSM_RETURN CSSMAPI
MDS_InstallFile(MDS_HANDLE inMDSHandle, const MDS_InstallDefaults *defaults,
	const char *bundlePath, const char *subdir, const char *file);

CSSM_RETURN CSSMAPI
MDS_RemoveSubservice(MDS_HANDLE inMDSHandle, const char *guid, uint32 ssid);

#ifdef __cplusplus
}
#endif

#endif /* _MDS_H_ */
