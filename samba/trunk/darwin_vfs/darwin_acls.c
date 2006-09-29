/* 
 * Darwin ACL VFS module
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *  
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "includes.h"
#include <membership.h>

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_VFS

BOOL acl_support_enabled(char *path)
{
	int error;
	struct attrlist alist;
	struct dirReplyBlock {
		unsigned long   length;
		struct vol_capabilities_attr volattr;
	} reply;
	struct statfs volstat;
	
	if (statfs(path, &volstat) != 0) {
		DEBUG(4,("acl_support_enabled: statfs errno (%d) strerror(%s)\n", errno, strerror(errno)));
		return False;
	}
	
	memset( &alist, 0, sizeof(alist));
	memset( &reply, 0, sizeof(reply));

	alist.bitmapcount = ATTR_BIT_MAP_COUNT;
	alist.volattr = ATTR_VOL_INFO | ATTR_VOL_CAPABILITIES;
	
	error = getattrlist( volstat.f_mntonname, &alist, &reply, sizeof(reply), 0);
	
	if ( error == 0 ) {
		
		if (reply.volattr.valid[VOL_CAPABILITIES_INTERFACES] & VOL_CAP_INT_EXTENDED_SECURITY
			&& reply.volattr.capabilities[VOL_CAPABILITIES_INTERFACES] & VOL_CAP_INT_EXTENDED_SECURITY) {
				DEBUG(4,("acl_support_enabled: volume(%s)\n", volstat.f_mntonname));
				return True;
			}
	} else {
		DEBUG(4,("acl_support_enabled: getattrlist errno (%d) strerror(%s)\n", errno, strerror(errno)));
	}
	return False;				
}

/****************************************************************************
 Unpack a SEC_DESC into a UNIX owner and group.
****************************************************************************/

static BOOL darwin_unpack_nt_owners(SMB_STRUCT_STAT *psbuf, uid_t *puser, gid_t *pgrp, uint32 security_info_sent, SEC_DESC *psd)
{
	DOM_SID owner_sid;
	DOM_SID grp_sid;

	*puser = (uid_t)99;
	*pgrp = (gid_t)99;

	if(security_info_sent == 0) {
		DEBUG(0,("unpack_nt_owners: no security info sent !\n"));
		return True;
	}

	/*
	 * Validate the owner and group SID's.
	 */

	memset(&owner_sid, '\0', sizeof(owner_sid));
	memset(&grp_sid, '\0', sizeof(grp_sid));

	DEBUG(5,("unpack_nt_owners: validating owner_sids.\n"));

	/*
	 * Don't immediately fail if the owner sid cannot be validated.
	 * This may be a group chown only set.
	 */

	if (security_info_sent & OWNER_SECURITY_INFORMATION) {
		sid_copy(&owner_sid, psd->owner_sid);
		if (!NT_STATUS_IS_OK(sid_to_uid(&owner_sid, puser))) {
#if ACL_FORCE_UNMAPPABLE
			/* this allows take ownership to work reasonably */
			extern struct current_user current_user;
			*puser = current_user.uid;
#else
			DEBUG(3,("unpack_nt_owners: unable to validate owner sid for %s\n",
				 sid_string_static(&owner_sid)));
			return False;
#endif
		}
 	}

	/*
	 * Don't immediately fail if the group sid cannot be validated.
	 * This may be an owner chown only set.
	 */

	if (security_info_sent & GROUP_SECURITY_INFORMATION) {
		sid_copy(&grp_sid, psd->grp_sid);
		if (!NT_STATUS_IS_OK(sid_to_gid( &grp_sid, pgrp))) {
#if ACL_FORCE_UNMAPPABLE
			/* this allows take group ownership to work reasonably */
			extern struct current_user current_user;
			*pgrp = current_user.gid;
#else
			DEBUG(3,("unpack_nt_owners: unable to validate group sid.\n"));
			return False;
#endif
		}
	}

	DEBUG(5,("unpack_nt_owners: owner_sids validated.\n"));

	return True;
}


/****************************************************************************
 Try to chown a file. We will be able to chown it under the following conditions.

  1) If we have root privileges, then it will just work.
  2) If we have write permission to the file and dos_filemodes is set
     then allow chown to the currently authenticated user.
****************************************************************************/

static int darwin_try_chown(connection_struct *conn, const char *fname, uid_t uid, gid_t gid)
{
	int ret;
	extern struct current_user current_user;
	files_struct *fsp;
	SMB_STRUCT_STAT st;

	/* try the direct way first */
	ret = SMB_VFS_CHOWN(conn, fname, uid, gid);
	if (ret == 0)
		return 0;

	if(!CAN_WRITE(conn) || !lp_dos_filemode(SNUM(conn)))
		return -1;

	if (SMB_VFS_STAT(conn,fname,&st))
		return -1;

	fsp = open_file_fchmod(conn,fname,&st);
	if (!fsp)
		return -1;

	/* only allow chown to the current user. This is more secure,
	   and also copes with the case where the SID in a take ownership ACL is
	   a local SID on the users workstation 
	*/
	uid = current_user.uid;

	become_root();
	/* Keep the current file gid the same. */
	ret = SMB_VFS_FCHOWN(fsp, fsp->fd, uid, (gid_t)-1);
	unbecome_root();

	close_file_fchmod(fsp);

	return ret;
}

int map_darwinaclflags_to_ntaclflags (acl_flagset_t flags)
{
	int ntflags = 0;
	
	/* SEC_ACE_FLAG_VALID_INHERIT - ??? - AUDIT ACE FLAG */
	
	if (acl_get_flag_np(flags, ACL_FLAG_DEFER_INHERIT) == 1)
		DEBUG(0,("map_darwinacltype_to_ntacltype: !!!! ACL_FLAG_DEFER_INHERIT !!!!\n"));

	if (acl_get_flag_np(flags, ACL_ENTRY_INHERITED) == 1)
		ntflags |= SEC_ACE_FLAG_INHERITED_ACE; /* W2k */

	if (acl_get_flag_np(flags, ACL_ENTRY_FILE_INHERIT) == 1)
		ntflags |= SEC_ACE_FLAG_OBJECT_INHERIT;

	if (acl_get_flag_np(flags, ACL_ENTRY_DIRECTORY_INHERIT) == 1)
		ntflags |= SEC_ACE_FLAG_CONTAINER_INHERIT;

	if (acl_get_flag_np(flags, ACL_ENTRY_LIMIT_INHERIT) == 1)
		ntflags |= SEC_ACE_FLAG_NO_PROPAGATE_INHERIT; /* ??? */

	if (acl_get_flag_np(flags, ACL_ENTRY_ONLY_INHERIT) == 1)
		ntflags |= SEC_ACE_FLAG_INHERIT_ONLY;
	
	return ntflags;
}

