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

#include <netat/appletalk.h>
#include <netat/nbp.h>

#define	SET_ERRNO(e) errno = e

/* Return non-zero if nve string is valid */

static int validate_nvestr (nve, metachar, minlen, metaok)
	at_nvestr_t	nve;
	char		metachar;
	int		minlen, metaok;
{
	u_char		*c;

	/* If it's a metacharacter and metacharacters are allowed, then return ok */
	if ((nve.str[0] == metachar) && (nve.len == 1) && metaok)
		return(1);

 	/* Check size */
	if (nve.len < (unsigned) minlen || nve.len > (unsigned) NBP_NVE_STR_SIZE)
		return (0);
	
	/* Check for illegal characters */
	for (c = nve.str; c < (nve.str + nve.len); c++) {
	    if (((*c == '=' ) || (*c == NBP_SPL_WILDCARD)) && !metaok)
		return (0);
	}

	return (1); 
}


/* Return non-zero if entity is valid */

int _nbp_validate_entity_ (entity, metaok, zoneok)
	at_entity_t	*entity;
	int		metaok;		/* TRUE if metacharacters are ok. */
	int		zoneok;		/* TRUE if zones other than '*' are ok. */
{
	/* Validate names: length and characters */
	if (!validate_nvestr(entity->object, '=', 1, metaok) ||
		!validate_nvestr(entity->type, '=', 1, metaok) ||
		!validate_nvestr(entity->zone, '*', 1, TRUE) ||
		(!zoneok && (entity->zone.len > 1 || entity->zone.str[0] != '*')))
			return (0);

	return (1);
}


/* Make an entity name struct from three NULL-terminated object, type, and
 * zone strings.
 */

int nbp_make_entity (entity, obj, type, zone)
	at_entity_t		*entity;
	char			*obj, *type, *zone;
{
	strncpy((char *) entity->object.str, obj, sizeof(entity->object.str));
	entity->object.len = strlen(obj);

	strncpy((char *) entity->type.str, type, sizeof(entity->type.str));
	entity->type.len = strlen(type);

	strncpy((char *) entity->zone.str, zone, sizeof(entity->zone.str));
	entity->zone.len = strlen(zone);

	/* validate names: length and characters */
	if (!validate_nvestr(entity->object, '=', 1, 1) ||
			!validate_nvestr(entity->type, '=', 1, 1) ||
			!validate_nvestr(entity->zone, '*', 1, 1)) {
		SET_ERRNO(EINVAL);
		return (-1);
	}

	return (0); 
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
	char	*obj, *type, *zone;
	char	buf[NBP_TUPLE_SIZE];

	if ((int) strlen(str) > NBP_TUPLE_SIZE-1) {
		SET_ERRNO(EINVAL);
		return (-1);
	}

	strcpy(buf, str);

	obj = buf;
	if (type = strchr(buf, ':')) {
		/* We have an object name with a type */
		*type++ = '\0';
		if (zone = strchr(type, '@')) 
			*zone++ = '\0';
		else 
			/* No zone, use * as default */
			zone = "*";

	} else {
		/* No type name, must be object only*/
		if (strchr(buf, '@') != NULL) {
			/* can't allow object@zone */
			SET_ERRNO(EINVAL);
			return (-1);
		}
		type = "=";
		zone = "*";
	}

	return(nbp_make_entity(entity, obj, type, zone));
}

