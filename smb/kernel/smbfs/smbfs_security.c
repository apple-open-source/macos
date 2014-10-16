/*
 * Copyright (c) 2009 - 2013 Apple Inc. All rights reserved.
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

#include <sys/mount.h>
#include <sys/kauth.h>
#include <sys/syslog.h>

#include <sys/smb_byte_order.h>
#include <sys/smb_apple.h>
#include <sys/mchain.h>
#include <sys/msfscc.h>

#include <netsmb/smb.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_conn.h>
#include <smbfs/smbfs.h>
#include <smbfs/smbfs_node.h>
#include <smbfs/smbfs_subr.h>
#include <smbfs/smbfs_subr_2.h>
#include <smbfs/smbfs_security.h>
#include <smbfs/smb_rq_2.h>


#define MAX_SID_PRINTBUFFER	256	/* Used to print out the sid in case of an error */
#define DEBUG_ACLS 0

/*
 * Directory Service generates these UUIDs for SIDs that are unknown. These UUIDs 
 * are used so we can round trip a translation from SID-->UUID-->SID. The first 
 * 12 bytes are well known and allow us to tell if this is a temporary UUID.
 */
static const uint8_t tmpuuid1[12] = {	0xFF, 0xFF, 0xEE, 0xEE, 0xDD, 0xDD, 
										0xCC, 0xCC, 0xBB, 0xBB, 0xAA, 0xAA};
static const uint8_t tmpuuid2[12] = {	0xAA, 0xAA, 0xBB, 0xBB, 0xCC, 0xCC, 
										0xDD, 0xDD, 0xEE, 0xEE, 0xFF, 0xFF};

static const ntsid_t unix_users_domsid =
{ 1, 1, {0, 0, 0, 0, 0, 22}, {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} };

static const ntsid_t unix_groups_domsid =
{ 1, 1, {0, 0, 0, 0, 0, 22}, {2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} };

static void * smb_sdoffset(struct ntsecdesc *w_secp, size_t w_seclen, int sd_type);

#define sdowner(s, s_len) (struct ntsid *)smb_sdoffset(s, s_len, OWNER_SECURITY_INFORMATION)
#define sdgroup(s, s_len) (struct ntsid *)smb_sdoffset(s, s_len, GROUP_SECURITY_INFORMATION)
#define sdsacl(s, s_len) (struct ntacl *)smb_sdoffset(s, s_len, SACL_SECURITY_INFORMATION)
#define sddacl(s, s_len) (struct ntacl *)smb_sdoffset(s, s_len, DACL_SECURITY_INFORMATION)

#if DEBUG_ACLS
static void
smb_print_guid(guid_t *uuidp)
{
    char *user = NULL;
    char *group = NULL;
    uid_t uid = 0;
    gid_t gid = 0;
    
    SMB_MALLOC(user, char *, MAXPATHLEN, M_TEMP, M_WAITOK | M_ZERO);
    if (user == NULL) {
        SMBERROR("user failed malloc\n");
        return;
    }
    
    SMB_MALLOC(group, char *, MAXPATHLEN, M_TEMP, M_WAITOK | M_ZERO);
    if (group == NULL) {
        SMBERROR("group failed malloc\n");
        return;
    }
   
    if (is_memberd_tempuuid(uuidp)) {
        SMBERROR("\tguid: TEMPUUID \n");
    }
    else {
        SMBERROR("\tguid: 0x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x \n",
                 uuidp->g_guid[0], uuidp->g_guid[1], uuidp->g_guid[2],
                 uuidp->g_guid[3], uuidp->g_guid[4], uuidp->g_guid[5],
                 uuidp->g_guid[6], uuidp->g_guid[7], uuidp->g_guid[8],
                 uuidp->g_guid[9], uuidp->g_guid[10], uuidp->g_guid[11],
                 uuidp->g_guid[12], uuidp->g_guid[13], uuidp->g_guid[14],
                 uuidp->g_guid[15]
                 );
        kauth_cred_guid2uid(uuidp, &uid);
        kauth_cred_guid2gid(uuidp, &gid);
        kauth_cred_guid2pwnam(uuidp, user);
        kauth_cred_guid2grnam(uuidp, group);
        SMBERROR("\tuser/group: %s (%d)/%s (%d) \n", user, uid, group, gid);
    }
    
    if (user) {
        SMB_FREE(user, M_TEMP);
    }
    
    if (group) {
        SMB_FREE(group, M_TEMP);
    }
}


static void
smb_print_acl(struct smbnode *np, const char *function, struct kauth_acl *acl)
{
    uint32_t i;
    char *buffer = NULL;
    size_t buf_len = MAXPATHLEN * 2;
    
    SMB_MALLOC(buffer, char *, buf_len, M_TEMP, M_WAITOK | M_ZERO);
    if (buffer == NULL) {
        SMBERROR("buffer failed malloc\n");
        return;
    }

    if ((np == NULL) || (acl == NULL)) {
        SMBERROR("node or acl is null \n");
        return;
    }

    SMBERROR_LOCK(np, "function: %s node %s \n", function, np->n_name);
    
    SMBERROR("acl_entrycount %d\n", acl->acl_entrycount);
    
    bzero(buffer, buf_len);
    if (acl->acl_flags & KAUTH_ACL_DEFER_INHERIT) {
        strlcat(buffer, "defer_inherit ", buf_len);
    }
    if (acl->acl_flags & KAUTH_ACL_NO_INHERIT) {
        strlcat(buffer, "no_inherit ", buf_len);
    }
    SMBERROR("acl_flags 0x%x ( %s) \n", acl->acl_flags, buffer);
    
    if (acl->acl_entrycount != KAUTH_FILESEC_NOACL) {
        for (i = 0; i < acl->acl_entrycount; i++) {
            SMBERROR("ACE: %d \n", i);
            smb_print_guid(&acl->acl_ace[i].ace_applicable);

            /* Try to print out ace_flags in same order as ls does */
            bzero(buffer, MAXPATHLEN);
            if (acl->acl_ace[i].ace_flags & KAUTH_ACE_INHERITED) {
                strlcat(buffer, "inherited ", buf_len);
            }
            if (acl->acl_ace[i].ace_flags & KAUTH_ACE_FILE_INHERIT) {
                strlcat(buffer, "file_inherit ", buf_len);
            }
            if (acl->acl_ace[i].ace_flags & KAUTH_ACE_DIRECTORY_INHERIT) {
                strlcat(buffer, "dir_inherit ", buf_len);
            }
            if (acl->acl_ace[i].ace_flags & KAUTH_ACE_LIMIT_INHERIT) {
                strlcat(buffer, "limit_inherit ", buf_len);
            }
            if (acl->acl_ace[i].ace_flags & KAUTH_ACE_ONLY_INHERIT) {
                strlcat(buffer, "only_inherit ", buf_len);
            }
            if (acl->acl_ace[i].ace_flags & KAUTH_ACE_SUCCESS) {
                strlcat(buffer, "success ", buf_len);
            }
            if (acl->acl_ace[i].ace_flags & KAUTH_ACE_FAILURE) {
                strlcat(buffer, "failure ", buf_len);
            }
            switch(acl->acl_ace[i].ace_flags & KAUTH_ACE_KINDMASK) {
				case KAUTH_ACE_PERMIT:
                    strlcat(buffer, "allow ", buf_len);
					break;
			    case KAUTH_ACE_DENY:
                    strlcat(buffer, "deny ", buf_len);
					break;
			    case KAUTH_ACE_AUDIT:
                    strlcat(buffer, "audit ", buf_len);
					break;
			    case KAUTH_ACE_ALARM:
                    strlcat(buffer, "alarm ", buf_len);
					break;
			    default:
                    strlcat(buffer, "unknown_kind ", buf_len);
			}
            
            SMBERROR("\tflags 0x%x ( %s) \n", acl->acl_ace[i].ace_flags,
                     buffer);

            /* Try to print out ace_rights in same order as ls does */
            bzero(buffer, buf_len);
            if (acl->acl_ace[i].ace_rights & KAUTH_VNODE_READ_DATA) {
                strlcat(buffer, "read ", buf_len);
            }
            if (acl->acl_ace[i].ace_rights & KAUTH_VNODE_WRITE_DATA) {
                strlcat(buffer, "write ", buf_len);
            }
            if (acl->acl_ace[i].ace_rights & KAUTH_VNODE_EXECUTE) {
                strlcat(buffer, "execute ", buf_len);
            }
            if (acl->acl_ace[i].ace_rights & KAUTH_VNODE_DELETE) {
                strlcat(buffer, "delete ", buf_len);
            }
            if (acl->acl_ace[i].ace_rights & KAUTH_VNODE_APPEND_DATA) {
                strlcat(buffer, "append ", buf_len);
            }
            if (acl->acl_ace[i].ace_rights & KAUTH_VNODE_DELETE_CHILD) {
                strlcat(buffer, "delete_child ", buf_len);
            }
            if (acl->acl_ace[i].ace_rights & KAUTH_VNODE_READ_ATTRIBUTES) {
                strlcat(buffer, "read_attr ", buf_len);
            }
            if (acl->acl_ace[i].ace_rights & KAUTH_VNODE_WRITE_ATTRIBUTES) {
                strlcat(buffer, "write_attr ", buf_len);
            }
            if (acl->acl_ace[i].ace_rights & KAUTH_VNODE_READ_EXTATTRIBUTES) {
                strlcat(buffer, "read_ext_attr ", buf_len);
            }
            if (acl->acl_ace[i].ace_rights & KAUTH_VNODE_WRITE_EXTATTRIBUTES) {
                strlcat(buffer, "write_ext_attr ", buf_len);
            }
            if (acl->acl_ace[i].ace_rights & KAUTH_VNODE_READ_SECURITY) {
                strlcat(buffer, "read_security ", buf_len);
            }
            if (acl->acl_ace[i].ace_rights & KAUTH_VNODE_WRITE_SECURITY) {
                strlcat(buffer, "write_security ", buf_len);
            }
            if (acl->acl_ace[i].ace_rights & KAUTH_VNODE_CHANGE_OWNER) {
                strlcat(buffer, "change_owner ", buf_len);
            }
            if (acl->acl_ace[i].ace_rights & KAUTH_ACE_GENERIC_READ) {
                strlcat(buffer, "generic_read ", buf_len);
            }
            if (acl->acl_ace[i].ace_rights & KAUTH_ACE_GENERIC_WRITE) {
                strlcat(buffer, "generic_write ", buf_len);
            }
            if (acl->acl_ace[i].ace_rights & KAUTH_ACE_GENERIC_EXECUTE) {
                strlcat(buffer, "generic_execute ", buf_len);
            }
            if (acl->acl_ace[i].ace_rights & KAUTH_ACE_GENERIC_ALL) {
                strlcat(buffer, "generic_all ", buf_len);
            }
            if (acl->acl_ace[i].ace_rights & KAUTH_VNODE_SYNCHRONIZE) {
                strlcat(buffer, "synchronize ", buf_len);
            }
            SMBERROR("\trights 0x%x ( %s) \n", acl->acl_ace[i].ace_rights,
                     buffer);
        }
    }
    else {
        SMBERROR("No ACE's \n");
    }

    if (buffer) {
        SMB_FREE(buffer, M_TEMP);
    }
}
#endif

