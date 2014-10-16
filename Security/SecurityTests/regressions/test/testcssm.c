/*
 * Copyright (c) 2005 Apple Computer, Inc. All Rights Reserved.
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
 *
 * cssmlib.c
 */

#include <stdlib.h>
#include <Security/cssmapi.h>
#include <Security/cssmapple.h>

#include "testcssm.h"
#include "testmore.h"

const CSSM_API_MEMORY_FUNCS gMemFuncs =
{
	(CSSM_MALLOC)malloc,
	(CSSM_FREE)free,
	(CSSM_REALLOC)realloc,
	(CSSM_CALLOC)calloc,
	NULL /* context */
};

int
cssm_attach(const CSSM_GUID *guid, CSSM_HANDLE *handle)
{
	setup("cssm_attach");

	CSSM_VERSION version = {2, 0};
	CSSM_PVC_MODE pvcPolicy = CSSM_PVC_NONE;
	return (ok_status(CSSM_Init(&version, CSSM_PRIVILEGE_SCOPE_NONE,
			&gGuidCssm, CSSM_KEY_HIERARCHY_NONE, &pvcPolicy,
			NULL /* reserved */), "CSSM_Init") &&
		ok_status(CSSM_ModuleLoad(guid, CSSM_KEY_HIERARCHY_NONE, NULL, NULL),
			"CSSM_ModuleLoad") &&
		ok_status(CSSM_ModuleAttach(guid, &version, &gMemFuncs,
			0 /* SubserviceID */, CSSM_SERVICE_DL, 0 /* CSSM_ATTACH_FLAGS */,
			CSSM_KEY_HIERARCHY_NONE, NULL, 0, NULL, handle),
			"CSSM_ModuleAttach"));
}

int
cssm_detach(const CSSM_GUID *guid, CSSM_HANDLE handle)
{
	setup("cssm_detach");

	return ok_status(CSSM_ModuleDetach(handle), "CSSM_ModuleDetach") &&
		ok_status(CSSM_ModuleUnload(guid, NULL, NULL), "CSSM_ModuleUnload") &&
		ok_status(CSSM_Terminate(), "CSSM_Terminate");
}
