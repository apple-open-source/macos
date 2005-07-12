/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: smbfs_vnops.c,v 1.128.36.1 2005/05/27 02:35:28 lindak Exp $
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/xattr.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/unistd.h>
#include <sys/lockf.h>
#include <sys/mman.h>

#include <sys/kauth.h>

#include <sys/syslog.h>
#include <sys/smb_apple.h>
#include <sys/attr.h>
#include <sys/mchain.h>
#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>

#include <fs/smbfs/smbfs.h>
#include <fs/smbfs/smbfs_node.h>
#include <fs/smbfs/smbfs_subr.h>
#include <fs/smbfs/smbfs_lockf.h>

#include <sys/buf.h>
#include <sys/md5.h>

/*
 * Prototypes for SMBFS vnode operations
 */
static int smbfs_create0(struct vnop_create_args *);
static int smbfs_mknod(struct vnop_mknod_args *);
static int smbfs_open(struct vnop_open_args *);
static int smbfs_mmap(struct vnop_mmap_args *);
static int smbfs_mnomap(struct vnop_mnomap_args *);
static int smbfs_close(struct vnop_close_args *);
static int smbfs_getattr(struct vnop_getattr_args *);
static int smbfs_setattr(struct vnop_setattr_args *);
static int smbfs_read(struct vnop_read_args *);
static int smbfs_write(struct vnop_write_args *);
static int smbfs_fsync(struct vnop_fsync_args *);
static int smbfs_remove(struct vnop_remove_args *);
static int smbfs_link(struct vnop_link_args *);
static int smbfs_lookup(struct vnop_lookup_args *);
static int smbfs_rename(struct vnop_rename_args *);
static int smbfs_mkdir(struct vnop_mkdir_args *);
static int smbfs_rmdir(struct vnop_rmdir_args *);
static int smbfs_symlink(struct vnop_symlink_args *);
static int smbfs_readdir(struct vnop_readdir_args *);
static int smbfs_readlink(struct vnop_readlink_args *);
static int smbfs_pathconf(struct vnop_pathconf_args *ap);
static int smbfs_advlock(struct vnop_advlock_args *);
static int smbfs_blktooff(struct vnop_blktooff_args *);
static int smbfs_offtoblk(struct vnop_offtoblk_args *);
static int smbfs_pagein(struct vnop_pagein_args *);
static int smbfs_pageout(struct vnop_pageout_args *);
static int smbfs_getxattr(struct vnop_getxattr_args *);
static int smbfs_setxattr(struct vnop_setxattr_args *);
static int smbfs_removexattr(struct vnop_removexattr_args *);
static int smbfs_listxattr(struct vnop_listxattr_args *);

vnop_t **smbfs_vnodeop_p;
static struct vnodeopv_entry_desc smbfs_vnodeop_entries[] = {
	{ &vnop_default_desc,		(vnop_t *) vnop_defaultop },
	{ &vnop_advlock_desc,		(vnop_t *) smbfs_advlock },
	{ &vnop_close_desc,		(vnop_t *) smbfs_close },
	{ &vnop_create_desc,		(vnop_t *) smbfs_create0 },
	{ &vnop_fsync_desc,		(vnop_t *) smbfs_fsync },
	{ &vnop_getattr_desc,		(vnop_t *) smbfs_getattr },
	{ &vnop_pagein_desc,		(vnop_t *) smbfs_pagein },
	{ &vnop_inactive_desc,		(vnop_t *) smbfs_inactive },
	{ &vnop_ioctl_desc,		(vnop_t *) smbfs_ioctl },
	{ &vnop_link_desc,		(vnop_t *) smbfs_link },
	{ &vnop_lookup_desc,		(vnop_t *) smbfs_lookup },
	{ &vnop_mkdir_desc,		(vnop_t *) smbfs_mkdir },
	{ &vnop_mknod_desc,		(vnop_t *) smbfs_mknod },
	{ &vnop_mmap_desc,		(vnop_t *) smbfs_mmap },
	{ &vnop_mnomap_desc,		(vnop_t *) smbfs_mnomap },
	{ &vnop_open_desc,		(vnop_t *) smbfs_open },
	{ &vnop_pathconf_desc,		(vnop_t *) smbfs_pathconf },
	{ &vnop_pageout_desc,		(vnop_t *) smbfs_pageout },
	{ &vnop_read_desc,		(vnop_t *) smbfs_read },
	{ &vnop_readdir_desc,		(vnop_t *) smbfs_readdir },
	{ &vnop_readlink_desc,		(vnop_t *) smbfs_readlink },
	{ &vnop_reclaim_desc,		(vnop_t *) smbfs_reclaim },
	{ &vnop_remove_desc,		(vnop_t *) smbfs_remove },
	{ &vnop_rename_desc,		(vnop_t *) smbfs_rename },
	{ &vnop_rmdir_desc,		(vnop_t *) smbfs_rmdir },
	{ &vnop_setattr_desc,		(vnop_t *) smbfs_setattr },
	{ &vnop_symlink_desc,		(vnop_t *) smbfs_symlink },
	{ &vnop_write_desc,		(vnop_t *) smbfs_write },
	{ &vnop_searchfs_desc,		(vnop_t *) err_searchfs },
	{ &vnop_offtoblk_desc,		(vnop_t *) smbfs_offtoblk },
	{ &vnop_blktooff_desc,		(vnop_t *) smbfs_blktooff },
	{ &vnop_getxattr_desc,		(vnop_t *) smbfs_getxattr },
	{ &vnop_setxattr_desc,		(vnop_t *) smbfs_setxattr },
	{ &vnop_removexattr_desc,	(vnop_t *) smbfs_removexattr },
	{ &vnop_listxattr_desc,		(vnop_t *) smbfs_listxattr },
	{ NULL, NULL }
};

struct vnodeopv_desc smbfs_vnodeop_opv_desc =
	{ &smbfs_vnodeop_p, smbfs_vnodeop_entries };

VNODEOP_SET(smbfs_vnodeop_opv_desc);

#if SMB_TRACE_ENABLED

int smbtraceindx = 0;
struct smbtracerec smbtracebuf[SMBTBUFSIZ] = {{0,0,0,0}};
uint smbtracemask = 0x00000000;

#endif

/*
 * smbfs_down is called when either an smb_rq_simple or smb_t2_request call
 * has a request time out. It uses vfs_event_signal() to tell interested
 * parties the connection with the server is "down".
 * 
 * Note our UE Timeout is what triggers being here.  The operation
 * timeout may be much longer.  XXX If a connection has responded to
 * any request in the last UETIMEOUT seconds then we do not label it "down"
 * We probably need a different event & dialogue for the case of a
 * connection being responsive to at least one but not all operations.
 */
void
smbfs_down(struct smbmount *smp)
{
	if (!smp || (smp->sm_status & SM_STATUS_TIMEO))
		return;
	vfs_event_signal(&(vfs_statfs(smp->sm_mp))->f_fsid, VQ_NOTRESP, 0);
	smp->sm_status |= SM_STATUS_TIMEO;
}

/*
 * smbfs_up is called when smb_rq_simple or smb_t2_request has successful
 * communication with a server. It uses vfs_event_signal() to tell interested
 * parties the connection is OK again if the connection was having problems.
 */
void
smbfs_up(struct smbmount *smp)
{
	if (!smp || !(smp->sm_status & SM_STATUS_TIMEO))
		return;
	smp->sm_status &= ~SM_STATUS_TIMEO;
	vfs_event_signal(&(vfs_statfs(smp->sm_mp))->f_fsid, VQ_NOTRESP, 1);
}

void
smbfs_dead(struct smbmount *smp)
{
	if (!smp || (smp->sm_status & SM_STATUS_DEAD))
		return;
	vfs_event_signal(&(vfs_statfs(smp->sm_mp))->f_fsid, VQ_DEAD, 0);
	smp->sm_status |= SM_STATUS_DEAD;
}

PRIVSYM int
smbi_open(vnode_t vp, int mode, vfs_context_t vfsctx)
{
	struct vnop_open_args a;

	a.a_desc = &vnop_open_desc;
	a.a_vp = vp;
	a.a_mode = mode;
	a.a_context = vfsctx;
	return (smbfs_open(&a));
}

static char sidprintbuf[1024];

PRIVSYM char *
smb_sid2str(struct ntsid *sidp)
{
	char *s = sidprintbuf;
	int subs = sidsubauthcount(sidp);
	u_int64_t auth = 0;
	unsigned i, *ip;
 
	for (i = 0; i < sizeof(sidp->sid_authority); i++)
		auth = (auth << 8) | sidp->sid_authority[i];
	s += sprintf(s, "S-%u-%llu", sidp->sid_revision, auth);
	if (!SMBASSERT(subs <= KAUTH_NTSID_MAX_AUTHORITIES))
		subs = KAUTH_NTSID_MAX_AUTHORITIES;
	for (ip = sidsub(sidp); subs--; ip++)   
		s += sprintf(s, "-%u", *ip);    
	return (sidprintbuf);
}

PRIVSYM void
smb_sid2sid16(struct ntsid *sidp, ntsid_t *sid16p)
{
	int i;

	bzero(sid16p, sizeof(*sid16p));
	sid16p->sid_kind = sidp->sid_revision;
	sid16p->sid_authcount = sidsubauthcount(sidp);
	for (i = 0; i < sizeof(sid16p->sid_authority); i++)
		sid16p->sid_authority[i] = sidp->sid_authority[i];
	for (i = 0; i < sid16p->sid_authcount; i++)
		sid16p->sid_authorities[i] = (sidsub(sidp))[i];
}

PRIVSYM void
smb_sid_endianize(struct ntsid *sidp)
{
	u_int32_t *subauthp = sidsub(sidp);
	int 	n = sidsubauthcount(sidp);

	while (n--) {
		*subauthp = letohl(*subauthp);
		subauthp++;
	}
}

static int
smbfs_getsecurity(struct smbnode *np, struct vnode_attr *vap,
		  struct smb_cred *credp)
{
	struct smb_share *ssp = np->n_mount->sm_share;
	int error, cerror;
	struct ntsecdesc	*w_sec = NULL;	/* Wire sec descriptor */
	u_int32_t selector = 0, acecount, j, aflags;
	struct ntacl	*w_dacl;	/* Wire DACL */
	struct ntsid	*w_sidp;	/* Wire SID */
	kauth_acl_t	res = NULL;	/* acl result buffer */
	struct ntace	*w_acep;	/* Wire ACE */
	kauth_ace_rights_t	arights;
	u_int32_t	w_rights;
	ntsid_t	sid;	/* temporary, for a kauth sid */
	u_int16_t	fid = 0;

	if (VATTR_IS_ACTIVE(vap, va_acl)) {
		selector |= DACL_SECURITY_INFORMATION;
		vap->va_acl = NULL;	/* default */
	}
	if (VATTR_IS_ACTIVE(vap, va_guuid)) {
		selector |= GROUP_SECURITY_INFORMATION;
		vap->va_guuid = kauth_null_guid;	/* default */
	}
	if (VATTR_IS_ACTIVE(vap, va_uuuid)) {
		selector |= OWNER_SECURITY_INFORMATION;
		vap->va_uuuid = kauth_null_guid;	/* default */
	}
	error = smbfs_smb_tmpopen(np, STD_RIGHT_READ_CONTROL_ACCESS,
				  credp, &fid);
	if (error)
		return (error);
	error = smbfs_smb_getsec(ssp, fid, credp, selector, &w_sec);
	cerror = smbfs_smb_tmpclose(np, fid, credp);
	if (cerror)
		SMBERROR("error %d closing fid %d file %.*s\n",
			 cerror, fid, np->n_nmlen, np->n_name);
	if (error)
		return (error);
	if (w_sec == NULL)	/* XXX: or EBADRPC? */
		return (0);
	if (VATTR_IS_ACTIVE(vap, va_guuid)) {
		w_sidp = sdgroup(w_sec);
		if (!w_sidp) {
			SMBERROR("no group sid received, file %.*s\n",
				 np->n_nmlen, np->n_name);
		} else {
			smb_sid_endianize(w_sidp);
			smb_sid2sid16(w_sidp, &sid);
			error = kauth_cred_ntsid2guid(&sid, &vap->va_guuid);
			if (error) {
				SMBERROR("ntsid2guid 1 %d file %.*s sid %s\n",
					 error, np->n_nmlen, np->n_name,
					 smb_sid2str(w_sidp));
				goto exit;
			}
		}
	}
	if (VATTR_IS_ACTIVE(vap, va_uuuid)) {
		w_sidp = sdowner(w_sec);
		if (!w_sidp) {
			SMBERROR("no user sid received, file %.*s\n",
				 np->n_nmlen, np->n_name);
		} else {
			smb_sid_endianize(w_sidp);
			smb_sid2sid16(w_sidp, &sid);
			error = kauth_cred_ntsid2guid(&sid, &vap->va_uuuid);
			if (error) {
				SMBERROR("ntsid2guid 2 %d file %.*s sid %s\n",
					 error, np->n_nmlen, np->n_name,
					 smb_sid2str(w_sidp));
				goto exit;
			}
		}
	}
	if (VATTR_IS_ACTIVE(vap, va_acl)) {
		w_dacl = sddacl(w_sec);
		if (w_dacl == NULL)
			goto exit;
		acecount = aclacecount(w_dacl);
		res = kauth_acl_alloc(acecount);
		if (!res) {
			error = ENOMEM;
			goto exit;
		}
		res->acl_entrycount = acecount;
		res->acl_flags = sdflags(w_sec);
		if (sdflags(w_sec) & SD_DACL_PROTECTED)
			res->acl_flags |= KAUTH_FILESEC_NO_INHERIT;
		else
			res->acl_flags &= ~KAUTH_FILESEC_NO_INHERIT;
		for (j = 0, w_acep = aclace(w_dacl);
		     j < acecount;
		     j++, w_acep = aceace(w_acep)) {
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
				SMBERROR("ACE type %d file(%.*s)\n",
					 acetype(w_acep), np->n_nmlen,
					 np->n_name);
				error = EPROTO;	/* XXX or EIO or ??? */
				goto exit;
			}
			w_sidp = acesid(w_acep);
			smb_sid_endianize(w_sidp);
			smb_sid2sid16(w_sidp, &sid);
			error = kauth_cred_ntsid2guid(&sid, &res->acl_ace[j].ace_applicable);
			if (error) {
				SMBERROR("ntsid2guid 3 %d file %.*s sid %s\n",
					 error, np->n_nmlen, np->n_name,
					 smb_sid2str(w_sidp));
				goto exit;
			}
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
			if (aceflags(w_acep) & UNDEF_ACE_FLAG)
				SMBERROR("unknown ACE flag on file(%.*s)\n",
					 np->n_nmlen, np->n_name);
			if (aceflags(w_acep) & SUCCESSFUL_ACCESS_ACE_FLAG)
				aflags |= KAUTH_ACE_SUCCESS;
			if (aceflags(w_acep) & FAILED_ACCESS_ACE_FLAG)
				aflags |= KAUTH_ACE_FAILURE;
			res->acl_ace[j].ace_flags = aflags;
			w_rights = acerights(w_acep);
			arights = 0;
			if (w_rights & GENERIC_RIGHT_READ_ACCESS)
				arights |= KAUTH_ACE_GENERIC_READ;
			if (w_rights & GENERIC_RIGHT_WRITE_ACCESS)
				arights |= KAUTH_ACE_GENERIC_WRITE;
			if (w_rights & GENERIC_RIGHT_EXECUTE_ACCESS)
				arights |= KAUTH_ACE_GENERIC_EXECUTE;
			if (w_rights & GENERIC_RIGHT_ALL_ACCESS)
				arights |= KAUTH_ACE_GENERIC_ALL;
			if (w_rights & STD_RIGHT_SYNCHRONIZE_ACCESS)
				arights |= KAUTH_VNODE_SYNCHRONIZE;
			if (w_rights & STD_RIGHT_WRITE_OWNER_ACCESS)
				arights |= KAUTH_VNODE_CHANGE_OWNER;
			if (w_rights & STD_RIGHT_WRITE_DAC_ACCESS)
				arights |= KAUTH_VNODE_WRITE_SECURITY;
			if (w_rights & STD_RIGHT_READ_CONTROL_ACCESS)
				arights |= KAUTH_VNODE_READ_SECURITY;
			if (w_rights & STD_RIGHT_DELETE_ACCESS)
				arights |= KAUTH_VNODE_DELETE;

			if (w_rights & SA_RIGHT_FILE_WRITE_ATTRIBUTES)
				arights |= KAUTH_VNODE_WRITE_ATTRIBUTES;
			if (w_rights & SA_RIGHT_FILE_READ_ATTRIBUTES)
				arights |= KAUTH_VNODE_READ_ATTRIBUTES;
			if (w_rights & SA_RIGHT_FILE_DELETE_CHILD)
				arights |= KAUTH_VNODE_DELETE_CHILD;
			if (w_rights & SA_RIGHT_FILE_EXECUTE)
				arights |= KAUTH_VNODE_EXECUTE;
			if (w_rights & SA_RIGHT_FILE_WRITE_EA)
				arights |= KAUTH_VNODE_WRITE_EXTATTRIBUTES;
			if (w_rights & SA_RIGHT_FILE_READ_EA)
				arights |= KAUTH_VNODE_READ_EXTATTRIBUTES;
			if (w_rights & SA_RIGHT_FILE_APPEND_DATA)
				arights |= KAUTH_VNODE_APPEND_DATA;
			if (w_rights & SA_RIGHT_FILE_WRITE_DATA)
				arights |= KAUTH_VNODE_WRITE_DATA;
			if (w_rights & SA_RIGHT_FILE_READ_DATA)
				arights |= KAUTH_VNODE_READ_DATA;
			res->acl_ace[j].ace_rights = arights;
		}
		vap->va_acl = res;
	}