int map_darwinaclperms_to_ntaclperms (acl_permset_t	perms)
{
	uint32 ntperms = 0;

	if (acl_get_perm_np(perms, ACL_READ_DATA) == 1) { /* ACL_LIST_DIRECTORY */
			ntperms |=  FILE_READ_DATA;
			DEBUG(4,("map_darwinaclperms_to_ntaclperms:  [acl_get_perm_np ACL_READ_DATA/ACL_LIST_DIRECTORY ] FILE_READ_DATA/FILE_LIST_DIRECTORY \n"));
	}

	if (acl_get_perm_np(perms, ACL_WRITE_DATA) == 1) { /* ACL_ADD_FILE */
			ntperms |=  FILE_WRITE_DATA;
			DEBUG(4,("map_darwinaclperms_to_ntaclperms:  [acl_get_perm_np ACL_WRITE_DATA/ACL_ADD_FILE] FILE_WRITE_DATA/FILE_ADD_FILE\n"));
	}

	if (acl_get_perm_np(perms, ACL_EXECUTE) == 1) { /* ACL_SEARCH */
			ntperms |=  FILE_EXECUTE;
			DEBUG(4,("map_darwinaclperms_to_ntaclperms:  [acl_get_perm_np ACL_EXECUTE/ACL_SEARCH] FILE_EXECUTE/FILE_TRAVERSE\n"));
	}

	if (acl_get_perm_np(perms, ACL_DELETE) == 1) {
			ntperms |=  STD_RIGHT_DELETE_ACCESS;
			DEBUG(4,("map_darwinaclperms_to_ntaclperms:  [acl_get_perm_np ACL_DELETE] STD_RIGHT_DELETE_ACCESS\n"));
	}

	if (acl_get_perm_np(perms, ACL_APPEND_DATA) == 1) { /* ACL_ADD_SUBDIRECTORY */
			ntperms |=  FILE_APPEND_DATA;
			DEBUG(4,("map_darwinaclperms_to_ntaclperms:  [acl_get_perm_np ACL_APPEND_DATA/ACL_ADD_SUBDIRECTORY] FILE_APPEND_DATA/SEC_RIGHTS_CREATE_CHILD\n"));
	}

	if (acl_get_perm_np(perms, ACL_DELETE_CHILD) == 1) {
			ntperms |=  FILE_DELETE_CHILD;
			DEBUG(4,("map_darwinaclperms_to_ntaclperms:  [acl_get_perm_np ACL_DELETE_CHILD] FILE_DELETE_CHILD\n"));
	}

	if (acl_get_perm_np(perms, ACL_READ_ATTRIBUTES) == 1) {
			ntperms |=  FILE_READ_ATTRIBUTES;
			DEBUG(4,("map_darwinaclperms_to_ntaclperms:  [acl_get_perm_np ACL_READ_ATTRIBUTES] FILE_READ_ATTRIBUTES\n"));
	}

	if (acl_get_perm_np(perms, ACL_READ_EXTATTRIBUTES) == 1) {
			ntperms |=  FILE_READ_EA;
			DEBUG(4,("map_darwinaclperms_to_ntaclperms:  [acl_get_perm_np ACL_READ_EXTATTRIBUTES] FILE_READ_EA\n"));
	}

	if (acl_get_perm_np(perms, ACL_WRITE_ATTRIBUTES) == 1) {
			ntperms |=  FILE_WRITE_ATTRIBUTES;
			DEBUG(4,("map_darwinaclperms_to_ntaclperms:  [acl_get_perm_np ACL_WRITE_ATTRIBUTES] FILE_WRITE_ATTRIBUTES\n"));
	}

	if (acl_get_perm_np(perms, ACL_WRITE_EXTATTRIBUTES) == 1) {
			ntperms |=  FILE_WRITE_EA;
			DEBUG(4,("map_darwinaclperms_to_ntaclperms:  [acl_get_perm_np ACL_WRITE_EXTATTRIBUTES] FILE_WRITE_EA\n"));
	}

	if (acl_get_perm_np(perms, ACL_READ_SECURITY) == 1) {
			ntperms |=  READ_CONTROL_ACCESS;
			DEBUG(4,("map_darwinaclperms_to_ntaclperms:  [acl_get_perm_np ACL_READ_SECURITY] READ_CONTROL_ACCESS\n"));
	}

	if (acl_get_perm_np(perms, ACL_WRITE_SECURITY) == 1) {
			ntperms |=  WRITE_DAC_ACCESS;
			DEBUG(4,("map_darwinaclperms_to_ntaclperms:  [acl_get_perm_np ACL_WRITE_SECURITY] WRITE_DAC_ACCESS\n"));
	}

	if (acl_get_perm_np(perms, ACL_CHANGE_OWNER) == 1) {
			ntperms |=  WRITE_OWNER_ACCESS;
			DEBUG(4,("map_darwinaclperms_to_ntaclperms:  [acl_get_perm_np ACL_CHANGE_OWNER] WRITE_OWNER_ACCESS\n"));
	}

//	if (acl_get_perm_np(perms, KAUTH_VNODE_SYNCHRONIZE) == 1)
			ntperms |=  SYNCHRONIZE_ACCESS;

	DEBUG(4,("map_darwinaclperms_to_ntaclperms: ntperms(%x)\n", ntperms));
	
	return ntperms;
}

int map_darwinace_to_ntace(acl_tag_t tag_type)
{
	switch(tag_type)
	{
		case ACL_EXTENDED_ALLOW:
			return SEC_ACE_TYPE_ACCESS_ALLOWED;
			break;
		case ACL_EXTENDED_DENY:
			return SEC_ACE_TYPE_ACCESS_DENIED;
			break;
		case ACL_UNDEFINED_TAG:
		default:
			DEBUG(0,("map_darwinace_to_ntace: !!!! ACL_UNDEFINED_TAG !!!!\n"));
			return SEC_ACE_TYPE_ACCESS_DENIED;
			break;
	}
}

int uuid_to_sid(uuid_t *qualifier, DOM_SID *sid)
{
	int result = -1;
	int id_type = -1;
	uid_t id = 99; /* unknown */
	char uustr[40];
	
	uuid_unparse(*qualifier, uustr);
	DEBUG(4,("uuid_to_sid: uuid_unparse: (%s) \n",uustr));
	
	if ((result = mbr_uuid_to_id( *qualifier, &id, &id_type)) == -1)
		DEBUG(0,("uuid_to_sid: [%d]mbr_uuid_to_id: (%s) \n",errno, strerror(errno) ));
	else {
		DEBUG(4,("uuid_to_sid: mbr_uuid_to_id: uuid(%s) id(%d) type(%s)\n",uustr, id, id_type ==  ID_TYPE_UID ? "USER": "GROUP"));
		if (id_type == ID_TYPE_UID) {
			if (!NT_STATUS_IS_OK(uid_to_sid( sid, id )))
				result = -1;
		} else if (id_type == ID_TYPE_GID) {
			if (!NT_STATUS_IS_OK(gid_to_sid( sid, id )))
				result = -1;
		} else {
			DEBUG(0,("uuid_to_sid: !!! INVALID ID_TYPE(%d) !!!  :ID(%d)\n", id_type, id));
			result = -1;
		}
	}
	return result;
}

static int unix_perms_to_acl_perms(mode_t mode, int r_mask, int w_mask, int x_mask)
{
	int ret = 0;

	if (mode & r_mask)
		ret |= FILE_GENERIC_READ;
	if (mode & w_mask)
		ret |= FILE_GENERIC_WRITE;
	if (mode & x_mask)
		ret |= FILE_GENERIC_EXECUTE;

	return ret;
}

