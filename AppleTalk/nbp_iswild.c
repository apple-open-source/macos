/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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

/*
 *	Copyright (c) 1988, 1989 Apple Computer, Inc. 
 *
 *	The information contained herein is subject to change without
 *	notice and  should not be  construed as a commitment by Apple
 *	Computer, Inc. Apple Computer, Inc. assumes no responsibility
 *	for any errors that may appear.
 *
 *	Confidential and Proprietary to Apple Computer, Inc.
 */


#include <unistd.h>
#include <string.h>

#include <netat/appletalk.h>
#include <netat/nbp.h>

int	nbp_iswild (entity)
at_entity_t	*entity;
{

	if (entity->object.len < NBP_NVE_STR_SIZE)
		entity->object.str[entity->object.len] = '\0';
	if (entity->type.len < NBP_NVE_STR_SIZE)
		entity->type.str[entity->type.len] = '\0';
	if (	strchr(entity->object.str, '=')			|| 
		strchr(entity->object.str, NBP_SPL_WILDCARD)	||
		strchr(entity->type.str, '=')			||
		strchr(entity->type.str, NBP_SPL_WILDCARD)
	)
		return (1);
	else
		return (0);
}