exit:
	if (w_sec)
		FREE(w_sec, M_TEMP);
	if (error && res)
		kauth_acl_free(res);
	return (error);
}

static int
smbfs_setsecurity(vnode_t vp, struct vnode_attr *vap, struct smb_cred *credp)
{
	struct smbnode *np = VTOSMB(vp);
	struct smb_share *ssp = np->n_mount->sm_share;
	u_int32_t selector = 0, acecount;
	struct ntsid	*w_usr = NULL, *w_grp = NULL, *w_sidp;
	struct ntacl	*w_dacl = NULL;	/* Wire DACL */
	int error = 0, cerror;
	struct ntace	*w_acep;	/* Wire ACE */
	struct kauth_ace *acep;
	u_int8_t aflags;
	u_int32_t arights, openrights;
	size_t needed;
	u_int16_t fsecflags = 0;
	struct ntsecdesc	*w_sec = NULL;	/* Wire sec descriptor */
	u_int16_t	fid = 0;

	openrights = STD_RIGHT_READ_CONTROL_ACCESS;
	if (VATTR_IS_ACTIVE(vap, va_acl)) {
		openrights |= STD_RIGHT_WRITE_DAC_ACCESS;
		selector |= DACL_SECURITY_INFORMATION;
	}
	if (VATTR_IS_ACTIVE(vap, va_guuid) &&
	    !kauth_guid_equal(&vap->va_guuid, &kauth_null_guid)) {
		openrights |= STD_RIGHT_WRITE_OWNER_ACCESS;
		selector |= GROUP_SECURITY_INFORMATION;
		MALLOC(w_grp, struct ntsid *, MAXSIDLEN, M_TEMP, M_WAITOK);
		bzero(w_grp, MAXSIDLEN);
		error = kauth_cred_guid2ntsid(&vap->va_guuid, (ntsid_t *)w_grp);
		if (error) {
			SMBERROR("kauth_cred_guid2ntsid error %d\n", error);
			goto exit;
		}
		smb_sid_endianize(w_grp);
	}
	if (VATTR_IS_ACTIVE(vap, va_uuuid) &&
	    !kauth_guid_equal(&vap->va_uuuid, &kauth_null_guid)) {
		openrights |= STD_RIGHT_WRITE_OWNER_ACCESS;
		selector |= OWNER_SECURITY_INFORMATION;
		MALLOC(w_usr, struct ntsid *, MAXSIDLEN, M_TEMP, M_WAITOK);
		bzero(w_usr, MAXSIDLEN);
		error = kauth_cred_guid2ntsid(&vap->va_uuuid, (ntsid_t *)w_usr);
		if (error) {
			SMBERROR("kauth_cred_guid2ntsid %d file %.*s\n",
				 error, np->n_nmlen, np->n_name);
			goto exit;
		}
		smb_sid_endianize(w_usr);
	}
	if (VATTR_IS_ACTIVE(vap, va_acl) && vap->va_acl != NULL) {
		if (vap->va_acl->acl_entrycount > UINT16_MAX) {
			SMBERROR("acl_entrycount=%d, file(%.*s)\n",
				 vap->va_acl->acl_entrycount, np->n_nmlen,
				 np->n_name);
			error = EINVAL;
			goto exit;
		}
		acecount = vap->va_acl->acl_entrycount;
		needed = sizeof(struct ntacl) +
		 	acecount * (sizeof(struct ntace) + MAXSIDLEN);
		MALLOC(w_dacl, struct ntacl *, needed, M_TEMP, M_WAITOK);
		bzero(w_dacl, needed);
		wset_aclrevision(w_dacl);
		wset_aclacecount(w_dacl, acecount);
		for (w_acep = aclace(w_dacl), acep = &vap->va_acl->acl_ace[0];
		     acecount--;
		     w_acep = aceace(w_acep), acep++) {
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
				SMBERROR("ace_flags=0x%x, file(%.*s)\n",
					 acep->ace_flags, np->n_nmlen,
					 np->n_name);
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
				arights |= GENERIC_RIGHT_READ_ACCESS;
			if (acep->ace_rights & KAUTH_ACE_GENERIC_WRITE)
				arights |= GENERIC_RIGHT_WRITE_ACCESS;
			if (acep->ace_rights & KAUTH_ACE_GENERIC_EXECUTE)
				arights |= GENERIC_RIGHT_EXECUTE_ACCESS;
			if (acep->ace_rights & KAUTH_ACE_GENERIC_ALL)
				arights |= GENERIC_RIGHT_ALL_ACCESS;
			if (acep->ace_rights & KAUTH_VNODE_SYNCHRONIZE)
				arights |= STD_RIGHT_SYNCHRONIZE_ACCESS;
			if (acep->ace_rights & KAUTH_VNODE_CHANGE_OWNER)
				arights |= STD_RIGHT_WRITE_OWNER_ACCESS;
			if (acep->ace_rights & KAUTH_VNODE_WRITE_SECURITY)
				arights |= STD_RIGHT_WRITE_DAC_ACCESS;
			if (acep->ace_rights & KAUTH_VNODE_READ_SECURITY)
				arights |= STD_RIGHT_READ_CONTROL_ACCESS;
			if (acep->ace_rights & KAUTH_VNODE_WRITE_EXTATTRIBUTES)
				arights |= SA_RIGHT_FILE_WRITE_EA;
			if (acep->ace_rights & KAUTH_VNODE_READ_EXTATTRIBUTES)
				arights |= SA_RIGHT_FILE_READ_EA;
			if (acep->ace_rights & KAUTH_VNODE_WRITE_ATTRIBUTES)
				arights |= SA_RIGHT_FILE_WRITE_ATTRIBUTES;
			if (acep->ace_rights & KAUTH_VNODE_READ_ATTRIBUTES)
				arights |= SA_RIGHT_FILE_READ_ATTRIBUTES;
			if (acep->ace_rights & KAUTH_VNODE_DELETE_CHILD)
				arights |= SA_RIGHT_FILE_DELETE_CHILD;
			if (acep->ace_rights & KAUTH_VNODE_APPEND_DATA)
				arights |= SA_RIGHT_FILE_APPEND_DATA;
			if (acep->ace_rights & KAUTH_VNODE_DELETE)
				arights |= STD_RIGHT_DELETE_ACCESS;
			if (acep->ace_rights & KAUTH_VNODE_EXECUTE)
				arights |= SA_RIGHT_FILE_EXECUTE;
			if (acep->ace_rights & KAUTH_VNODE_WRITE_DATA)
				arights |= SA_RIGHT_FILE_WRITE_DATA;
			if (acep->ace_rights & KAUTH_VNODE_READ_DATA)
				arights |= SA_RIGHT_FILE_READ_DATA;
			wset_acerights(w_acep, arights);
			w_sidp = acesid(w_acep);
			error = kauth_cred_guid2ntsid(&acep->ace_applicable,
						      (ntsid_t *)w_sidp);
			if (error) {
				SMBERROR("kauth_cred_guid2ntsid %d file %.*s\n",
					 error, np->n_nmlen, np->n_name);
				goto exit;
			}
			smb_sid_endianize(w_sidp);
			wset_acelen(w_acep,
				    sizeof(struct ntace) + sidlen(w_sidp));
		}
		wset_acllen(w_dacl, ((char *)w_acep - (char *)w_dacl));
	}
	error = smbfs_smb_tmpopen(np, openrights, credp, &fid);
	if (error)
		goto exit;
	/*
	 * We fetch a sec desc to get the flag bits we need to write back.
	 * Note we have to ask for everything as Windows will give back
	 * zeroes for any bits it thinks we don't care about.
	 */
	error = smbfs_smb_getsec(ssp, fid, credp, OWNER_SECURITY_INFORMATION |
						  GROUP_SECURITY_INFORMATION |
						  DACL_SECURITY_INFORMATION,
				 &w_sec);
	if (error || w_sec == NULL) {
		SMBERROR("smbfs_smb_getsec %d file %.*s\n", error,
			 np->n_nmlen, np->n_name);
	} else {
		fsecflags = sdflags(w_sec);
		if (VATTR_IS_ACTIVE(vap, va_acl)) {
			fsecflags |= SD_DACL_PRESENT;
			if (vap->va_acl != NULL) {
				if (vap->va_acl->acl_flags & KAUTH_FILESEC_NO_INHERIT)
					fsecflags |= SD_DACL_PROTECTED;
				else
					fsecflags &= ~SD_DACL_PROTECTED;
			}
			/* no mapping exits for KAUTH_FILESEC_DEFER_INHERIT */
			if (fsecflags & SD_DACL_PROTECTED)
				selector |= PROTECTED_DACL_SECURITY_INFORMATION;
			else
				selector |= UNPROTECTED_DACL_SECURITY_INFORMATION;
		}
		error = smbfs_smb_setsec(ssp, fid, credp, selector,
					 fsecflags, w_usr, w_grp, NULL, w_dacl);
	}
	cerror = smbfs_smb_tmpclose(np, fid, credp);
	if (cerror)
		SMBERROR("error %d closing fid %d file %.*s\n",
			 cerror, fid, np->n_nmlen, np->n_name);
exit:
	if (w_usr)
		FREE(w_usr, M_TEMP);
	if (w_grp)
		FREE(w_grp, M_TEMP);
	if (w_sec)
		FREE(w_sec, M_TEMP);
	if (w_dacl)
		FREE(w_dacl, M_TEMP);
	return (error);
}

static int
smbfs_mmap(ap)
	struct vnop_mmap_args /* {
		struct vnodeop_desc *a_desc; 
		vnode_t a_vp;
		int a_fflags;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t 	vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);
	int error = 0;

	if (!(np->n_flag & NISMAPPED)) {
 		error = smbi_open(vp, ap->a_fflags & PROT_WRITE ?
				      FWRITE|FREAD : FREAD, ap->a_context);
 		if (error) {
			char  errbuf[32];
			proc_t p = vfs_context_proc(ap->a_context);

			proc_name(proc_pid(p), &errbuf[0], 32);
			SMBERROR("mmap error %d pid %d(%.*s) file(%.*s)\n",
				 error, proc_pid(p), 32, &errbuf[0],
				 np->n_nmlen, np->n_name);
		} else
			np->n_flag |= NISMAPPED;
	}
	return (error);
}

static int
smbfs_mnomap(ap)
	struct vnop_mnomap_args /* {
		struct vnodeop_desc *a_desc; 
		vnode_t a_vp;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t 	vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);
	int error = 0;

	if (np->n_flag & NISMAPPED) {
		error = smbi_close(vp, FWRITE|FREAD, ap->a_context);
		np->n_flag &= ~NISMAPPED;
	}
	return (error);
}

static int
smbfs_open(ap)
	struct vnop_open_args /* {
		vnode_t a_vp;
		int  a_mode;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t 	vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);
	struct smb_cred scred;
	struct vnode_attr vattr;
	int mode = ap->a_mode, attrcacheupdated = 0;
	u_int32_t rights, rightsrcvd;
	u_int16_t fid;
	int error, cerror, vtype;

	vtype = vnode_vtype(vp);

	if (vtype != VREG && vtype != VDIR) { /* XXX VLNK? */
		SMBFSERR("open eacces vtype=%d\n", vnode_vtype(vp));
		return (EACCES);
	}
	if (vtype == VDIR) {
		np->n_dirrefs++;
		return (0);
	}
	VATTR_INIT(&vattr);
	VATTR_WANTED(&vattr, va_modify_time);
	if (np->n_flag & NMODIFIED) {
		error = smbfs_vinvalbuf(vp, V_SAVE, ap->a_context, 1);
		if (error == EINTR)
			return (error);
		smbfs_attr_cacheremove(np);
		error = smbi_getattr(vp, &vattr, ap->a_context);
		if (error)
			return (error);
		np->n_mtime.tv_sec = vattr.va_modify_time.tv_sec;
	} else {
		error = smbi_getattr(vp, &vattr, ap->a_context);
		if (error)
			return (error);
		if (np->n_mtime.tv_sec != vattr.va_modify_time.tv_sec) {
			error = smbfs_vinvalbuf(vp, V_SAVE, ap->a_context, 1);
			if (error == EINTR)
				return (error);
			np->n_mtime.tv_sec = vattr.va_modify_time.tv_sec;
		}
	}
	/*
	 * If we already have it open, check to see if current rights
	 * are sufficient for this open.
	 */
	if (np->n_fidrefs) {
		int upgrade = 0;

		if ((mode & FWRITE) &&
		    !(np->n_rights & (SA_RIGHT_FILE_WRITE_DATA |
				      GENERIC_RIGHT_ALL_ACCESS |
				      GENERIC_RIGHT_WRITE_ACCESS)))
			upgrade = 1;
		if ((mode & FREAD) &&
		    !(np->n_rights & (SA_RIGHT_FILE_READ_DATA |
				      GENERIC_RIGHT_ALL_ACCESS |
				      GENERIC_RIGHT_READ_ACCESS)))
			upgrade = 1;
		if (!upgrade) {
			/*
			 *  the existing open is good enough
			 */
			np->n_fidrefs++;
			return (0);
		}
	}
	rights = np->n_fidrefs ? np->n_rights : 0;
	/*
	 * we always ask for READ_CONTROL so we can always get the
	 * owner/group IDs to satisfy a stat.
	 * XXX: verify that works with "drop boxes"
	 */
	rights |= STD_RIGHT_READ_CONTROL_ACCESS;
	if ((mode & FREAD))
		rights |= SA_RIGHT_FILE_READ_DATA;
	if ((mode & FWRITE))
		rights |= SA_RIGHT_FILE_APPEND_DATA | SA_RIGHT_FILE_WRITE_DATA;
	smb_scred_init(&scred, ap->a_context);
	error = smbfs_smb_open(np, rights, &scred, &attrcacheupdated, &fid,
			       NULL, 0, 0, NULL, &rightsrcvd);
	if (!error) {
		if (np->n_fidrefs > 1) {
			/*
			 * We already had it open (presumably because it was
			 * open with insufficient rights.) Close old wire-open.
			 */
			cerror = smbfs_smb_close(np->n_mount->sm_share,
						 np->n_fid, &np->n_mtime,
						 &scred);
			if (cerror)
				SMBERROR("error %d closing %.*s\n", cerror,
					 np->n_nmlen, np->n_name);
		}
		np->n_fid = fid;
		np->n_rights = rightsrcvd;
	}
	/*
	 * remove this from the attr_cache if open could not
	 * update the existing cached entry
	 */
	if (error || !attrcacheupdated)
		smbfs_attr_cacheremove(np);
	return (error);
}