int map_darwinacl_to_ntacl(filesec_t fsect, SEC_ACE *nt_ace_list, BOOL acl_support)
{
	acl_t darwin_acl = NULL;
	acl_entry_t  entry = NULL;
	acl_tag_t tag_type; /* ACL_EXTENDED_ALLOW | ACL_EXTENDED_DENY */
	acl_flagset_t	flags; /* inheritance bits */
	acl_permset_t	perms;
	uuid_t *qualifier = NULL; /* user or group */
	extern DOM_SID global_sid_World;    /* Everyone */
	DOM_SID owner_sid;
	DOM_SID group_sid;
	DOM_SID sid;
	uid_t uid = 99;
	gid_t gid = 99;
	mode_t mode = 0;
	int acl_perms = 0;
	SEC_ACCESS acc = {};
	
	int num_aces = 0;
	
	if (acl_support) {
		if (filesec_get_property(fsect, FILESEC_ACL, &darwin_acl) == -1) {
			DEBUG(3,("map_darwinacl_to_ntacl: filesec_get_property - FILESEC_ACL: errno(%d) - (%s)\n",errno, strerror(errno)));
		}
				
		for (num_aces = 0; (darwin_acl != NULL) && (acl_get_entry(darwin_acl, entry == NULL ? ACL_FIRST_ENTRY : ACL_NEXT_ENTRY, &entry) == 0); num_aces++)
		{
			if ((qualifier = acl_get_qualifier(entry)) == NULL)
				continue;
			if (acl_get_tag_type(entry, &tag_type) != 0)
				continue;
			if (acl_get_flagset_np(entry, &flags) != 0)
				continue;
			if (acl_get_permset(entry, &perms) != 0)
				continue;
	
			if (uuid_to_sid(qualifier, &sid) == -1) {
				acl_free(qualifier);
				continue;
			} else {
				acl_free(qualifier);		
			}
					
			init_sec_access(&acc,map_darwinaclperms_to_ntaclperms(perms));
	
	
			DEBUG(4,("map_darwinacl_to_ntacl: acc.mask(%X) tag_type(%x), flags(%X)\n", acc.mask, tag_type, flags ));
			
			init_sec_ace(&nt_ace_list[num_aces], &sid, map_darwinace_to_ntace(tag_type), acc, map_darwinaclflags_to_ntaclflags(flags));
	
		} 
	}
	
	if (filesec_get_property(fsect, FILESEC_OWNER, &uid) == -1) {
		DEBUG(0,("filesec_get_property - FILESEC_OWNER: errno(%d) - (%s)\n",errno, strerror(errno)));
	} else {
		DEBUG(4,("FILESEC_OWNER[%d]\n",uid));
		uid_to_sid( &owner_sid, uid );
	}

	if (filesec_get_property(fsect, FILESEC_GROUP, &gid) == -1) {
		DEBUG(0,("filesec_get_property - FILESEC_OWNER: errno(%d) - (%s)\n",errno, strerror(errno)));
	} else {
		DEBUG(4,("FILESEC_GROUP[%d]\n",gid));
		gid_to_sid( &group_sid, gid );
	}

	if (filesec_get_property(fsect, FILESEC_MODE, &mode) == -1) {
		DEBUG(0,("filesec_get_property - FILESEC_MODE: errno(%d) - (%s)\n",errno, strerror(errno)));
	} else {
		DEBUG(4,("FILESEC_MODE[%x]\n",mode));
	}
	
	// user
	acl_perms = unix_perms_to_acl_perms(mode, S_IRUSR, S_IWUSR, S_IXUSR);
	if (acl_perms) {
		init_sec_access(&acc, acl_perms);
		init_sec_ace(&nt_ace_list[num_aces++], &owner_sid, SEC_ACE_TYPE_ACCESS_ALLOWED, acc, acl_support ? SEC_ACE_FLAG_INHERITED_ACE | SEC_ACE_FLAG_NO_PROPAGATE_INHERIT: 0);
	}
	// group
	acl_perms = unix_perms_to_acl_perms(mode, S_IRGRP, S_IWGRP, S_IXGRP);
	if (acl_perms) {
		init_sec_access(&acc, acl_perms);
		init_sec_ace(&nt_ace_list[num_aces++], &group_sid, SEC_ACE_TYPE_ACCESS_ALLOWED, acc, acl_support ? SEC_ACE_FLAG_INHERITED_ACE | SEC_ACE_FLAG_NO_PROPAGATE_INHERIT: 0);
	}
	// everyone
	acl_perms = unix_perms_to_acl_perms(mode, S_IROTH, S_IWOTH, S_IXOTH);
	if (acl_perms) {
		init_sec_access(&acc, acl_perms);
		init_sec_ace(&nt_ace_list[num_aces++], &global_sid_World, SEC_ACE_TYPE_ACCESS_ALLOWED, acc, acl_support ? SEC_ACE_FLAG_INHERITED_ACE : 0);
	}


	DEBUG(4,("map_darwinacl_to_ntacl: num_aces(%d)\n", num_aces ));
	
	return num_aces;
}

static size_t darwin_get_nt_acl_internals(vfs_handle_struct *handle, files_struct *fsp, uint32 security_info, SEC_DESC **ppdesc)
{
	SMB_STRUCT_STAT sbuf;
	DOM_SID owner_sid;
	DOM_SID group_sid;
	SEC_ACE nt_ace_list[40] = {};
	int num_nt_aces = 0;
	SEC_ACL *psa = NULL;
	SEC_DESC *psd = NULL;
	size_t sd_size = 0;
	filesec_t fsect = NULL;
	BOOL acl_support = True;
	
	
	DEBUG(4,("darwin_get_nt_acl_internals: called for file %s\n", fsp->fsp_name));

	fsect = filesec_init();
	if (statx_np(fsp->fsp_name, &sbuf, fsect) == -1) {
		DEBUG(0,("statx_np (%s): errno(%d) - (%s)\n",fsp->fsp_name, errno, strerror(errno)));
		goto cleanup;
	}
	
	
	uid_to_sid( &owner_sid, sbuf.st_uid );
	gid_to_sid( &group_sid, sbuf.st_gid );

	if ((security_info & DACL_SECURITY_INFORMATION) && !(security_info & PROTECTED_DACL_SECURITY_INFORMATION)) {
		memset(nt_ace_list, '\0', (40) * sizeof(SEC_ACE) );
		num_nt_aces =  map_darwinacl_to_ntacl(fsect, nt_ace_list, acl_support );

		if (num_nt_aces) {
			if((psa = make_sec_acl( main_loop_talloc_get(), ACL_REVISION, num_nt_aces, nt_ace_list)) == NULL) {
				DEBUG(0,("darwin_get_nt_acl_internals: Unable to malloc space for acl.\n"));
				goto cleanup;
			}
		} else {
			DEBUG(4,("darwin_get_nt_acl_internals : No ACLs on file (%s) !\n", fsp->fsp_name ));
			goto cleanup;
		}
	} /* security_info & DACL_SECURITY_INFORMATION */

	psd = make_standard_sec_desc( main_loop_talloc_get(),
			(security_info & OWNER_SECURITY_INFORMATION) ? &owner_sid : NULL,
			(security_info & GROUP_SECURITY_INFORMATION) ? &group_sid : NULL,
			psa,
			&sd_size);

	if(!psd) {
		DEBUG(0,("darwin_get_nt_acl_internals: Unable to malloc space for security descriptor.\n"));
		sd_size = 0;
	} 

	*ppdesc = psd;

 cleanup:
 	if (NULL != fsect)
		filesec_free(fsect);
	return sd_size;
}

static size_t darwin_fget_nt_acl(vfs_handle_struct *handle, files_struct *fsp, int fd, uint32 security_info, SEC_DESC **ppdesc)
{
	BOOL acl_support = False;
	
	
	acl_support = acl_support_enabled( fsp->fsp_name );
	DEBUG(4,("darwin_fget_nt_acl: called for file %s acl_support(%d)\n", fsp->fsp_name, acl_support));

	if (acl_support)
		return (darwin_get_nt_acl_internals(handle, fsp, security_info, ppdesc));	
	else
		return SMB_VFS_NEXT_FGET_NT_ACL(handle, fsp, fd, security_info, ppdesc);
}

static size_t darwin_get_nt_acl(vfs_handle_struct *handle, files_struct *fsp, const char *name, uint32 security_info, SEC_DESC **ppdesc)
{
	BOOL acl_support = False;
	
	
	acl_support = acl_support_enabled( fsp->fsp_name );
	DEBUG(4,("darwin_get_nt_acl: called for file %s acl_support(%d)\n", fsp->fsp_name, acl_support));

	if (acl_support)
		return (darwin_get_nt_acl_internals(handle, fsp, security_info, ppdesc));	
	else
		return SMB_VFS_NEXT_GET_NT_ACL(handle, fsp, name, security_info, ppdesc);
}


acl_tag_t map_ntacetype_to_darwinacetype(uint32 ace)
{
	switch(ace)
	{
		case SEC_ACE_TYPE_ACCESS_ALLOWED:
			return ACL_EXTENDED_ALLOW ;
			break;
		case SEC_ACE_TYPE_ACCESS_DENIED :
			return ACL_EXTENDED_DENY;
			break;
		default:
			DEBUG(0,("map_ntacetype_to_darwinacetype: !!!! ACL_UNDEFINED_TAG !!!!\n"));
			return ACL_UNDEFINED_TAG;
			break;
	}
}


static BOOL map_sid_to_uuid(DOM_SID *sid, uuid_t *uuid)
{
	BOOL result = False;
	uid_t id = 99; /* unknown */
	int status = 0;
	
	/* SID -> UID/GID -> UUID */
	
	if (resolvable_wellknown_sid(sid)) {
		if ((status = mbr_sid_to_uuid( (const nt_sid_t*)sid, *uuid)) != 0)
			DEBUG(0,("[%d]map_sid_to_uuid: [mbr_sid_to_uuid] errno(%d) - (%s)\n", status, errno, strerror(errno)));
		else
			result = True;
	} else {
		if (NT_STATUS_IS_OK(sid_to_uid(sid, &id)))
		{
			if((status = mbr_uid_to_uuid(id, *uuid)) != 0)
				DEBUG(0,("[%d]map_sid_to_uuid: [mbr_uid_to_uuid] uid(%d) errno(%d) - (%s)\n", status, id, errno, strerror(errno)));
			else
				result = True;
		} else if (NT_STATUS_IS_OK(sid_to_gid( sid, &id))) {
			if((status = mbr_gid_to_uuid(id, *uuid)) != 0)
				DEBUG(0,("[%d]map_sid_to_uuid: [mbr_gid_to_uuid] gid(%d), errno(%d) - (%s)\n", status, id, errno, strerror(errno)));
			else
				result = True;	
		} else {
			DEBUG(0,("map_sid_to_uuid: !!!! UNMAPPED  SID !!!!\n"));	
		}
	}
	return result;
}

