/*
 * Copyright (c) 2012 Apple Inc. All rights reserved.
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

#ifndef _SMBFS_ATTRLIST_H_
#define _SMBFS_ATTRLIST_H_

#include <sys/appleapiopts.h>

#ifdef KERNEL
#ifdef __APPLE_API_PRIVATE
#include <sys/attr.h>
#include <sys/vnode.h>

#include <smbfs/smbfs_node.h>


struct attrblock {
	struct attrlist *ab_attrlist;
	void **ab_attrbufpp;
	void **ab_varbufpp;
	int	ab_flags;
	int	ab_blocksize;
	vfs_context_t ab_context;
};

/* 
 * The following define the attributes that HFS supports:
 */

#define SMBFS_ATTR_CMN_VALID				\
	(ATTR_CMN_NAME | ATTR_CMN_DEVID	|		\
	 ATTR_CMN_FSID | ATTR_CMN_OBJTYPE |		\
	 ATTR_CMN_OBJTAG | ATTR_CMN_OBJID |		\
	 ATTR_CMN_OBJPERMANENTID | ATTR_CMN_PAROBJID |	\
	 ATTR_CMN_SCRIPT | ATTR_CMN_CRTIME |		\
	 ATTR_CMN_MODTIME | ATTR_CMN_CHGTIME |		\
	 ATTR_CMN_ACCTIME | ATTR_CMN_BKUPTIME |		\
	 ATTR_CMN_FNDRINFO |ATTR_CMN_OWNERID |		\
	 ATTR_CMN_GRPID | ATTR_CMN_ACCESSMASK |		\
	 ATTR_CMN_FLAGS | ATTR_CMN_USERACCESS |		\
	 ATTR_CMN_FILEID | ATTR_CMN_PARENTID )

#define SMBFS_ATTR_CMN_SEARCH_VALID	\
	(ATTR_CMN_NAME | ATTR_CMN_OBJID |	\
	 ATTR_CMN_PAROBJID | ATTR_CMN_CRTIME |	\
	 ATTR_CMN_MODTIME | ATTR_CMN_CHGTIME | 	\
	 ATTR_CMN_ACCTIME | ATTR_CMN_BKUPTIME |	\
	 ATTR_CMN_FNDRINFO | ATTR_CMN_OWNERID |	\
	 ATTR_CMN_GRPID	| ATTR_CMN_ACCESSMASK | \
	 ATTR_CMN_FILEID | ATTR_CMN_PARENTID ) 



#define SMBFS_ATTR_DIR_VALID				\
	(ATTR_DIR_LINKCOUNT | ATTR_DIR_ENTRYCOUNT | ATTR_DIR_MOUNTSTATUS)

#define SMBFS_ATTR_DIR_SEARCH_VALID	\
	(ATTR_DIR_ENTRYCOUNT)

#define SMBFS_ATTR_FILE_VALID				  \
	(ATTR_FILE_LINKCOUNT |ATTR_FILE_TOTALSIZE |	  \
	 ATTR_FILE_ALLOCSIZE | ATTR_FILE_IOBLOCKSIZE |	  \
	 ATTR_FILE_CLUMPSIZE | ATTR_FILE_DEVTYPE |	  \
	 ATTR_FILE_DATALENGTH | ATTR_FILE_DATAALLOCSIZE | \
	 ATTR_FILE_RSRCLENGTH | ATTR_FILE_RSRCALLOCSIZE)

#define SMBFS_ATTR_FILE_SEARCH_VALID		\
	(ATTR_FILE_DATALENGTH | ATTR_FILE_DATAALLOCSIZE |	\
	 ATTR_FILE_RSRCLENGTH | ATTR_FILE_RSRCALLOCSIZE )

int smbfs_vnop_readdirattr(struct vnop_readdirattr_args *ap);

#endif /* __APPLE_API_PRIVATE */
#endif /* KERNEL */
#endif /* ! _SMBFS_ATTRLIST_H_ */