/*
 * Check to see if this is a temporary uuid, generated by Directory Service
 */
int 
is_memberd_tempuuid(const guid_t *uuidp)
{
	if ((bcmp(uuidp, tmpuuid1, sizeof(tmpuuid1)) == 0) || 
		(bcmp(uuidp, tmpuuid2, sizeof(tmpuuid2)) == 0)) {
		return TRUE;
	}
	return FALSE;
}

/*
 * Free any memory and clear any value used by the acl caching
 * We have a lock around this routine to make sure no one plays
 * with these values until we are done.
 */
void
smbfs_clear_acl_cache(struct smbnode *np)
{
	lck_mtx_lock(&np->f_ACLCacheLock);
    if (np->acl_cache_data) {
        SMB_FREE(np->acl_cache_data, M_TEMP);
    }
	np->acl_cache_data = NULL;
	np->acl_cache_timer = 0;
	np->acl_error = 0;
	np->acl_cache_len = 0;
	lck_mtx_unlock(&np->f_ACLCacheLock);
}

/*
 * Get a pointer to the offset requested. Verify that the offset is 
 * is in bounds and the structure does not go past the end of the buffer.
 */
static void * 
smb_sdoffset(struct ntsecdesc *w_secp, size_t w_seclen, int sd_type)
{
	void	*rt_ptr;
	int32_t	sd_off = 0;
	int32_t	sd_len = 0;
	int32_t	end_len = 0;
	
	if (sd_type == OWNER_SECURITY_INFORMATION) {
		sd_len = (int32_t)sizeof(struct ntsid);
		sd_off = letohl(w_secp->OffsetOwner);
	}
	else if (sd_type == GROUP_SECURITY_INFORMATION) {
		sd_len = (int32_t)sizeof(struct ntsid);		
		sd_off = letohl(w_secp->OffsetGroup);
	}
	else if (sd_type == DACL_SECURITY_INFORMATION) {
		sd_len = (int32_t)sizeof(struct ntacl);		
		sd_off = letohl(w_secp->OffsetDacl);
	}
	else if (sd_type == SACL_SECURITY_INFORMATION) {
		sd_len = (int32_t)sizeof(struct ntacl);		
		sd_off = letohl(w_secp->OffsetSacl);
	}
	
	/* Make sure w_seclen is reasonable, once typed cast */
	if ((int32_t)w_seclen < 0)
		return 	NULL;
	
	/* Make sure the length is reasonable */
	if (sd_len > (int32_t)w_seclen)
		return 	NULL;
	
	/* 
	 * Make sure the offset is reasonable. NOTE: We can get a zero offset which
	 * is legal, just means no entry was sent. So we just return a null pointer
	 * since that doesn't cause an error.
	 */
	if ((sd_off <= 0) || (sd_off > (int32_t)w_seclen))
		return 	NULL;
	
	/* Make sure adding them together is reasonable */
	end_len = sd_off+sd_len;
	if ((end_len < 0) || (end_len > (int32_t)w_seclen))
		return 	NULL;
	
	rt_ptr = sd_off+(uint8_t *)w_secp;
	
	return 	rt_ptr;
}

/*
 * Used for debugging and writing error messages into the system log. Still  
 * needs to have buffer checking done on it. 
 */
static void 
smb_printsid(struct ntsid *sidptr, char *sidendptr, const char *printstr, 
				  const char *filename, int index, int error)
{
	char sidprintbuf[MAX_SID_PRINTBUFFER];
	char *s = sidprintbuf;
	int subs;
	uint64_t auth = 0;
	unsigned i, *ip;
	size_t len;
	uint32_t *subauthptr = (uint32_t *)((char *)sidptr + sizeof(struct ntsid));
	char *subauthendptr;
	
	bzero(sidprintbuf, MAX_SID_PRINTBUFFER);
	for (i = 0; i < sizeof(sidptr->sid_authority); i++)
		auth = (auth << 8) | sidptr->sid_authority[i];
	s += snprintf(s, MAX_SID_PRINTBUFFER, "S-%u-%llu", sidptr->sid_revision, auth);
	
	subs = sidptr->sid_subauthcount;
	if (subs > KAUTH_NTSID_MAX_AUTHORITIES) {
		SMBERROR("sid_subauthcount > KAUTH_NTSID_MAX_AUTHORITIES : %d\n", subs);
		subs = KAUTH_NTSID_MAX_AUTHORITIES;
	}
	/*
	 * We know that sid_subauthcount has to be less than or equal to 
	 * KAUTH_NTSID_MAX_AUTHORITIES which is currently 16. So the highest
	 * this can go is 16 * sizeof(uint32_t) so no overflow problem here.
	 */
	subauthendptr = (char *)((char *)subauthptr + (subs * sizeof(uint32_t)));
	
	if (subauthendptr > sidendptr) {
		len = MAX_SID_PRINTBUFFER - (s - sidprintbuf);
		(void)snprintf(s, len, " buffer overflow prevented: %p > %p", 
					   subauthendptr, sidendptr); 
		return;		
	}
	
	for (ip = subauthptr; subs--; ip++)  { 
		len = MAX_SID_PRINTBUFFER - (s - sidprintbuf);
		DBG_ASSERT(len > 0)
		s += snprintf(s, len, "-%u", *ip); 
	}
	
	if (error) {
		SMBWARNING("%s: sid[%d] = %s error = %d %s%s\n", printstr, index, 
				   sidprintbuf, error, (filename) ? "for " : "", filename);
	} else {
		SMBWARNING("%s: sid[%d] = %s %s%s\n", printstr, index, sidprintbuf, 
				   (filename) ? "for " : "", filename);
	}
}

static int 
smb_sid_is_equal(const ntsid_t * rhs, const ntsid_t * lhs)
{
	if (rhs->sid_kind != lhs->sid_kind) {
		return 0;
	}
	
	if (rhs->sid_authcount != lhs->sid_authcount) {
		return 0;
	}
	
	if (bcmp(rhs->sid_authority, lhs->sid_authority,
			 sizeof(rhs->sid_authority)) != 0) {
		return 0;
	}
	
	if (bcmp(rhs->sid_authorities, lhs->sid_authorities,
			 sizeof(uint32_t) * rhs->sid_authcount) != 0) {
		return 0;
	}
	return 1;
}

/* 
 * Return 1 or 0, depending on whether the SID is in the domain given by the 
 * domain SID. 
 */
static int 
smb_sid_in_domain(const ntsid_t * domain, const ntsid_t * sid)
{
	ntsid_t tmp = *sid;
	
	if (tmp.sid_authcount == 0) {
		SMBDEBUG("Bogus network sid sid_authcount = %d\n", tmp.sid_authcount);
		return 0;
	}
	tmp.sid_authcount -= 1;
	return smb_sid_is_equal(domain, &tmp);
}

/*
 * If a Windows Server 2008 R2 NFS share is configured to enable Unmapped UNIX 
 * User Access and there is no existing mapping available to the NFSserver (via 
 * either [MS-UNMP] or [RFC2307]) then the server will encode the owner, group, 
 * and mode of a file into a security descriptor directly using generated SIDs. 
 * The NFSserver uses a specific sub-authority (SECURITY_NFS_ID_BASE_RID == 0x00000058) 
 * relative to the well known authority "NT Authority" (SECURITY_NT_AUTHORITY == {0,0,0,0,0,5}). 
 * The NFSserver then uses further relative sub-authorities to build SIDs for 
 * different NfsTypes that represent the owner (0x00000001), the group (0x00000002), 
 * and the permissions mask (0x00000003) for the file. A further SID is also 
 * generated, which is used to store the other, or world access mask, within the 
 * security descriptor.
 *
 * "<NTSecurityAuthority>-<SECURITY_NFS_ID_BASE_RID>-<NfsSidType>-<NfsSidValue>"
 *  
 * To construct a complete security descriptor, the NFSserver generates a set 
 * of NFS-specific SIDs based on the UID, GID, and mode bits to be represented:
 *  
 * Owner SID based on the UID (for example, "S-1-5-88-1-<uid>")
 * Group SID based on the GID (for example, "S-1-5-88-2-<gid>")
 * Mode SID based on the UNIX mode bits (for example, "S-1-5-88-3-<mode>")
 * Other SID, a constant value (for example, "S-1-5-88-4")
 */
static const uint8_t security_nt_authority[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x05};
#define SECURITY_NFS_ID_BASE_RID	0x00000058
enum {
	NfsSidTypeOwner = 1,
	NfsSidTypeGroup = 2,
	NfsSidTypeModes = 3,
	NfsSidTypeOther = 4
};

static Boolean 
WindowsNfsSID(struct smbnode *np, ntsid_t *sidptr)
{
	if ((sidptr->sid_kind == 1) && (sidptr->sid_authcount == 3) && 
		(memcmp(sidptr->sid_authority, security_nt_authority, sizeof(security_nt_authority)) == 0) && 
		(sidptr->sid_authorities[0] == SECURITY_NFS_ID_BASE_RID)) {
		
		switch (sidptr->sid_authorities[1]) {
			case NfsSidTypeOwner:
				SMB_LOG_ACCESS_LOCK(np, "%s has a NfsSidTypeOwner of %d\n",
                                    np->n_name, sidptr->sid_authorities[2]);
				np->n_nfs_uid = sidptr->sid_authorities[2];
				break;
			case NfsSidTypeGroup:
				SMB_LOG_ACCESS_LOCK(np, "%s has a NfsSidTypeGroup of %d\n",
                                    np->n_name, sidptr->sid_authorities[2]);
				np->n_nfs_gid = sidptr->sid_authorities[2];
				break;
			case NfsSidTypeModes:
				SMB_LOG_ACCESS_LOCK(np, "%s has a NfsSidTypeModes of O%o\n",
                                    np->n_name, sidptr->sid_authorities[2]);
				np->n_flag |= NHAS_POSIXMODES;
				np->n_mode &= ~ACCESSPERMS;
				np->n_mode |= (mode_t)(sidptr->sid_authorities[2] & ACCESSPERMS);
				break;
			case NfsSidTypeOther:
				SMB_LOG_ACCESS_LOCK(np, "%s has a NfsSidTypeOther of O%o\n",
                                    np->n_name, sidptr->sid_authorities[2]);
				break;
			default:
				SMB_LOG_ACCESS_LOCK(np, "%s: unknown NfsSidType of 0x%x 0x%x\n", np->n_name,
                                    sidptr->sid_authorities[1], sidptr->sid_authorities[2]);
				break;
		}
		return TRUE;
	}
	return FALSE;
}