static BOOL map_ntflags_to_darwinflags(SEC_ACE *ace, acl_flagset_t *the_flagset)
{
	BOOL result = False;
	
	DEBUG(4,("map_ntflags_to_darwinflags: flags(%X)\n",ace->flags));

	if (ace->flags & SEC_ACE_FLAG_OBJECT_INHERIT) 
	{
		if (acl_add_flag_np(*the_flagset, ACL_ENTRY_FILE_INHERIT) != 0) 
		{
			DEBUG(0,("map_ntflags_to_darwinflags: SEC_ACE_FLAG_OBJECT_INHERIT [acl_add_flag_np ACL_ENTRY_FILE_INHERIT] errno(%d) - (%s)\n",errno, strerror(errno)));
			goto exit_on_error;		
		} else {
			DEBUG(4,("map_ntflags_to_darwinflags: SEC_ACE_FLAG_OBJECT_INHERIT [acl_add_flag_np ACL_ENTRY_FILE_INHERIT]\n"));		
		}
	}

	if (ace->flags & SEC_ACE_FLAG_CONTAINER_INHERIT) 
	{
		if (acl_add_flag_np(*the_flagset, ACL_ENTRY_DIRECTORY_INHERIT) != 0) 
		{
			DEBUG(0,("map_ntflags_to_darwinflags: SEC_ACE_FLAG_CONTAINER_INHERIT [acl_add_flag_np ACL_ENTRY_DIRECTORY_INHERIT] errno(%d) - (%s)\n",errno, strerror(errno)));
			goto exit_on_error;		
		} else {
			DEBUG(4,("map_ntflags_to_darwinflags: SEC_ACE_FLAG_CONTAINER_INHERIT [acl_add_flag_np ACL_ENTRY_DIRECTORY_INHERIT]\n"));		
		}
	}

	if (ace->flags & SEC_ACE_FLAG_NO_PROPAGATE_INHERIT) 
	{
		if (acl_add_flag_np(*the_flagset, ACL_ENTRY_LIMIT_INHERIT) != 0) 
		{
			DEBUG(0,("map_ntflags_to_darwinflags: SEC_ACE_FLAG_NO_PROPAGATE_INHERIT [acl_add_flag_np ACL_ENTRY_LIMIT_INHERIT] errno(%d) - (%s)\n",errno, strerror(errno)));
			goto exit_on_error;		
		} else {
			DEBUG(4,("map_ntflags_to_darwinflags: SEC_ACE_FLAG_NO_PROPAGATE_INHERIT [acl_add_flag_np ACL_ENTRY_LIMIT_INHERIT]\n"));		
		}
	}

	if (ace->flags & SEC_ACE_FLAG_INHERIT_ONLY) 
	{
		if (acl_add_flag_np(*the_flagset, ACL_ENTRY_ONLY_INHERIT) != 0) 
		{
			DEBUG(0,("map_ntflags_to_darwinflags: SEC_ACE_FLAG_INHERIT_ONLY [acl_add_flag_np ACL_ENTRY_ONLY_INHERIT] errno(%d) - (%s)\n",errno, strerror(errno)));
			goto exit_on_error;		
		} else {
			DEBUG(4,("map_ntflags_to_darwinflags: SEC_ACE_FLAG_INHERIT_ONLY [acl_add_flag_np ACL_ENTRY_ONLY_INHERIT]\n"));		
		}
	}

	if (ace->flags & SEC_ACE_FLAG_INHERITED_ACE)  /* New for Windows 2000 */
	{
		if (acl_add_flag_np(*the_flagset, ACL_ENTRY_INHERITED) != 0) 
		{
			DEBUG(0,("map_ntflags_to_darwinflags: SEC_ACE_FLAG_INHERITED_ACE [acl_add_flag_np ACL_ENTRY_INHERITED] errno(%d) - (%s)\n",errno, strerror(errno)));
			goto exit_on_error;		
		} else {
			DEBUG(4,("map_ntflags_to_darwinflags: SEC_ACE_FLAG_INHERITED_ACE [acl_add_flag_np ACL_ENTRY_INHERITED]\n"));		
		}
	}

	result = True;
exit_on_error:
	return result;
}