static int
smbfs_close(ap)
	struct vnop_close_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		int  a_fflag;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t 	vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);
	struct smb_cred scred;
	int error = 0;

	smb_scred_init(&scred, ap->a_context);

	if (vnode_isdir(vp)) {
		if (--np->n_dirrefs)
			return (0);
		if (np->n_dirseq) {
			smbfs_smb_findclose(np->n_dirseq, &scred);
			np->n_dirseq = NULL;
		}
	} else {
		if (np->n_fidrefs == 0)
			return (0);
		error = smbfs_vinvalbuf(vp, V_SAVE, ap->a_context, 1);
		if (--np->n_fidrefs)
			return (error);
		/*
		 * XXX - if we have the file open for reading and writing,
		 * and we're closing the last open for writing, it'd be
		 * nice if we could open for reading and close for reading
		 * and writing, so we give up our write access and stop
		 * blocking other clients from doing deny-write opens.
		 */
		if (!vnode_isinuse(vp, 1)) {
			error = smbfs_smb_close(np->n_mount->sm_share,
						np->n_fid, NULL, &scred);
			if (error)
				SMBERROR("error %d closing %.*s\n",
					 error, np->n_nmlen, np->n_name);
			np->n_fid = 0;
		} else if (!(np->n_flag & NISMAPPED))
			SMBERROR("vnode still in use! file(%.*s)\n",
				 np->n_nmlen, np->n_name);
	}
	if (np->n_flag & NATTRCHANGED)
		smbfs_attr_cacheremove(np);
	return (error);
}

PRIVSYM int
smbi_close(vnode_t vp, int fflag, vfs_context_t vfsctx)
{
	struct vnop_close_args a;

	a.a_desc = &vnop_close_desc;
	a.a_vp = vp;
	a.a_fflag = fflag;
	a.a_context = vfsctx;
	return (smbfs_close(&a));
}


PRIVSYM int
smbi_getattr(vnode_t vp, struct vnode_attr *vap, vfs_context_t vfsctx)
{       
	struct vnop_getattr_args a;

	a.a_desc = &vnop_getattr_desc;
	a.a_vp = vp;    
	a.a_vap = vap;
	a.a_context = vfsctx;
	return (smbfs_getattr(&a));
}

PRIVSYM int
smbi_setattr(vnode_t vp, struct vnode_attr *vap, vfs_context_t vfsctx)
{       
	struct vnop_setattr_args a;

	a.a_desc = &vnop_setattr_desc;
	a.a_vp = vp;    
	a.a_vap = vap;
	a.a_context = vfsctx;
	return (smbfs_setattr(&a));
}

PRIVSYM int
smbfs_getids(struct smbnode *np, struct smb_cred *scrp)
{
	struct smbmount *smp = np->n_mount;
	struct smb_share *ssp = smp->sm_share;
	struct ntsecdesc	*w_sec = NULL;	/* Wire sec descriptor */
	uid_t	uid;
	gid_t	gid;
	struct ntsid *w_sidp;
	int error, cerror;
	ntsid_t	sid;
	u_int16_t fid = 0;

	error = smbfs_smb_tmpopen(np, STD_RIGHT_READ_CONTROL_ACCESS, scrp,
				  &fid);
	if (error)
		return (error);
	error = smbfs_smb_getsec(ssp, fid, scrp,
				 OWNER_SECURITY_INFORMATION |
				 GROUP_SECURITY_INFORMATION, &w_sec);
	cerror = smbfs_smb_tmpclose(np, fid, scrp);
	if (cerror)
		SMBERROR("error %d closing fid %d file %.*s\n",
			 cerror, fid, np->n_nmlen, np->n_name);
	if (error) {
		SMBERROR("getsec error %d file %.*s\n", error,
			 np->n_nmlen, np->n_name);
		goto out;
	}
	/*
	 * A null w_sec commonly means a FAT filesystem.  Our
	 * caller will fallback to owner being the uid who
	 * mounted the filesystem.
	 */
	if (w_sec == NULL) {
		error = EBADRPC;
		goto out;
	}
	w_sidp = sdowner(w_sec);
	if (!w_sidp) {
		SMBERROR("no owner, file %.*s\n", np->n_nmlen, np->n_name);
		error = ESRCH;
		goto out;
	}
	smb_sid_endianize(w_sidp);
	smb_sid2sid16(w_sidp, &sid);
	error = kauth_cred_ntsid2uid(&sid, &uid);
	if (error) {
		if (error != ENOENT) {
			SMBERROR("sid2uid error %d file %.*s sid %s\n",
				 error, np->n_nmlen, np->n_name,
				 smb_sid2str(w_sidp));
			goto out;
		}
#if notanymore /* per 3988608 */
		/*
		 * XXX This stinks! But how else can we deal with groups
		 * owning items, rather than users owning them? A real
		 * solution is for memberd to reserve an ID range for
		 * owners which are groups not users and return us those
		 * IDs off the kauth_cred_ntsid2uid above.
		 * Note we don't use KAUTH_UID_NONE here as that would
		 * lose information - we wouldn't be able to display a real
		 * text-name for the "uid", nor convert the "uid" back
		 * to a SID.
		 */
		if ((error = kauth_cred_ntsid2gid(&sid, (gid_t *)&uid))) {
			SMBERROR("sid2gid error %d file %.*s sid %s\n",
				 error, np->n_nmlen, np->n_name,
				 smb_sid2str(w_sidp));
			goto out;
		}
#else
		uid = KAUTH_UID_NONE;
#endif
	}
	w_sidp = sdgroup(w_sec);
	if (!w_sidp) {
		SMBERROR("no group sid, file %.*s\n", np->n_nmlen, np->n_name);
		error = ESRCH;
		goto out;
	}
	smb_sid_endianize(w_sidp);
	smb_sid2sid16(w_sidp, &sid);
	if ((error = kauth_cred_ntsid2gid(&sid, &gid))) {
		SMBERROR("ntsid2gid error %d file %.*s sid %s\n",
			 error, np->n_nmlen, np->n_name, smb_sid2str(w_sidp));
		goto out;
	}
	np->n_uid = uid;
	np->n_gid = gid;
out:
	if (w_sec)
		FREE(w_sec, M_TEMP);
	return (error);
}

/*
 * smbfs_getattr call from vfs.
 */
static int
smbfs_getattr(ap)
	struct vnop_getattr_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		struct vnode_attr *a_vap;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t 	vp = ap->a_vp;
	struct vnode_attr *vap = ap->a_vap;
	struct smbnode *np = VTOSMB(vp);
	struct smbmount *smp = VTOSMBFS(vp);
	struct smbfattr fattr;
	struct smb_cred scred;
	int error;

	smb_scred_init(&scred, ap->a_context);
	if (smp->sm_flags & FILE_PERSISTENT_ACLS &&
	    (VATTR_IS_ACTIVE(vap, va_acl) || VATTR_IS_ACTIVE(vap, va_guuid) ||
	     VATTR_IS_ACTIVE(vap, va_uuuid))) {
		error = smbfs_getsecurity(np, vap, &scred);
		if (error)
			return (error);
		/*
		 * Failing to VATTR_SET_SUPPORTED something which was
		 * requested causes fallback to EAs, which we never want.
		 */
		if (VATTR_IS_ACTIVE(vap, va_acl))
			VATTR_SET_SUPPORTED(vap, va_acl);
		if (VATTR_IS_ACTIVE(vap, va_guuid))
			VATTR_SET_SUPPORTED(vap, va_guuid);
		if (VATTR_IS_ACTIVE(vap, va_uuuid))
			VATTR_SET_SUPPORTED(vap, va_uuuid);
	}
	error = smbfs_attr_cachelookup(vp, vap, &scred);
	if (error != ENOENT)
		return (error);
	error = smbfs_smb_lookup(np, NULL, NULL, &fattr, &scred);
	if (error)
		return (error);
	smbfs_attr_cacheenter(vp, &fattr);
	return (smbfs_attr_cachelookup(vp, vap, &scred));
}

static int
smbfs_setattr(ap)
	struct vnop_setattr_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		struct vnode_attr *a_vap;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t 	vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);
	struct smbmount *smp = VTOSMBFS(vp);
	struct vnode_attr *vap = ap->a_vap;
	struct timespec *mtime, *atime;
	struct smb_cred scred;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	u_quad_t tsize = 0;
	int error = 0, cerror, modified = 0;
	off_t newround;
	u_int16_t fid = 0;
	struct vnode_attr vattr;

	/*
	 * If our caller is trying to set multiple attributes, they
	 * can make no assumption about what order they are done in.
	 * Here we try to do them in order of decreasing likelihood
	 * of failure, just to minimize the chance we'll wind up
	 * with a partially complete request.
	 */

	smb_scred_init(&scred, ap->a_context);
	if (smp->sm_flags & FILE_PERSISTENT_ACLS &&
	    (VATTR_IS_ACTIVE(vap, va_acl) || VATTR_IS_ACTIVE(vap, va_guuid) ||
	     VATTR_IS_ACTIVE(vap, va_uuuid))) {
		error = smbfs_setsecurity(vp, vap, &scred);
		if (error)
			goto out;
		/*
		 * Failing to VATTR_SET_SUPPORTED something which was
		 * requested causes fallback to EAs, which we never want.
		 */
		if (VATTR_IS_ACTIVE(vap, va_acl))
			VATTR_SET_SUPPORTED(vap, va_acl);
		if (VATTR_IS_ACTIVE(vap, va_guuid))
			VATTR_SET_SUPPORTED(vap, va_guuid);
		if (VATTR_IS_ACTIVE(vap, va_uuuid))
			VATTR_SET_SUPPORTED(vap, va_uuuid);
		modified = 1;
	}

	/*
	 * If the server supports the UNIX extensions, right here is where
	 * we'd support changes to uid, gid, mode, and possibly va_flags.
	 * For now we claim to have made any such changes, so that the vfs
	 * above us won't use a fallback strategy of generating
	 * dot-underscore files to keep these attributes - the DU files
	 * are seen as litter ("turds") by Win admins.
	 */
	if (VATTR_IS_ACTIVE(vap, va_uid))
		VATTR_SET_SUPPORTED(vap, va_uid);
	if (VATTR_IS_ACTIVE(vap, va_gid))
		VATTR_SET_SUPPORTED(vap, va_gid);
	if (VATTR_IS_ACTIVE(vap, va_mode))
		VATTR_SET_SUPPORTED(vap, va_mode);
	if (VATTR_IS_ACTIVE(vap, va_flags))
		VATTR_SET_SUPPORTED(vap, va_flags);

	if (VATTR_IS_ACTIVE(vap, va_data_size)) {
 		tsize = np->n_size;
		newround = round_page_64((off_t)vap->va_data_size);
		if ((off_t)tsize > newround) {
			if (!ubc_sync_range(vp, newround, (off_t)tsize,
					    UBC_INVALIDATE)) {
				SMBERROR("ubc_sync_range failure\n");
				error = EIO;
				goto out;
			}
		}
		/*
		 * XXX VM coherence on extends -  consider delaying
		 * this until after zero fill (smbfs_0extend)
		 */
		smbfs_setsize(vp, (off_t)vap->va_data_size);

		error = smbfs_smb_tmpopen(np, SA_RIGHT_FILE_WRITE_DATA |
					      SA_RIGHT_FILE_APPEND_DATA,
					  &scred, &fid);
		if (error == 0)
			error = smbfs_smb_setfsize(np, fid, vap->va_data_size,
						   &scred);
		if (!error && tsize < vap->va_data_size)
			error = smbfs_0extend(vp, fid, tsize, vap->va_data_size,
					      &scred, SMBWRTTIMO);
		cerror = smbfs_smb_tmpclose(np, fid, &scred);
		if (cerror)
			SMBERROR("error %d closing fid %d file %.*s\n",
				 cerror, fid, np->n_nmlen, np->n_name);
		if (error) {
			smbfs_setsize(vp, (off_t)tsize);
			goto out;
		}
		smp->sm_statfstime = 0;	/* blow away statfs cache */
		VATTR_SET_SUPPORTED(vap, va_data_size);
		modified = 1;
  	}
	/*
	 * Note that it's up to the caller to provide (or not) fallbacks for
	 * change_time & backup_time, as we don't support them.
	 *
	 * XXX Should we support create_time here too?
	 */
	mtime = VATTR_IS_ACTIVE(vap, va_modify_time) ? &vap->va_modify_time
						     : NULL;
	atime = VATTR_IS_ACTIVE(vap, va_access_time) ? &vap->va_access_time
						     : NULL;
	if (mtime || atime) {
		/*
		 * If file is opened with write-attributes capability, 
		 * we use handle-based calls.  If not, we use path-based ones.
		 */
		if (np->n_fidrefs == 0 ||
		    !(np->n_rights & (SA_RIGHT_FILE_WRITE_ATTRIBUTES |
				      GENERIC_RIGHT_ALL_ACCESS |
				      GENERIC_RIGHT_WRITE_ACCESS))) {
			if (vcp->vc_flags & SMBV_WIN95) {
				error = smbi_open(vp, FWRITE, ap->a_context);
				if (!error) {
/* XXX?
					error = smbfs_smb_setfattrNT(np,
						np->n_dosattr, mtime, atime,
						&scred);
					VATTR_INIT(&vattr);
					VATTR_WANTED(&vattr, va_modify_time);
					smbi_getattr(vp, &vattr, ap->a_context);
*/
					if (mtime)
						np->n_mtime = *mtime;
					smbi_close(vp, FWRITE, ap->a_context);
				}
			} else if ((vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS)) {
			       /* 
				* NT4 doesn't understand "NT" style SMBs apparently.
				* for NT4 we use the old SET_PATH_INFO 
				*/
				if (!(vcp->vc_flags & SMBV_NT4)) { /* not sure yet */
					error = smbfs_smb_setpattrNT(np, np->n_dosattr,
							mtime, atime, &scred);
					if (error == EBADRPC) { /* NT4 response */
						vcp->vc_flags |= SMBV_NT4; /* remember */
						error = smbfs_smb_setpattr(np, NULL, 0, 
							np->n_dosattr, mtime, &scred);
					}
				} else /* already know it's NT4 */
					error = smbfs_smb_setpattr(np, NULL, 0, 
							np->n_dosattr, mtime, &scred);
				/* endif - NT4 */
			} else if (SMB_DIALECT(vcp) >= SMB_DIALECT_LANMAN2_0) {
				error = smbfs_smb_setptime2(np, mtime, atime, 0,
							    &scred);
			} else
				error = smbfs_smb_setpattr(np, NULL, 0, 0,
							   mtime, &scred);
		} else {
			if (vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS) {
				error = smbfs_smb_setfattrNT(np, np->n_dosattr,
							     mtime, atime,
							     &scred);
			} else if (SMB_DIALECT(vcp) >= SMB_DIALECT_LANMAN1_0) {
				error = smbfs_smb_setftime(np, mtime, atime,
							   &scred);
			} else
				error = smbfs_smb_setpattr(np, NULL, 0, 0,
							   mtime, &scred);
		}
		if (error)
			goto out;
		if (mtime)
			VATTR_SET_SUPPORTED(vap, va_modify_time);
		if (atime)
			VATTR_SET_SUPPORTED(vap, va_access_time);
		modified = 1;
	}