/*
 * The calling routine will guarantee that sidptr has enough space to hold the 
 * ntsid structure, but we need to protect ourself from going pass the size
 * of the ntsid structure and any values contain inside the ntsid structure.
 *
 * We no longer need to call smb_sid_endianize before calling this routine. We
 * now do it place, should help with performance.
 */
static void 
smb_sid2sid16(struct ntsid *sidptr, ntsid_t *sid16ptr, char *sidendptr)
{
	uint8_t ii;
	uint32_t *subauthptr = (uint32_t *)((char *)sidptr + sizeof(struct ntsid));
	char *subauthendptr;
	
	bzero(sid16ptr, sizeof(*sid16ptr));
	if (sidptr->sid_subauthcount > KAUTH_NTSID_MAX_AUTHORITIES) {
		SMBERROR("sidp->sid_subauthcount count too big: %d\n", 
				 sidptr->sid_subauthcount);
		return;
	}
	
	/*
	 * We know that sid_subauthcount has to be less than or equal to 
	 * KAUTH_NTSID_MAX_AUTHORITIES which is currently 16. So the highest
	 * this can go is 16 * sizeof(uint32_t) so no overflow problem here.
	 */
	subauthendptr = (char *)((char *)subauthptr + 
							 (sidptr->sid_subauthcount * sizeof(uint32_t)));
	if (subauthendptr > sidendptr) {
		SMBERROR("Too many sid authorities: %p %p\n", subauthendptr, sidendptr);
		return;		
	}
	sid16ptr->sid_kind = sidptr->sid_revision;
	sid16ptr->sid_authcount = sidptr->sid_subauthcount;
	
	/* Why not just a bcopy? */
	for (ii = 0; ii < sizeof(sid16ptr->sid_authority); ii++)
		sid16ptr->sid_authority[ii] = sidptr->sid_authority[ii];
	
	for (ii = 0; ii < sid16ptr->sid_authcount; ii++) {
		sid16ptr->sid_authorities[ii] = letohl(*subauthptr);
		subauthptr++;
	}
}

/*
 * The calling routine will guarantee that sidptr has enough space to hold the 
 * ntsid structure, but we need to protect ourself from going pass the size
 * of the ntsid structure and any values contain inside the ntsid structure.
 *
 * Really over kill, but what the heck lets double check that the user land
 * code didn't send us something bad.
 */
static void 
smb_sid_endianize(struct ntsid *sidptr, size_t len)
{
	char *sidendptr = (char *)sidptr + len;
	uint32_t *subauthptr = (uint32_t *)((char *)sidptr + sizeof(struct ntsid));
	char *subauthendptr;
	int n;
	
	/*
	 * We know that sid_subauthcount has to be less than or equal to 
	 * KAUTH_NTSID_MAX_AUTHORITIES which is currently 16. So the highest
	 * this can go is 16 * sizeof(uint32_t) so no overflow problem here.
	 */
	subauthendptr = (char *)((char *)subauthptr + 
							 (sidptr->sid_subauthcount * sizeof(uint32_t)));	
	if (subauthendptr > sidendptr) {
		SMBERROR("Too many sid authorities: %p %p\n", subauthendptr, sidendptr);
		return;		
	}
	
	n = sidptr->sid_subauthcount;
	while (n--) {
		*subauthptr = letohl(*subauthptr);
		subauthptr++;
	}
}

/*
 * This is the main routine that goes across the network to get our acl 
 * information. We now always ask for everything so we can make less calls. If 
 * the cache data is up to date then we will return that information. We also do 
 * negative caching, if the server returns an error we cache that fact and 
 * continue to return the error until the cache information times out. 
 *
 * Remember that the vfs will help us with caching, but not in the negative case. 
 * Also it does not solve the problem of multiple different calls coming into us 
 * back to back. So in a typical case we will get the following calls and they
 * will require an acl lookup for each item.
 * 
 * UID and GID request
 * Do we have write access
 * Do we have read access
 * Do we have search/excute access
 *
 * So by caching we are removing 12 network calls for each file in a directory. 
 * We only hold on to this cache for a very short time, because it has a memory 
 * cost that we don't want to pay for any real length of time. This is ok, becasue 
 * one we go through this process the vfs layer will handle the longer caching of 
 * these request.
 */
static int 
smbfs_update_acl_cache(struct smb_share *share, struct smbnode *np, 
					   vfs_context_t context, struct ntsecdesc **w_sec, 
					   size_t *seclen)
{
	uint32_t selector = OWNER_SECURITY_INFORMATION | 
						GROUP_SECURITY_INFORMATION | 
						DACL_SECURITY_INFORMATION;
	SMBFID fid = 0;
	struct timespec	ts;
	struct ntsecdesc *acl_cache_data = NULL;
	size_t acl_cache_len = 0;
	int	error = 0;
	time_t attrtimeo;
	int use_cached_data = 0;
    
    /* If we are in reconnect, use cached data if we have it */
    if (np->acl_cache_timer != 0) {
        use_cached_data = (share->ss_flags & SMBS_RECONNECTING);
    }

	/* Check to see if the cache has timed out */
    SMB_CACHE_TIME(ts, np, attrtimeo);
    if (((ts.tv_sec - np->acl_cache_timer) <= attrtimeo) ||
        use_cached_data) {
		/* Ok the cache is still good take a lock and retrieve the data */
		lck_mtx_lock(&np->f_ACLCacheLock);
        
        SMB_CACHE_TIME(ts, np, attrtimeo);
        if (((ts.tv_sec - np->acl_cache_timer) <= attrtimeo) ||
            use_cached_data) {
			/* We have the lock and the cache is still good, use the cached ACL */
			goto done;
		}
        else {
			/*
			 * Cache expired while we were waiting on the lock, release the lock
			 * and get the ACL from the network.
			 */
			lck_mtx_unlock(&np->f_ACLCacheLock);
		}
    }
	
    if (!(SSTOVC(share)->vc_flags & SMBV_SMB2)) {
        /* Only open file if its SMB 1 */
        error = smbfs_tmpopen(share, np, SMB2_READ_CONTROL, &fid, context);
    }

	if (error == 0) {
		int cerror;
		
        error = smbfs_smb_getsec(share, np, SMB2_READ_CONTROL | SMB2_SYNCHRONIZE, fid, selector,
                                 (struct ntsecdesc **)&acl_cache_data, 
                                 &acl_cache_len, context);
        
        if (!(SSTOVC(share)->vc_flags & SMBV_SMB2)) {
            /* Only close file if its SMB 1 */
            cerror = smbfs_tmpclose(share, np, fid, context);
            if (cerror) {
                SMBWARNING_LOCK(np, "error %d closing fid %llx file %s\n",
                                cerror, fid, np->n_name);
            }
        }

        if ((error == 0) && (acl_cache_data == NULL))
			error = EBADRPC;
	}
	
	/* Don't let anyone play with the acl cache until we are done */
	lck_mtx_lock(&np->f_ACLCacheLock);
    
    if ((error == ETIMEDOUT) && (np->acl_cache_timer != 0)) {
        /* Just return the cached data */
        error = 0;
        np->acl_error = error;
        goto done;
    }
    
	/* Free the old data no longer needed */
	if (np->acl_cache_data)
		SMB_FREE(np->acl_cache_data, M_TEMP);
    
	np->acl_cache_data = acl_cache_data;
	np->acl_cache_len = acl_cache_len;
	np->acl_error = error;
    
	/* We have new information reset our timer  */
    np->acl_cache_timer = ts.tv_sec;
	
done:
	if (np->acl_error || (np->acl_cache_data == NULL)) {
		*w_sec = NULL;
		*seclen = 0;
		if (np->acl_error == 0)
			np->acl_error =  EBADRPC; /* Should never happen, but just to be safe */
	} else {
		SMB_MALLOC(*w_sec, struct ntsecdesc *, np->acl_cache_len, M_TEMP, M_WAITOK);
		if (*w_sec) {
			*seclen = np->acl_cache_len;		
			bcopy(np->acl_cache_data, *w_sec, np->acl_cache_len);
		} else {
			*w_sec = np->acl_cache_data;
			*seclen = np->acl_cache_len;
			np->acl_cache_data = NULL;
			np->acl_cache_len = 0;
			np->acl_cache_timer = 0;
		}
	}
	error = np->acl_error;
	lck_mtx_unlock(&np->f_ACLCacheLock);
	return error;	
}

/*
 * Universal routine for getting the UUID/GUID and setting the nodes 
 * uid/gid. We will get the nodes UUID and set its uid if the owner flag
 * is set, otherwise we get the nodes GUID and set its gid. 
 */
