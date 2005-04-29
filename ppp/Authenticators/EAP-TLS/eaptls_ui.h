/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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



#ifndef __EAPTLS_UI_H__
#define __EAPTLS_UI_H__


/* type of request from backend to UI */
#define REQUEST_TRUST_EVAL	1

/* type of response from UI to back end */
#define RESPONSE_OK			0
#define RESPONSE_ERROR		1
#define RESPONSE_CANCEL		2


typedef struct eaptls_ui_ctx 
{
    u_int16_t	len;			/* length of this context */
    u_int16_t	id;				/* generation id, to match request/response */
    u_int16_t	request;        /* type of request from backend to UI */
    u_int16_t	response;       /* type of request from backend to UI */
} eaptls_ui_ctx;


int eaptls_ui_load(CFBundleRef bundle, void *logdebug, void *logerror);
void eaptls_ui_dispose();

int eaptls_ui_trusteval(CFDictionaryRef publishedProperties, 
					void *data_in, int data_in_len,
                    void **data_out, int *data_out_len);


#endif