out:
	if (modified) {
		/*
		 * Invalidate attribute cache in case if server doesn't set
		 * required attributes.
		 */
		smbfs_attr_cacheremove(np);	/* invalidate cache */
		VATTR_INIT(&vattr);
		VATTR_WANTED(&vattr, va_modify_time);
		smbi_getattr(vp, &vattr, ap->a_context);
		np->n_mtime.tv_sec = vattr.va_modify_time.tv_sec;
	}
	return (error);
}

static int
smb_flushvp(vnode_t  vp, vfs_context_t vfsctx, int inval)
{
	struct smb_cred scred;
	int error = 0, error2;

	/* XXX provide nowait option? */
	if (inval)
		inval = UBC_INVALIDATE;
	if (!ubc_sync_range(vp, (off_t)0, smb_ubc_getsize(vp),
			    UBC_PUSHALL | UBC_SYNC | inval)) {
		SMBERROR("ubc_sync_range failure\n");
		error = EIO;
	}
	smb_scred_init(&scred, vfsctx);
	error2 = smbfs_smb_flush(VTOSMB(vp), &scred);
	return (error2 ? error2 : error);
}


static int
smb_flushrange(vnode_t  vp, uio_t uio)
{
	off_t soff, eoff;

	soff = trunc_page_64(uio_offset(uio));
	eoff = round_page_64(uio_offset(uio) + uio_resid(uio));
	if (!ubc_sync_range(vp, soff, eoff, UBC_PUSHDIRTY|UBC_INVALIDATE)) {
		SMBERROR("ubc_sync_range failure\n");
		return (EIO);
	}
	return (0);
}

/*
 * smbfs_read call.
 */
static int
smbfs_read(ap)
	struct vnop_read_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t  a_vp;
		uio_t a_uio;
		int a_ioflag;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t 	vp = ap->a_vp;
	uio_t uio = ap->a_uio;
	off_t soff, eoff;
	upl_t upl;
	int error, xfersize;
	user_ssize_t remaining;
	struct smbnode *np = VTOSMB(vp);
	struct smb_cred scred;
	int vtype;
	struct vnode_attr vattr;

	vtype = vnode_vtype(vp);

	if (vtype != VREG && vtype != VDIR && vtype != VLNK)
		return (EPERM);
	/*
	 * History: This logic was removed because of an assumption
	 * that this would be handled in higher layers. That did not
	 * happen, so I'm restoring the essential logic, although 
	 * the code is not identical (e. g. calling VOP_OPEN is
	 * no longer possible).
	 *
 	 * FreeBSD vs Darwin VFS difference; we can get VNOP_READ without
 	 * preceeding open via the exec path, so do it implicitly.
	 *
	 * According to my testing, after the execution is complete,
	 * VNOP_INACTIVE closes the extra network file handle,
	 * and decrements the open count, and that is a very good thing. 
 	 */
 	if (!np->n_fidrefs) {
 		error = smbi_open(vp, FREAD, ap->a_context);
 		if (error)
 			return (error);
 	}
	/*
	 * Here we push any mmap-dirtied pages.
	 * The vnode lock is held, so we shouldn't need to lock down
	 * all pages across the whole vop.
	 */
	error = smb_flushrange(vp, uio);
 	if (error)
 		return (error);

	VATTR_INIT(&vattr);
	VATTR_WANTED(&vattr, va_modify_time);
	VATTR_WANTED(&vattr, va_data_size);
	if (np->n_flag & NMODIFIED) {
		smbfs_attr_cacheremove(np);
		error = smbi_getattr(vp, &vattr, ap->a_context);
		if (error)
			return (error);
		np->n_mtime.tv_sec = vattr.va_modify_time.tv_sec;
	} else {
		error = smbi_getattr(vp, &vattr, ap->a_context);
		if (error)
			return (error);
		if (np->n_mtime.tv_sec != vattr.va_modify_time.tv_sec) {
			error = smbfs_vinvalbuf(vp, V_SAVE, ap->a_context, 1);
			if (error)
				return (error);
			np->n_mtime.tv_sec = vattr.va_modify_time.tv_sec;
		}
	}

	smb_scred_init(&scred, ap->a_context);
	error = smbfs_smb_flush(np, &scred);
 	if (error)
 		return (error);
	/*
	 * In order to maintain some synchronisation 
	 * between memory-mapped access and reads from 
	 * a file, we build a upl covering the range
	 * we're about to read, and once the read
	 * completes, dump all the pages.
	 *
	 * Loop reading chunks small enough to be covered by a upl.
	 */
	while (!error && uio_resid(uio) > 0) {
		remaining = uio_resid(uio);
		xfersize = MIN(remaining, MAXPHYS);
		/* create a upl for this range */
		soff = trunc_page_64(uio_offset(uio));
		eoff = round_page_64(uio_offset(uio) + xfersize);
		error = ubc_create_upl(vp, soff, (long)(eoff - soff), &upl,
				       NULL, 0);
		if (error)
			break;
		uio_setresid(uio, xfersize);
		/* do the wire transaction */
		error = smbfs_readvnode(vp, uio, ap->a_context, &vattr);
		/* dump the pages */
		if (ubc_upl_abort(upl, UPL_ABORT_DUMP_PAGES))
			panic("smbfs_read: ubc_upl_abort");
		uio_setresid(uio, (uio_resid(uio) + (remaining - xfersize)));
		if (uio_resid(uio) == remaining) /* nothing transferred? */
			break;
	}
	return (error);
}

static int
smbfs_write(ap)
	struct vnop_write_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t  a_vp;
		uio_t a_uio;
		int a_ioflag;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t 	vp = ap->a_vp;
	struct smbmount *smp = VTOSMBFS(vp);
	uio_t uio = ap->a_uio;
	off_t soff, eoff;
	upl_t upl;
	int error, xfersize;
	user_ssize_t remaining;
	int timo = SMBWRTTIMO;

	if ( !vnode_isreg(vp))
		return (EPERM);
	/*
	 * Here we push any mmap-dirtied pages.
	 * The vnode lock is held, so we shouldn't need to lock down
	 * all pages across the whole vop.
	 */
	error = smb_flushrange(vp, uio);

	/*
	 * Note that since our lower layers take the uio directly,
	 * we don't copy it into these pages; we're going to 
	 * invalidate them all when we're done anyway.
	 *
	 * Loop writing chunks small enough to be covered by a upl.
	 */
	while (!error && uio_resid(uio) > 0) {
		remaining = uio_resid(uio);
		xfersize = MIN(remaining, MAXPHYS);
		/* create a upl for this range */
		soff = trunc_page_64(uio_offset(uio));
		eoff = round_page_64(uio_offset(uio) + xfersize);
		error = ubc_create_upl(vp, soff, (long)(eoff - soff), &upl,
				       NULL, 0);
		if (error)
			break;
		uio_setresid(uio, xfersize);
		/* do the wire transaction */
		error = smbfs_writevnode(vp, uio, ap->a_context, ap->a_ioflag,
					 timo);
		timo = 0;
		/* dump the pages */
		if (ubc_upl_abort(upl, UPL_ABORT_DUMP_PAGES))
			panic("smbfs_write: ubc_upl_abort");
		uio_setresid(uio, (uio_resid(uio) + (remaining - xfersize)));
		if (uio_resid(uio) == remaining) /* nothing transferred? */
			break;
	}
	/* if success, blow away statfs cache */
	if (!error)
		smp->sm_statfstime = 0;
	return (error);
}

static int
smbfs_aceinacl(struct kauth_ace *acep, kauth_acl_t acl,
		int start, int entries)
{
	int j;

	for (j = start; j < entries; j++)
		if (!bcmp(acep, &acl->acl_ace[j], sizeof(*acep)))
			return (1);
	return (0);
}

static int
smbfs_fixinheritance(vnode_t vp, struct vnode_attr *vap, int *fixed,
		     vfs_context_t vfsctx)
{
	int j, error = 0;
	struct kauth_ace *acep;

	*fixed = 0;	/* we'll set this if we modify (fix) va_acl */
	VATTR_INIT(vap);
	VATTR_WANTED(vap, va_acl);
	VATTR_WANTED(vap, va_uuuid);
	error = smbi_getattr(vp, vap, vfsctx);
	if (error) {
		SMBERROR("smbi_getattr %d, ignored\n", error);
		return (error);
	}
	/* Note caller use va_acl so caller deallocates it */
	if (!VATTR_IS_SUPPORTED(vap, va_acl) || vap->va_acl == NULL ||
	    vap->va_acl->acl_entrycount == 0)
		return (0);
	/* if the acl claims to be defaulted, then trust that */
	if (vap->va_acl->acl_flags & SD_DACL_DEFAULTED)
		return (0);
	/* if the acl has any inherited entries then all is well */
	for (j = 0; j < vap->va_acl->acl_entrycount; j++)
		if (vap->va_acl->acl_ace[j].ace_flags & KAUTH_ACE_INHERITED)
			return (0);
	/*
	 * If we're here all the ACEs appear to be explicit.  That can happen
	 * when 1) the ACL was defaulted but Windows didn't set the 
	 * DEFAULTED bit (yes this happens) or 2) the ACEs were inherited
	 * but Windows didn't set the INHERITED bits (yes this happens too).
	 *
	 * No foolproof hack to distinguish 1) and 2) is apparent.   A case
	 * of 2) can be constructed which will look exactly like 1).  It is
	 * probably even possible to do that with apparently identical
	 * parent objects.
	 *
	 * This hack presumes the downside of treating 1) as 2) is small,
	 * relative to the downside of treating 2) as 1).
	 *
	 * Defaulted ACLs observed have two ACEs, the first having the
	 * SID of the owner, the second having S-1-5-18.  Both ACEs are
	 * ALLOWs of "everything", ie wire-rights 0x1f01ff.
	 */

	if (!kauth_guid_equal(&vap->va_acl->acl_ace[0].ace_applicable,
			      &vap->va_uuuid))
		goto fixit;

#define ALLOFTHEM (KAUTH_VNODE_READ_DATA | KAUTH_VNODE_WRITE_DATA | \
		KAUTH_VNODE_APPEND_DATA | KAUTH_VNODE_READ_EXTATTRIBUTES | \
		KAUTH_VNODE_WRITE_EXTATTRIBUTES | KAUTH_VNODE_EXECUTE | \
		KAUTH_VNODE_DELETE_CHILD | KAUTH_VNODE_READ_ATTRIBUTES | \
		KAUTH_VNODE_WRITE_ATTRIBUTES | KAUTH_VNODE_DELETE | \
		KAUTH_VNODE_READ_SECURITY | KAUTH_VNODE_WRITE_SECURITY | \
		KAUTH_VNODE_TAKE_OWNERSHIP | KAUTH_VNODE_SYNCHRONIZE)

	for (j = 0; j < vap->va_acl->acl_entrycount; j++) {
		acep = &vap->va_acl->acl_ace[j];
		if ((acep->ace_flags & KAUTH_ACE_KINDMASK) != KAUTH_ACE_PERMIT)
			goto fixit;
		if (acep->ace_rights != ALLOFTHEM)
			goto fixit;
	}
	/*
	 * Presume it is case 1), an improperly marked defaulted ACL.
	 */
	return (0);

fixit:
	/*
	 * Presume it is case 2), an improperly marked inherited ACL.
	 */
	*fixed = 1;
	for (j = 0; j < vap->va_acl->acl_entrycount; j++)
		vap->va_acl->acl_ace[j].ace_flags |= KAUTH_ACE_INHERITED;
	return (0);
}

/*
 * This is an internal utility function called from our mkdir and create vops.
 * It composes the requested acl, if any, with the one the server may have
 * produced when it created the new file or directory.
 */