static void 
smbfs_set_node_identifier(struct smbnode *np, struct ntsecdesc *w_sec, 
						  size_t seclen, guid_t *unique_identifier, int owner)
{	
	struct smbmount *smp = np->n_mount;
	struct ntsid	*w_sidp = NULL;
	ntsid_t			sid;
	uid_t			*node_identifier;
	int				error;
	
	if (owner) {
		if (w_sec)	/* Getting the security descriptor failed */
			w_sidp = sdowner(w_sec, seclen);
		node_identifier = &np->n_uid;
	} else {
		if (w_sec)	/* Getting the security descriptor failed */
			w_sidp = sdgroup(w_sec, seclen);
		node_identifier = &np->n_gid;
	}
	
	if (!w_sidp || !w_sec) {
		SMB_LOG_ACCESS_LOCK(np, "no %s sid received, file %s\n",
                            (owner) ? "user" : "group", np->n_name);
		goto error_out;
	}
	
	smb_sid2sid16(w_sidp, &sid, (char*)w_sec+seclen);
	/* We are mapping the owner id, so if its a match replace it with the local id */
	if (owner && (smp->sm_flags & MNT_MAPS_NETWORK_LOCAL_USER) && 
		(bcmp(&smp->ntwrk_sids[0], &sid, sizeof(sid)) == 0)) {
		*unique_identifier = smp->sm_args.uuid;
		*node_identifier = smp->sm_args.uid;
		return; /* We are done */
	}
	
	error = kauth_cred_ntsid2guid(&sid, unique_identifier);
	if (error) {
		if (smbfs_loglevel == SMB_ACL_LOG_LEVEL) {
            lck_rw_lock_shared(&np->n_name_rwlock);
			smb_printsid(w_sidp, (char*)w_sec+seclen, "Owner/Group lookup failed",
						 (const char  *)np->n_name, 0, error);
            lck_rw_unlock_shared(&np->n_name_rwlock);
        }
		goto error_out;
	} 
	
	/* 
	 * This is a cheap call since we lookup the uuid/guid above, so the kernel 
	 * will have the uid or gid in its cache. If we get a temp gid/uid and we
	 * already have a uid/gid always use the one we already have.
	 */
	if (is_memberd_tempuuid(unique_identifier) && 
		(*node_identifier != KAUTH_UID_NONE)) {
		return; /* We already have a real uid/gid from the server keep using it */
	}
	
	if (owner)
		error = kauth_cred_ntsid2uid(&sid, node_identifier);
	else
		error = kauth_cred_ntsid2gid(&sid, node_identifier);
	if (error == 0)
		return; /* We are done */
	
error_out:
	/* Not sure what else to do here, so we default to the mounted users uid/gid */
	if (*node_identifier == KAUTH_UID_NONE)
		*node_identifier = (owner) ? smp->sm_args.uid : smp->sm_args.gid;
	/* Something bad happen and we didn't get the UUID/GUID */
	if (kauth_guid_equal(unique_identifier, &kauth_null_guid)) {
		/* At this point we have a uid/gid, so use it to get the UUID/GUID */
		if (owner)
			error = kauth_cred_uid2guid(*node_identifier, unique_identifier);
		else 
			error = kauth_cred_gid2guid(*node_identifier, unique_identifier);
		/* Should never error out in the case, but just in case lets log it */
		if (error) {
			SMB_LOG_ACCESS_LOCK(np, "%s couldn't translate the uid/gid %d to a UUID/GUID, error = %d\n",
                                np->n_name, *node_identifier, error);
		}
	}
}

/*
 * This routine will retrieve the owner, group and any ACLs associate with
 * this node. We treat an access error the same as an empty security descriptor.
 *
 * The calling routine must hold a reference on the share
 *
 */
int 
smbfs_getsecurity(struct smb_share *share, struct smbnode *np,
                  struct vnode_attr *vap, vfs_context_t context)
{
	struct smbmount		*smp = np->n_mount;
	int					error;
	struct ntsecdesc	*w_sec;	/* Wire sec descriptor */
	size_t				seclen = 0;
	kauth_acl_t			res = NULL;	/* acl result buffer */
	
	/* We do not support acl access on a stream node */
	if (vnode_isnamedstream(np->n_vnode))
		return EINVAL;
	
    SMB_LOG_KTRACE(SMB_DBG_SMBFS_GET_SEC | DBG_FUNC_START, 0, 0, 0, 0, 0);

	if (VATTR_IS_ACTIVE(vap, va_acl))
		vap->va_acl = NULL;					/* default */
	
	if (VATTR_IS_ACTIVE(vap, va_guuid))
		vap->va_guuid = kauth_null_guid;	/* default */
	
	if (VATTR_IS_ACTIVE(vap, va_uuuid))
		vap->va_uuuid = kauth_null_guid;	/* default */
	
	/* Check to make sure we have current acl information */
	error = smbfs_update_acl_cache(share, np, context, &w_sec, &seclen);
	if (error) {
		if (w_sec)
			SMB_FREE(w_sec, M_TEMP);
		w_sec = NULL;
		/* 
		 * When should we eat the error and when shouldn't we, that is the
		 * real question? Any error here will fail the copy engine. Not sure 
		 * thats what we really want. We currently only ignore an access error,
		 * but in the future we may want return all errors or none. The old
		 * code ignored all errors, now we only ignore EACCES.
		 */
		if (error == EACCES) {
			error = 0;
		}
        else {
			SMB_LOG_ACCESS_LOCK(np, "smbfs_update_acl_cache of %s failed with error = %d\n",
                                np->n_name, error);
		}
	}
	
	/* 
	 * The smbfs_set_node_identifier routine will check to see if w_sec
	 * is null. If null it will do what is need to return the correct 
	 * values.
     *
     * smbfs_set_node_identifier will set the np->n_uid/n_gid based on the 
     * ACL values returned by the server
	 */
	if (VATTR_IS_ACTIVE(vap, va_guuid)) {
		smbfs_set_node_identifier(np, w_sec, seclen, &vap->va_guuid, FALSE);
	}
	if (VATTR_IS_ACTIVE(vap, va_uuuid)) {
		smbfs_set_node_identifier(np, w_sec, seclen, &vap->va_uuuid, TRUE);
	}
	
	if (VATTR_IS_ACTIVE(vap, va_acl)) {
		struct ntacl		*w_dacl = NULL;
		char				*endptr;
		uint32_t			acecount, j, aflags;
		struct ntsid		*w_sidp;	/* Wire SID */
		struct ntace		*w_acep = NULL;	/* Wire ACE */
		kauth_ace_rights_t	arights;
		uint32_t			w_rights;
		ntsid_t				sid;	/* temporary, for a kauth sid */
		
		if (w_sec)
			w_dacl = sddacl(w_sec, seclen);
		if (!w_dacl || !w_sec)
			goto exit;
		/* Is there anything we can do to verify acecount, just not sure */
		acecount = letohs(w_dacl->acl_acecount);
		res = kauth_acl_alloc(acecount);
		if (!res) {
			error = ENOMEM;
			goto exit;
		}
		/* Only count entries we add to the array, don't count dropped entries */
		res->acl_entrycount = 0;
		res->acl_flags = letohs(w_sec->ControlFlags);
		if (res->acl_flags & SE_DACL_PROTECTED)
			res->acl_flags |= KAUTH_FILESEC_NO_INHERIT;
		else
			res->acl_flags &= ~KAUTH_FILESEC_NO_INHERIT;
		
		endptr = (char *)w_sec+seclen;
		
		for (j = 0, w_acep = aclace(w_dacl); (((char *)acesid(w_acep) < endptr) && 
				(j < acecount));  j++, w_acep = aceace(w_acep)) {
			int	warn_error = 0;
			
			switch(acetype(w_acep)) {
			    case ACCESS_ALLOWED_ACE_TYPE:
					aflags = KAUTH_ACE_PERMIT;
					break;
			    case ACCESS_DENIED_ACE_TYPE:
					aflags = KAUTH_ACE_DENY;
					break;
			    case SYSTEM_AUDIT_ACE_TYPE:
					aflags = KAUTH_ACE_AUDIT;
					break;
			    case SYSTEM_ALARM_ACE_TYPE:
					aflags = KAUTH_ACE_ALARM;
					break;
			    default:
					SMBERROR_LOCK(np, "ACE type %d file(%s)\n", acetype(w_acep), np->n_name);
					error = EPROTO;	/* Should it be EIO */
					goto exit;
			}
			w_sidp = acesid(w_acep);
			if ((char *)w_sidp+sizeof(*w_sidp) > endptr) {
				SMBERROR_LOCK(np, "ACE type %d file(%s) would have caused a buffer overrun!\n",
                              acetype(w_acep), np->n_name);
                
				error = EPROTO;	/* Should it be EIO */
				goto exit;				
			}
			smb_sid2sid16(w_sidp, &sid, (char*)w_sec+seclen);
			if (WindowsNfsSID(np, &sid)) {
				continue;
			}
			if ((smp->sm_flags & MNT_MAPS_NETWORK_LOCAL_USER) && 
				(bcmp(&smp->ntwrk_sids[0], &sid, sizeof(sid)) == 0)) {
				res->acl_ace[res->acl_entrycount].ace_applicable = smp->sm_args.uuid;
			} else {
				warn_error = kauth_cred_ntsid2guid(&sid, &res->acl_ace[res->acl_entrycount].ace_applicable);
			}
			if (warn_error) {
				if (smbfs_loglevel == SMB_ACL_LOG_LEVEL) {
                    lck_rw_lock_shared(&np->n_name_rwlock);
					smb_printsid(w_sidp, (char*)w_sec+seclen, "ACL lookup failed",
								 (const char  *)np->n_name, j, warn_error);
                    lck_rw_unlock_shared(&np->n_name_rwlock);
				}
				continue;
			}
#if DEBUG_ACLS
            else {
                lck_rw_lock_shared(&np->n_name_rwlock);
                smb_printsid(w_sidp, (char*)w_sec+seclen, "sid maps to",
                             (const char  *)np->n_name, j, 0);
                lck_rw_unlock_shared(&np->n_name_rwlock);
                smb_print_guid(&res->acl_ace[res->acl_entrycount].ace_applicable);
            }
#endif
            
			if (aceflags(w_acep) & OBJECT_INHERIT_ACE_FLAG)
				aflags |= KAUTH_ACE_FILE_INHERIT;
			if (aceflags(w_acep) & CONTAINER_INHERIT_ACE_FLAG)
				aflags |= KAUTH_ACE_DIRECTORY_INHERIT;
			if (aceflags(w_acep) & NO_PROPAGATE_INHERIT_ACE_FLAG)
				aflags |= KAUTH_ACE_LIMIT_INHERIT;
			if (aceflags(w_acep) & INHERIT_ONLY_ACE_FLAG)
				aflags |= KAUTH_ACE_ONLY_INHERIT;
			if (aceflags(w_acep) & INHERITED_ACE_FLAG)
				aflags |= KAUTH_ACE_INHERITED;
			if (aceflags(w_acep) & UNDEF_ACE_FLAG) {
				SMBERROR_LOCK(np, "unknown ACE flag on file(%s)\n", np->n_name);
            }
			if (aceflags(w_acep) & SUCCESSFUL_ACCESS_ACE_FLAG)
				aflags |= KAUTH_ACE_SUCCESS;
			if (aceflags(w_acep) & FAILED_ACCESS_ACE_FLAG)
				aflags |= KAUTH_ACE_FAILURE;
			res->acl_ace[res->acl_entrycount].ace_flags = aflags;
			
            w_rights = acerights(w_acep);
			arights = 0;
			if (w_rights & SMB2_GENERIC_READ)
				arights |= KAUTH_ACE_GENERIC_READ;
			if (w_rights & SMB2_GENERIC_WRITE)
				arights |= KAUTH_ACE_GENERIC_WRITE;
			if (w_rights & SMB2_GENERIC_EXECUTE)
				arights |= KAUTH_ACE_GENERIC_EXECUTE;
			if (w_rights & SMB2_GENERIC_ALL)
				arights |= KAUTH_ACE_GENERIC_ALL;
			if (w_rights & SMB2_SYNCHRONIZE)
				arights |= KAUTH_VNODE_SYNCHRONIZE;
			if (w_rights & SMB2_WRITE_OWNER)
				arights |= KAUTH_VNODE_CHANGE_OWNER;
			if (w_rights & SMB2_WRITE_DAC)
				arights |= KAUTH_VNODE_WRITE_SECURITY;
			if (w_rights & SMB2_READ_CONTROL)
				arights |= KAUTH_VNODE_READ_SECURITY;
			if (w_rights & SMB2_DELETE)
				arights |= KAUTH_VNODE_DELETE;
			
			if (w_rights & SMB2_FILE_WRITE_ATTRIBUTES)
				arights |= KAUTH_VNODE_WRITE_ATTRIBUTES;
			if (w_rights & SMB2_FILE_READ_ATTRIBUTES)
				arights |= KAUTH_VNODE_READ_ATTRIBUTES;
			if (w_rights & SMB2_FILE_DELETE_CHILD)
				arights |= KAUTH_VNODE_DELETE_CHILD;
			if (w_rights & SMB2_FILE_EXECUTE)
				arights |= KAUTH_VNODE_EXECUTE;
			if (w_rights & SMB2_FILE_WRITE_EA)
				arights |= KAUTH_VNODE_WRITE_EXTATTRIBUTES;
			if (w_rights & SMB2_FILE_READ_EA)
				arights |= KAUTH_VNODE_READ_EXTATTRIBUTES;
			if (w_rights & SMB2_FILE_APPEND_DATA)
				arights |= KAUTH_VNODE_APPEND_DATA;
			if (w_rights & SMB2_FILE_WRITE_DATA)
				arights |= KAUTH_VNODE_WRITE_DATA;
			if (w_rights & SMB2_FILE_READ_DATA)
				arights |= KAUTH_VNODE_READ_DATA;
			res->acl_ace[res->acl_entrycount].ace_rights = arights;

			/* Success we have an entry, now count it */
			res->acl_entrycount++;
		}
#if DEBUG_ACLS
        smb_print_acl(np, "smbfs_getsecurity", res);
#endif

		/* Only return the acl if we have at least one ace. */ 
		if (res->acl_entrycount) {
			vap->va_acl = res;
			res = NULL;			
		}
	}
	
exit:
	if (VATTR_IS_ACTIVE(vap, va_acl))
		VATTR_SET_SUPPORTED(vap, va_acl);
	if (VATTR_IS_ACTIVE(vap, va_guuid))
		VATTR_SET_SUPPORTED(vap, va_guuid);
	if (VATTR_IS_ACTIVE(vap, va_uuuid))
		VATTR_SET_SUPPORTED(vap, va_uuuid);
	
