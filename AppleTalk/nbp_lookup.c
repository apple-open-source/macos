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

/* "@(#)nbp_lookup.c: 2.0, 1.7; 9/27/89; Copyright 1988-89, Apple Computer, Inc." */

/*
 * Title:	nbp_lookup.c
 *
 * Facility:	AppleTalk Zone Information Protocol Library Interface
 *
 * Author:	Gregory Burns, Creation Date: Jul-14-1988
 *
 * History:
 * X01-001	Gregory Burns	14-Jul-1988
 *	 	Initial Creation.
 *
 */

#include <string.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/param.h>

#include <netat/appletalk.h>
#include <netat/nbp.h>
#include <netat/zip.h>

#include "at_proto.h"

#define	SET_ERRNO(e) errno = e

static int change_embedded_wildcard(at_entity_t *entity);
static int change_nvestr(at_nvestr_t *nve);

int
nbp_lookup (entity, buf, max, retry)
	at_entity_t	*entity;
	at_nbptuple_t	*buf; 	/* will be filled in with reply */
	int		max;
	at_retry_t	*retry;
{
	int		got;
	at_nvestr_t	this_zone;
	at_entity_t	l_entity;

	/* Make a copy of the entity param so we can freely modify it. */
	l_entity = *entity;

	if (l_entity.zone.len == 1 && l_entity.zone.str[0] == '*') {
		/* looking for the entity in THIS zone.... if this is
		 * an ethertalk iff, then susbstitute by the
		 * real zone name
		 */
		if (zip_getmyzone(ZIP_DEF_INTERFACE, &this_zone) == -1) {
			fprintf(stderr,"nbp_lookup: zip_getmyzone failed\n");
			return (-1);
		}
		l_entity.zone = this_zone;
	}

	if (change_embedded_wildcard(&l_entity) < 0) {
	    SET_ERRNO(EINVAL);
	    fprintf(stderr,"nbp_lookup: change_embedded failed\n");
	    return (-1);
	}

	if ((got = _nbp_send_(NBP_LKUP, NULL, &l_entity, buf, max, retry)) < 0) {
		fprintf(stderr,"nbp_lookup: _nbp_send_ failed got=%d\n", got);
		return (-1);
	}
	return (got);
}	

static	int
change_embedded_wildcard(entity)
at_entity_t	*entity;
{
	if ((entity->object.len > 1) &&
	    !change_nvestr (&entity -> object))
	    return (-1);
	
	if ((entity->type.len > 1) &&
	    !change_nvestr (&entity -> type))
	    return (-1);

	return (0);
}

static int change_nvestr (nve)
at_nvestr_t	*nve;
{
    u_char		*c;
    int		one_meta = 0;
    
    for (c = nve -> str; c < (nve -> str + nve -> len); c++) {
	if ((*c == '=' ) || (*c == NBP_SPL_WILDCARD)) {
	    if (one_meta)
		return (0);
	    else {
		one_meta++;
		*c = NBP_SPL_WILDCARD;
	    }
	}
    }
    return (1); 
}