static void
smbfs_composeacl(struct vnode_attr *vap, vnode_t vp, vfs_context_t vfsctx)
{
	struct vnode_attr svrva;
	kauth_acl_t newacl;
	kauth_acl_t savedacl = NULL;
	int error, fixed;
	int32_t j, entries, dupstart, allocated;
	struct kauth_ace *acep;

	/*
	 * To ensure no EA-fallback-ACL gets made by vnode_setattr_fallback...
	 */
	if (VATTR_IS_ACTIVE(vap, va_acl))
		VATTR_SET_SUPPORTED(vap, va_acl);
	/*
	 * Get the newly created ACL, and fix potentially unset
	 * "inherited" bits.
	 */
	error = smbfs_fixinheritance(vp, &svrva, &fixed, vfsctx);
	if (error)
		goto out;

	/*
	 * No filesec being set on this create/mkdir?
	 */
	if (!VATTR_IS_ACTIVE(vap, va_acl)) {
		if (fixed) {
			VATTR_SET(vap, va_acl, NULL);
		} else {
			error = smbi_setattr(vp, vap, vfsctx);
			if (error)
				SMBERROR("smbi_setattr %d, ignored\n", error);
			goto out;
		}
	}

	/*
	 * No ACEs requested?  In which case the composition is what's
	 * already there (unless we had to fix unset inheritance bits.)
	 */
	if (vap->va_acl == NULL || vap->va_acl->acl_entrycount == 0) {
		if (fixed) {
			savedacl = vap->va_acl;
			vap->va_acl = svrva.va_acl;
			error = smbi_setattr(vp, vap, vfsctx);
			vap->va_acl = savedacl;
		} else {
			VATTR_CLEAR_ACTIVE(vap, va_acl);
			error = smbi_setattr(vp, vap, vfsctx);
			VATTR_SET_ACTIVE(vap, va_acl);
		}
		if (error)
			SMBERROR("smbi_setattr+1 %d, ignored\n", error);
		goto out;
	}
	/*
	 * if none were created we just slam in the requested ACEs
	 */
	if (!VATTR_IS_SUPPORTED(&svrva, va_acl) || svrva.va_acl == NULL ||
	    svrva.va_acl->acl_entrycount == 0) {
		error = smbi_setattr(vp, vap, vfsctx);
		if (error)
			SMBERROR("smbi_setattr+2, error %d ignored\n", error);
		goto out;
	}
	allocated = vap->va_acl->acl_entrycount + svrva.va_acl->acl_entrycount;
	newacl = kauth_acl_alloc(allocated);
	if (newacl == NULL) {
		SMBERROR("kauth_acl_alloc, %d\n", allocated);
		error = ENOMEM;
		goto out;
	}
	/*
	 * It seems impossible to safely compose flags given a "black box"
	 * server, so we leave them as the server set them.
	 * XXX: Might we want to allow some way to get SD_DACL_PROTECTED
	 * set or unset here?
	 */
	newacl->acl_flags = svrva.va_acl->acl_flags;

	/*
	 * Finally, the composition:
	 *
	 * (RED, REA, RID, RIA) is what we receive with the vnop.  Those are:
	 *	RED - Requested Explicit Deny
	 *	REA - Requested Explicit Allow
	 *	RID - Requested Inherited Deny
	 *	RIA - Requested Inherited Allow
	 * That's the canonical order the ACEs should have arrived in, but
	 * here we don't depend on them being in order.
	 *
	 * (SED, SEA, SID, SIA) is what be on the server, now that it has
	 * created our new object.  Those are:
	 *	SED - Server Explicit (defaulted) Deny
	 *	SEA - Server Explicit (defaulted) Allow
	 *	SID - Server Inherited Deny
	 *	SIA - Server Inherited Allow
	 * On W2K the observed "defaulted" ACEs are an allow-all ACE for
	 * the object owner and another allow ACE for S-1-5-18, the Server OS.
	 *
	 * Here we take the (RED, REA, RID, RIA) and the (SED, SEA, SID, SIA)
	 * and write back (SED, RED, SEA, REA, SID, RID, SIA, RIA)
	 *
	 * Note all non-deny ACEs, for instance audit or alarm types, can be
	 * treated the same w/r/t canonicalizing the ACE order.
	 *
	 * Note when adding Requested ACEs we ensure they aren't duplicates.
	 */
	entries = 0;		/* output index for ACL we're building */
	dupstart = entries;	/* first one that need be tested for dupe */
	/* SED */
	for (j = 0; j < svrva.va_acl->acl_entrycount; j++) {
		acep = &svrva.va_acl->acl_ace[j];
		if (acep->ace_flags & KAUTH_ACE_INHERITED)
			continue;
		if ((acep->ace_flags & KAUTH_ACE_KINDMASK) == KAUTH_ACE_DENY)
			newacl->acl_ace[entries++] = *acep;
	}
	/* RED */
	for (j = 0; j < vap->va_acl->acl_entrycount; j++) {
		acep = &vap->va_acl->acl_ace[j];
		if (acep->ace_flags & KAUTH_ACE_INHERITED)
			continue;
		if ((acep->ace_flags & KAUTH_ACE_KINDMASK) == KAUTH_ACE_DENY)
			if (!smbfs_aceinacl(acep, newacl, dupstart, entries))
				newacl->acl_ace[entries++] = *acep;
	}
	dupstart = entries;
	/* SEA */
	for (j = 0; j < svrva.va_acl->acl_entrycount; j++) {
		acep = &svrva.va_acl->acl_ace[j];
		if (acep->ace_flags & KAUTH_ACE_INHERITED ||
		    (acep->ace_flags & KAUTH_ACE_KINDMASK) == KAUTH_ACE_DENY)
			continue;
		newacl->acl_ace[entries++] = *acep;
	}
	/* REA */
	for (j = 0; j < vap->va_acl->acl_entrycount; j++) {
		acep = &vap->va_acl->acl_ace[j];
		if (acep->ace_flags & KAUTH_ACE_INHERITED ||
		    (acep->ace_flags & KAUTH_ACE_KINDMASK) == KAUTH_ACE_DENY)
			continue;
		if (!smbfs_aceinacl(acep, newacl, dupstart, entries))
			newacl->acl_ace[entries++] = *acep;
	}
	dupstart = entries;
	/* SID */
	for (j = 0; j < svrva.va_acl->acl_entrycount; j++) {
		acep = &svrva.va_acl->acl_ace[j];
		if (acep->ace_flags & KAUTH_ACE_INHERITED &&
		    (acep->ace_flags & KAUTH_ACE_KINDMASK) == KAUTH_ACE_DENY)
			newacl->acl_ace[entries++] = *acep;
	}
	/* RID */
	for (j = 0; j < vap->va_acl->acl_entrycount; j++) {
		acep = &vap->va_acl->acl_ace[j];
		if (acep->ace_flags & KAUTH_ACE_INHERITED &&
		    (acep->ace_flags & KAUTH_ACE_KINDMASK) == KAUTH_ACE_DENY)
			if (!smbfs_aceinacl(acep, newacl, dupstart, entries))
				newacl->acl_ace[entries++] = *acep;
	}
	dupstart = entries;
	/* SIA */
	for (j = 0; j < svrva.va_acl->acl_entrycount; j++) {
		acep = &svrva.va_acl->acl_ace[j];
		if ((acep->ace_flags & KAUTH_ACE_KINDMASK) == KAUTH_ACE_DENY)
			continue;
		if (acep->ace_flags & KAUTH_ACE_INHERITED)
			newacl->acl_ace[entries++] = *acep;
	}
	/* RIA */
	for (j = 0; j < vap->va_acl->acl_entrycount; j++) {
		acep = &vap->va_acl->acl_ace[j];
		if ((acep->ace_flags & KAUTH_ACE_KINDMASK) == KAUTH_ACE_DENY)
			continue;
		if (acep->ace_flags & KAUTH_ACE_INHERITED)
			if (!smbfs_aceinacl(acep, newacl, dupstart, entries))
				newacl->acl_ace[entries++] = *acep;
	}
	if (entries > allocated)
		panic("smb stomped memory");
	newacl->acl_entrycount = entries;
	savedacl = vap->va_acl;
	vap->va_acl = newacl;
	error = smbi_setattr(vp, vap, vfsctx);
	if (error)
		SMBERROR("smbi_setattr+3, error %d ignored\n", error);
	kauth_acl_free(vap->va_acl);
	vap->va_acl = savedacl;
out:
	if (VATTR_IS_SUPPORTED(&svrva, va_acl) && svrva.va_acl != NULL)
		kauth_acl_free(svrva.va_acl);
	return;
}

/*
 * Create a regular file.  Or a "symlink", in which case
 * (wdata, wlen) specifies an initial write to the file.
 */
static int
smbfs_create(ap, wdata, wlen)
	struct vnop_create_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_dvp;
		vnode_t *a_vpp;
		struct componentname *a_cnp;
		struct vnode_attr *a_vap;
		vfs_context_t a_context;
	} */ *ap;
	char * wdata;
	u_int wlen;
{
	vnode_t 	dvp = ap->a_dvp;
	struct vnode_attr *vap = ap->a_vap;
	vnode_t 	*vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct smbnode *dnp = VTOSMB(dvp);
	struct smbmount *smp = VTOSMBFS(dvp);
	vnode_t 	vp;
	struct smbfattr fattr;
	struct smb_cred scred;
	const char *name = cnp->cn_nameptr;
	int nmlen = cnp->cn_namelen;
	int error, cerror;
	uio_t uio;
	u_int16_t fid;

	*vpp = NULL;
	if (vap->va_type != VREG && vap->va_type != VLNK)
		return (ENOTSUP);
	smb_scred_init(&scred, ap->a_context);
	
	error = smbfs_smb_create(dnp, name, nmlen, &scred, &fid,
				 NTCREATEX_DISP_CREATE, 0);
	if (error)
		return (error);
	if (wdata) {
		uio = uio_create(1, 0, UIO_SYSSPACE, UIO_WRITE);
		uio_addiov(uio, CAST_USER_ADDR_T(wdata), wlen);
		error = smb_write(smp->sm_share, fid, uio, &scred, SMBWRTTIMO);
		uio_free(uio);
	}
	cerror = smbfs_smb_close(smp->sm_share, fid, NULL, &scred);
	if (cerror)
		SMBERROR("error %d closing \"%.*s/%.*s\"\n", cerror,
			 dnp->n_nmlen, dnp->n_name, strlen(name), name);
	if (error)
		return (error);
	error = smbfs_smb_lookup(dnp, &name, &nmlen, &fattr, &scred);
	if (error)
		return (error);
	smbfs_attr_touchdir(dnp);
	error = smbfs_nget(VTOVFS(dvp), dvp, name, nmlen, &fattr, &vp,
			   cnp->cn_flags & MAKEENTRY,
			   vap->va_type == VLNK ?  VLNK : 0);
	if (error)
		goto bad;
	smbfs_composeacl(vap, vp, ap->a_context);
	*vpp = vp;
	error = 0;
bad:
	if (name != cnp->cn_nameptr)
		smbfs_name_free(name);
	/* if success, blow away statfs cache */
	if (!error)
		smp->sm_statfstime = 0;
	return (error);
}

static int
smbfs_create0(ap)
	struct vnop_create_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_dvp;
		vnode_t *a_vpp;
		struct componentname *a_cnp;
		struct vnode_attr *a_vap;
		vfs_context_t a_context;
	} */ *ap; 
{
	return (smbfs_create(ap, NULL, 0));
}


static int
smbfs_remove(ap)
	struct vnop_remove_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t  a_dvp;
		vnode_t  a_vp;
		struct componentname * a_cnp;
		int a_flags;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t 	vp = ap->a_vp;
	struct smbnode *dnp = VTOSMB(ap->a_dvp);
	proc_t p = vfs_context_proc(ap->a_context);
	struct smbnode *np = VTOSMB(vp);
	struct smbmount *smp = VTOSMBFS(vp);
	struct smb_cred scred;
	int error;

	if (vnode_isinuse(vp, 0)) {
		error = EBUSY;
		goto out;
	}
	if (vnode_vtype(vp) == VREG && np->n_size &&
	    !ubc_sync_range(vp, (off_t)0, (off_t)np->n_size, UBC_INVALIDATE)) {
		SMBERROR("ubc_sync_range failure\n");
		error = EIO;
		goto out;
	}
	smbfs_setsize(vp, (off_t)0);
	smb_scred_init(&scred, ap->a_context);
	if (np->n_fidrefs) {
 		SMBASSERT(np->n_fidrefs == 1);
		np->n_fidrefs = 0;
		error = smbfs_smb_close(np->n_mount->sm_share, np->n_fid, 
					NULL, &scred);
		if (error)
			SMBERROR("error %d closing %.*s\n", error,
				 np->n_nmlen, np->n_name);
		np->n_fid = 0;
	}
	cache_purge(vp);
	error = smbfs_smb_delete(np, &scred, NULL, 0, 0);
	smb_vhashrem(np);
	smbfs_attr_touchdir(dnp);
out:
	if (error == EBUSY) {
		char errbuf[32];

		(void)proc_name(proc_pid(p), &errbuf[0], 32);
		SMBERROR("warning: pid %d(%.*s) unlink open file(%.*s)\n",
			 proc_pid(p), 32, &errbuf[0],
			 np->n_nmlen, np->n_name);
	}
	if (!error)
		(void) vnode_recycle(vp);
	/* if success, blow away statfs cache */
	if (!error)
		smp->sm_statfstime = 0;
	return (error);
}

/*
 * smbfs_file rename call
 */
static int
smbfs_rename(ap)
	struct vnop_rename_args  /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_fdvp;
		vnode_t a_fvp;
		struct componentname *a_fcnp;
		vnode_t a_tdvp;
		vnode_t a_tvp;
		struct componentname *a_tcnp;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t 	fvp = ap->a_fvp;
	vnode_t 	tvp = ap->a_tvp;
	vnode_t 	fdvp = ap->a_fdvp;
	vnode_t 	tdvp = ap->a_tdvp;
	struct smbmount *smp = VTOSMBFS(fvp);
	struct componentname *tcnp = ap->a_tcnp;
	struct componentname *fcnp = ap->a_fcnp;
	proc_t p = vfs_context_proc(ap->a_context);
	struct smb_cred scred;
	u_int16_t flags = 6;
	int error=0;
	int hiderr;
	struct smbnode *fnp = VTOSMB(fvp);
	struct smbnode *tnp = tvp ? VTOSMB(tvp) : NULL;
	struct smbnode *tdnp = VTOSMB(tdvp);
	struct smbnode *fdnp = VTOSMB(fdvp);
	int vtype;

	/* Check for cross-device rename */
	if ((vnode_mount(fvp) != vnode_mount(tdvp)) ||
	    (tvp && (vnode_mount(fvp) != vnode_mount(tvp)))) {
		error = EXDEV;
		goto out;
out:
		if (error == EBUSY) {
			char errbuf[32];

			proc_name(proc_pid(p), &errbuf[0], 32);
			SMBERROR("warning: pid %d(%.*s) rename open file(%.*s)\n",
				 proc_pid(p), 32, &errbuf[0],
				 fnp->n_nmlen, fnp->n_name);
		}
		nop_rename(ap);
		return (error);
	}

	/*
	 * Since there are no hard links (from our client point of view)
	 * fvp==tvp means the arguments are case-variants.  (If they
	 * were identical the rename syscall doesnt call us.)
	 */
	if (tvp && tvp != fvp && vnode_isinuse(tvp, 0)) {
		error = EBUSY;
		goto out;
	}
	flags = 0x10;			/* verify all writes */

	vtype = vnode_vtype(fvp);

	if (vtype == VDIR) {
		flags |= 2;
	} else if (vtype == VREG || vtype == VLNK) {
		flags |= 1;
	} else {
		error = EINVAL;
		goto out;
	}
	smb_scred_init(&scred, ap->a_context);
#ifdef XXX
	/*
	 * Samba doesn't implement SMB_COM_MOVE call...
	 */
	if (SMB_DIALECT(SSTOCN(smp->sm_share)) >= SMB_DIALECT_LANMAN1_0) {
		error = smbfs_smb_move(fnp, tdnp, tcnp->cn_nameptr,
				       tcnp->cn_namelen, flags, &scred);
	} else
#endif
	{
		/*
		 * vnode lock gives local atomicity for delete+rename
		 * distributed atomicity XXX
		 */
		if (tvp && tvp != fvp) {
			cache_purge(tvp);
			error = smbfs_smb_delete(tnp, &scred, NULL, 0, 0);
			if (error)
				goto out;
			smb_vhashrem(tnp);
		}
		cache_purge(fvp);
#ifdef OPENRENAME
		if (fnp->n_fid)
			error = smbfs_smb_t2rename(fnp,
					   fdvp == tdvp ? 0 : VTOSMB(tdvp),
					   tcnp->cn_nameptr, tcnp->cn_namelen,
					   &scred, 1);
		if (!fnp->n_fid || error)
#endif
		error = smbfs_smb_rename(fnp, tdnp, tcnp->cn_nameptr,
					 tcnp->cn_namelen, &scred);
#ifndef OPENRENAME
		/*
		 * XXX
		 * If file is open and server *allowed* the rename we should
		 * alter n_name (or entire node) so that reconnections
		 * would use the correct name.
		 */
#endif
		if (error && fnp->n_fidrefs) { /* XXX dire bogosity */
			if (vnode_isinuse(fvp, 0)) {
				error = EBUSY;
				goto out;
			}
 			SMBASSERT(fnp->n_fidrefs == 1);
			error = smb_flushvp(fvp, ap->a_context, 1); /* inval */
 			if (error)
				log(LOG_WARNING,
				    "smbfs_rename: flush error=%d\n", error);
			fnp->n_fidrefs = 0;
			error = smbfs_smb_close(VTOSMBFS(fvp)->sm_share,
						fnp->n_fid, NULL, &scred);
			if (error)
				SMBERROR("error %d closing %.*s\n", error,
					 fnp->n_nmlen, fnp->n_name);
			fnp->n_fid = 0;
			error = smbfs_smb_rename(fnp, tdnp,
	 					 tcnp->cn_nameptr,
	 					 tcnp->cn_namelen, &scred);
		}
	}
	if (!error) {
		smb_vhashrem(fnp);
#ifdef OPENRENAME
		/*
		 * XXX
		 * If file is open and server *allowed* the rename we should
		 * alter n_name (or entire node) so that reconnections
		 * would use the correct name.
		 */
#endif
	}
	/*
	 *	Source			Target
	 *	Dot	Hidden		Dot	HIDE
	 *	Dot	Unhidden	Dot	HIDE! (Puma recovery)
	 *	NoDot	Hidden		Dot	HIDE (Win hid it)
	 *	NoDot	Unhidden	Dot	HIDE
	 *	Dot	Hidden		NoDot	UNHIDE
	 *	Dot	Unhidden	NoDot	UNHIDE
	 *	NoDot	Hidden		NoDot	HIDE! (Win hid it)
	 *	NoDot	Unhidden	NoDot	UNHIDE
	 */
	if (!error && tcnp->cn_nameptr[0] == '.') {
		if ((hiderr = smbfs_smb_hideit(tdnp, tcnp->cn_nameptr,
					       tcnp->cn_namelen, &scred)))
			SMBERROR("hiderr %d\n", hiderr);
	} else if (!error && tcnp->cn_nameptr[0] != '.' &&
		   fcnp->cn_nameptr[0] == '.') {
		if ((hiderr = smbfs_smb_unhideit(tdnp, tcnp->cn_nameptr,
					       tcnp->cn_namelen, &scred)))
			SMBERROR("(un)hiderr %d\n", hiderr);
	}

	if (vnode_isdir(fvp)) {
		if (tvp != NULL && vnode_isdir(tvp))
			cache_purge(tdvp);
		cache_purge(fdvp);
	}
	if (error == EBUSY) {
		char errbuf[32];

		proc_name(proc_pid(p), &errbuf[0], 32);
		SMBERROR("warning: pid %d(%.*s) rename open file(%.*s)\n",
			 proc_pid(p), 32, &errbuf[0],
			 fnp->n_nmlen, fnp->n_name);
	}
	smbfs_attr_touchdir(fdnp);
	if (tdvp != fdvp)
		smbfs_attr_touchdir(tdnp);
	/* if success, blow away statfs cache */
	if (!error)
		smp->sm_statfstime = 0;
	return (error);
}