	if (res)
		kauth_acl_free(res);
	
	if (w_sec)
		SMB_FREE(w_sec, M_TEMP);
	
    SMB_LOG_KTRACE(SMB_DBG_SMBFS_GET_SEC | DBG_FUNC_END, error, 0, 0, 0, 0);
	return error;
}

/*
 * Fill in the network ace used to describe the posix uid, gid and modes.
 */
static struct ntace *
set_nfs_ace(struct ntace *w_acep, ntsid_t *nfs_sid, size_t needed)
{
	struct ntsid	*w_sidp;
	
	wset_acetype(w_acep, ACCESS_DENIED_ACE_TYPE);
	wset_aceflags(w_acep, 0);
	wset_acerights(w_acep, 0);
	w_sidp = acesid(w_acep);
	bcopy(nfs_sid, w_sidp, sizeof(ntsid_t));
	smb_sid_endianize(w_sidp, needed);
	wset_acelen(w_acep, sizeof(struct ntace) + sidlen(w_sidp));			
	return aceace(w_acep);
}

/*
 * This routine will set the owner, group and any ACLs associate with
 * this node. 
 *
 * The calling routine must hold a reference on the share
 *
 */
int 
smbfs_setsecurity(struct smb_share *share, vnode_t vp, struct vnode_attr *vap, 
				  vfs_context_t context)
{
	struct smbnode *np = VTOSMB(vp);
	struct smbmount *smp = np->n_mount;
	uint32_t selector = 0, acecount;
	struct ntsid	*w_usr = NULL, *w_grp = NULL, *w_sidp;
	struct ntacl	*w_dacl = NULL;	/* Wire DACL */
	int error;
	struct ntace *w_acep, *start_acep;	/* Wire ACE */
	struct kauth_ace *acep;
	uint8_t aflags;
	uint32_t arights, openrights;
	size_t needed;
	uint16_t ControlFlags = 0;
	SMBFID	fid = 0;
	uuid_string_t out_str;
	
	/* We do not support acl access on a stream node */
	if (vnode_isnamedstream(vp))
		return ENOTSUP;
	
    SMB_LOG_KTRACE(SMB_DBG_SMBFS_SET_SEC | DBG_FUNC_START, 0, 0, 0, 0, 0);

	openrights = SMB2_READ_CONTROL | SMB2_SYNCHRONIZE;
	error = 0;
    
	if (VATTR_IS_ACTIVE(vap, va_guuid) &&  !kauth_guid_equal(&vap->va_guuid, &kauth_null_guid)) {
		SMB_MALLOC(w_grp, struct ntsid *, MAXSIDLEN, M_TEMP, M_WAITOK);
		bzero(w_grp, MAXSIDLEN);
		error = kauth_cred_guid2ntsid(&vap->va_guuid, (ntsid_t *)w_grp);
		if (error) {
			uuid_unparse(*((const uuid_t *)&vap->va_guuid), out_str);
			SMBERROR("kauth_cred_guid2ntsid failed with va_guuid %s and error %d\n", 
					 out_str, error);
			goto exit;
		}
		smb_sid_endianize(w_grp, MAXSIDLEN);
		openrights |= SMB2_WRITE_OWNER;
		selector |= GROUP_SECURITY_INFORMATION;
	}
    
	if (VATTR_IS_ACTIVE(vap, va_uuuid) && !kauth_guid_equal(&vap->va_uuuid, &kauth_null_guid)) {
		SMB_MALLOC(w_usr, struct ntsid *, MAXSIDLEN, M_TEMP, M_WAITOK);
		bzero(w_usr, MAXSIDLEN);
		/* We are mapping the owner id, so if its a match replace it with the network sid */
		if ((smp->sm_flags & MNT_MAPS_NETWORK_LOCAL_USER) && 
			(kauth_guid_equal(&smp->sm_args.uuid, &vap->va_uuuid))) {
			bcopy(&smp->ntwrk_sids[0], w_usr, sizeof(ntsid_t));
			error = 0;
		} else {
			error = kauth_cred_guid2ntsid(&vap->va_uuuid, (ntsid_t *)w_usr);
		}
		if (error) {
			uuid_unparse(*((const uuid_t *)&vap->va_uuuid), out_str);
			SMBERROR("kauth_cred_guid2ntsid failed with va_uuuid %s and error %d\n", 
					 out_str, error);
			goto exit;
		}
		smb_sid_endianize(w_usr, MAXSIDLEN);
		openrights |= SMB2_WRITE_OWNER;
		selector |= OWNER_SECURITY_INFORMATION;
	}
    
	if (VATTR_IS_ACTIVE(vap, va_acl)) {
		ntsid_t nfs_sid;

		openrights |= SMB2_WRITE_DAC;
		selector |= DACL_SECURITY_INFORMATION;
        
		if (vap->va_acl) {
#if DEBUG_ACLS
            smb_print_acl(np, "smbfs_setsecurity", vap->va_acl);
#endif
			if (vap->va_acl->acl_flags & KAUTH_FILESEC_NO_INHERIT) {
				selector |= PROTECTED_DACL_SECURITY_INFORMATION;
				ControlFlags |= SE_DACL_PROTECTED;
			} else {
				selector |= UNPROTECTED_DACL_SECURITY_INFORMATION;
				ControlFlags &= ~SE_DACL_PROTECTED;
			}
		}
		
		if ((vap->va_acl == NULL) || (vap->va_acl->acl_entrycount == KAUTH_FILESEC_NOACL)) {
			ControlFlags |= SE_DACL_PRESENT;
			/* If we are removing the ACL nothing left to do but set it. */
			goto set_dacl;
		}
		
		if (vap->va_acl->acl_entrycount > KAUTH_ACL_MAX_ENTRIES) {
			SMBERROR_LOCK(np, "acl_entrycount=%d, file(%s)\n",
                          vap->va_acl->acl_entrycount, np->n_name);
			error = EINVAL;
			goto exit;
		}
        
		acecount = vap->va_acl->acl_entrycount;
		if (np->n_nfs_uid != KAUTH_UID_NONE) {
			/* Make room for the Windows NFS UID ACE */
			acecount += 1;
		}
        
		if (np->n_nfs_gid != KAUTH_GID_NONE) {
			/* Make room for the Windows NFS GID ACE */
			acecount += 1;
		}
        
		if (np->n_flag & NHAS_POSIXMODES) {
			/* Make room for the Windows NFS Modes ACE */
			acecount += 1;
		}
        
		needed = sizeof(struct ntacl) + acecount * (sizeof(struct ntace) + MAXSIDLEN);
		SMB_MALLOC(w_dacl, struct ntacl *, needed, M_TEMP, M_WAITOK);
		bzero(w_dacl, needed);
		w_dacl->acl_revision = 0x02;
		wset_aclacecount(w_dacl, acecount);
		
		start_acep = aclace(w_dacl);
		nfs_sid.sid_kind = 1;
		nfs_sid.sid_authcount = 3;
		memcpy(nfs_sid.sid_authority, security_nt_authority, sizeof(security_nt_authority));
		nfs_sid.sid_authorities[0] = SECURITY_NFS_ID_BASE_RID;

		if (np->n_nfs_uid != KAUTH_UID_NONE) {			
			/* Set the Windows nfs uid ace */
			nfs_sid.sid_authorities[1] = NfsSidTypeOwner;
			nfs_sid.sid_authorities[2] = np->n_nfs_uid;
			start_acep = set_nfs_ace(start_acep,&nfs_sid, needed);
			acecount--;
		}
        
		if (np->n_nfs_gid != KAUTH_GID_NONE) {
			/* Set the Windows nfs gid ace */
			nfs_sid.sid_authorities[1] = NfsSidTypeGroup;
			nfs_sid.sid_authorities[2] = np->n_nfs_gid;
			start_acep = set_nfs_ace(start_acep,&nfs_sid, needed);
			acecount--;
		}
        
		if (np->n_flag & NHAS_POSIXMODES) {
			/* Set the Windows nfs posix modes ace */
			nfs_sid.sid_authorities[1] = NfsSidTypeModes;
			nfs_sid.sid_authorities[2] = np->n_mode;
			start_acep = set_nfs_ace(start_acep,&nfs_sid, needed);
			acecount--;
		}

		for (w_acep = start_acep, acep = &vap->va_acl->acl_ace[0];
		     acecount--; w_acep = aceace(w_acep), acep++) {
			switch(acep->ace_flags & KAUTH_ACE_KINDMASK) {
				case KAUTH_ACE_PERMIT:
					wset_acetype(w_acep, ACCESS_ALLOWED_ACE_TYPE);
					break;
			    case KAUTH_ACE_DENY:
					wset_acetype(w_acep, ACCESS_DENIED_ACE_TYPE);
					break;
			    case KAUTH_ACE_AUDIT:
					wset_acetype(w_acep, SYSTEM_AUDIT_ACE_TYPE);
					break;
			    case KAUTH_ACE_ALARM:
					wset_acetype(w_acep, SYSTEM_ALARM_ACE_TYPE);
					break;
			    default:
					SMBERROR_LOCK(np, "ace_flags=0x%x, file(%s)\n",
                                  acep->ace_flags, np->n_name);
					error = EINVAL;
					goto exit;
			}
			aflags = 0;
			if (acep->ace_flags & KAUTH_ACE_INHERITED)
				aflags |= INHERITED_ACE_FLAG;
			if (acep->ace_flags & KAUTH_ACE_FILE_INHERIT)
				aflags |= OBJECT_INHERIT_ACE_FLAG;
			if (acep->ace_flags & KAUTH_ACE_DIRECTORY_INHERIT)
				aflags |= CONTAINER_INHERIT_ACE_FLAG;
			if (acep->ace_flags & KAUTH_ACE_LIMIT_INHERIT)
				aflags |= NO_PROPAGATE_INHERIT_ACE_FLAG;
			if (acep->ace_flags & KAUTH_ACE_ONLY_INHERIT)
				aflags |= INHERIT_ONLY_ACE_FLAG;
			if (acep->ace_flags & KAUTH_ACE_SUCCESS)
				aflags |= SUCCESSFUL_ACCESS_ACE_FLAG;
			if (acep->ace_flags & KAUTH_ACE_FAILURE)
				aflags |= FAILED_ACCESS_ACE_FLAG;
			wset_aceflags(w_acep, aflags);
			arights = 0;
			if (acep->ace_rights & KAUTH_ACE_GENERIC_READ)
				arights |= SMB2_GENERIC_READ;
			if (acep->ace_rights & KAUTH_ACE_GENERIC_WRITE)
				arights |= SMB2_GENERIC_WRITE;
			if (acep->ace_rights & KAUTH_ACE_GENERIC_EXECUTE)
				arights |= SMB2_GENERIC_EXECUTE;
			if (acep->ace_rights & KAUTH_ACE_GENERIC_ALL)
				arights |= SMB2_GENERIC_ALL;
			if (acep->ace_rights & KAUTH_VNODE_SYNCHRONIZE)
				arights |= SMB2_SYNCHRONIZE;
			if (acep->ace_rights & KAUTH_VNODE_CHANGE_OWNER)
				arights |= SMB2_WRITE_OWNER;
			if (acep->ace_rights & KAUTH_VNODE_WRITE_SECURITY)
				arights |= SMB2_WRITE_DAC;
			if (acep->ace_rights & KAUTH_VNODE_READ_SECURITY)
				arights |= SMB2_READ_CONTROL;
			if (acep->ace_rights & KAUTH_VNODE_WRITE_EXTATTRIBUTES)
				arights |= SMB2_FILE_WRITE_EA;
			if (acep->ace_rights & KAUTH_VNODE_READ_EXTATTRIBUTES)
				arights |= SMB2_FILE_READ_EA;
			if (acep->ace_rights & KAUTH_VNODE_WRITE_ATTRIBUTES)
				arights |= SMB2_FILE_WRITE_ATTRIBUTES;
			if (acep->ace_rights & KAUTH_VNODE_READ_ATTRIBUTES)
				arights |= SMB2_FILE_READ_ATTRIBUTES;
			if (acep->ace_rights & KAUTH_VNODE_DELETE_CHILD)
				arights |= SMB2_FILE_DELETE_CHILD;
			if (acep->ace_rights & KAUTH_VNODE_APPEND_DATA)
				arights |= SMB2_FILE_APPEND_DATA;
			if (acep->ace_rights & KAUTH_VNODE_DELETE)
				arights |= SMB2_DELETE;
			if (acep->ace_rights & KAUTH_VNODE_EXECUTE)
				arights |= SMB2_FILE_EXECUTE;
			if (acep->ace_rights & KAUTH_VNODE_WRITE_DATA)
				arights |= SMB2_FILE_WRITE_DATA;
			if (acep->ace_rights & KAUTH_VNODE_READ_DATA)
				arights |= SMB2_FILE_READ_DATA;
            
            /* <15782523> Always set the Synchronize bit for now */
            arights |= SMB2_SYNCHRONIZE;
            
			wset_acerights(w_acep, arights);
			w_sidp = acesid(w_acep);
            
			if ((smp->sm_flags & MNT_MAPS_NETWORK_LOCAL_USER) && 
				(kauth_guid_equal(&smp->sm_args.uuid, &acep->ace_applicable))) {
				bcopy(&smp->ntwrk_sids[0], w_sidp, sizeof(ntsid_t));
			}
            else {
				error = kauth_cred_guid2ntsid(&acep->ace_applicable, (ntsid_t *)w_sidp);
			}
#if DEBUG_ACLS
            lck_rw_lock_shared(&np->n_name_rwlock);
            smb_printsid(w_sidp, (char*)w_sidp+sidlen(w_sidp), "guid maps to",
                             (const char  *)np->n_name, acecount, 0);
            lck_rw_unlock_shared(&np->n_name_rwlock);

            smb_print_guid(&acep->ace_applicable);
#endif 
            
			if (error) {
				uuid_unparse(*((const uuid_t *)&acep->ace_applicable), out_str);
				SMBERROR("kauth_cred_guid2ntsid failed with va_acl %s and error %d\n", 
						 out_str, error);
				goto exit;
			}
			smb_sid_endianize(w_sidp, needed);
			wset_acelen(w_acep, sizeof(struct ntace) + sidlen(w_sidp));
		}
		wset_acllen(w_dacl, ((char *)w_acep - (char *)w_dacl));
	}
	
set_dacl:
    if (!(SSTOVC(share)->vc_flags & SMBV_SMB2)) {
        /* Only open file if its SMB 1 */
        error = smbfs_tmpopen(share, np, openrights, &fid, context);
    }
    
	if (error == 0) {
        error = smbfs_smb_setsec(share, np, openrights,
                                 fid, selector, ControlFlags,
                                 w_usr, w_grp, NULL, w_dacl, context);
        
        if (!(SSTOVC(share)->vc_flags & SMBV_SMB2)) {
            /* Only close file if its SMB 1 */
            (void)smbfs_tmpclose(share, np, fid, context);
        }
	}
exit:
    if (w_usr != NULL) {
        SMB_FREE(w_usr, M_TEMP);
    }
    if (w_grp != NULL) {
        SMB_FREE(w_grp, M_TEMP);
    }
    if (w_dacl != NULL) {
        SMB_FREE(w_dacl, M_TEMP);
    }
    
	/* The current cache is out of date clear it */
	smbfs_clear_acl_cache(np);
    
    SMB_LOG_KTRACE(SMB_DBG_SMBFS_SET_SEC | DBG_FUNC_END, error, 0, 0, 0, 0);
	return (error);
}