static BOOL map_ntperms_to_darwinperms(SEC_ACCESS *perms, acl_permset_t *the_permset)
{
	BOOL result = False;
	
	DEBUG(4,("map_ntperms_to_darwinperms: ntperms(%X)\n",perms->mask));
	
	/* Generic Access Rights */
	if (perms->mask & DELETE_ACCESS) 
	{
		if (acl_add_perm(*the_permset, ACL_DELETE) != 0) 
		{
			DEBUG(0,("map_ntperms_to_darwinperms: DELETE_ACCESS [acl_add_perm ACL_DELETE] errno(%d) - (%s)\n",errno, strerror(errno)));
			goto exit_on_error;		
		} else {
			DEBUG(4,("map_ntperms_to_darwinperms: DELETE_ACCESS [acl_add_perm ACL_DELETE]\n"));		
		}
	}

	if (perms->mask & READ_CONTROL_ACCESS) 
	{
		if (acl_add_perm(*the_permset, ACL_READ_SECURITY) != 0) 
		{
			DEBUG(0,("map_ntperms_to_darwinperms: READ_CONTROL_ACCESS [acl_add_perm ACL_READ_SECURITY] errno(%d) - (%s)\n",errno, strerror(errno)));
			goto exit_on_error;		
		} else {
			DEBUG(4,("map_ntperms_to_darwinperms: READ_CONTROL_ACCESS [acl_add_perm ACL_READ_SECURITY]\n"));		
		}
	}

	if (perms->mask & WRITE_DAC_ACCESS) 
	{
		if (acl_add_perm(*the_permset, ACL_WRITE_SECURITY) != 0) 
		{
			DEBUG(0,("map_ntperms_to_darwinperms: WRITE_DAC_ACCESS [acl_add_perm ACL_WRITE_SECURITY] errno(%d) - (%s)\n",errno, strerror(errno)));
			goto exit_on_error;		
		} else {
			DEBUG(4,("map_ntperms_to_darwinperms: WRITE_DAC_ACCESS [acl_add_perm ACL_WRITE_SECURITY]\n"));		
		}
	}

	if (perms->mask & WRITE_OWNER_ACCESS) 
	{
		if (acl_add_perm(*the_permset, ACL_CHANGE_OWNER) != 0) 
		{
			DEBUG(0,("map_ntperms_to_darwinperms: WRITE_OWNER_ACCESS [acl_add_perm ACL_CHANGE_OWNER] errno(%d) - (%s)\n",errno, strerror(errno)));
			goto exit_on_error;		
		} else {
			DEBUG(4,("map_ntperms_to_darwinperms: WRITE_OWNER_ACCESS [acl_add_perm ACL_CHANGE_OWNER]\n"));		
		}
	}
#if 0
	if (perms->mask & SYNCHRONIZE_ACCESS) 
	{
		if (acl_add_perm(*the_permset, KAUTH_VNODE_SYNCHRONIZE) != 0) 
		{
			DEBUG(0,("map_ntperms_to_darwinperms: SYNCHRONIZE_ACCESS [acl_add_perm KAUTH_VNODE_SYNCHRONIZE] errno(%d) - (%s)\n",errno, strerror(errno)));
			goto exit_on_error;		
		} else {
			DEBUG(4,("map_ntperms_to_darwinperms: SYNCHRONIZE_ACCESS [acl_add_perm KAUTH_VNODE_SYNCHRONIZE]\n"));		
		}
	}
#endif
/* File Object specific access rights */

	if (perms->mask & FILE_READ_DATA) 
	{
		if (acl_add_perm(*the_permset, ACL_READ_DATA) != 0)
		{
			DEBUG(0,("map_ntperms_to_darwinperms: FILE_READ_DATA [acl_add_perm ACL_READ_DATA] errno(%d) - (%s)\n",errno, strerror(errno)));
			goto exit_on_error;		
		} else {
			DEBUG(4,("map_ntperms_to_darwinperms: FILE_READ_DATA [acl_add_perm ACL_READ_DATA]\n"));		
		}
	}

	if (perms->mask & FILE_WRITE_DATA) //ACL_ADD_FILE
	{
		if (acl_add_perm(*the_permset, ACL_WRITE_DATA) != 0) 
		{
			DEBUG(0,("map_ntperms_to_darwinperms: FILE_WRITE_DATA [acl_add_perm ACL_ADD_FILE/ACL_WRITE_DATA] errno(%d) - (%s)\n",errno, strerror(errno)));
			goto exit_on_error;		
		} else {
			DEBUG(4,("map_ntperms_to_darwinperms: FILE_WRITE_DATA [acl_add_perm ACL_ADD_FILE/ACL_WRITE_DATA]\n"));		
		}
	}

	if (perms->mask & FILE_APPEND_DATA) 
	{
		if (acl_add_perm(*the_permset, ACL_APPEND_DATA) != 0)  //ACL_ADD_SUBDIRECTORY
		{
			DEBUG(0,("map_ntperms_to_darwinperms: FILE_APPEND_DATA [acl_add_perm ACL_ADD_SUBDIRECTORY/ACL_APPEND_DATA] errno(%d) - (%s)\n",errno, strerror(errno)));
			goto exit_on_error;		
		} else {
			DEBUG(4,("map_ntperms_to_darwinperms: FILE_APPEND_DATA [acl_add_perm ACL_ADD_SUBDIRECTORY/ACL_APPEND_DATA]\n"));		
		}
	}

	if (perms->mask & FILE_READ_EA) 
	{
		if (acl_add_perm(*the_permset, ACL_READ_EXTATTRIBUTES) != 0) 
		{
			DEBUG(0,("map_ntperms_to_darwinperms: FILE_READ_EA [acl_add_perm ACL_READ_EXTATTRIBUTES] errno(%d) - (%s)\n",errno, strerror(errno)));
			goto exit_on_error;		
		} else {
			DEBUG(4,("map_ntperms_to_darwinperms: FILE_READ_EA [acl_add_perm ACL_READ_EXTATTRIBUTES]\n"));		
		}
	}

	if (perms->mask & FILE_WRITE_EA) 
	{
		if (acl_add_perm(*the_permset, ACL_WRITE_EXTATTRIBUTES) != 0) 
		{
			DEBUG(0,("map_ntperms_to_darwinperms: FILE_WRITE_EA [acl_add_perm ACL_WRITE_EXTATTRIBUTES] errno(%d) - (%s)\n",errno, strerror(errno)));
			goto exit_on_error;		
		} else {
			DEBUG(4,("map_ntperms_to_darwinperms: FILE_WRITE_EA [acl_add_perm ACL_WRITE_EXTATTRIBUTES]\n"));		
		}
	}

	if (perms->mask & FILE_EXECUTE) 
	{
		if (acl_add_perm(*the_permset, ACL_EXECUTE) != 0)  // ACL_SEARCH
		{
			DEBUG(0,("map_ntperms_to_darwinperms: FILE_EXECUTE [acl_add_perm ACL_SEARCH/ACL_EXECUTE] errno(%d) - (%s)\n",errno, strerror(errno)));
			goto exit_on_error;		
		} else {
			DEBUG(4,("map_ntperms_to_darwinperms: FILE_EXECUTE [acl_add_perm ACL_SEARCH/ACL_EXECUTE]\n"));		
		}
	}

	if (perms->mask & FILE_DELETE_CHILD) 
	{
		if (acl_add_perm(*the_permset, ACL_DELETE_CHILD) != 0) 
		{
			DEBUG(0,("map_ntperms_to_darwinperms: FILE_DELETE_CHILD [acl_add_perm ACL_DELETE_CHILD] errno(%d) - (%s)\n",errno, strerror(errno)));
			goto exit_on_error;		
		} else {
			DEBUG(4,("map_ntperms_to_darwinperms: FILE_DELETE_CHILD [acl_add_perm ACL_DELETE_CHILD]\n"));		
		}
	}


	if (perms->mask & FILE_READ_ATTRIBUTES) 
	{
		if (acl_add_perm(*the_permset, ACL_READ_ATTRIBUTES) != 0) 
		{
			DEBUG(0,("map_ntperms_to_darwinperms: FILE_READ_ATTRIBUTES [acl_add_perm ACL_READ_ATTRIBUTES] errno(%d) - (%s)\n",errno, strerror(errno)));
			goto exit_on_error;		
		} else {
			DEBUG(4,("map_ntperms_to_darwinperms: FILE_READ_ATTRIBUTES [acl_add_perm ACL_READ_ATTRIBUTES]\n"));		
		}
	}

	if (perms->mask & FILE_WRITE_ATTRIBUTES) 
	{
		if (acl_add_perm(*the_permset, ACL_WRITE_ATTRIBUTES) != 0) 
		{
			DEBUG(0,("map_ntperms_to_darwinperms: FILE_WRITE_ATTRIBUTES [acl_add_perm ACL_WRITE_ATTRIBUTES] errno(%d) - (%s)\n",errno, strerror(errno)));
			goto exit_on_error;		
		} else {
			DEBUG(4,("map_ntperms_to_darwinperms: FILE_WRITE_ATTRIBUTES [acl_add_perm ACL_WRITE_ATTRIBUTES]\n"));		
		}
	}
	result = True;
exit_on_error:
	return result;
}
/****************************************************************************
 Map NT perms to a UNIX mode_t.
****************************************************************************/

#define FILE_SPECIFIC_READ_BITS (FILE_READ_DATA|FILE_READ_EA|FILE_READ_ATTRIBUTES|READ_CONTROL_ACCESS)
#define FILE_SPECIFIC_WRITE_BITS (FILE_WRITE_DATA|FILE_APPEND_DATA|FILE_WRITE_EA|FILE_WRITE_ATTRIBUTES)
#define FILE_SPECIFIC_EXECUTE_BITS (FILE_EXECUTE)

static mode_t map_nt_perms( SEC_ACCESS sec_access, int type)
{
	mode_t mode = 0;

	switch(type) {
	case S_IRUSR:
		if(sec_access.mask & GENERIC_ALL_ACCESS)
			mode = S_IRUSR|S_IWUSR|S_IXUSR;
		else {
			mode |= (sec_access.mask & (GENERIC_READ_ACCESS|FILE_SPECIFIC_READ_BITS)) ? S_IRUSR : 0;
			mode |= (sec_access.mask & (GENERIC_WRITE_ACCESS|FILE_SPECIFIC_WRITE_BITS)) ? S_IWUSR : 0;
			mode |= (sec_access.mask & (GENERIC_EXECUTE_ACCESS|FILE_SPECIFIC_EXECUTE_BITS)) ? S_IXUSR : 0;
		}
		break;
	case S_IRGRP:
		if(sec_access.mask & GENERIC_ALL_ACCESS)
			mode = S_IRGRP|S_IWGRP|S_IXGRP;
		else {
			mode |= (sec_access.mask & (GENERIC_READ_ACCESS|FILE_SPECIFIC_READ_BITS)) ? S_IRGRP : 0;
			mode |= (sec_access.mask & (GENERIC_WRITE_ACCESS|FILE_SPECIFIC_WRITE_BITS)) ? S_IWGRP : 0;
			mode |= (sec_access.mask & (GENERIC_EXECUTE_ACCESS|FILE_SPECIFIC_EXECUTE_BITS)) ? S_IXGRP : 0;
		}
		break;
	case S_IROTH:
		if(sec_access.mask & GENERIC_ALL_ACCESS)
			mode = S_IROTH|S_IWOTH|S_IXOTH;
		else {
			mode |= (sec_access.mask & (GENERIC_READ_ACCESS|FILE_SPECIFIC_READ_BITS)) ? S_IROTH : 0;
			mode |= (sec_access.mask & (GENERIC_WRITE_ACCESS|FILE_SPECIFIC_WRITE_BITS)) ? S_IWOTH : 0;
			mode |= (sec_access.mask & (GENERIC_EXECUTE_ACCESS|FILE_SPECIFIC_EXECUTE_BITS)) ? S_IXOTH : 0;
		}
		break;
	}

	return mode;
} 