/*
 * sometime it will come true...
 */
static int
smbfs_link(ap)
	struct vnop_link_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		vnode_t a_tdvp;
		struct componentname *a_cnp;
		vfs_context_t a_context;
	} */ *ap;
{
	proc_t p = vfs_context_proc(ap->a_context);
	struct smbnode *np = VTOSMB(ap->a_vp);
	char errbuf[32];

	proc_name(proc_pid(p), &errbuf[0], 32);
	SMBERROR("warning: pid %d(%.*s) hardlink(%.*s)\n",
		 proc_pid(p), 32, &errbuf[0],
		 np->n_nmlen, np->n_name);
	return (err_link(ap));
}

char smb_symmagic[SMB_SYMMAGICLEN] = {'X', 'S', 'y', 'm', '\n'};

/*
 * smbfs_symlink link create call.
 */
static int
smbfs_symlink(ap)
	struct vnop_symlink_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_dvp;
		vnode_t *a_vpp;
		struct componentname *a_cnp;
		struct vnode_attr *a_vap;
		char *a_target;
		vfs_context_t a_context;
	} */ *ap;
{
	int error;
	struct vnop_create_args a;
	MD5_CTX md5;
	u_int32_t state[4];
	u_int targlen, datalen, filelen;
	char *wbuf, *wp;
	vnode_t 	vp;

	targlen = strlen(ap->a_target);
	datalen = SMB_SYMHDRLEN + targlen;
	if (datalen > SMB_SYMLEN)
		return (ENAMETOOLONG);
	filelen = SMB_SYMLEN;

	MALLOC(wbuf, void *, filelen, M_TEMP, M_WAITOK);

	wp = wbuf;
	bcopy(smb_symmagic, wp, SMB_SYMMAGICLEN);
	wp += SMB_SYMMAGICLEN;
	(void)sprintf(wp, "%04d\n", targlen);
	wp += SMB_SYMLENLEN;
	MD5Init(&md5);
	MD5Update(&md5, ap->a_target, targlen);
	MD5Final((u_char *)state, &md5);
	(void)sprintf(wp, "%08x%08x%08x%08x\n", state[0], state[1],
		      state[2], state[3]);
	wp += SMB_SYMMD5LEN;
	bcopy(ap->a_target, wp, targlen);
	wp += targlen;
	if (datalen < filelen) {
		*wp++ = '\n';
		datalen++;
		if (datalen < filelen)
			memset((caddr_t)wp, ' ', filelen - datalen);
	}

	ap->a_vap->va_type = VLNK;

	a.a_dvp = ap->a_dvp;
	a.a_vpp = ap->a_vpp;
	a.a_cnp = ap->a_cnp;
	a.a_vap = ap->a_vap;
	a.a_context = ap->a_context;
	error = smbfs_create(&a, wbuf, filelen);
	if (!error) {
		ap->a_vpp = a.a_vpp;
		vp = *a.a_vpp;
		VTOSMB(vp)->n_size = targlen;
	}
	nop_create(&a);
	if (wbuf)
		FREE(wbuf, M_TEMP);
	return (error);
}

static int
smbfs_readlink(ap)
	struct vnop_readlink_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		uio_t a_uio; 
		vfs_context_t a_context;
	} */ *ap;
{       
	vnode_t 	vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);
	unsigned char *wbuf, *cp;
	u_int len, flen;
	struct smb_cred scred;
	struct smbmount *smp = VTOSMBFS(vp);
	uio_t uio;
	int error, cerror;
	struct smb_share *ssp = smp->sm_share;
	u_int16_t	fid = 0;

	if (vnode_vtype(vp) != VLNK)
		return (EINVAL);
	flen = SMB_SYMLEN;
	MALLOC(wbuf, void *, flen, M_TEMP, M_WAITOK);
	smb_scred_init(&scred, ap->a_context);

	error = smbfs_smb_tmpopen(np, SA_RIGHT_FILE_READ_DATA, &scred, &fid);
	if (error)
		goto out;
	uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
	uio_addiov(uio, CAST_USER_ADDR_T(wbuf), flen);
	error = smb_read(ssp, fid, uio, &scred);
	uio_free(uio);
	cerror = smbfs_smb_tmpclose(np, fid, &scred);
	if (cerror)
		SMBERROR("error %d closing fid %d file %.*s\n",
			 cerror, fid, np->n_nmlen, np->n_name);
	if (error)
		goto out;
	for (len = 0, cp = wbuf + SMB_SYMMAGICLEN;
	     cp < wbuf + SMB_SYMMAGICLEN + SMB_SYMLENLEN-1; cp++) {
		if (*cp < '0' || *cp > '9') {
			SMBERROR("symlink length nonnumeric: %c (0x%x)\n",
				 *cp, *cp);
			return (EINVAL);
		}
		len *= 10;
		len += *cp - '0';
	}
	if (len != np->n_size) {
		SMBERROR("symlink length payload changed from %u to %u\n",
				 (unsigned)np->n_size, len);
		np->n_size = len;
	}
	cp = wbuf + SMB_SYMHDRLEN;
	error = uiomove(cp, len, ap->a_uio);
out:;
	FREE(wbuf, M_TEMP);
	return (error);
}

static int
smbfs_mknod(ap) 
	struct vnop_mknod_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_dvp;
		vnode_t *a_vpp;
		struct componentname *a_cnp;
		struct vnode_attr *a_vap;
		vfs_context_t a_context;
	} */ *ap;
{
	proc_t p = vfs_context_proc(ap->a_context);
	char errbuf[32];

	proc_name(proc_pid(p), &errbuf[0], 32);
	SMBERROR("warning: pid %d(%.*s) mknod(%.*s)\n",
		 proc_pid(p), 32 , &errbuf[0],
		 ap->a_cnp->cn_namelen, ap->a_cnp->cn_nameptr);
	return (err_mknod(ap));
}

static int
smbfs_mkdir(ap)
	struct vnop_mkdir_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_dvp;
		vnode_t *a_vpp;
		struct componentname *a_cnp;
		struct vnode_attr *a_vap;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t 	dvp = ap->a_dvp;
	struct vnode_attr *vap = ap->a_vap;
	vnode_t 	vp;
	struct componentname *cnp = ap->a_cnp;
	struct smbnode *dnp = VTOSMB(dvp);
	struct smbmount *smp = VTOSMBFS(dvp);
	struct smb_cred scred;
	struct smbfattr fattr;
	const char *name = cnp->cn_nameptr;
	int len = cnp->cn_namelen;
	int error, hiderr;

	if (name[0] == '.' && (len == 1 || (len == 2 && name[1] == '.')))
		return (EEXIST);
	smb_scred_init(&scred, ap->a_context);
	error = smbfs_smb_mkdir(dnp, name, len, &scred);
	if (error)
		return (error);
	error = smbfs_smb_lookup(dnp, &name, &len, &fattr, &scred);
	if (error)
		return (error);
	smbfs_attr_touchdir(dnp);
	error = smbfs_nget(VTOVFS(dvp), dvp, name, len, &fattr, &vp,
			   !MAKEENTRY, 0); /* XXX why not cnp's MAKEENTRY? */
	if (error)
		goto bad;
	if (name[0] == '.')
		if ((hiderr = smbfs_smb_hideit(VTOSMB(vp), NULL, 0, &scred)))
			SMBERROR("hiderr %d\n", hiderr);
	smbfs_composeacl(vap, vp, ap->a_context);
	*ap->a_vpp = vp;
	error = 0;
bad:
	if (name != cnp->cn_nameptr)
		smbfs_name_free(name);
	/* if success, blow away statfs cache */
	smp->sm_statfstime = 0;
	return (error);
}

/*
 * smbfs remove directory call
 */
static int
smbfs_rmdir(ap)
	struct vnop_rmdir_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_dvp;
		vnode_t a_vp;
		struct componentname *a_cnp;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t 	vp = ap->a_vp;
	vnode_t 	dvp = ap->a_dvp;
	struct smbmount *smp = VTOSMBFS(vp);
	struct smbnode *dnp = VTOSMB(dvp);
	struct smbnode *np = VTOSMB(vp);
	struct smb_cred scred;
	int error;

	/* XXX other OSX fs test fs nodes here, not vnodes. Why? */
	if (dvp == vp) {
		error = EINVAL;
		goto bad;
	}

	smb_scred_init(&scred, ap->a_context);
	error = smbfs_smb_rmdir(np, &scred);
	dnp->n_flag |= NMODIFIED;
	smbfs_attr_touchdir(dnp);
	cache_purge(dvp);
	cache_purge(vp);
	smb_vhashrem(np);
bad:
	/* if success, blow away statfs cache */
	if (!error)
		smp->sm_statfstime = 0;
	return (error);
}

/*
 * smbfs_readdir call
 */
static int
smbfs_readdir(ap)
	struct vnop_readdir_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		uio_t a_uio;
		int *a_eofflag;
		int *a_ncookies;
		u_long **a_cookies;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t 	vp = ap->a_vp;
	uio_t uio = ap->a_uio;
	int error;

	if (!vnode_isdir(vp))
		return (EPERM);
#ifdef XXX
	if (ap->a_ncookies) {
		printf("smbfs_readdir: no support for cookies now...");
		return (ENOTSUP);
	}
#endif
	error = smbfs_readvnode(vp, uio, ap->a_context, NULL);
	return (error);
}

PRIVSYM int
smbi_fsync(vnode_t vp, int waitfor, vfs_context_t vfsctx)
{
	struct vnop_fsync_args a;

	a.a_desc = &vnop_fsync_desc;
	a.a_vp = vp;
	a.a_waitfor = waitfor;
	a.a_context = vfsctx;
	return (smbfs_fsync(&a));
}

static int
smbfs_fsync(ap)
	struct vnop_fsync_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t  a_vp;
		int  a_waitfor;
		vfs_context_t a_context;
	} */ *ap;
{
	int error;

	error = smb_flushvp(ap->a_vp, ap->a_context, 0); /* no inval */
	if (!error)
		VTOSMBFS(ap->a_vp)->sm_statfstime = 0;
	nop_fsync(ap);
	return (error);
}


static int
smbfs_pathconf (ap)
	struct vnop_pathconf_args  /* {
		struct vnodeop_desc *a_desc;
		vnode_t  a_vp;
		int a_name;
		register_t *a_retval;
		vfs_context_t a_context;
	} */ *ap;
{
	struct smbmount *smp = VFSTOSMBFS(VTOVFS(ap->a_vp));
	struct smb_vc *vcp = SSTOVC(smp->sm_share);
	register_t *retval = ap->a_retval;
	int error = 0;
	
	switch (ap->a_name) {
	    case _PC_LINK_MAX:
		*retval = 0;
		break;
	    case _PC_NAME_MAX:
		*retval = (SMB_DIALECT(vcp) >= SMB_DIALECT_LANMAN2_0) ?
			  255 : 12;
		break;
	    case _PC_PATH_MAX:
		*retval = 800;	/* XXX: a correct one ? */
		break;
	    default:
		error = EINVAL;
	}
	return (error);
}


int
smbfs_ioctl(ap)
	struct vnop_ioctl_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		u_long a_command;
		caddr_t a_data;
		int a_fflag;
		vfs_context_t a_context;
	} */ *ap;
{
	#pragma unused(ap)
	return (EINVAL);
}


/* SMB locks do not map to POSIX.1 advisory locks in several ways:
 * 1 - SMB provides no way to find an existing lock on the server.
 *     So, the F_GETLK operation can only report locks created by processes
 *     on this client. 
 * 2 - SMB locks cannot overlap an existing locked region of a file. So,
 *     F_SETLK/F_SETLKW operations that establish locks cannot extend an
 *     existing lock.
 * 3 - When unlocking a SMB locked region, the region to unlock must correspond
 *     exactly to an existing locked region. So, F_SETLK F_UNLCK operations
 *     cannot split an existing lock or unlock more than was locked (this is
 *     especially important because file files are closed, we recieve a request
 *     to unlock the entire file: l_whence and l_start point to the beginning
 *     of the file, and l_len is zero).
 * The result... SMB cannot support POSIX.1 advisory locks. It can however
 * support BSD flock() locks, so that's what this implementation will allow. 
 */
int
smbfs_advlock(ap)
	struct vnop_advlock_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		caddr_t  a_id;
		int  a_op;
		struct flock *a_fl;
		int  a_flags;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	caddr_t id = ap->a_id;
	int operation = ap->a_op;
	struct flock *fl = ap->a_fl;
	int flags = ap->a_flags;
	struct smbmount *smp = VFSTOSMBFS(vnode_mount(vp));
	struct smb_share *ssp = smp->sm_share;
	struct smbnode *np = VTOSMB(vp);
 	struct smb_cred scred;
	off_t start, end;
	u_int64_t len;
	struct smbfs_lockf *lock;
	int error;
	/* Since the pid passed to the SMB server is only 16 bits and a_id
	 * is 32 bits, and since we are negotiating locks between local processes
	 * with the code in smbfs_lockf.c, just pass a 1 for our pid to the server.
	 */
	caddr_t smbid = (caddr_t)1;
	int largelock = (SSTOVC(ssp)->vc_sopt.sv_caps & SMB_CAP_LARGE_FILES) != 0;
	u_int32_t timeout;
	
	/* Only regular files can have locks */
	if ( !vnode_isreg(vp))
		return (EISDIR);
	
	/* No support for F_POSIX style locks */
	if (flags & F_POSIX)
		return (err_advlock(ap));
    
	/*
	 * Avoid the common case of unlocking when smbnode has no locks.
	 */
	if (np->smb_lockf == (struct smbfs_lockf *)0) {
		if (operation != F_SETLK) {
			fl->l_type = F_UNLCK;
			return (0);
		}
	}
	
	/*
	 * Convert the flock structure into a start, end, and len.
	 */
	start = 0;
	switch (fl->l_whence) {
	case SEEK_SET:
	case SEEK_CUR:
		/*
		 * Caller is responsible for adding any necessary offset
		 * when SEEK_CUR is used.
		 */
		start = fl->l_start;
		break;
	case SEEK_END:
		start = np->n_size + fl->l_start;
		break;
	default:
		return (EINVAL);
	}

	if (start < 0)
		return (EINVAL);
	if (!largelock && (start & 0xffffffff00000000LL))
		return (EINVAL);
	if (fl->l_len == 0) {
		/* lock from start to EOF */
 		end = -1;
		if (!largelock)
			/* maximum file size 2^32 - 1 bytes */
			len = 0x00000000ffffffffULL - start;
		else
			/* maximum file size 2^64 - 1 bytes */
			len = 0xffffffffffffffffULL - start;
	} else {
		/* lock fl->l_len bytes from start */
		end = start + fl->l_len - 1;
		len = fl->l_len;
	}

	/* F_FLOCK style locks only use F_SETLK and F_UNLCK,
	 * and always lock the entire file.
	 */
	if ((operation != F_SETLK && operation != F_UNLCK) ||
		start != 0 || end != -1) {
		return (EINVAL);
	}
	
	/*
	 * Create the lockf structure
	 */
	MALLOC(lock, struct smbfs_lockf *, sizeof *lock, M_LOCKF, M_WAITOK);
	lock->lf_start = start;
	lock->lf_end = end;
	lock->lf_id = id;
	lock->lf_smbnode = np;
	lock->lf_type = fl->l_type;
	lock->lf_next = (struct smbfs_lockf *)0;
	TAILQ_INIT(&lock->lf_blkhd);
	lock->lf_flags = flags;

	smb_scred_init(&scred, ap->a_context);
	timeout = (flags & F_WAIT) ? -1 : 0;

	/*
	 * Do the requested operation.
	 */
	switch(operation) {
	case F_SETLK:
		/* get local lock */
		error = smbfs_setlock(lock);
		if (!error) {
			/* get remote lock */
			error = smbfs_smb_lock(np, SMB_LOCK_EXCL, smbid, start, len, largelock, &scred, timeout);
			if (error) {
				/* remote lock failed */
				/* Create another lockf structure for the clear */
				MALLOC(lock, struct smbfs_lockf *, sizeof *lock, M_LOCKF, M_WAITOK);
				lock->lf_start = start;
				lock->lf_end = end;
				lock->lf_id = id;
				lock->lf_smbnode = np;
				lock->lf_type = F_UNLCK;
				lock->lf_next = (struct smbfs_lockf *)0;
				TAILQ_INIT(&lock->lf_blkhd);
				lock->lf_flags = flags;
				/* clear local lock (this will always be successful) */
				(void) smbfs_clearlock(lock);
				FREE(lock, M_LOCKF);
			}
		}
		break;
	case F_UNLCK:
		/* clear local lock (this will always be successful) */
		error = smbfs_clearlock(lock);
		FREE(lock, M_LOCKF);
		/* clear remote lock */
		error = smbfs_smb_lock(np, SMB_LOCK_RELEASE, smbid, start, len, largelock, &scred, timeout);
		break;
	case F_GETLK:
		error = smbfs_getlock(lock, fl);
		FREE(lock, M_LOCKF);
		break;
	default:
		error = EINVAL;
		_FREE(lock, M_LOCKF);
		break;
	}
	
	if (error == EDEADLK && !(flags & F_WAIT))
		error = EAGAIN;

	return (error);
}