/*
* The calling routine must hold a reference on the share
*/
void 
smb_get_sid_list(struct smb_share *share, struct smbmount *smp, struct mdchain *mdp, 
					  uint32_t ntwrk_sids_cnt, uint32_t ntwrk_sid_size)
{
	uint32_t ii;
	int error;
	void *sidbufptr = NULL;
	char *endsidbufptr;
	char *nextsidbufptr;
	struct ntsid *ntwrk_wire_sid;
	ntsid_t *ntwrk_sids = NULL;
	ntsid_t tmpsid;
	uint32_t sidCnt = 0;
		
	if ((ntwrk_sids_cnt == 0) || (ntwrk_sid_size == 0)) {
		SMBDEBUG("ntwrk_sids_cnt = %d ntwrk_sid_size = %d\n", 
				 ntwrk_sids_cnt, ntwrk_sid_size);
		goto done; /* Nothing to do here we are done */
	}
	
	/* Never allocate more than we could have received in this message */
	if (ntwrk_sid_size > SSTOVC(share)->vc_txmax) {
		SMBDEBUG("Too big ntwrk_sid_size = %d\n", ntwrk_sid_size);
		goto done;
	}
	
	/* Max number we will support, about 9K */
	if (ntwrk_sids_cnt > KAUTH_ACL_MAX_ENTRIES) 
		ntwrk_sids_cnt = KAUTH_ACL_MAX_ENTRIES;
	
	SMB_MALLOC(ntwrk_sids, void *, ntwrk_sids_cnt * sizeof(*ntwrk_sids) , M_TEMP, 
		   M_WAITOK | M_ZERO);
	if (ntwrk_sids == NULL) {
		SMBDEBUG("ntwrk_sids malloc failed!\n");
		goto done;		
	}
	SMB_MALLOC(sidbufptr, void *, ntwrk_sid_size, M_TEMP, M_WAITOK);
	if (sidbufptr == NULL) {
		SMBDEBUG("SID malloc failed!\n");
		goto done;
	}
	error = md_get_mem(mdp, sidbufptr, ntwrk_sid_size, MB_MSYSTEM);
	if (error) {
		SMBDEBUG("Could get the list of sids? error = %d\n", error);
		goto done;
	}
	
	endsidbufptr = (char *)sidbufptr + ntwrk_sid_size;
	nextsidbufptr = sidbufptr;
	for (ii = 0; ii < ntwrk_sids_cnt; ii++) {		
		ntwrk_wire_sid = (struct ntsid *)nextsidbufptr;		
		nextsidbufptr += sizeof(*ntwrk_wire_sid);
		/* Make sure we don't overrun our buffer */
		if (nextsidbufptr > endsidbufptr) {
			SMBDEBUG("Network sid[%d] buffer to small start %p current %p end %p\n", 
					 ii, sidbufptr, nextsidbufptr, endsidbufptr);
			break;
		}
		/* 
		 * We are done with nextsidbufptr for this loop, reset it to the next 
		 * entry. The smb_sid2sid16 routine will protect us from any buffer overruns,
		 * so no need to check here.
		 */
		nextsidbufptr += (ntwrk_wire_sid->sid_subauthcount * sizeof(uint32_t));
		
		smb_sid2sid16(ntwrk_wire_sid, &tmpsid, endsidbufptr);
		
		/* Don't store any unix_users or unix_groups sids */
		if (!smb_sid_in_domain(&unix_users_domsid, &tmpsid) &&
			!smb_sid_in_domain(&unix_groups_domsid, &tmpsid)) {
			ntwrk_sids[sidCnt++] = tmpsid;
		} else {
			SMBDEBUG("Skipping ntwrk_wire_sid entry %d\n", ii);
			continue;
		}
		
		if (smbfs_loglevel == SMB_ACL_LOG_LEVEL) {
			smb_printsid(ntwrk_wire_sid, endsidbufptr, "WHOAMI network", NULL, ii, 0);
		}		
	}
	
	/* We skipped some unix_users or unix_groups, resize the buffer down */
	if (sidCnt != ntwrk_sids_cnt) {
		size_t sidarraysize = sidCnt * sizeof(*ntwrk_sids);
		ntsid_t *holdSids = ntwrk_sids;

		ntwrk_sids = NULL;
		SMB_MALLOC(ntwrk_sids, void *, sidarraysize, M_TEMP, M_WAITOK | M_ZERO);
		if (ntwrk_sids) {
			bcopy(holdSids, ntwrk_sids, sidarraysize);
		}
		SMB_FREE(holdSids, M_TEMP);
	}
	
	/*
	 * We found a list of sid returned by the server, we alway use those over
	 * the LSA ones. Remove the LSA ones and mark that we have WHOAMI SIDS.
	 */
	if (sidCnt && ntwrk_sids) {
		SMB_FREE(smp->ntwrk_sids, M_TEMP);
		smp->ntwrk_sids_cnt = sidCnt;
		smp->ntwrk_sids = ntwrk_sids;
		ntwrk_sids = NULL;
		UNIX_CAPS(share) |= UNIX_QFS_POSIX_WHOAMI_SID_CAP;
	}
	
done:
	/* Just clean up */
	SMB_FREE(ntwrk_sids, M_TEMP);
	SMB_FREE(sidbufptr, M_TEMP);
}