static BOOL map_ntacl_to_posixperms(SEC_ACL *dacl, DOM_SID *owner_sid, DOM_SID *group_sid, mode_t *mode)
{
	BOOL result = False;
	extern DOM_SID global_sid_World;    /* Everyone */
	SEC_ACE *psa = NULL;
	int i;
		
	for(i = 0; i < dacl->num_aces; i++) {
		psa = &dacl->ace[i];
		if (psa->type == SEC_ACE_TYPE_ACCESS_ALLOWED) {
			if (sid_equal(&psa->trustee, owner_sid)) {
				*mode |= map_nt_perms( psa->info, S_IRUSR);
				result = True;
			} else if (sid_equal(&psa->trustee, group_sid)) {
				*mode |=  map_nt_perms( psa->info, S_IRGRP);
				result = True;
			} else if (sid_equal(&psa->trustee, &global_sid_World)) {
				*mode |= map_nt_perms( psa->info, S_IROTH);
				result = True;
			}
		} else {
			DEBUG(3,("map_ntacl_to_posixperms: unable to set anything but an ALLOW ACE.\n"));
		}

	}
	
	DEBUG(4,("map_ntacl_to_posixperms (%d)\n", result));
	return result;
}

static mode_t map_ntacl_to_mode(files_struct *fsp, SEC_ACL *dacl, DOM_SID *owner_sid, DOM_SID *group_sid, mode_t mode) {
	
	extern DOM_SID global_sid_World; /* Everyone */
	int i=0;
	SEC_ACE *psa = NULL;
	mode_t r_mode = 0;

	uint32 u_tempread = 0,g_tempread = 0,o_tempread = 0;
	uint32 u_tempwrite = 0,g_tempwrite = 0,o_tempwrite = 0;
	uint32 u_tempexec = 0,g_tempexec = 0,o_tempexec = 0;
	uint32 re_read;
	uint32 re_write;
	uint32 re_exec;

	
	if (dacl == NULL) {
		DEBUG(4,("map_ntacl_to_mode: NULL parameter"));
		return mode; // just return the passed mode
	}

	for (i=0;i < dacl->num_aces;i++) {
		psa = &dacl->ace[i];
		if ((psa->type == SEC_ACE_TYPE_ACCESS_ALLOWED) )  {
			if (sid_equal(&psa->trustee, owner_sid)) { // set owner permission
				if ((re_read = (psa->info.mask  & FILE_SPECIFIC_READ_BITS))) {
					u_tempread |= re_read;
				}
				if ((re_write = (psa->info.mask &  FILE_SPECIFIC_WRITE_BITS))  )  {
					u_tempwrite |= re_write;
				}
				if ((re_exec = (psa->info.mask & FILE_SPECIFIC_EXECUTE_BITS)) ) {
					u_tempexec |= re_exec;
				}
			} else if (sid_equal(&psa->trustee, group_sid)) { //set group permission 
				if ((re_read = (psa->info.mask & FILE_SPECIFIC_READ_BITS))) {
					g_tempread |= re_read;
				}
				if ((re_write = (psa->info.mask &  FILE_SPECIFIC_WRITE_BITS)) )  {
					g_tempwrite |= re_write;
				}
				if ((re_exec = (psa->info.mask & FILE_SPECIFIC_EXECUTE_BITS)) ) {
					g_tempexec |= re_exec;
				}
			} else if (sid_equal(&psa->trustee,&global_sid_World)) {
				if ((re_read = (psa->info.mask & FILE_SPECIFIC_READ_BITS))) {
					o_tempread |= re_read;
				}
				if ((re_write = (psa->info.mask &  FILE_SPECIFIC_WRITE_BITS)))  {
					o_tempwrite |= re_write;
				}
				if ((re_exec = (psa->info.mask & FILE_SPECIFIC_EXECUTE_BITS)) ) {
					o_tempexec |= re_exec;
				}
			}
		}
	}

	if (u_tempread == FILE_SPECIFIC_READ_BITS) {
		DEBUG(4,("map_ntacl_to_mode : [USER] FILE_SPECIFIC_READ_BITS \n"));
		r_mode |= (S_IRUSR );
	}
	if (u_tempexec == FILE_SPECIFIC_EXECUTE_BITS) {
		DEBUG(4,("map_ntacl_to_mode : [USER] FILE_SPECIFIC_EXECUTE_BITS \n"));
		r_mode |= (S_IXUSR);
	}

	if (u_tempwrite == FILE_SPECIFIC_WRITE_BITS) {
		DEBUG(4,("map_ntacl_to_mode : [USER] FILE_SPECIFIC_WRITE_BITS \n"));
		r_mode |= (S_IWUSR);
	}

	if (g_tempread == FILE_SPECIFIC_READ_BITS) {
		DEBUG(4,("map_ntacl_to_mode : [GROUP] FILE_SPECIFIC_READ_BITS \n"));
		r_mode |= (S_IRGRP );
	}
	if (g_tempexec == FILE_SPECIFIC_EXECUTE_BITS) {
		DEBUG(4,("map_ntacl_to_mode : [GROUP] FILE_SPECIFIC_EXECUTE_BITS \n"));
		r_mode |= (S_IXGRP);
	}

	if (g_tempwrite == FILE_SPECIFIC_WRITE_BITS) {
		DEBUG(4,("map_ntacl_to_mode : [GROUP] FILE_SPECIFIC_WRITE_BITS \n"));
		r_mode |= (S_IWGRP);
	}

	if (o_tempread == FILE_SPECIFIC_READ_BITS) {
		DEBUG(4,("map_ntacl_to_mode : [OTHER] FILE_SPECIFIC_READ_BITS \n"));
		r_mode |= (S_IROTH);
	}
	if (o_tempexec == FILE_SPECIFIC_EXECUTE_BITS) {
		DEBUG(4,("map_ntacl_to_mode : [OTHER] FILE_SPECIFIC_EXECUTE_BITS \n"));
		r_mode |= (S_IXOTH);
	}

	if (o_tempwrite == FILE_SPECIFIC_WRITE_BITS) {
		DEBUG(4,("map_ntacl_to_mode : [OTHER] FILE_SPECIFIC_WRITE_BITS \n"));
		r_mode |= (S_IWOTH);
	}
		

// now loop through DENIED access to make sure none of the bits set are in the Deny group
	for (i=0;i < dacl->num_aces;i++) {
		psa = &dacl->ace[i];
		if ((psa->type == SEC_ACE_TYPE_ACCESS_DENIED) )  {				
			if (sid_equal(&psa->trustee, owner_sid)) { // owner permissions
				if (psa->info.mask  & FILE_SPECIFIC_READ_BITS) {
					r_mode &= ~(S_IRUSR );
				}
				if (psa->info.mask  & FILE_SPECIFIC_EXECUTE_BITS) {
					r_mode &= ~( S_IXUSR);
				}
				if ((psa->info.mask &  FILE_SPECIFIC_WRITE_BITS) )  {
					r_mode &= ~(S_IWUSR);
				}
			} else if (sid_equal(&psa->trustee, group_sid)) { // group permissions
				if (psa->info.mask & FILE_SPECIFIC_READ_BITS) {
					r_mode &= ~(S_IRGRP);
				}
				if (psa->info.mask  & FILE_SPECIFIC_EXECUTE_BITS) {
					r_mode &= ~( S_IXGRP);
				}
				if ((psa->info.mask &  FILE_SPECIFIC_WRITE_BITS) )  {
					r_mode &= ~(S_IWGRP);
				}
			} else if (sid_equal(&psa->trustee,&global_sid_World)) { // everyone permissions
				if (psa->info.mask & FILE_SPECIFIC_READ_BITS) {
					r_mode &= ~(S_IROTH);
				}
				if (psa->info.mask  & FILE_SPECIFIC_EXECUTE_BITS) {
					r_mode &= ~( S_IXOTH);
				}
				if ((psa->info.mask &  FILE_SPECIFIC_WRITE_BITS) )  {
					r_mode &= ~(S_IWOTH);
				}
			}
		}
	}
			
	DEBUG(4,("map_ntacl_to_mode : mode (%X)\n", r_mode));
	return r_mode;
}
				 


