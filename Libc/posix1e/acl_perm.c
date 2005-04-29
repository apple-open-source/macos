/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#include <sys/appleapiopts.h>
#include <sys/types.h>
#include <sys/acl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "aclvar.h"

int
acl_add_perm(acl_permset_t permset, acl_perm_t perm)
{
	/* XXX validate perms */
	_ACL_VALIDATE_PERM(perm);

	permset->ap_perms |= perm;
	return(0);
}

int
acl_clear_perms(acl_permset_t permset)
{
	/* XXX validate perms */

	permset->ap_perms = 0;
	return(0);
}

int
acl_delete_perm(acl_permset_t permset, acl_perm_t perm)
{
	/* XXX validate perms */
	_ACL_VALIDATE_PERM(perm);

	permset->ap_perms &= ~perm;
	return(0);
}

int
acl_get_perm_np(acl_permset_t permset, acl_perm_t perm)
{
	_ACL_VALIDATE_PERM(perm);

	return((perm & permset->ap_perms) ? 1 : 0);
}

int
acl_get_permset(acl_entry_t entry, acl_permset_t *permset_p)
{
	_ACL_VALIDATE_ENTRY(entry);

	*permset_p = (acl_permset_t)&entry->ae_perms;
	return(0);
}

int
acl_set_permset(acl_entry_t entry, acl_permset_t permset)
{
	_ACL_VALIDATE_ENTRY(entry);

	entry->ae_perms = permset->ap_perms;
	return(0);
}