static int
smbfs_pathcheck(struct smbmount *smp, const char *name, int nmlen, int nameiop)
{
	const char *cp, *endp;
	int error;

	/* Check name only if CREATE, DELETE, or RENAME */
	if (nameiop == LOOKUP)
		return (0);

	/*
	 * Normally, we'd return EINVAL when the name is syntactically invalid,
	 * but ENAMETOOLONG makes it clear that the name is the problem (and
	 * allows Carbon to return a more meaningful error).
	 */
	error = ENAMETOOLONG;

	/*
	 * Note: This code does not prevent the smb file system client
	 * from creating filenames which are difficult to use with
	 * other clients. For example, you can create "  foo  " or
	 * "foo..." which cannot be moved, renamed, or deleted by some
	 * other clients.
	 */
	if (!nmlen)
		return (error);
	if (SMB_DIALECT(SSTOVC(smp->sm_share)) < SMB_DIALECT_LANMAN2_0) {
		/*
		 * Name should conform short 8.3 format
		 */

		/* Look for optional period */
		cp = index(name, '.');
		if (cp != NULL) {
			/*
			 * If there's a period, then:
			 *   1 - the main part of the name must be 1 to 8 chars long
			 *   2 - the extension must be 1 to 3 chars long
			 *   3 - there cannot be more than one period
			 * On a DOS volume, a trailing period in a name is ignored,
			 * so we don't want to create "foo." and confuse programs
			 * when the file actually created is "foo"
			 */
			if ((cp == name) ||	/* no name chars */
				(cp - name > 8) || /* name is too long */
				((nmlen - ((long)(cp - name) + 1)) > 3) || /* extension is too long */
				(nmlen == ((long)(cp - name) + 1)) || /* no extension chars */
				(index(cp + 1, '.') != NULL)) { /* multiple periods */
				return (error);
			}
		} else {
			/*
			 * There is no period, so main part of the name
			 * must be no longer than 8 chars.
			 */
			if (nmlen > 8)
				return (error);
		}
		/* check for illegal characters */
		for (cp = name, endp = name + nmlen; cp < endp; ++cp) {
			/*
			 * check for other 8.3 illegal characters, wildcards,
			 * and separators.
			 *
			 * According to the FAT32 File System spec, the following
			 * characters are illegal in 8.3 file names: Values less than 0x20,
			 * and the values 0x22, 0x2A, 0x2B, 0x2C, 0x2E, 0x2F, 0x3A, 0x3B,
			 * 0x3C, 0x3D, 0x3E, 0x3F, 0x5B, 0x5C, 0x5D, and 0x7C.
			 * The various SMB specs say the same thing with the additional
			 * illegal character 0x20 -- control characters (0x00...0x1f)
			 * are not mentioned. So, we'll add the space character and
			 * won't filter out control characters unless we find they cause
			 * interoperability problems.
			 */
			switch (*cp) {
			case 0x20:	/* space */
			case 0x22:	/* "     */
			case 0x2A:	/* *     */
			case 0x2B:	/* +     */
			case 0x2C:	/* ,     */
						/* 0x2E (period) was handled above */
			case 0x2F:	/* /     */
			case 0x3A:	/* :     */
			case 0x3B:	/* ;     */
			case 0x3C:	/* <     */
			case 0x3D:	/* =     */
			case 0x3E:	/* >     */
			case 0x3F:	/* ?     */
			case 0x5B:	/* [     */
			case 0x5C:	/* \     */
			case 0x5D:	/* ]     */
			case 0x7C:	/* |     */
				/* illegal character found */
				return (error);
				break;
			default:
				break;
			}
		}
	} else {
		/*
		 * Long name format
		 */

		/* make sure the name isn't too long */
		if (nmlen > 255)
			return (error);
		/* check for illegal characters */
		for (cp = name, endp = name + nmlen; cp < endp; ++cp) {
			/*
			 * The set of illegal characters in long names is the same as
			 * 8.3 except the characters 0x20, 0x2b, 0x2c, 0x3b, 0x3d, 0x5b,
			 * and 0x5d are now legal, and the restrictions on periods was
			 * removed.
			 */
			switch (*cp) {
			case 0x22:	/* "     */
			case 0x2A:	/* *     */
			case 0x2F:	/* /     */
			case 0x3A:	/* :     */
			case 0x3C:	/* <     */
			case 0x3E:	/* >     */
			case 0x3F:	/* ?     */
			case 0x5C:	/* \     */
			case 0x7C:	/* |     */
				/* illegal character found */
				return (error);
				break;
			default:
				break;
			}
		}
	}
	return (0);
}


/*
 * Things go even weird without fixed inode numbers...
 */
int
smbfs_lookup(ap)
	struct vnop_lookup_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_dvp;
		vnode_t *a_vpp;
		struct componentname *a_cnp;
		vfs_context_t a_context;
	} */ *ap;
{
	struct componentname *cnp = ap->a_cnp;
	proc_t p = vfs_context_proc(ap->a_context);
	vnode_t dvp = ap->a_dvp;
	vnode_t *vpp = ap->a_vpp;
	vnode_t vp;
	struct smbmount *smp;
	struct mount *mp = vnode_mount(dvp);
	struct smbnode *dnp;
	struct smbfattr fattr, *fap;
	struct smb_cred scred;
	const char *name = cnp->cn_nameptr;
	int flags = cnp->cn_flags;
	int nameiop = cnp->cn_nameiop;
	int nmlen = cnp->cn_namelen;
	int wantparent, error, islastcn, isdot;
	int supplen;
	struct smb_vc *vcp;

	smp = VFSTOSMBFS(mp);
	
	vcp = SSTOVC(smp->sm_share);

	smp = VFSTOSMBFS(mp);
	supplen = (SMB_DIALECT(vcp) >= SMB_DIALECT_LANMAN2_0) ? 255 : 12;
	
	if (cnp->cn_namelen > supplen)
		return (ENAMETOOLONG);
	if (!vnode_isdir(dvp))
		return (ENOTDIR);
	if ((flags & ISDOTDOT) && vnode_isvroot(dvp)) {
		SMBFSERR("invalid '..'\n");
		return (EIO);
	}
	islastcn = flags & ISLASTCN;
	if (islastcn && vfs_isrdonly(mp) && nameiop != LOOKUP)
		return (EROFS);
	wantparent = flags & (LOCKPARENT|WANTPARENT);

	*vpp = NULLVP;
	error = cache_lookup(dvp, vpp, cnp);
	switch (error) {
	case ENOENT:	/* negative cache entry - treat as cache miss */
		error = 0;
		/* FALLTHROUGH */
	case 0:		/* cache miss */
		break;
	case -1:	/* cache hit */
		/*
		 * On CREATE we can't trust a cache hit as if it is stale
		 * and the object doesn't exist on the server returning zero
		 * here would cause the vfs layer to, for instance, EEXIST
		 * the mkdir.
		 */
		if (nameiop != CREATE)
			return (0);
		if (*vpp) {
			cache_purge(*vpp);
			vnode_put(*vpp);
			*vpp = NULLVP;
		}
		break;
	default:	/* unknown & unexpected! */
		log(LOG_WARNING, "smbfs_lookup: cache_enter error=%d\n", error);
		return (error);
	}
	/* 
	 * entry is not in the name cache
	 *
	 * validate syntax of name.  ENAMETOOLONG makes it clear the name
	 * is the problem
	 */
	error = smbfs_pathcheck(smp, cnp->cn_nameptr, cnp->cn_namelen, nameiop);
	if (error) {
		char  errbuf[32];
		proc_name(proc_pid(p), &errbuf[0], 32);

		SMBERROR("warning: pid %d(%.*s) bad filename(%.*s)\n",
		 		proc_pid(p), 32 , &errbuf[0], nmlen, name);
		return (ENAMETOOLONG);
	}

	dnp = VTOSMB(dvp);
	isdot = (nmlen == 1 && name[0] == '.');
	error = 0;
	smb_scred_init(&scred, ap->a_context);
	fap = &fattr;
	/* this can allocate a new "name" so use "out" from here on */
	if (flags & ISDOTDOT) {
		error = smbfs_smb_lookup(dnp->n_parent, NULL, NULL, fap, &scred);
	} else
		error = smbfs_smb_lookup(dnp, &name, &nmlen, fap, &scred);
	if (error) {
		/*
		 * note the EJUSTRETURN code in lookup()
		 */
		if ((nameiop == CREATE || nameiop == RENAME) &&
		    error == ENOENT && islastcn)
			error = EJUSTRETURN;
	} else if (nameiop == RENAME && islastcn && wantparent) {
		if (isdot) {
			error = EISDIR;
		} else {
			error = smbfs_nget(mp, dvp, name, nmlen, fap, &vp,
					   !MAKEENTRY, 0);
			if (!error)
				*vpp = vp;
		}
	} else if (nameiop == DELETE && islastcn) {
		if (isdot) {
			error = vnode_get(dvp);
			if (!error)
				*vpp = dvp;
		} else {
			error = smbfs_nget(mp, dvp, name, nmlen, fap, &vp,
					   !MAKEENTRY, 0);
			if (!error)
				*vpp = vp;
		}
	} else if (flags & ISDOTDOT) {
		error = smbfs_nget(mp, dvp, name, nmlen, NULL, &vp,
				   !MAKEENTRY, 0);
		if (!error)
			*vpp = vp;
	} else if (isdot) {
		error = vnode_get(dvp);
		if (!error)
			*vpp = dvp;
	} else {
		error = smbfs_nget(mp, dvp, name, nmlen, fap, &vp,
				   cnp->cn_flags & MAKEENTRY, 0);
		if (!error)
			*vpp = vp;
	}
#if notyetneeded
out:
#endif
	if (name != cnp->cn_nameptr)
		smbfs_name_free(name);
	return (error);
}


/* offtoblk converts a file offset to a logical block number */
static int 
smbfs_offtoblk(ap)
	struct vnop_offtoblk_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		off_t a_offset;
		daddr64_t *a_lblkno;
		vfs_context_t a_context;
	} */ *ap;
{
	*ap->a_lblkno = ap->a_offset / PAGE_SIZE_64;
	return (0);
}


/* blktooff converts a logical block number to a file offset */
static int     
smbfs_blktooff(ap)
	struct vnop_blktooff_args /* {   
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		daddr64_t a_lblkno;
		off_t *a_offset;
		vfs_context_t a_context;
	} */ *ap;
{	
	*ap->a_offset = (off_t)ap->a_lblkno * PAGE_SIZE_64;
	return (0);
}


static int
smbfs_pagein(ap)
	struct vnop_pagein_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t 	a_vp,
		upl_t		a_pl,
		vm_offset_t	a_pl_offset,
		off_t		a_f_offset, 
		size_t		a_size,
		int		a_flags
		vfs_context_t a_context;
	} */ *ap;
{       
	vnode_t vp;
	upl_t pl;
	size_t size;
	off_t f_offset;
	vm_offset_t pl_offset, ioaddr;
	int error, nocommit;
	struct smbnode *np;
	struct smbmount *smp;
	struct smb_cred scred;
	uio_t uio;
	kern_return_t   kret;

	f_offset = ap->a_f_offset;
	size = ap->a_size;
	pl = ap->a_pl;
	pl_offset = ap->a_pl_offset;
	vp = ap->a_vp;
	nocommit = ap->a_flags & UPL_NOCOMMIT;
	np = VTOSMB(vp);
	if (size <= 0 || f_offset < 0 || f_offset >= (off_t)np->n_size ||
	    f_offset & PAGE_MASK_64 || size & PAGE_MASK) {
		error = EINVAL;
		goto exit;
	}
	kret = ubc_upl_map(pl, &ioaddr);
	if (kret != KERN_SUCCESS)
		panic("smbfs_pagein: ubc_upl_map %d!", kret);
	smb_scred_init(&scred, ap->a_context);

	uio = uio_create(1, f_offset, UIO_SYSSPACE, UIO_READ);
	if (f_offset + size > (off_t)np->n_size) { /* stop at EOF */
		size -= PAGE_SIZE;
		size += np->n_size & PAGE_MASK_64;
	}
	uio_addiov(uio, CAST_USER_ADDR_T(ioaddr + pl_offset), size);

	smp = VFSTOSMBFS(vnode_mount(vp));
	(void) smbfs_smb_flush(np, &scred);
	error = smb_read(smp->sm_share, np->n_fid, uio, &scred);
	if (!error && uio_resid(uio))
		error = EFAULT;
	uio_free(uio);
	if (!error && size != ap->a_size)
		bzero((caddr_t)(ioaddr + pl_offset) + size, ap->a_size - size);
	kret = ubc_upl_unmap(pl);
	if (kret != KERN_SUCCESS)
		panic("smbfs_pagein: ubc_upl_unmap %d", kret);
exit:
	if (error)
		log(LOG_WARNING, "smbfs_pagein: read error=%d\n", error);
	if (nocommit)
		return (error);
	if (error) {
		(void)ubc_upl_abort_range(pl, pl_offset, ap->a_size,
					  UPL_ABORT_ERROR |
					  UPL_ABORT_FREE_ON_EMPTY);
	} else
		(void)ubc_upl_commit_range(pl, pl_offset, ap->a_size,
					   UPL_COMMIT_CLEAR_DIRTY |
					   UPL_COMMIT_FREE_ON_EMPTY);
	return (error);
}