static BOOL map_ntacl_to_darwinacl(SEC_ACL *dacl, acl_t *original_acl, acl_t *darwin_acl, uint16 desc_flags)
{
	BOOL result = False;
	BOOL uuid_err = False;
	SEC_ACE *psa = NULL;
	int i = 0;
		
	if (darwin_acl == NULL || dacl == NULL) {
		DEBUG(0,("map_ntacl_to_darwinacl: NULL Parameter dacl(%p) darwin_acl(&=%p)\n", dacl, darwin_acl));
		return False;
	}
	
	*darwin_acl = acl_init(1);
	
	if (*darwin_acl == NULL)
	{
		DEBUG(0,("map_ntacl_to_darwinacl: [acl_init] errno(%d) - (%s)\n",errno, strerror(errno)));
		goto exit_on_error;
	}

	for(i = 0; i < dacl->num_aces && !uuid_err; i++) {
		psa = &dacl->ace[i];
		acl_entry_t darwin_acl_entry = NULL;
		acl_permset_t the_permset;
		acl_flagset_t the_flagset;
		uuid_t	uuid;
		
		if((psa->type == SEC_ACE_TYPE_ACCESS_ALLOWED) || (psa->type == SEC_ACE_TYPE_ACCESS_DENIED)) {
		
			if (!map_sid_to_uuid(&psa->trustee, &uuid))
			{
				DEBUG(0,("map_ntacl_to_darwinacl: [map_sid_to_uuid] errno(%d) - (%s)\n",errno, strerror(errno)));
				uuid_err = True;
				continue;
			}
			
			if (acl_create_entry(darwin_acl, &darwin_acl_entry) != 0)
			{
				DEBUG(0,("map_ntacl_to_darwinacl: [acl_create_entry] errno(%d) - (%s)\n",errno, strerror(errno)));
				continue;	
			}
			
			if (acl_set_tag_type(darwin_acl_entry, map_ntacetype_to_darwinacetype(psa->type)) != 0)
			{
				DEBUG(0,("map_ntacl_to_darwinacl: [acl_set_tag_type] errno(%d) - (%s)\n",errno, strerror(errno)));
				if(acl_delete_entry(*darwin_acl, darwin_acl_entry) != 0)
					DEBUG(0,("map_ntacl_to_darwinacl: [acl_delete_entry] errno(%d) - (%s)\n",errno, strerror(errno)));
				continue;		
			}
	
			if (acl_set_qualifier(darwin_acl_entry, &uuid) != 0)
			{
				DEBUG(0,("map_ntacl_to_darwinacl: [acl_set_qualifier] errno(%d) - (%s)\n",errno, strerror(errno)));
				if(acl_delete_entry(*darwin_acl, darwin_acl_entry) != 0)
					DEBUG(0,("map_ntacl_to_darwinacl: [acl_delete_entry] errno(%d) - (%s)\n",errno, strerror(errno)));
				continue;		
			}
	
			if (acl_get_permset(darwin_acl_entry, &the_permset) != 0)
			{
				DEBUG(0,("map_ntacl_to_darwinacl: [acl_get_permset] errno(%d) - (%s)\n",errno, strerror(errno)));
				if(acl_delete_entry(*darwin_acl, darwin_acl_entry) != 0)
					DEBUG(0,("map_ntacl_to_darwinacl: [acl_delete_entry] errno(%d) - (%s)\n",errno, strerror(errno)));
				continue;		
			}
	
			if (acl_clear_perms(the_permset) != 0)
			{
				DEBUG(0,("map_ntacl_to_darwinacl: [acl_clear_perms] errno(%d) - (%s)\n",errno, strerror(errno)));
				if(acl_delete_entry(*darwin_acl, darwin_acl_entry) != 0)
					DEBUG(0,("map_ntacl_to_darwinacl: [acl_delete_entry] errno(%d) - (%s)\n",errno, strerror(errno)));
				continue;		
			}
			
			if (map_ntperms_to_darwinperms(&psa->info, &the_permset))
			{
				if (acl_set_permset(darwin_acl_entry, the_permset) != 0)
				{
					printf("map_ntacl_to_darwinacl: [acl_set_permset] errno(%d) - (%s)\n",errno, strerror(errno));
					if(acl_delete_entry(*darwin_acl, darwin_acl_entry) != 0)
						DEBUG(0,("map_ntacl_to_darwinacl: [acl_delete_entry] errno(%d) - (%s)\n",errno, strerror(errno)));
					continue;		
				}
			} else {
				DEBUG(0,("map_ntacl_to_darwinacl: map_ntperms_to_darwinperms FAILED\n"));
				if(acl_delete_entry(*darwin_acl, darwin_acl_entry) != 0)
					DEBUG(0,("map_ntacl_to_darwinacl: [acl_delete_entry] errno(%d) - (%s)\n",errno, strerror(errno)));
				continue;		
			}

			if (acl_get_flagset_np(darwin_acl_entry, &the_flagset) != 0)
			{
				DEBUG(0,("map_ntacl_to_darwinacl: [acl_get_flagset_np] errno(%d) - (%s)\n",errno, strerror(errno)));
				if(acl_delete_entry(*darwin_acl, darwin_acl_entry) != 0)
					DEBUG(0,("map_ntacl_to_darwinacl: [acl_delete_entry] errno(%d) - (%s)\n",errno, strerror(errno)));
				continue;		
			}

			if (map_ntflags_to_darwinflags(psa, &the_flagset))
			{
				if (acl_set_flagset_np(darwin_acl_entry, the_flagset) != 0)
				{
					printf("map_ntacl_to_darwinacl: [acl_set_flagset_np] errno(%d) - (%s)\n",errno, strerror(errno));
					if(acl_delete_entry(*darwin_acl, darwin_acl_entry) != 0)
						DEBUG(0,("map_ntacl_to_darwinacl: [acl_delete_entry] errno(%d) - (%s)\n",errno, strerror(errno)));
					continue;		
				}
			} else {
				DEBUG(0,("map_ntacl_to_darwinacl: map_ntperms_to_darwinperms FAILED\n"));
				if(acl_delete_entry(*darwin_acl, darwin_acl_entry) != 0)
					DEBUG(0,("map_ntacl_to_darwinacl: [acl_delete_entry] errno(%d) - (%s)\n",errno, strerror(errno)));
				continue;		
			}
			
			DEBUG(4,("map_ntacl_to_darwinacl: entry [%d]\n",i));
		} else {
			DEBUG(3,("map_ntacl_to_darwinacl: unable to set anything but an ALLOW or DENY ACE.\n"));
		}

	}
	if (!uuid_err) {
		if (original_acl && *original_acl && !(desc_flags & SE_DESC_DACL_PROTECTED)) {
			int entry_id = ACL_FIRST_ENTRY;
			acl_entry_t current_entry = NULL;
			acl_entry_t  new_entry = NULL;
			acl_flagset_t the_flagset;
			
			for (entry_id = 0; acl_get_entry(*original_acl, current_entry == NULL ? ACL_FIRST_ENTRY : ACL_NEXT_ENTRY, &current_entry) == 0; entry_id++)
			{
				if (acl_get_flagset_np(current_entry, &the_flagset) != 0) 
				{
					DEBUG(0,("map_ntacl_to_darwinacl: [acl_get_flagset_np] errno(%d) - (%s)\n",errno, strerror(errno)));
					continue;		
				}  else {
					if (acl_get_flag_np(the_flagset, ACL_ENTRY_INHERITED) == 1) {
						if (acl_create_entry(darwin_acl, &new_entry) == 0) {
							if (acl_copy_entry(new_entry, current_entry) == 0) {
								DEBUG(3,("map_ntacl_to_darwinacl: [acl_copy_entry] COPIED INHERITED ACE (%d)\n", entry_id));												
							} else {
								DEBUG(0,("map_ntacl_to_darwinacl: [acl_copy_entry] errno(%d) - (%s)\n",errno, strerror(errno)));												
							}
						} else {
							DEBUG(0,("map_ntacl_to_darwinacl: [acl_create_entry] errno(%d) - (%s)\n",errno, strerror(errno)));
							continue;							
						}

					}
				}
			}
		}
		result = True;
	}
	
exit_on_error:
	DEBUG(4,("map_ntacl_to_darwinacl (%d)\n", result));
	return result;
}