/*
 * Need to check to see if the maximum access rights needs to be updated. We
 * use the node's change time to determine when we need to update. We check to
 * see if the node's change time has changed since the last time we got the
 * maximum access rights. The change time is cached as part of the node's meta
 * data. So the maximum access rights is cached based on the node's meta cache
 * timer and the node's change time.
 *
 * Now we have some issues we need to deal with here. We can get the maximum 
 * access rights from the extended open reply. Windows server always return the 
 * correct maximum access rights, but some servers lie. Samba just returns that
 * you have full access. Also if the server doesn't support the extended open
 * reply we never test again and the cache never expired. 
 *
 * For servers that supports the extended open reply we just believe they are
 * returning the correct information.
 *
 * If the server doesn't support the extended open reply, then we will set the
 * maximum access rights to full access. We mark that the call failed so we
 * don't need to update ever agian.
 *
 * The calling routine must hold a reference on the share
 *
 */
uint32_t 
smbfs_get_maximum_access(struct smb_share *share, vnode_t vp, vfs_context_t context)
{
	struct smbnode *np;
	uint32_t maxAccessRights;
    int error;
    SMBFID	fid = 0;
	
	/* 
	 * Need to have the node locked while getting the maximum access rights. In
	 * the future we may want to only lock what we need.
	 */
	if (smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK))
		return 0;
	
    SMB_LOG_KTRACE(SMB_DBG_SMBFS_GET_MAX_ACCESS | DBG_FUNC_START, 0, 0, 0, 0, 0);

	np = VTOSMB(vp);
	np->n_lastvop = smbfs_get_maximum_access;
    
	/*
	 * We can't open a reparse point that has a Dfs tag, so don't even try. Let
	 * the server handle any security issues.
	 */
	if ((np->n_dosattr & SMB_EFA_REPARSE_POINT) && 
        (np->n_reparse_tag == IO_REPARSE_TAG_DFS)) {
		np->maxAccessRights = SA_RIGHT_FILE_ALL_ACCESS | STD_RIGHT_ALL_ACCESS;
		goto done;
	}
    
	/* This server doesn't support maximum access, just lie */
	if (np->n_flag & NO_EXTENDEDOPEN) {
        /* When smb1fs_smb_ntcreatex() sets NO_EXTENDEDOPEN, it also sets
         * np->maxAccessRights to "all access".  So we don't need to modify
         * np->maxAccessRights here.
         */
		goto done;
    }
    
    /*
     * This server doesn't support extended security, so assume
     * it doesn't support maximum access. We grant all access and
     * let the server make the final call.
     */
	if (!(VC_CAPS(SSTOVC(share)) & SMB_CAP_EXT_SECURITY)) {
        np->maxAccessRights = SA_RIGHT_FILE_ALL_ACCESS | STD_RIGHT_ALL_ACCESS;
		goto done;
    }

	/* Cache is still good, return the last value we had */
	if (timespeccmp(&np->maxAccessRightChTime, &np->n_chtime, ==))
		goto done;

    /* 
	 * For Windows we only need to open and close the item. Need to see if 
	 * we can write a routine that does a CreateAndX/close in one message.  
	 * Also Windows server allow us to get the maximum access by opening the 
	 * file without requesting access. This allows us to get the maximum  
	 * access, even when we can't read the security descriptor. We need to 
	 * test with other servers and see if they behave the same as windows or 
	 * do we need to open them with SMB2_READ_CONTROL?
	 *
	 * <9874997> In Lion, we started using maximal access returned by the
	 * server. One odd setup with a Windows 2003 server where the maximal
	 * access returned in the Tree Connect response (share ACL) gave more 
	 * access than the maximal access given by CreateAndX on the '\' folder
	 * (filesystem ACL).
	 * Treat the root folder as a special case.
	 * 1) If the CreateAndX fails on the root, then assume full access
	 * 2) If the CreateAndX works on the root, if no ReadAttr or Execute BUT
	 * the share ACL grants ReadAttr or Execute then assume '\' also has
	 * ReadAttr or Execute.
	 */
    /*
	 * We could solve a lot of headaches by testing for the servers that do
	 * not support opening the item with no access. Something like the following
	 * should work (accessOpenModes defaults to zero):
	 *
	 * error = smbfs_tmpopen(share, np, share->accessOpenModes, &fid, context);
	 * if (error && share->firstAccessOpen) {
	 *      share->accessOpenModes = SMB2_READ_CONTROL;
	 *      error = smbfs_tmpopen(share, np, share->accessOpenModes, &fid, context);
	 *  }
	 *  share->firstAccessOpen = TRUE;
	 *
	 * At this point we should just trust what they say, may want to make an exception
	 * for the root node, and non darwin Unix systems.
	 */

	 if (SSTOVC(share)->vc_flags & SMBV_SMB2) {
        struct smbfattr *fap = NULL;
        uint32_t desired_access = 0;
        enum vtype vnode_type = vnode_isdir(np->n_vnode) ? VDIR : VREG;
        uint32_t share_access = NTCREATEX_SHARE_ACCESS_ALL;
        uint64_t create_flags = SMB2_CREATE_GET_MAX_ACCESS;
        uint32_t ntstatus = 0;

        /* 
         * Do a compound create/close 
         * Note: this always does a create/close over the wire and never
         * uses an existing open file like smbfs_tmpopen can do.
         */
        SMB_MALLOC(fap, 
                    struct smbfattr *, 
                    sizeof(struct smbfattr), 
                    M_SMBTEMP, 
                    M_WAITOK | M_ZERO);
        if (fap == NULL) {
            SMBERROR("SMB_MALLOC failed\n");
            error = ENOMEM;
        }
        else {
            /* Send a Create/Close */
            error = smb2fs_smb_cmpd_create(share, np,
                                           NULL, 0,
                                           NULL, 0,
                                           desired_access, vnode_type,
                                           share_access, FILE_OPEN,
                                           create_flags, &ntstatus,
                                           NULL, fap,
                                           NULL, context);
            /* 
             * smb2fs_smb_cmpd_create() will update the vnodes 
             * maxAccessRights
             */
                
            SMB_FREE(fap, M_SMBTEMP);
        }
    }
    else {
        error = smbfs_tmpopen(share, np, 0, &fid, context);
        if (!error) {
            smbfs_tmpclose(share, np, fid, context);
        }
    }
        
    if (error) {
        if (error != EACCES) {
            /* We have no idea why it failed, give them full access. */
            np->maxAccessRights = SA_RIGHT_FILE_ALL_ACCESS | STD_RIGHT_ALL_ACCESS;
        } else {
            if (vnode_isvroot(np->n_vnode)) {
                np->maxAccessRights = SA_RIGHT_FILE_ALL_ACCESS | STD_RIGHT_ALL_ACCESS;
            } else if (((!UNIX_SERVER(SSTOVC(share))) || (SSTOVC(share)->vc_flags & SMBV_DARWIN))) {
                /* Windows or Darwin Server and they told us we have no access. */
                np->maxAccessRights = 0;
            } else {
                np->maxAccessRights = SA_RIGHT_FILE_ALL_ACCESS | STD_RIGHT_ALL_ACCESS;
            }
        }

        SMB_LOG_ACCESS_LOCK(np, "Opening %s failed with error = %d, granting %s access\n",
                            np->n_name, error, (np->maxAccessRights) ? "full" : "no");

        /*
         * The open call failed so the cache timer wasn't set, so do that
         * here
         */
        np->maxAccessRightChTime = np->n_chtime;
    }
    else {
        /* Special case the root vnode */
        if (vnode_isvroot(np->n_vnode)) {
            /*
             * its the root folder, if no Execute, but share grants
             * Execute then grant Execute to root folder
             */
            if ((!(np->maxAccessRights & SMB2_FILE_EXECUTE)) &&
                (share->maxAccessRights & SMB2_FILE_EXECUTE)) {
                np->maxAccessRights |= SMB2_FILE_EXECUTE;
            }
            
            /*
             * its the root, if no ReadAttr, but share grants
             * ReadAttr then grant ReadAttr to root
             */
            if ((!(np->maxAccessRights & SMB2_FILE_READ_ATTRIBUTES)) &&
                (share->maxAccessRights & SMB2_FILE_READ_ATTRIBUTES)) {
                np->maxAccessRights |= SMB2_FILE_READ_ATTRIBUTES;
            }
        }
    }

	SMB_LOG_ACCESS_LOCK(np, "%s maxAccessRights = 0x%x\n", np->n_name, np->maxAccessRights);