static int
smbfs_pageout(ap) 
	struct vnop_pageout_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode	*a_vp,
		upl_t	a_pl,
		vm_offset_t	a_pl_offset,
		off_t	a_f_offset,
		size_t	a_size,   
		int	a_flags
		vfs_context_t a_context;
	} */ *ap;
{       
	vnode_t vp;
	upl_t pl;
	size_t size;
	off_t f_offset;
	vm_offset_t pl_offset, ioaddr;
	int error, nocommit;
	struct smbnode *np;
	struct smbmount *smp;
	struct smb_cred scred;
	uio_t uio;
	kern_return_t   kret;

	f_offset = ap->a_f_offset;
	size = ap->a_size;
	pl = ap->a_pl;
	pl_offset = ap->a_pl_offset;
	vp = ap->a_vp;
	smp = VFSTOSMBFS(vnode_mount(vp));
	nocommit = ap->a_flags & UPL_NOCOMMIT;
	if (pl == (upl_t)NULL)
		panic("smbfs_pageout: no upl");
	np = VTOSMB(vp);
	if (size <= 0 || f_offset < 0 || f_offset >= (off_t)np->n_size ||
	    f_offset & PAGE_MASK_64 || size & PAGE_MASK) {
		error = EINVAL;
		goto exit;
	}
	if (vnode_vfsisrdonly(vp)) {
		error = EROFS;
		goto exit;
	}
	kret = ubc_upl_map(pl, &ioaddr);
	if (kret != KERN_SUCCESS)
		panic("smbfs_pageout: ubc_upl_map %d!", kret);
	smb_scred_init(&scred, ap->a_context);

	uio = uio_create(1, f_offset, UIO_SYSSPACE, UIO_WRITE);
	if (f_offset + size > (off_t)np->n_size) { /* stop at EOF */
		size -= PAGE_SIZE;
		size += np->n_size & PAGE_MASK_64;
	}
	uio_addiov(uio, CAST_USER_ADDR_T(ioaddr + pl_offset), size);

	error = smb_write(smp->sm_share, np->n_fid, uio, &scred, SMBWRTTIMO);
	uio_free(uio);
	np->n_flag |= (NFLUSHWIRE | NATTRCHANGED);
	kret = ubc_upl_unmap(pl);
	if (kret != KERN_SUCCESS)
		panic("smbfs_pageout: ubc_upl_unmap %d", kret);
exit:
	if (error)
		log(LOG_WARNING, "smbfs_pageout: write error=%d\n", error);
	else {
		/* if success, blow away statfs cache */
		smp->sm_statfstime = 0;
	}
	if (nocommit)
		return (error);
	if (error) {
		(void)ubc_upl_abort_range(pl, pl_offset, ap->a_size,
					  UPL_ABORT_DUMP_PAGES |
					  UPL_ABORT_FREE_ON_EMPTY);
	} else
		(void)ubc_upl_commit_range(pl, pl_offset, ap->a_size,
					   UPL_COMMIT_CLEAR_DIRTY |
					   UPL_COMMIT_FREE_ON_EMPTY);
	return (error);
}

/*
 * GOALS:
 *
 * 1. Keep pre-Tiger working, meaning we must read & write AD files for
 *    legacy attributes (resource fork and finder info).
 *    ("AD" is AppleDouble - in this case we mean dot-underscore files.)
 *
 * 2. Prepare to co-exist with a no-AD future, meaning we prefer
 *    streams over AD xattrs, and that we put *only* the two legacy
 *    attributes into AD xattrs.
 *
 * NOTES:
 *
 *    {get,set,remove,list}stream mean do the required smb protocol gorp.
 *    That fails if the remote fs is FAT rather than NTFS.  In that case
 *    we return ENOTSUP, which is an exception to goal #2 above.
 *
 *    KPI should be considered for:
 *        is-a-protected-xattr (com.apple.system.*)
 *        is-returnable-xattr (above plus length and utf8 checks)
 *
 * PSUEDOCODE:
 *
 * setxattr
 *
 *    if legacy-name (resource fork or finder info)
 *        map name to SFM/Thurbsy name
 *        setstream on that (logging interesting errors)
 *        return ENOTSUP // so AD xattr gets set regardless
 *    else
 *        setstream
 *
 * listxattr
 *
 *    liststreams
 *    remove ":" prefix and ":$DATA" suffixes
 *    remove any com.apple.system.* names
 *    map any SFM/Thurbsy names to our com.apple legacy names
 *    return ENOTSUP //so any AD xattrs get appended, which may mean dupes
 *
 * removexattr
 *
 *    if legacy-name (resource fork or finder info)
 *        map name to SFM/Thurbsy name
 *        removestream on that (logging interesting errors)
 *        return ENOTSUP // so AD xattr gets removed regardless
 *    else
 *        removestream
 *
 * getxattr
 *
 *    if legacy-name (resource fork or finder info)
 *        map name to SFM/Thurbsy name
 *        getstream on that
 *        if the stream wasn't found
 *            return ENOTSUP // so AD xattr can be found
 *    else
 *        getstream
 *
 * XXX If pre-Tiger writes to an AD *after* Tiger writes then the AD and the
 * native xattr will be out of sync.  pre-Tiger and Tiger (slash SFM slash
 * Thurbsy) will be happy at first, but Tiger will fetch a stale xattr and
 * is likely to rewrite something based upon stale...
 * A candidate solution was for getxattr to first compare mod times of the
 * AD file and the base file, and if the AD file looked newer, to try getting
 * legacy-data there before going to the native stream via getstream.  But
 * ordinary pre-Tiger file copy results in the AD and base files having almost
 * the same mtime, and a Tiger setstream does that too.  No viable solution is
 * apparent yet.  Note that modifying a native-stream sets the base file mtime
 * only - there is no independant stream mod time available to us from a
 * Windows server.
 * XXX This approach is "best effort" and at least any problems seen will be
 * eliminated by upgrading from pre-Tiger to Tiger.  On the plus side is that
 * we should have no transition pain with Tiger to post-Tiger, and should
 * interoperate with existing Thurbsy and Service For Macintosh environments.
 */

static char *
xattr2sfm(char *xa)
{
	if (!bcmp(xa, XATTR_RESOURCEFORK_NAME, sizeof(XATTR_RESOURCEFORK_NAME)))
		return (SFM_RESOURCEFORK_NAME);
	if (!bcmp(xa, XATTR_FINDERINFO_NAME, sizeof(XATTR_FINDERINFO_NAME)))
		return (SFM_FINDERINFO_NAME);
	return (NULL);
}

static int
smbfs_setxattr(ap)
	struct vnop_setxattr_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		char *a_name;
		uio_t a_uio;
		int a_options;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	char *name = ap->a_name;
	char *sfmname;
	uio_t uio = ap->a_uio;
	int options = ap->a_options;
	int error, cerror;
	struct smb_cred scred;
	u_int16_t fid;
	int disp = 0;
	struct smbnode *np = VTOSMB(vp);
	struct smbmount *smp = VTOSMBFS(vp);

	if (!(smp->sm_flags & FILE_NAMED_STREAMS))
		return (ENOTSUP);
	sfmname = xattr2sfm(name);
#ifndef DUAL_EAS	/* XXX */
	/*
	 * In Tiger Carbon still accesses dot-underscore files directly, so...
	 * For Tiger we store resource-fork and finder-info only in the
	 * dot-underscore file (ENOTSUP triggers vn_setxattr to do that.)
	 * and *not* into the SFM/Thursby AFP_* streams.
	 */
	if (sfmname)
		return (ENOTSUP);
#endif
	switch(options & (XATTR_CREATE|XATTR_REPLACE)) {
		case XATTR_CREATE:
			if (sfmname)
				return (EJUSTRETURN);
			disp = NTCREATEX_DISP_CREATE;
			break;
		case XATTR_REPLACE:
			if (sfmname)
				return (EJUSTRETURN);
			disp = NTCREATEX_DISP_OVERWRITE;
			break;
		case 0:
			disp = NTCREATEX_DISP_OVERWRITE_IF;
			break;
		/*
		 * This case should have been screened off by VFS
		 */
		case (XATTR_CREATE|XATTR_REPLACE):
			return (EINVAL);
	}

	if (sfmname) {
		/*
		 * Ensure we don't write off the end of the 32 byte
		 * finder-info (into Windows' 12 byte area).
		 */
		if (!strcmp(sfmname, SFM_FINDERINFO_NAME) &&
		    uio_offset(uio) + uio_resid(uio) > FINDERINFOSIZE)
			return (EINVAL);
		name = sfmname;
		uio = uio_duplicate(uio);
	}

	smb_scred_init(&scred, ap->a_context);
	error = smbfs_smb_create(np, name, strlen(name), &scred, &fid, disp, 1);
	if (error) {
		if (sfmname)
			SMBERROR("error %d creating \"%.*s:%.*s\"\n", error,
				 np->n_nmlen, np->n_name, strlen(name), name);
		goto out;
	}
	/*
	 * If writing finder-info, munge the uio so we write at
	 * offset 16 where SFM/Thurbsy keep the actual finder info.
	 */
	if (sfmname && !strcmp(sfmname, SFM_FINDERINFO_NAME)) {
		uio_setoffset(uio, uio_offset(uio) + 4*4);
		error = smb_write(smp->sm_share, fid, uio, &scred, SMBWRTTIMO);
		uio_setoffset(uio, uio_offset(uio) - 4*4);
	} else
		error = smb_write(smp->sm_share, fid, uio, &scred, SMBWRTTIMO);
	if (error && sfmname)
		SMBERROR("error %d writing \"%.*s:%.*s\"\n", error,
			 np->n_nmlen, np->n_name, strlen(name), name);
	/* --np->n_fidrefs; */ /* create above doesn't take ref */
	cerror = smbfs_smb_close(smp->sm_share, fid, NULL, &scred);
	if (cerror)
		SMBERROR("error %d closing \"%.*s:%.*s\"\n", cerror,
			 np->n_nmlen, np->n_name, strlen(name), name);
out:
	if (sfmname) {
		/*
		 * The errno returned here will make vn_setxattr write the
		 * attribute out to the dot-underscore file too.  That's
		 * also why we have a uio_duplicate/uio_free pair in this
		 * path - so offset, resid, and any other fields are in
		 * their original state for the default_setxattr
		 */
		error = ENOTSUP;
		if (uio)
			uio_free(uio);
	}
	return (error);
}

static int
smbfs_listxattr(ap)
	struct vnop_listxattr_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		uio_t a_uio;
		size_t *a_size;
		int a_options;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	uio_t uio = ap->a_uio;
	size_t *sizep = ap->a_size;
	struct smbnode *np = VTOSMB(vp);
	struct smbmount *smp = VTOSMBFS(vp);
	struct smb_cred scred;
	int error;

	/*
	 * Samba does not (yet) set FILE_NAMED_STREAMS,
	 * nor is it set on FAT volumes.  In such cases
	 * we list only the dot-underscore file...
	 */ 
	if (!(smp->sm_flags & FILE_NAMED_STREAMS))
		return (ENOTSUP);

	smb_scred_init(&scred, ap->a_context);
	error = smbfs_smb_qstreaminfo(np, &scred, uio, sizep);
	if (error) {
#ifndef XXX	/* are there errors we want to turn into ENOTSUP? */
		SMBERROR("error %d file \"%.*s\"\n", error,
			 np->n_nmlen, np->n_name);
#endif
		return (error);
	}
	/*
	 * Now we return ENOTSUP to get default_listxattr to append
	 * any xattrs which are in a dot-underscore file.
	 */
	return (ENOTSUP);
}

static int
smbfs_removexattr(ap)
	struct vnop_removexattr_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp; 
		char * a_name; 
		int a_options;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	char *name = ap->a_name;
	char *sfmname;
	int error;
	struct smb_cred scred;
	struct smbnode *np = VTOSMB(vp);
	struct smbmount *smp = VTOSMBFS(vp);

	if (!(smp->sm_flags & FILE_NAMED_STREAMS))
		return (ENOTSUP);

	sfmname = xattr2sfm(name);
#ifndef DUAL_EAS	/* XXX */
	/*
	 * In Tiger Carbon still accesses dot-underscore files directly, so...
	 * For Tiger we remove resource-fork and finder-info only from the
	 * dot-underscore file (ENOTSUP triggers vn_removexattr to do that)
	 * and *not* from the SFM/Thursby AFP_* streams.
	 */
	if (sfmname)
		return (ENOTSUP);
#endif
	if (sfmname)
		name = sfmname;
	/*
	 * If we are removing finder-info we have no viable choice but to
	 * remove the whole AFP_AfpInfo stream, which includes fields for
	 * backup-time and ProDOS info.
	 */
	smb_scred_init(&scred, ap->a_context);
	error = smbfs_smb_delete(np, &scred, name, strlen(name), 1);
	if (sfmname) {
		if (error && error != ENOENT)
			SMBERROR("error %d deleting \"%.*s:%.*s\"\n", error,
				 np->n_nmlen, np->n_name, strlen(name), name);
		if (error)
			error = ENOTSUP;
		else
			error = EJUSTRETURN;
	} else {	/* just an ordinary EA */
		if (error == ENOENT)
			error = ENOATTR;
	}
	return (error);
}

static int
smbfs_getxattr(ap)
	struct vnop_getxattr_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		char * a_name;
		uio_t a_uio;
		size_t *a_size;
		int a_options;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	char *name = ap->a_name;
	char *sfmname;
	uio_t uio = ap->a_uio;
	size_t *sizep = ap->a_size;
	u_int16_t fid;
	int error, cerror;
	struct smb_cred scred;
	struct smbnode *np = VTOSMB(vp);
	struct smbmount *smp = VTOSMBFS(vp);

	if (!(smp->sm_flags & FILE_NAMED_STREAMS))
		return (ENOTSUP);

	sfmname = xattr2sfm(name);
#ifndef DUAL_EAS	/* XXX */
	/*
	 * In Tiger Carbon still accesses dot-underscore files directly, so...
	 * For Tiger we get resource-fork and finder-info from the
	 * dot-underscore file (ENOTSUP triggers vn_getxattr to do that)
	 * and *not* from the SFM/Thursby AFP_* streams.
	 */
	if (sfmname)
		return (ENOTSUP);
#endif
	if (sfmname)
		name = sfmname;

	smb_scred_init(&scred, ap->a_context);
	error = smbfs_smb_open(np, SA_RIGHT_FILE_READ_DATA, &scred, NULL, &fid,
			       name, strlen(name), 1, sizep, NULL);
	if (error) {
		if (sfmname && error != ENOENT)
			SMBERROR("error %d opening \"%.*s:%.*s\"\n", error,
				 np->n_nmlen, np->n_name, strlen(name), name);
		goto out;
	}
	if (sizep && sfmname && !strcmp(sfmname, SFM_FINDERINFO_NAME))
		*sizep = FINDERINFOSIZE; /* XXX vfs should do this 4002750 */
	if (uio) {
		/*
		 * When reading finder-info, munge the uio so we read at
		 * offset 16 where SFM/Thurbsy store the actual finder info.
		 * Also ensure we don't read past those 32 bytes, exposing
		 * Windows-private data.
		 */
		if (sfmname && !strcmp(sfmname, SFM_FINDERINFO_NAME)) {
			user_ssize_t r = uio_resid(uio);

			if (uio_offset(uio) >= FINDERINFOSIZE) {
				uio_setresid(uio, 0);
			} else if (uio_offset(uio) + r > FINDERINFOSIZE)
				uio_setresid(uio,
					     FINDERINFOSIZE - uio_offset(uio));
			r = r - uio_resid(uio);
			uio_setoffset(uio, uio_offset(uio) + 4*4);
			error = smb_read(smp->sm_share, fid, uio, &scred);
			uio_setoffset(uio, uio_offset(uio) - 4*4);
			uio_setresid(uio, uio_resid(uio) + r);
		} else
			error = smb_read(smp->sm_share, fid, uio, &scred);
		if (sfmname && error)
			SMBERROR("error %d reading \"%.*s:%.*s\"\n", error,
				 np->n_nmlen, np->n_name, strlen(name), name);
	}
	/* --np->n_fidrefs; */	/* xattr open above doesn't take ref */
	cerror = smbfs_smb_close(smp->sm_share, fid, NULL, &scred);
	if (cerror)
		SMBERROR("error %d closing \"%.*s:%.*s\"\n", cerror,
			 np->n_nmlen, np->n_name, strlen(name), name);
out:
	if (sfmname && error)
		error = ENOTSUP;
	return (error);
}