static BOOL darwin_set_nt_acl_internals(vfs_handle_struct *handle, files_struct *fsp, uint32 security_info_sent, SEC_DESC *psd)
{
	connection_struct *conn = fsp->conn;
	uid_t user = (uid_t)99; /* unknown */
	gid_t grp = (gid_t)99; /* unknown */
	SMB_STRUCT_STAT sbuf;  
	DOM_SID file_owner_sid;
	DOM_SID file_grp_sid;
	mode_t orig_mode = (mode_t)0;
	uid_t orig_uid;
	gid_t orig_gid;
	BOOL need_chown = False;
	extern struct current_user current_user;
	acl_t darwin_acl = NULL,   original_acl = NULL;
	BOOL acl_support = True;
	
	DEBUG(4,("darwin_set_nt_acl_internals: called for file %s\n", fsp->fsp_name ));

	if (!CAN_WRITE(conn)) {
		DEBUG(10,("darwin_set_nt_acl_internals: set acl rejected on read-only share\n"));
		return False;
	}

	/*
	 * Get the current state of the file.
	 */

	if(fsp->is_directory || fsp->fd == -1) {
		if(SMB_VFS_STAT(fsp->conn,fsp->fsp_name, &sbuf) != 0)
			return False;
	} else {
		if(SMB_VFS_FSTAT(fsp,fsp->fd,&sbuf) != 0)
			return False;
	}

	acl_support = acl_support_enabled( fsp->fsp_name );
	/* Save the original elements we check against. */
	orig_mode = sbuf.st_mode;
	orig_uid = sbuf.st_uid;
	orig_gid = sbuf.st_gid;

	/*
	 * Unpack the user/group/world id's.
	 */

	if (!darwin_unpack_nt_owners( &sbuf, &user, &grp, security_info_sent, psd))
		return False;

	/*
	 * Do we need to chown ?
	 */

	if (((user != (uid_t)99) && (orig_uid != user)) || (( grp != (gid_t)99) && (orig_gid != grp)))
		need_chown = True;

	/*
	 * Chown before setting ACL only if we don't change the user, or
	 * if we change to the current user, but not if we want to give away
	 * the file.
	 */

	if (need_chown && (user == (uid_t)99 || user == current_user.uid)) {

		DEBUG(3,("darwin_set_nt_acl_internals: chown %s. uid = %u, gid = %u.\n",
				fsp->fsp_name, (unsigned int)user, (unsigned int)grp ));

		if(darwin_try_chown( fsp->conn, fsp->fsp_name, user, grp) == -1) {
			DEBUG(3,("darwin_set_nt_acl: chown %s, %u, %u failed. Error = %s.\n",
				fsp->fsp_name, (unsigned int)user, (unsigned int)grp, strerror(errno) ));
			return False;
		}

		/*
		 * Recheck the current state of the file, which may have changed.
		 * (suid/sgid bits, for instance)
		 */

		if(fsp->is_directory) {
			if(SMB_VFS_STAT(fsp->conn, fsp->fsp_name, &sbuf) != 0) {
				return False;
			}
		} else {

			int ret;
    
			if(fsp->fd == -1)
				ret = SMB_VFS_STAT(fsp->conn, fsp->fsp_name, &sbuf);
			else
				ret = SMB_VFS_FSTAT(fsp,fsp->fd,&sbuf);
  
			if(ret != 0)
				return False;
		}

		/* Save the original elements we check against. */
		orig_mode = sbuf.st_mode;
		orig_uid = sbuf.st_uid;
		orig_gid = sbuf.st_gid;

		/* We did it, don't try again */
		need_chown = False;
	}

	uid_to_sid( &file_owner_sid, sbuf.st_uid );
	gid_to_sid( &file_grp_sid, sbuf.st_gid );


	if(security_info_sent == 0) {
		DEBUG(0,("darwin_set_nt_acl_internals: no security info sent !\n"));
		return False;
	}

	/*
	 * If no DACL then this is a chown only security descriptor.
	 */

	if(!(security_info_sent & DACL_SECURITY_INFORMATION) || !psd->dacl)
		return True;

	
	original_acl = acl_get_file(fsp->fsp_name, ACL_TYPE_EXTENDED);
	
	if (!map_ntacl_to_darwinacl(psd->dacl, &original_acl, &darwin_acl, psd->type)) {
		return False;
	} else {
		mode_t new_mode = 0;
		if(acl_set_file(fsp->fsp_name, ACL_TYPE_EXTENDED, darwin_acl) != 0) {
			DEBUG(0,("darwin_set_nt_acl_internals: [acl_set_file] errno(%d) - (%s)\n",errno, strerror(errno)));
			return False;
		}  else {
			if ((psd->type & SE_DESC_DACL_PROTECTED)) {
				new_mode = map_ntacl_to_mode(fsp, psd->dacl, &file_owner_sid, &file_grp_sid, orig_mode);
				if (orig_mode != new_mode) {
					if(SMB_VFS_CHMOD(conn,fsp->fsp_name, new_mode) == -1) {
						DEBUG(3,("darwin_set_nt_acl_internals: [acl_support] chmod %s, 0%o failed. Error = %s.\n",
								fsp->fsp_name, (unsigned int)new_mode, strerror(errno) ));
						return False;
					}
				}
			}
		}
	}
	/* Any chown pending? */
	if (need_chown) {

		DEBUG(3,("darwin_set_nt_acl_internals: chown %s. uid = %u, gid = %u.\n",
			fsp->fsp_name, (unsigned int)user, (unsigned int)grp ));

		if(darwin_try_chown( fsp->conn, fsp->fsp_name, user, grp) == -1) {
			DEBUG(3,("darwin_set_nt_acl_internals: chown %s, %u, %u failed. Error = %s.\n",
				fsp->fsp_name, (unsigned int)user, (unsigned int)grp, strerror(errno) ));
			return False;
		}
	}

	return True;
}

static BOOL darwin_fset_nt_acl(vfs_handle_struct *handle, files_struct *fsp, int fd, uint32 security_info_sent, SEC_DESC *psd)
{
	BOOL acl_support = False;
	
	
	acl_support = acl_support_enabled( fsp->fsp_name );
	DEBUG(4,("darwin_fset_nt_acl: called for file %s acl_support(%d)\n", fsp->fsp_name, acl_support));

	if (acl_support)
		return (darwin_set_nt_acl_internals(handle, fsp, security_info_sent, psd));
	else
		return SMB_VFS_NEXT_FSET_NT_ACL(handle, fsp, fd, security_info_sent, psd);
}

static BOOL darwin_set_nt_acl(vfs_handle_struct *handle, files_struct *fsp, const char *name, uint32 security_info_sent, SEC_DESC *psd)
{
	BOOL acl_support = False;
	
	
	acl_support = acl_support_enabled( fsp->fsp_name );
	DEBUG(4,("darwin_set_nt_acl: called for file %s acl_support(%d)\n", fsp->fsp_name, acl_support));

	if (acl_support)
		return (darwin_set_nt_acl_internals(handle, fsp, security_info_sent, psd));
	else
		return SMB_VFS_NEXT_SET_NT_ACL(handle, fsp, name, security_info_sent, psd);
}

/* VFS operations structure */

static vfs_op_tuple darwin_acls_ops[] = {	
	{SMB_VFS_OP(darwin_fget_nt_acl),	SMB_VFS_OP_FGET_NT_ACL,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(darwin_get_nt_acl),		SMB_VFS_OP_GET_NT_ACL,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(darwin_fset_nt_acl),	SMB_VFS_OP_FSET_NT_ACL,	SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(darwin_set_nt_acl),		SMB_VFS_OP_SET_NT_ACL,	SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(NULL),			SMB_VFS_OP_NOOP,		SMB_VFS_LAYER_NOOP}
};

NTSTATUS init_module(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "darwin_acls", darwin_acls_ops);
}