done:
	maxAccessRights = np->maxAccessRights;
	smbnode_unlock(VTOSMB(vp));

    SMB_LOG_KTRACE(SMB_DBG_SMBFS_GET_MAX_ACCESS | DBG_FUNC_END, 0, 0, 0, 0, 0);
    return maxAccessRights;
}

/* 
 * The composition:
 *
 * (RED, REA, RID, RIA) is what we receive with the vnop. Those are:
 *	RED - Requested Explicit Deny
 *	REA - Requested Explicit Allow
 *	RID - Requested Inherited Deny
 *	RIA - Requested Inherited Allow
 * That's the canonical order the ACEs should have arrived in, but in
 * reality we should never get inherited ACE here and the order could
 * be different depending on what the calling application is trying to
 * accomplish. If copying the item they may want to add a full ACE as
 * the first element and then remove it when they are done. We no longer
 * inforce the order at our level.
 *
 * (SED, SEA, SID, SIA) is what we receive from the server. Those are:
 *	SED - Server Explicit (defaulted) Deny
 *	SEA - Server Explicit (defaulted) Allow
 *	SID - Server Inherited Deny
 *	SIA - Server Inherited Allow
 *
 * This is the canonical order the ACEs should have arrived in, but in
 * reality we should only get inherited ACE here. Now Samba will send 
 * us a DIRECT ACE that represents the POSIX MODES. We always trust that
 * the server has these stored in the correct canonical.
 *
 * NOTE: Windows normally has an allow-all ACE for the object owner and 
 * another allow ACE for Local System.
 *
 * NOTE: If we were going the put these in canonical orer this is what
 * we would need to do. We would take the (RED, REA, RID, RIA) and the 
 * (SED, SEA, SID, SIA) and write back (SED, RED, SEA, REA, SID, RID, SIA, RIA)
 * All non-deny ACEs, for instance audit or alarm types, can be
 * treated the same w/r/t canonicalizing the ACE order.
 *
 * With that said this is what we do here. We create a new ACL that is large
 * enough to hold both ACLS. We remove any inherited ACEs that are in the 
 * VNOP ACL. We then combind the two set of ACEs into one set VNOP ACEs first
 * followed by the SERVER ACEs. Since we are only adding direct ACEs and the 
 * server ACEs should all be inherited this sould be correct.
 */
int 
smbfs_compose_create_acl(struct vnode_attr *vap, struct vnode_attr *svrva, 
						 kauth_acl_t *savedacl)
{
	int32_t entries, allocated;
	struct kauth_ace *acep;
	kauth_acl_t newacl;
	uint32_t j;

	allocated = vap->va_acl->acl_entrycount + svrva->va_acl->acl_entrycount;
	newacl = kauth_acl_alloc(allocated);
	if (newacl == NULL) {
		SMBERROR("kauth_acl_alloc, %d\n", allocated);
		return ENOMEM;
	}
	
	newacl->acl_flags = svrva->va_acl->acl_flags;
	entries = 0;		/* output index for ACL we're building */
	
	/* First add the vnop ACEs, skipping any inherited ACEs or dups */
	for (j = 0; j < vap->va_acl->acl_entrycount; j++) {
		acep = &vap->va_acl->acl_ace[j];
		if (acep->ace_flags & KAUTH_ACE_INHERITED) {
			SMBERROR("Skipping ACE becuase VNOP is trying to set an inherited ACE\n");
			continue;
		}
		newacl->acl_ace[entries++] = *acep;
		if (entries > allocated) {
			kauth_acl_free(newacl);
			return EINVAL;
		}
	}
	
	/* Now add the create ACE assume they are all inherited. */
	for (j = 0; j < svrva->va_acl->acl_entrycount; j++) {
		acep = &svrva->va_acl->acl_ace[j];
		newacl->acl_ace[entries++] = *acep;
		if (entries > allocated) {
			kauth_acl_free(newacl);
			return EINVAL;
		}
	}
	newacl->acl_entrycount = entries;
	*savedacl = vap->va_acl;
	vap->va_acl = newacl;
	return 0;
}	

/* 
 * Check to see if Directory Service understands this sid
 */
int 
smbfs_is_sid_known(ntsid_t *sid)
{
	int error;
	guid_t unique_identifier;
	ntsid_t sidFromUUID;
	
	/* See if DS can translate the SID into a UUID */ 
	error = kauth_cred_ntsid2guid(sid, &unique_identifier);
	if (error) {
		SMBDEBUG("kauth_cred_ntsid2guid failed error = %d\n", error);
		return FALSE;
	}
	/* See if DS gave us a temp UUID */ 
	if (is_memberd_tempuuid(&unique_identifier)) {
		return FALSE;
	}
	/* See if DS can translate the UUID back into a SID */ 
	error = kauth_cred_guid2ntsid(&unique_identifier, &sidFromUUID);
	if (error) {
		SMBDEBUG("kauth_cred_guid2ntsid failed error = %d\n", error);
		return FALSE;
	}
	/* Could we round trip the sid, nope the turn off ACLS */ 
	if (memcmp(&sidFromUUID, sid, sizeof(sidFromUUID)) != 0) {
		SMBWARNING("Couldn't round trip the SID\n");
		return FALSE;
	}
	return TRUE;
} 

/*
 * This items doesn't have any ACL, must only have posix modes. Just set the
 * posix ace.
 */
static int
smbfs_set_default_nfs_ace(struct smb_share *share, struct smbnode *np, vfs_context_t context)
{
	int error;
	SMBFID	fid = 0;
	ntsid_t nfs_sid;
	size_t needed;
	uint32_t acecount = 1;
	struct ntacl *w_dacl = NULL;	/* Wire DACL */
	struct ntace *w_acep;	/* Wire ACE */
	
    if (!(SSTOVC(share)->vc_flags & SMBV_SMB2)) {
        /* Only open file if its SMB 1 */
        error = smbfs_tmpopen(share, np, SMB2_READ_CONTROL | SMB2_WRITE_DAC, &fid,
                              context);
        if (error) {
            return error;
        }
    }
	
	needed = sizeof(struct ntacl) + acecount * (sizeof(struct ntace) + MAXSIDLEN);
	SMB_MALLOC(w_dacl, struct ntacl *, needed, M_TEMP, M_WAITOK | M_ZERO);
	w_dacl->acl_revision = 0x02;
	wset_aclacecount(w_dacl, acecount);
	
	w_acep = aclace(w_dacl);
	nfs_sid.sid_kind = 1;
	nfs_sid.sid_authcount = 3;
	memcpy(nfs_sid.sid_authority, security_nt_authority, sizeof(security_nt_authority));
	nfs_sid.sid_authorities[0] = SECURITY_NFS_ID_BASE_RID;
	/* Set the Windows nfs posix modes ace */
	nfs_sid.sid_authorities[1] = NfsSidTypeModes;
	nfs_sid.sid_authorities[2] = np->n_mode;
	w_acep = set_nfs_ace(w_acep, &nfs_sid, needed);
	wset_acllen(w_dacl, ((char *)w_acep - (char *)w_dacl));
    
	error = smbfs_smb_setsec(share, np, SMB2_READ_CONTROL | SMB2_WRITE_DAC | SMB2_SYNCHRONIZE,
                             fid, DACL_SECURITY_INFORMATION, 0, NULL, NULL,
                             NULL, w_dacl, context);
	
    if (!(SSTOVC(share)->vc_flags & SMBV_SMB2)) {
        /* Only close file if its SMB 1 */
        (void)smbfs_tmpclose(share, np, fid, context);
    }
    
    SMB_FREE(w_dacl, M_TEMP);
    
	/* The current cache is out of date clear it */
	smbfs_clear_acl_cache(np);
	return error;
}

/*
 * Use the Windows NFS ACE to hold the Posix modes. Should we store the UID and
 * GID? Currently no, we only care about the posix modes, use the OWNER and
 * GROUP ACE to handle the uid/gid.
 */
int
smbfs_set_ace_modes(struct smb_share *share, struct smbnode *np, uint64_t vamode, vfs_context_t context)
{
	int error;
	struct vnode_attr va;
	mode_t		save_mode = np->n_mode;
	
	/* Get the ACL from the server, so we can do the set */
	memset(&va, 0, sizeof(va));
	VATTR_INIT(&va);
	VATTR_SET_ACTIVE(&va, va_acl);
	error = smbfs_getsecurity(share, np, &va, context);
	if (error) {
		return error;
	}	
	/* 
	 * Set that we have a Windows NFS posix mode ace, so the set acl routine 
	 * will add them to the ACL. 
	 */
	np->n_flag |= NHAS_POSIXMODES;
	np->n_mode &= ~ACCESSPERMS;
	np->n_mode |= (mode_t)(vamode & ACCESSPERMS);
	if (va.va_acl == NULL) {
		error = smbfs_set_default_nfs_ace(share, np,context);
	} else {
		VATTR_SET_ACTIVE(&va, va_acl);
		error = smbfs_setsecurity(share, np->n_vnode, &va, context);
	}

	if (error) {
		/* Reset the posix modes back since we failed. */
		np->n_mode &= ~ACCESSPERMS;
		np->n_mode |= (mode_t)(save_mode & ACCESSPERMS);
	}
	/* Free any ACL we got from the smbfs_getsecurity routine */
	if (va.va_acl) {
		kauth_acl_free(va.va_acl);
	}
	return error;
}
