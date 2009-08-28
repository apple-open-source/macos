/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *	Copyright (c) 1988, 1989, 1998 Apple Computer, Inc. 
 *
 *	The information contained herein is subject to change without
 *	notice and  should not be  construed as a commitment by Apple
 *	Computer, Inc. Apple Computer, Inc. assumes no responsibility
 *	for any errors that may appear.
 *
 *	Confidential and Proprietary to Apple Computer, Inc.
 *
 */

/* "@(#)nbp_parse.c: 2.0, 1.10; 12/12/90; Copyright 1988-89, Apple Computer, Inc." */

/*
 * Title:	nbp_parse.c
 *
 * Facility:	AppleTalk Name Binding Protocol Library Interface
 *
 * Author:	Gregory Burns, Creation Date: Jun-24-1988
 *
 * History:
 * X01-001	Gregory Burns	24-Jun-1988
 *	 	Initial Creation.
 *
 */

#include <ctype.h>
#include <mach/boolean.h>
#include <string.h>
#include <sys/errno.h>

#include "at_proto.h"

#define	SET_ERRNO(e) errno = e



/* Make an entity name struct from three NULL-terminated object, type, and
 * zone strings.
 */

int nbp_make_entity (entity, obj, type, zone)
	at_entity_t		*entity;
	char			*obj, *type, *zone;
{
	SET_ERRNO(ENXIO);
	return (-1);
}


/* Build an entity name struct from a string of the format:
 *	object
 *	object:type
 *	object:type@zone
 */

int nbp_parse_entity (entity, str)
	at_entity_t	*entity;
	char		*str;
{
	SET_ERRNO(ENXIO);
	return (-1);
}

