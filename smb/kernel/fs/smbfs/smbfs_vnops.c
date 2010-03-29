/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2009 Apple Inc. All rights reserved.
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
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/xattr.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/unistd.h>
#include <sys/lockf.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <vfs/vfs_support.h>
#include <sys/namei.h>

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
#include <netsmb/smb_converter.h>

#include <sys/buf.h>
#include <libkern/crypto/md5.h>

char smb_symmagic[SMB_SYMMAGICLEN] = {'X', 'S', 'y', 'm', '\n'};

int smbfs_setattr(vnode_t vp, struct vnode_attr *vap, vfs_context_t context);

/*
 * Supports reading a faked up symbolic link. This is Conrad and Steve
 * French method for storing and reading symlinks on Window Servers.
 */
static int smbfs_windows_readlink(struct smb_share *ssp, struct smbnode *np, 
								  struct uio *a_uio, vfs_context_t context)
{
	unsigned char *wbuf, *cp;
	u_int len, flen;
	uio_t uio;
	int error, cerror;
	u_int16_t	fid = 0;
	
	flen = SMB_SYMLEN;
	MALLOC(wbuf, void *, flen, M_TEMP, M_WAITOK);
	
	error = smbfs_smb_tmpopen(np, SA_RIGHT_FILE_READ_DATA, context, &fid);
	if (error)
		goto out;
	uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
	uio_addiov(uio, CAST_USER_ADDR_T(wbuf), flen);
	error = smb_read(ssp, fid, uio, context);
	uio_free(uio);
	cerror = smbfs_smb_tmpclose(np, fid, context);
	if (cerror)
		SMBERROR("error %d closing fid %d file %s\n", cerror, fid, np->n_name);
	if (error)
		goto out;
	for (len = 0, cp = wbuf + SMB_SYMMAGICLEN;
	     cp < wbuf + SMB_SYMMAGICLEN + SMB_SYMLENLEN-1; cp++) {
		if (*cp < '0' || *cp > '9') {
			SMBERROR("symlink length nonnumeric: %c (0x%x)\n", *cp, *cp);
			return (EINVAL);
		}
		len *= 10;
		len += *cp - '0';
	}
	if (len != np->n_size) {
		SMBERROR("symlink length payload changed from %u to %u\n", (unsigned)np->n_size, len);
		np->n_size = len;
	}
	cp = wbuf + SMB_SYMHDRLEN;
	error = uiomove((caddr_t)cp, (int)len, a_uio);
out:;
	FREE(wbuf, M_TEMP);
	return (error);
}

/*
 * Create the data required for a faked up symbolic link. This is Conrad and Steve
 * French method for storing and reading symlinks on Window Servers.
 * 
 */
static void * smbfs_create_windows_symlink_data(const char *target, size_t targetlen, 
												u_int32_t *rtlen)
{
	MD5_CTX md5;
	u_int32_t state[4];
	u_int32_t datalen, filelen;
	char *wbuf, *wp;
	int	 maxwplen;
	u_int32_t targlen = (u_int32_t)targetlen;
	
	datalen = SMB_SYMHDRLEN + targlen;
	filelen = SMB_SYMLEN;
	maxwplen = filelen;
	
	MALLOC(wbuf, void *, filelen, M_TEMP, M_WAITOK);
	
	wp = wbuf;
	bcopy(smb_symmagic, wp, SMB_SYMMAGICLEN);
	wp += SMB_SYMMAGICLEN;
	maxwplen -= SMB_SYMMAGICLEN;
	(void)snprintf(wp, maxwplen, "%04d\n", targlen);
	wp += SMB_SYMLENLEN;
	maxwplen -= SMB_SYMLENLEN;
	MD5Init(&md5);
	MD5Update(&md5, (unsigned char *)target, targlen);
	MD5Final((u_char *)state, &md5);
	(void)snprintf(wp, maxwplen, "%08x%08x%08x%08x\n", htobel(state[0]),
				   htobel(state[1]), htobel(state[2]), htobel(state[3]));
	wp += SMB_SYMMD5LEN;
	bcopy(target, wp, targlen);
	wp += targlen;
	if (datalen < filelen) {
		*wp++ = '\n';
		datalen++;
		if (datalen < filelen)
			memset((caddr_t)wp, ' ', filelen - datalen);
	}
	*rtlen = filelen;
	return wbuf;
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
	if (np->acl_cache_data)
		FREE(np->acl_cache_data, M_TEMP);
	np->acl_cache_data = NULL;
	np->acl_cache_timer.tv_sec = 0;
	np->acl_cache_timer.tv_nsec = 0;
	np->acl_error = 0;
	np->acl_cache_len = 0;
	lck_mtx_unlock(&np->f_ACLCacheLock);
}
/*
 * smbfs_down is called when we have a message that timeout or we are
 * starting a reconnect. It uses vfs_event_signal() to tell interested parties 
 * the connection with the server is "down".
 */
void
smbfs_down(struct smb_share *share)
{
	struct smbmount *smp;
	
	lck_mtx_lock(&share->ss_mntlock);
	smp = share->ss_mount;
	if (smp && !(smp->sm_status & SM_STATUS_TIMEO)) {
		/* If we have a ss_mount then we have a sm_mp */
		vfs_event_signal(&(vfs_statfs(smp->sm_mp))->f_fsid, VQ_NOTRESP, 0);
		smp->sm_status |= SM_STATUS_TIMEO;
	}
	lck_mtx_unlock(&share->ss_mntlock);
}

/*
 * smbfs_up is called when we receive a successful response to a message or we have 
 * successfully reconnect. It uses vfs_event_signal() to tell interested parties 
 * the connection is OK again  if the connection was having problems.
 */
void
smbfs_up(struct smb_share *share)
{
	struct smbmount *smp;
	
	lck_mtx_lock(&share->ss_mntlock);
	smp = share->ss_mount;
	if (smp && (smp->sm_status & SM_STATUS_TIMEO)) {
		smp->sm_status &= ~SM_STATUS_TIMEO;
		/* If we have a ss_mount then we have a sm_mp */
		vfs_event_signal(&(vfs_statfs(smp->sm_mp))->f_fsid, VQ_NOTRESP, 1);		
	}
	lck_mtx_unlock(&share->ss_mntlock);
}

void
smbfs_dead(struct smb_share *share)
{
	struct smbmount *smp;
	
	lck_mtx_lock(&share->ss_mntlock);
	smp = share->ss_mount;
	if (smp && !(smp->sm_status & SM_STATUS_DEAD)) {
		/* If we have a ss_mount then we have a sm_mp */
		vfs_event_signal(&(vfs_statfs(smp->sm_mp))->f_fsid, VQ_DEAD, 0);
		smp->sm_status |= SM_STATUS_DEAD;
	}
	lck_mtx_unlock(&share->ss_mntlock);
}

/*
 * Get a pointer to the offset requested. Verify that the offset is 
 * is in bounds and the structure does not go past the end of the buffer.
 */
void * smb_sdoffset(struct ntsecdesc *w_secp, size_t w_seclen, int sd_type)
{
	void	*rt_ptr;
	int32_t	sd_off = 0;
	int32_t	sd_len = 0;
	int32_t	end_len = 0;
	
	if (sd_type == OWNER_SECURITY_INFORMATION) {
		sd_len = (int32_t)sizeof(struct ntsid);
		sd_off = letohl(w_secp->sd_owneroff);
	}
	else if (sd_type == GROUP_SECURITY_INFORMATION) {
		sd_len = (int32_t)sizeof(struct ntsid);		
		sd_off = letohl(w_secp->sd_groupoff);
	}
	else if (sd_type == DACL_SECURITY_INFORMATION) {
		sd_len = (int32_t)sizeof(struct ntacl);		
		sd_off = letohl(w_secp->sd_dacloff);
	}
	else if (sd_type == SACL_SECURITY_INFORMATION) {
		sd_len = (int32_t)sizeof(struct ntacl);		
		sd_off = letohl(w_secp->sd_sacloff);
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

	rt_ptr = sd_off+(u_int8_t *)w_secp;

	return 	rt_ptr;
}

#define MAX_SID_PRINTBUFFER	256	/* Used to print out the sid in case of an error */
/*
 * Used for debugging and writing error messages into the system log. Still needs 
 * to have buffer checking done on it. 
*/
void smb_printsid(struct ntsid *sidptr, char *sidendptr, const char *printstr, 
				 const char *filename, int index, int error)
{
	char sidprintbuf[MAX_SID_PRINTBUFFER];
	char *s = sidprintbuf;
	int subs;
	u_int64_t auth = 0;
	unsigned i, *ip;
	size_t len;
	u_int32_t *subauthptr = (u_int32_t *)((char *)sidptr + sizeof(struct ntsid));
	char *subauthendptr;
	
	bzero(sidprintbuf, MAX_SID_PRINTBUFFER);
	for (i = 0; i < sizeof(sidptr->sid_authority); i++)
		auth = (auth << 8) | sidptr->sid_authority[i];
	s += snprintf(s, MAX_SID_PRINTBUFFER, "S-%u-%llu", sidptr->sid_revision, auth);
	
	subs = sidptr->sid_subauthcount;
	if (!SMBASSERT(subs <= KAUTH_NTSID_MAX_AUTHORITIES))
		subs = KAUTH_NTSID_MAX_AUTHORITIES;
	
	/*
	 * We know that sid_subauthcount has to be less than or equal to 
	 * KAUTH_NTSID_MAX_AUTHORITIES which is currently 16. So the highest
	 * this can go is 16 * sizeof(u_int32_t) so no overflow problem here.
	 */
	subauthendptr = (char *)((char *)subauthptr + (subs * sizeof(u_int32_t)));
	
	if (subauthendptr > sidendptr) {
		len = MAX_SID_PRINTBUFFER - (s - sidprintbuf);
		s += snprintf(s, len, " buffer overflow prevented: %p > %p", subauthendptr, sidendptr); 
		return;		
	}
	
	for (ip = subauthptr; subs--; ip++)  { 
		len = MAX_SID_PRINTBUFFER - (s - sidprintbuf);
		DBG_ASSERT(len > 0)
		s += snprintf(s, len, "-%u", *ip); 
	}

	if (error)
		SMBWARNING("%s: sid[%d] = %s error = %d %s%s\n", printstr, index, sidprintbuf, 
				   error, (filename) ? "for " : "", filename);
	else
		SMBWARNING("%s: sid[%d] = %s %s%s\n", printstr, index, sidprintbuf, 
				   (filename) ? "for " : "", filename);
}

static
int smb_sid_is_equal(const ntsid_t * rhs, const ntsid_t * lhs)
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

/* Return 1 or 0, depending on whether the SID is in the domain given by the domain SID. */
int smb_sid_in_domain(const ntsid_t * domain, const ntsid_t * sid)
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
 * The calling routine will guarantee that sidptr has enough space to hold the 
 * ntsid structure, but we need to protect ourself from going pass the size
 * of the ntsid structure and any values contain inside the ntsid structure.
 *
 * We no longer need to call smb_sid_endianize before calling this routine. We
 * now do it place, should help with performance.
 */
void smb_sid2sid16(struct ntsid *sidptr, ntsid_t *sid16ptr, char *sidendptr)
{
	u_int8_t ii;
	u_int32_t *subauthptr = (u_int32_t *)((char *)sidptr + sizeof(struct ntsid));
	char *subauthendptr;
	
	bzero(sid16ptr, sizeof(*sid16ptr));
	if (sidptr->sid_subauthcount > KAUTH_NTSID_MAX_AUTHORITIES) {
		SMBERROR("sidp->sid_subauthcount count too big: %d\n", sidptr->sid_subauthcount);
		return;
	}
	
	/*
	 * We know that sid_subauthcount has to be less than or equal to 
	 * KAUTH_NTSID_MAX_AUTHORITIES which is currently 16. So the highest
	 * this can go is 16 * sizeof(u_int32_t) so no overflow problem here.
	 */
	subauthendptr = (char *)((char *)subauthptr + (sidptr->sid_subauthcount * sizeof(u_int32_t)));
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
static void smb_sid_endianize(struct ntsid *sidptr, size_t len)
{
	char *sidendptr = (char *)sidptr + len;
	u_int32_t *subauthptr = (u_int32_t *)((char *)sidptr + sizeof(struct ntsid));
	char *subauthendptr;
	int n;

	/*
	 * We know that sid_subauthcount has to be less than or equal to 
	 * KAUTH_NTSID_MAX_AUTHORITIES which is currently 16. So the highest
	 * this can go is 16 * sizeof(u_int32_t) so no overflow problem here.
	 */
	subauthendptr = (char *)((char *)subauthptr + (sidptr->sid_subauthcount * sizeof(u_int32_t)));	
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
 * This is the main routine that goes across the network to get our acl information. We now always ask
 * for everything so we can make less calls. If the cache data is up to date then we will return that
 * information. We also do negative caching, if the server returns an error we cache that fact and 
 * continue to return the error until the cache information times out. 
 *
 * Remember that the vfs will help us with caching, but not in the negative case. Also it does not solve the
 * problem of multiple different calls coming into us back to back. So in a typical case we will get the following
 * calls and they will require an acl lookup for each item.
 * 
 * UID and GID request
 * Do we have write access
 * Do we have read access
 * Do we have search/excute access
 *
 * So by caching we are removing 12 network calls for each file in a directory. We only hold on to this cache
 * for a very short time, because it has a memory cost that we don't want to pay for any real length of time.
 * This is ok, becasue one we go through this process the vfs layer will handle the longer caching of these
 * request.
 */
static int smbfs_update_acl_cache(struct smb_share *ssp, struct smbnode *np, vfs_context_t context, 
								  struct ntsecdesc **w_sec, size_t *seclen)
{
	u_int32_t selector = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION;
	u_int16_t			fid = 0;
	struct timespec		ts;
	struct ntsecdesc	*acl_cache_data = NULL;
	size_t				acl_cache_len = 0;
	int					error;
	
	/* Check to see if the cache has time out */
	nanouptime(&ts);
	if (timespeccmp(&np->acl_cache_timer, &ts, >)) {
		/* Ok the cache is still good take a lock and retrieve the data */
		lck_mtx_lock(&np->f_ACLCacheLock);
		if (timespeccmp(&np->acl_cache_timer, &ts, >)) {
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
	
	error = smbfs_smb_tmpopen(np, STD_RIGHT_READ_CONTROL_ACCESS, context, &fid);
	if (error == 0) {
		int cerror;
			
		error = smbfs_smb_getsec(ssp, fid, context, selector, (struct ntsecdesc **)&acl_cache_data, &acl_cache_len);
		cerror = smbfs_smb_tmpclose(np, fid, context);
		if (cerror)
			SMBWARNING("error %d closing fid %d file %s\n", cerror, fid, np->n_name);	
		if ((error == 0) && (acl_cache_data == NULL))
			error = EBADRPC;
	}
	
	/* Don't let anyone play with the acl cache until we are done */
	lck_mtx_lock(&np->f_ACLCacheLock);
	/* Free the old data no longer needed */
	if (np->acl_cache_data)
		FREE(np->acl_cache_data, M_TEMP);
	np->acl_cache_data = acl_cache_data;
	np->acl_cache_len = acl_cache_len;
	np->acl_error = error;
	/* We have new information reset our timer  */
	SET_ACL_CACHE_TIME(np);
	
done:
	if (np->acl_error || (np->acl_cache_data == NULL)) {
		*w_sec = NULL;
		*seclen = 0;
		if (np->acl_error == 0)
			np->acl_error =  EBADRPC;	/* Should never happen, but just to be safe */
	} else {
		MALLOC(*w_sec, struct ntsecdesc *, np->acl_cache_len, M_TEMP, M_NOWAIT);
		if (*w_sec) {
			*seclen = np->acl_cache_len;		
			bcopy(np->acl_cache_data, *w_sec, np->acl_cache_len);
		} else {
			*w_sec = np->acl_cache_data;
			*seclen = np->acl_cache_len;
			np->acl_cache_data = NULL;
			np->acl_cache_len = 0;
			np->acl_cache_timer.tv_sec = 0;
			np->acl_cache_timer.tv_nsec = 0;
		}
	}
	error = np->acl_error;
	lck_mtx_unlock(&np->f_ACLCacheLock);
	return error;	
}

/*
 * We no longer return an error from this routine. We can either get the item or we can't. If we
 * can't then return null. Nothing else we can do and it makes the rest of the OS happy.
 */
int smbfs_getsecurity(struct smbnode *np, struct vnode_attr *vap, vfs_context_t context)
{
	struct smbmount		*smp = np->n_mount;
	struct smb_share	*ssp = np->n_mount->sm_share;
	int unix_extensions = ((UNIX_CAPS(SSTOVC(ssp)) & CIFS_UNIX_POSIX_PATH_OPERATIONS_CAP)) ? TRUE : FALSE;
	int					error;
	int					warn_error;
	struct ntsecdesc	*w_sec;	/* Wire sec descriptor */
	u_int32_t			acecount, j, aflags;
	struct ntacl		*w_dacl;	/* Wire DACL */
	struct ntsid		*w_sidp;	/* Wire SID */
	kauth_acl_t			res = NULL;	/* acl result buffer */
	struct ntace		*w_acep = NULL;	/* Wire ACE */
	kauth_ace_rights_t	arights;
	u_int32_t			w_rights;
	ntsid_t				sid;	/* temporary, for a kauth sid */
	size_t				seclen;
	uid_t				file_uid = KAUTH_UID_NONE;
	gid_t				file_gid = KAUTH_GID_NONE;

	/* We do not support acl access on a stream node */
	if (vnode_isnamedstream(np->n_vnode))
		return EINVAL;

	if (VATTR_IS_ACTIVE(vap, va_acl))
		vap->va_acl = NULL;					/* default */

	if (VATTR_IS_ACTIVE(vap, va_guuid))
		vap->va_guuid = kauth_null_guid;	/* default */

	if (VATTR_IS_ACTIVE(vap, va_uuuid))
		vap->va_uuuid = kauth_null_guid;	/* default */
	/* Check to make sure we have current acl information */
	error = smbfs_update_acl_cache(ssp, np, context, &w_sec, &seclen);
	/* We couldn't get the ACL, nothing left to do here */
	if (error) {
		SMBDEBUG("smbfs_update_acl_cache failed with error = %d\n", error);
		goto exit;
	}
	
	if (VATTR_IS_ACTIVE(vap, va_guuid)) {
		w_sidp = sdgroup(w_sec, seclen);
		if (!w_sidp) {
			SMBWARNING("no group sid received, file %s\n", np->n_name);
		} else {
			smb_sid2sid16(w_sidp, &sid, (char*)w_sec+seclen);
			warn_error = kauth_cred_ntsid2guid(&sid, &vap->va_guuid);
			if ((warn_error) && (smbfs_loglevel == SMB_ACL_LOG_LEVEL)) {
				smb_printsid(w_sidp, (char*)w_sec+seclen, "Group lookup failed", 
							 (const char  *)np->n_name, 0, warn_error);
			} else if (warn_error == 0) {
				/* 
				 * This is a cheap call since we lookup the va_guuid above the kernel 
				 * will have the gid in its cache 
				 */
				warn_error = kauth_cred_ntsid2gid(&sid, &file_gid);
				if (warn_error)
					SMBWARNING("kauth_cred_ntsid2gid failed on %s with error %d\n", np->n_name, warn_error);				
			}
		}
	}
	if (VATTR_IS_ACTIVE(vap, va_uuuid)) {
		w_sidp = sdowner(w_sec, seclen);
		if (!w_sidp) {
			SMBWARNING("no user sid received, file %s\n", np->n_name);
		} else {
			smb_sid2sid16(w_sidp, &sid, (char*)w_sec+seclen);
			warn_error = kauth_cred_ntsid2guid(&sid, &vap->va_uuuid);
			if ((warn_error) && (smbfs_loglevel == SMB_ACL_LOG_LEVEL)) {
				smb_printsid(w_sidp, (char*)w_sec+seclen, "Owner lookup failed", 
							 (const char  *)np->n_name, 0, warn_error);
			} else if (warn_error == 0) {
				/* 
				 * This is a cheap call since we lookup the va_guuid above the kernel 
				 * will have the gid in its cache 
				 */
				warn_error = kauth_cred_ntsid2uid(&sid, &file_uid);
				if (warn_error)
					SMBWARNING("kauth_cred_ntsid2uid failed on %s with error %d\n", np->n_name, warn_error);
			}
		}
	}
	
	if (VATTR_IS_ACTIVE(vap, va_acl)) {
		char *endptr;
		
		w_dacl = sddacl(w_sec, seclen);
		if (w_dacl == NULL)
			goto exit;
		/* Is there anything we can do to verify acecount, just not sure */
		acecount = letohs(w_dacl->acl_acecount);
		res = kauth_acl_alloc(acecount);
		if (!res) {
			error = ENOMEM;
			SMBDEBUG("kauth_acl_alloc failed with error = %d\n", error);
			goto exit;
		}
		/* Only count entries we add to the array, don't count dropped entries */
		res->acl_entrycount = 0;
		res->acl_flags = letohs(w_sec->sd_flags);
		if (res->acl_flags & SD_DACL_PROTECTED)
			res->acl_flags |= KAUTH_FILESEC_NO_INHERIT;
		else
			res->acl_flags &= ~KAUTH_FILESEC_NO_INHERIT;
		
		endptr = (char *)w_sec+seclen;
		
		for (j = 0, w_acep = aclace(w_dacl); 
			 (((char *)acesid(w_acep) < endptr) && (j < acecount)); 
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
				SMBERROR("ACE type %d file(%s)\n", acetype(w_acep), np->n_name);
				error = EPROTO;	/* Gets ignore currently, should it be EIO */
				goto exit;
			}
			w_sidp = acesid(w_acep);
			if ((char *)w_sidp+sizeof(*w_sidp) > endptr) {
				SMBERROR("ACE type %d file(%s) would have caused a buffer overrun!\n", acetype(w_acep), np->n_name);
				error = EPROTO;	/* Gets ignore currently, should it be EIO */
				goto exit;				
			}
			smb_sid2sid16(w_sidp, &sid, (char*)w_sec+seclen);
			warn_error = kauth_cred_ntsid2guid(&sid, &res->acl_ace[res->acl_entrycount].ace_applicable);
			/*
			 * Couldn't translate the sid, see if its in our list. If we find a 
			 * match then replace it with our owner sid.
			 */
			if (warn_error && smp->ntwrk_sids) {
				uint32_t sid_index;
				for (sid_index = 0; sid_index < smp->ntwrk_sids_cnt; sid_index++) {
					if (bcmp(&smp->ntwrk_sids[sid_index], &sid, sizeof(sid)) == 0) {
						/* Replace with the user's SID, always the first entry in the list */
						warn_error = kauth_cred_ntsid2guid(&smp->ntwrk_sids[0], &res->acl_ace[res->acl_entrycount].ace_applicable);
						if (smbfs_loglevel == SMB_ACL_LOG_LEVEL) {
							SMBWARNING("Found matching ntwrk_sids for file %s : %d\n", np->n_name, warn_error);
						}
					}
				}
			}
			if (warn_error) {
				if (smbfs_loglevel == SMB_ACL_LOG_LEVEL) {
					smb_printsid(w_sidp, (char*)w_sec+seclen, "ACL lookup failed", 
								 (const char  *)np->n_name, j, warn_error);
				}
				continue;
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
				SMBERROR("unknown ACE flag on file(%s)\n", np->n_name);
			if (aceflags(w_acep) & SUCCESSFUL_ACCESS_ACE_FLAG)
				aflags |= KAUTH_ACE_SUCCESS;
			if (aceflags(w_acep) & FAILED_ACCESS_ACE_FLAG)
				aflags |= KAUTH_ACE_FAILURE;
			res->acl_ace[res->acl_entrycount].ace_flags = aflags;
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
			res->acl_ace[res->acl_entrycount].ace_rights = arights;
			/* Success we have an entry, now count it */ 
			res->acl_entrycount++;
		}
		/* Only return the acl if we have at least one ace. */ 
		if (res->acl_entrycount) {
			vap->va_acl = res;
			res = NULL;			
		}
	}
exit:
	
	/*
	 * We attempted to get the file's uuid and guuid, so at this point we either
	 * got the uid and gid are we did't. Update the node information with the
	 * information we got from this call.
	 */
	 if ((VATTR_IS_ACTIVE(vap, va_guuid)) && (VATTR_IS_ACTIVE(vap, va_uuuid))) {
		 if ((file_uid != KAUTH_UID_NONE) || (file_gid != KAUTH_GID_NONE)) {
			 /*
			  * We treat both the uid and gid the same in this case, so do one of
			  * the following:
			  * 1. We translated the id, so use that id.
			  * 2. We couldn't translate the id and the node doesn't have an id
			  *    yet, so we just use the id of the user that mounted the volume.
			  */
			 if (file_uid != KAUTH_UID_NONE)
				 np->n_uid = file_uid;
			 else if (np->n_uid == KAUTH_UID_NONE)
				 np->n_uid = smp->sm_args.uid;
			 
			 if (file_gid != KAUTH_UID_NONE)
				 np->n_gid = file_gid;
			 else if (np->n_gid == KAUTH_GID_NONE)
				 np->n_gid = smp->sm_args.gid;
			 
			 /* 
			  * Not doing UNIX, so what should the ACCESS modes be set to, for 
			  * now stay with the method used in Tiger and Leopard. Someday we
			  * may want to set this to zero?
			  */
			 if (! unix_extensions)
				 np->n_mode |= ACCESSPERMS;
		 } else if (np->n_vnode && !unix_extensions) {
			 /* We have no idea, just  mapped the ids and access */
			 np->n_uid = smp->sm_args.uid;
			 np->n_gid = smp->sm_args.gid;
			 if (vnode_vtype(np->n_vnode) == VDIR)
				 np->n_mode |= smp->sm_args.dir_mode;
			 else	/* symlink or regular file */
				 np->n_mode |= smp->sm_args.file_mode;
		 }
	 }
	/*
	 * Currently we do not support the VNOP_ACCESS call (Radar 6669536). So we no longer
	 * return an error here, because the system just can't handle it. An error from
	 * this routine means no access to the item. Failing to set  VATTR_SET_SUPPORTED 
	 * on something which was requested causes fallback to EAs, which we never want.
	 * So we always return something, even if its just a NULL.
	 *
	 * So when we have Radar 6669536 working we should see what should be the
	 * correct values returned here. Need to look at the getattrlist code and 
	 * any calling return to see how they deal with these errors.
	 * 
	 */
	if (VATTR_IS_ACTIVE(vap, va_acl))
		VATTR_SET_SUPPORTED(vap, va_acl);
	if (VATTR_IS_ACTIVE(vap, va_guuid))
		VATTR_SET_SUPPORTED(vap, va_guuid);
	if (VATTR_IS_ACTIVE(vap, va_uuuid))
		VATTR_SET_SUPPORTED(vap, va_uuuid);
	
	if (res)
		kauth_acl_free(res);
	
	if (w_sec)
		FREE(w_sec, M_TEMP);
		
	return 0;
}

static int
smbfs_setsecurity(vnode_t vp, struct vnode_attr *vap, vfs_context_t context)
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
	size_t seclen = 0;
	
	/* We do not support acl access on a stream node */
	 if (vnode_isnamedstream(vp))
		 return ENOTSUP;
	
	/* Clear the ACL cache, after this call it will be out of date. */
	smbfs_clear_acl_cache(np);
		
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
			smb_hexdump(__FUNCTION__, "va_guuid: ", (u_char *)&vap->va_guuid, sizeof(vap->va_guuid));
			goto exit;
		}
		smb_sid_endianize(w_grp, MAXSIDLEN);
	}
	if (VATTR_IS_ACTIVE(vap, va_uuuid) &&
	    !kauth_guid_equal(&vap->va_uuuid, &kauth_null_guid)) {
		openrights |= STD_RIGHT_WRITE_OWNER_ACCESS;
		selector |= OWNER_SECURITY_INFORMATION;
		MALLOC(w_usr, struct ntsid *, MAXSIDLEN, M_TEMP, M_WAITOK);
		bzero(w_usr, MAXSIDLEN);
		error = kauth_cred_guid2ntsid(&vap->va_uuuid, (ntsid_t *)w_usr);
		if (error) {
			SMBERROR("kauth_cred_guid2ntsid (va_uuuid)  %d file %s\n", error, np->n_name);
			smb_hexdump(__FUNCTION__, "va_uuuid: ", (u_char *)&vap->va_uuuid, sizeof(vap->va_uuuid));
			goto exit;
		}
		smb_sid_endianize(w_usr, MAXSIDLEN);
	}
	if (VATTR_IS_ACTIVE(vap, va_acl) && (vap->va_acl != NULL) && 
		(vap->va_acl->acl_entrycount != KAUTH_FILESEC_NOACL)) {
		if (vap->va_acl->acl_entrycount > KAUTH_ACL_MAX_ENTRIES) {
			SMBERROR("acl_entrycount=%d, file(%s)\n", vap->va_acl->acl_entrycount, np->n_name);
			error = EINVAL;
			goto exit;
		}
		acecount = vap->va_acl->acl_entrycount;
		needed = sizeof(struct ntacl) +
		 	acecount * (sizeof(struct ntace) + MAXSIDLEN);
		MALLOC(w_dacl, struct ntacl *, needed, M_TEMP, M_WAITOK);
		bzero(w_dacl, needed);
		w_dacl->acl_revision = 0x02;
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
				SMBERROR("ace_flags=0x%x, file(%s)\n", acep->ace_flags, np->n_name);
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
				SMBERROR("kauth_cred_guid2ntsid (va_acl)  %d file %s\n", error,np->n_name);
				smb_hexdump(__FUNCTION__, "ace_applicable: ", (u_char *)&acep->ace_applicable, sizeof(acep->ace_applicable));
				goto exit;
			}
			smb_sid_endianize(w_sidp, needed);
			wset_acelen(w_acep, sizeof(struct ntace) + sidlen(w_sidp));
		}
		wset_acllen(w_dacl, ((char *)w_acep - (char *)w_dacl));
	}
	error = smbfs_smb_tmpopen(np, openrights, context, &fid);
	if (error)
		goto exit;
	/*
	 * We fetch a sec desc to get the flag bits we need to write back.
	 * Note we have to ask for everything as Windows will give back
	 * zeroes for any bits it thinks we don't care about.
	 */
	error = smbfs_smb_getsec(ssp, fid, context, 
							 OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, 
							 &w_sec, &seclen);
	if (error || w_sec == NULL) {
		SMBERROR("smbfs_smb_getsec %d file %s\n", error, np->n_name);
	} else {
		fsecflags = letohs(w_sec->sd_flags);
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
		error = smbfs_smb_setsec(ssp, fid, context, selector,
					 fsecflags, w_usr, w_grp, NULL, w_dacl);
	}
	cerror = smbfs_smb_tmpclose(np, fid, context);
	if (cerror)
		SMBERROR("error %d closing fid %d file %s\n", cerror, fid, np->n_name);
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

/*
 * We were doing an IO and recieved an error. Was the error caused because we were
 * reconnecting to the server. If yes then see if we can reopen the file. If everything
 * is ok and the file was reopened then get the fid we need for doing the IO.
 */
static int smbfs_io_reopen(vnode_t vp, uio_t uio, u_int16_t accessMode, u_int16_t *fid, int error, vfs_context_t context)
{
	struct smbnode *np = VTOSMB(vp);
	pid_t	pid = proc_pid(vfs_context_proc(context));
	
	if (np->f_openState != kNeedReopen)
		return(error);
	
	error = smbfs_smb_reopen_file(np, context);
	if (error)
		return(error);
	
	if (smbfs_findFileRef(vp, pid, accessMode, kCheckDenyOrLocks, uio_offset(uio), uio_resid(uio), NULL, fid))
			*fid = np->f_fid;
	DBG_ASSERT(*fid);	/* Should always have a fid at this point */
	return(0);
}

/*
 * smbfs_update_cache
 *
 * General routine that will update the meta data cache for  the vnode. If a vap
 * is passed in it will get filled in with the correct information, otherwise it
 * will be ignored.
 */
 int smbfs_update_cache(vnode_t vp, struct vnode_attr *vap, vfs_context_t context) 
 {
	 struct smbfattr fattr;
	 int error = smbfs_attr_cachelookup(vp, vap, context, FALSE);
 
	 if (error != ENOENT)
		 return (error);
	 error = smbfs_smb_lookup(VTOSMB(vp), NULL, NULL, &fattr, context);
	 if (error)
		 return (error);
	 smbfs_attr_cacheenter(vp, &fattr, TRUE, context);
	 return (smbfs_attr_cachelookup(vp, vap, context, FALSE));
 }
 
/*
 * smbfs_close -	The internal open routine, the vnode should be locked
 *		before this is called. We only handle VREG in this routine.
 */
int smbfs_close(vnode_t vp, int openMode, vfs_context_t context)
{
	struct smbnode		*np = VTOSMB(vp);
	struct proc		*p = vfs_context_proc(context);
	struct smb_share	*ssp = np->n_mount->sm_share;
	struct fileRefEntry	*fndEntry = NULL;
	struct fileRefEntry	*curr = NULL;
	int			error = 0;
	u_int16_t		accessMode = 0;
	u_int16_t		fid = 0;
	int32_t			needOpenFile;
	u_int16_t		openAccessMode;
	u_int32_t		rights;
	
	/* We have more than one open, so before closing see if the file needs to be reopened */
	if ((np->f_refcnt > 1) && (smbfs_smb_reopen_file(np, context) == EIO)) {
		SMBDEBUG(" %s waiting to be revoked\n", np->n_name);
		np->f_refcnt--;
		return (0);
	}
		
	if (openMode & FREAD)
		accessMode |= kAccessRead;

	if (openMode & FWRITE)
		accessMode |= kAccessWrite;

	/* Check the number of times Open() was called */
	if (np->f_refcnt == 1) {
		np->n_flag &= ~NFLUSHWIRE;	/* No need to do a wire flush since we are closing the file */
		ubc_msync(vp, 0, ubc_getsize(vp), NULL, UBC_PUSHDIRTY | UBC_SYNC | UBC_INVALIDATE);
		/* 
		 * This is the last Close() we will get, so close sharedForkRef 
		 * and any other forks created by ByteRangeLocks
		 */
		if (np->f_fid) {
			u_int16_t oldFID = np->f_fid;
						
			/* Close the shared file first. Clear out the refs to it 
			 * first so that no one else trys to use it while I'm waiting 
			 * for the close file reply to arrive.  There was a case 
			 * where cluster i/o was firing off after I sent the close 
			 * file req, but had not gotten the close file reply yet 
			 * and tyring to still use the shared file 
			 */
			np->f_fid = 0;		/* clear the ref num */
			np->f_accessMode = 0;
			np->f_rights = 0;
			np->f_openRWCnt = 0;
			np->f_openRCnt = 0;
			np->f_openWCnt = 0;
			np->f_needClose = 0;
			/*
			 * They didn't unlock the file before closing. A SMB close will remove
			 * any locks so lets free the memory associated with that lock.
			 */
			if (np->f_smbflock) {
				FREE(np->f_smbflock, M_LOCKF);
				np->f_smbflock = NULL;				
			}
			error = smbfs_smb_close(ssp, oldFID, context);
			if (error)
				SMBWARNING("close file failed %d on fid %d\n", error, oldFID);
		 }

		/* Remove forks that were opened due to ByteRangeLocks or DenyModes */
		lck_mtx_lock(&np->f_openDenyListLock);
		curr = np->f_openDenyList;
		while (curr != NULL) {
			error = smbfs_smb_close(ssp, curr->fid, context);
			if (error)
				SMBWARNING("close file failed %d on fid %d\n", error, curr->fid);
			curr = curr->next;
		}
		lck_mtx_unlock(&np->f_openDenyListLock);
		np->f_refcnt = 0;
		smbfs_removeFileRef (vp, NULL);		/* remove all file refs */
		/* 
		 * We did the last close on the file. This file is 
		 * marked for deletion on close. So lets delete it
		 * here. If we get an error then try again when the node
		 * becomes inactive.
		 */
		if (np->n_flag & NDELETEONCLOSE) {
			if (smbfs_smb_delete(np, context, NULL, 0, 0) == 0)
				np->n_flag &= ~NDELETEONCLOSE;
		}

		/* 
		 * It was not cacheable before, but now that all files are closed, 
		 * make it cacheable again (if its a valid cacheable file). If 
		 * we were caching should we remove attributes cache. Realy only
		 * matters when we turn on cluster io?
		 */
		if (vnode_isnocache(vp))
			vnode_clearnocache(vp);

		/* Did we change the file during the open */
		if (np->n_flag & NATTRCHANGED)
			np->attribute_cache_timer = 0;
		
		lck_mtx_lock(&np->f_openStateLock);
		if (np->f_openState == kNeedRevoke)
			error = 0;
		else np->f_openState = 0;	/* Clear the reopen flag, we are closed */
		lck_mtx_unlock(&np->f_openStateLock);
		goto exit;
	}
	/* More than one open */
	/* 
	 * See if we can match this Close() to a matching file that has byte range 
	 * locks or denyModes.
	 *
	 * NOTE: FHASLOCK can be set by open with O_EXCLUSIVE or O_SHARED which 
	 *	 maps to my deny modes or FHASLOCK could also have been set/cleared 
	 *	by calling flock directly.
	 *
	 * Cases that work are:
	 *	1)  Carbon using deny modes and thus FHASLOCK maps to my deny modes. 
	 *	    No flock being used.
	 *	2)  Cocoa using open with O_EXCLUSIVE or O_SHARED and not calling flock.
	 *	3)  Cocoa using open, then calling flock and later calling flock 
	 *	    to unlock before close.
	 *	4)  Cocoa open, then calling flock, but not calling flock to unlock 
	 *	    before close.  I would fall through to the shared fork code correctly.
	 *
	 * Cases that may not work are:
	 *	1)  Carbon using deny modes and then calling flock to unlock, thus 
	 *	    clearing FHASLOCK flag.I would assume it was the shared file.
	 *	2)  Cocoa open with O_EXCLUSIVE or O_SHARED and then calling flock 
	 *	    to lock and then unlock, then calling close.
	 *	3)  ??? 
	 */
	if (openMode & FHASLOCK) {
		u_int16_t tempAccessMode = accessMode;

		/* Try with denyWrite, if not found, then try with denyRead/denyWrite */
		tempAccessMode |= kDenyWrite;
		error = smbfs_findFileRef(vp, proc_pid(p), tempAccessMode, kExactMatch, 0, 0, &fndEntry, &fid);
		if (error != 0) {
			tempAccessMode |= kDenyRead;
			error = smbfs_findFileRef(vp, proc_pid(p), tempAccessMode, kExactMatch, 0, 0, &fndEntry, &fid);
		}
		if (error == 0)
			accessMode = tempAccessMode;
	}
	else {
		/* No deny modes used, so look for any forks opened for byteRangeLocks */
		error = smbfs_findFileRef(vp, proc_pid(p), accessMode, kExactMatch, 0, 0, &fndEntry, &fid);
	}

	/* always decrement the count, dont care if got an error or not */
	np->f_refcnt--;
	/*
	 * We have an Open Deny entry that is being used by more than one open call,
	 * just decrement it and get out.
	 */
	if ((error == 0) && fndEntry && (fndEntry->refcnt > 0)) {
		fndEntry->refcnt--;
		goto exit;
	}
	if (error == 0) {
		np->n_flag &= ~NFLUSHWIRE;	/* No need to do a wire flush since we are closing the file */
		error = smbfs_smb_close(ssp, fndEntry->fid, context);
		/* We are not going to get another close, so always remove it from the list */
		smbfs_removeFileRef(vp, fndEntry);
		goto exit;
	}
	/* Not an open deny mode open */ 
	needOpenFile = 0;
	openAccessMode = 0;
	fid = 0;
	rights = STD_RIGHT_READ_CONTROL_ACCESS;
  
	/* 
	 * Just return 0 for no err, but dont close the file since another 
	 * process is still using it 
	 */
	error = 0;
    
	/* Check to downgrade access mode if needed */
	switch (accessMode) {
	case (kAccessRead | kAccessWrite):
		np->f_openRWCnt -= 1;
		if ((np->f_openRWCnt == 0) && (np->f_openRCnt > 0) && (np->f_openWCnt == 0)) {
			/* drop from rw to read only */
			needOpenFile = 1;
			openAccessMode = kAccessRead;
			rights |= SA_RIGHT_FILE_READ_DATA;
		}
		/* Dont ever downgrade to write only since Unix expects read/write */
		break;
	case kAccessRead:
		np->f_openRCnt -= 1;
		/* Dont ever downgrade to write only since Unix expects read/write */
		break;
	case kAccessWrite:
		np->f_openWCnt -= 1;
		if ( (np->f_openRCnt > 0) && (np->f_openRWCnt == 0) && (np->f_openWCnt == 0) ) {
			/* drop from rw to read only */
			needOpenFile = 1;
			openAccessMode = kAccessRead;
			rights |= SA_RIGHT_FILE_READ_DATA;
		}
		break;
	}
	/* set up for the open fork */           
	if (needOpenFile == 1) {
		error = smbfs_smb_open(np, rights, NTCREATEX_SHARE_ACCESS_ALL, context, &fid);
		if (error == 0) {
			u_int16_t oldFID = np->f_fid;
			
			np->n_flag &= ~NFLUSHWIRE;	/* No need to do a wire flush since we are closing the file */
			/* We are downgrading the open flush it out any data */
			ubc_msync(vp, 0, ubc_getsize(vp), NULL, UBC_PUSHDIRTY | UBC_SYNC | UBC_INVALIDATE);
			/* Close the shared file first and use this new one now 
			 * Switch the ref before closing the old shared file so the 
			 * old file wont get used while its being closed. 
			 */
			np->f_fid = fid;	/* reset the ref num */
			np->f_accessMode = openAccessMode;
			np->f_rights = rights;
			error = smbfs_smb_close(ssp, oldFID, context);
			if (error)
				SMBWARNING("close file failed %d on fid %d\n", error, oldFID);
		}
	}
			 
exit:;
	return (error);
}

/*
 * smbfs_vnop_close - smbfs vnodeop entry point
 *	vnode_t a_vp;
 *	int a_fflags;
 *	vfs_context_t a_context;
 */
static int smbfs_vnop_close(struct vnop_close_args *ap)
{
	vnode_t 	vp = ap->a_vp;
	int		error = 0;
	
	if ( smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK) != 0)
		return (0);
	VTOSMB(vp)->n_lastvop = smbfs_vnop_close;
	
	if (vnode_isdir(vp)) {
		struct smbnode	*np = VTOSMB(vp);
		
		if (--np->d_refcnt) {
			error = 0;
		} else
			smbfs_closedirlookup(np, ap->a_context);
	}
	else if ( vnode_isreg(vp) )
		error = smbfs_close(vp, ap->a_fflag, ap->a_context);
	
	smbnode_unlock(VTOSMB(vp));
	return (error);
}

/*
 * smbfs_open -	The internal open routine, the vnode should be locked
 *		before this is called.
 */
int smbfs_open(vnode_t vp, int mode, vfs_context_t context)
{
	struct proc *p = vfs_context_proc(context);
	struct smbnode	*np = VTOSMB(vp);
	u_int16_t	accessMode = 0;
	u_int16_t	savedAccessMode = 0;
	int		addedRead = 0;
	u_int32_t	rights;
	u_int32_t	shareMode;
	u_int16_t	fid;
	int		error = 0;
	int		warning = 0;

	/* It was already open so see if the file needs to be reopened */
	if ((np->f_refcnt) && ((error = smbfs_smb_reopen_file(np, context)) != 0)) {
		SMBDEBUG(" %s waiting to be revoked\n", np->n_name);
		return (error);
	}
	
	/*
	 * We always ask for READ_CONTROL so we can always get the owner/group 
	 * IDs to satisfy a stat.
	 */
	rights = STD_RIGHT_READ_CONTROL_ACCESS;
	if (mode & FREAD) {
		accessMode |= kAccessRead;
		rights |= SA_RIGHT_FILE_READ_DATA;
	}
	if (mode & FWRITE) {
		accessMode |= kAccessWrite;
		rights |= SA_RIGHT_FILE_APPEND_DATA | SA_RIGHT_FILE_WRITE_DATA;
	}

	/*
	 * O_EXLOCK -> denyRead/denyWrite is always cacheable since we have exclusive access.
	 * O_SHLOCK -> denyWrite is always cacheable since we are the only one who can change the file.
	 * denyNone -> is not cacheable if from Carbon (a FSCTL call from Carbon will set the vnode to
	 *             be non cacheable). It is always cacheable from Unix since that is what home dirs
	 *             mainly use.
	 */
	shareMode = NTCREATEX_SHARE_ACCESS_ALL;
	if (mode & O_SHLOCK) {
		accessMode |= kDenyWrite;
		shareMode &= ~NTCREATEX_SHARE_ACCESS_WRITE; /* Remove the wr shared access */
	}

	if (mode & O_EXLOCK) {
		accessMode |= kDenyWrite;
		accessMode |= kDenyRead;
		shareMode &= ~(NTCREATEX_SHARE_ACCESS_WRITE | NTCREATEX_SHARE_ACCESS_READ); /* Remove the rdwr shared access */
	}
	savedAccessMode = accessMode;	/* Save the original access requested */ 

	if ((mode & O_EXLOCK) || (mode & O_SHLOCK)) {
		struct fileRefEntry *fndEntry = NULL;
		/* 
		 * if using deny modes and I had to open the file myself, then close 
		 * the file now so it does not interfere with the deny mode open.
		 * We only do this in read.
		 */
		if (np->f_needClose) {
			np->f_needClose = 0;
			warning = smbfs_close(vp,  FREAD, context);
			if (warning)
				SMBWARNING("error %d closing %s\n", warning, np->n_name);
		}

		/*
		 * In OS 9.x, if you opened a file for read only and it failed, and there 
		 * was a file opened already for read/write, then open worked.  Weird. 
		 * For X, if first open was r/w/dR/dW, r/w/dW, r/dR/dW, or r/dW,
		 * then a second open from same pid asking for r/dR/dW or r/dW will be allowed.
		 *
		 * See Radar 5050120 for an example of this happening.
		 */
		if ((accessMode & kAccessRead) && !(accessMode & kAccessWrite)) {
			error = smbfs_findFileRef(vp, proc_pid(p), kAccessRead, kAnyMatch, 0, 0, &fndEntry, &fid);			
			if ((error == 0) && fndEntry) {
				DBG_ASSERT(fndEntry);
				/* 
				 * We are going to reuse this Open Deny entry. Up the counter so we will know not
				 * to free it until the counter goes back to zero. 
				 */
				fndEntry->refcnt++;
				goto exit;				
			}
		}
		  
		fndEntry = NULL; /* Just because I am a little paranoid */ 
		/* Using deny modes, see if already in file list */
		error = smbfs_findFileRef(vp, proc_pid(p), accessMode, kExactMatch, 0, 0, &fndEntry, &fid);
		if (error == 0) {
			/* 
			 * Already in list due to previous open with deny modes. Can't have 
			 * two exclusive or two write/denyWrite. Multiple read/denyWrites are 
			 * allowed. 
			 */
			if ((mode & O_EXLOCK) || ((accessMode & kDenyWrite) &&  (accessMode & kAccessWrite)))
				error = EBUSY;
			else {
				DBG_ASSERT(fndEntry);
				/* 
				 * We are going to reuse this Open Deny entry. Up the counter so we will know not
				 * to free it until the counter goes back to zero. 
				 */
				fndEntry->refcnt++;
			}
		}
		else {
			/* not in list, so open new file */			
			error = smbfs_smb_open(np, rights, shareMode, context, &fid);
			if (error == 0) {
				/* if open worked, save the file ref into file list */
				smbfs_addFileRef (vp, p, accessMode, rights, fid, NULL);
			}
		}
		goto exit;
	
	}
	/* Removed Radar 4468345 now that Carbon is calling us the same as AFP */
	
	/*
	 * If we get here, then deny modes are NOT being used. If the open call is 
	 * coming in from Carbon, then Carbon will follow immediately with an FSCTL 
	 * to turn off caching (I am assuming that denyNone means this file will 
	 * be shared among multiple process and that ByteRangeLocking will be used).
	 *
	 * no deny modes, so use the shared file reference
	 *
	 */
	/* We have  open file descriptor for non deny mode opens */
	if (np->f_fid) {	/* Already open check to make sure current access is sufficient */
		int needUpgrade = 0;
		switch (np->f_accessMode) {
		case (kAccessRead | kAccessWrite):
			/* Currently RW, can't do any better than that so dont open a new fork */
			break;
		case kAccessRead:
			/* Currently only have Read access, if they want Write too, then open as RW */
			if (accessMode & kAccessWrite) {
				needUpgrade = 1;
				accessMode |= kAccessRead; /* keep orginal mode */
				rights |= SA_RIGHT_FILE_READ_DATA;
			}
			break;
		case kAccessWrite:
			/*  Currently only have Write access, if they want Read too, then open as RW */
			if (accessMode & kAccessRead) {
				needUpgrade = 1;
				accessMode |= kAccessWrite;
				rights |= SA_RIGHT_FILE_APPEND_DATA | SA_RIGHT_FILE_WRITE_DATA;
			}
			break;
		}
		if (! needUpgrade)	/*  the existing open is good enough */
			goto ShareOpen;
	}
	else if (accessMode == kAccessWrite) {
		/* 
	     * If opening with write only, try opening it with read/write. Unix 
	     * expects read/write access like open/map/close/PageIn. This also helps 
	     * the cluster code since if write only, the reads will fail in the 
	     * cluster code since it trys to page align the requests.  
	     */
		error = smbfs_smb_open(np, rights | SA_RIGHT_FILE_READ_DATA, shareMode, context, &fid);
		if (error == 0) {
			addedRead = 1;
			np->f_fid = fid;
			np->f_rights = rights | SA_RIGHT_FILE_READ_DATA;
			np->f_accessMode = accessMode | kAccessRead;
			goto ShareOpen;
		}
	}

	error = smbfs_smb_open(np, rights, shareMode, context, &fid);
	if (error)
		goto exit;
		
	/*
	 * We already had it open (presumably because it was open with insufficient 
	 * rights.) So now close the old open, if we already had it open.
	 */
	if (np->f_refcnt && np->f_fid) {
		warning = smbfs_smb_close(np->n_mount->sm_share, np->f_fid, context);
		if (warning)
			SMBWARNING("error %d closing %s\n", warning, np->n_name);
	}
	np->f_fid = fid;
	np->f_rights = rights;
	np->f_accessMode = accessMode;
	
ShareOpen:
          /* count nbr of opens with rw, r, w so can downgrade access in close if needed */
	switch (savedAccessMode) {
		case (kAccessWrite | kAccessRead):
			np->f_openRWCnt += 1;
			break;
		case kAccessRead:
			np->f_openRCnt += 1;
			break;
		case kAccessWrite:
			/* if opened with just write, then turn off cluster code */
			if (addedRead == 0)
				vnode_setnocache(vp);
			np->f_openWCnt += 1;
	break;
            }
exit:;
	if (!error) {	/* We opened the file or pretended too either way bump the count */
		np->f_refcnt++;
	}

	return (error);
}

/*
 * smbfs_vnop_open - smbfs vnodeop entry point
 *	vnode_t a_vp;
 *	int  a_mode;
 *	vfs_context_t a_context;
 */
static int smbfs_vnop_open(struct vnop_open_args *ap)
{
	vnode_t	vp = ap->a_vp;
	struct smbnode *np;
	int	error;

	/* We only open files and directorys */
	if (!vnode_isreg(vp) && !vnode_isdir(vp))
		return (EACCES);
		
	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);
	
	np = VTOSMB(vp);
//	SMBDEBUG("%s ap->a_mode = 0x%x\n", np->n_name, ap->a_mode);
	np->n_lastvop = smbfs_vnop_open;
	
	/* Just mark that the directory was opened */
	if (vnode_isdir(vp)) {
		np->d_refcnt++;
		error = 0;
	}
	else  {
		error = smbfs_open(vp, ap->a_mode, ap->a_context);
		/* 
		 * We created the file with different posix modes than request in smbfs_vnop_create. We need
		 * to correct that here, so set the posix modes to those request on create. See smbfs_composeacl
		 * for more details on this issue.
		 */
		if ((error == 0) && (np->set_create_va_mode) && (ap->a_mode & O_CREAT)) {
			struct vnode_attr vap;
			
			np->set_create_va_mode = FALSE;	/* Only try once */
			VATTR_INIT(&vap);
			VATTR_SET_ACTIVE(&vap, va_mode);
			vap.va_mode = np->create_va_mode;
			error = smbfs_setattr(vp, &vap, ap->a_context);
			if (error)	/* Got an error close the file and return the error */
				(void)smbfs_close(vp, ap->a_mode, ap->a_context);
		}
	}

	if (error == EBUSY)
		error = EAGAIN;
		
	smbnode_unlock(np);
	return(error);
}

/*
 * smbfs_vnop_mmap - smbfs vnodeop entry point
 *	vnode_t a_vp;
 *	int a_fflags;
 *	vfs_context_t a_context;
 *
 * The mmap routine is a hint that we need to keep the file open. We can get mutilple 
 * mmap before we get a mnomap. We only care about the first one. We need to take a
 * reference count on the open file and hold it open until we get a mnomap call. The
 * file should already be open when we get the mmap call and with the correct open mode
 * access.  So we shouldn't have to worry about upgrading because the open should have 
 * handled that for us. If the open was done using an Open Deny mode then we need to 
 * mark the open deny entry as being mmaped so the pagein, pageout, and mnomap routines
 * can find.
 *
 * NOTE: On return all errors are ignored except EPERM. 
 */
static int smbfs_vnop_mmap(struct vnop_mmap_args *ap)
{
	vnode_t			vp = ap->a_vp;
	struct smbnode *np = NULL;
	int				error = 0;
	u_int32_t		mode = (ap->a_fflags & PROT_WRITE) ? (FWRITE | FREAD) : FREAD;
	int				accessMode = (ap->a_fflags & PROT_WRITE) ? kAccessWrite : kAccessRead;
	int				pid = vfs_context_pid(ap->a_context);
	u_int16_t		fid;
	struct fileRefEntry *entry = NULL;
	
	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK)))
		return (EPERM);

	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_mmap;	

	/* We already have it mapped, just ignore this one */
	if (np->n_flag & NISMAPPED)
	    goto out;
	/*
	 * Since we should already be open with the correct modes then we should never
	 * need to really open the file. For now we try these three cases. 
	 *
	 * First try the simple case, we have a posix open with the correct access.
	 *
	 * Second see if we have a match in the open deny mode list. Still not 100% sure this
	 * will work ever time because they are passing the current context, which may not match
	 * the one passed to open. From taking to Joe he believe we will always be in the open 
	 * context when call. From my testing this seems to be true.
	 *
	 * Third just return EPERM.
	 */
	if (np->f_fid && (np->f_accessMode & accessMode))
		np->f_refcnt++;
	else if (smbfs_findFileRef(vp, pid, accessMode, kAnyMatch, 0, 0, &entry, &fid) == 0) {
		entry->refcnt++;
		entry->mmapped = TRUE;
		np->f_refcnt++;
	} else {
		SMBERROR("%s We could not find an open file with mode = 0x%x? \n", np->n_name, mode);
		error = EPERM;
		goto out;
	}
	np->n_flag |= NISMAPPED;
	np->f_mmapMode = mode;
out:	
	smbnode_unlock(np);
	return (error);
}

/*
 * smbfs_vnop_mnomap - smbfs vnodeop entry point
 *	vnode_t a_vp;
 *	vfs_context_t a_context;
 *
 * When called this is a hint that we can now close the file. We will not get any
 * more pagein or pageout calls without another mmap call.  If our reference count
 * is down to one then all we have to do is call close and it will clean everything
 * up. Otherwise we have a little more work to do see below for more details.
 *
 * NOTE: All errors are ignored by the calling routine
 */
static int smbfs_vnop_mnomap(struct vnop_mnomap_args *ap)
{
	vnode_t				vp = ap->a_vp;
	struct smbnode		*np;
	struct fileRefEntry *entry;
	int					error = 0;

	if (smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK))
		return (EPERM);	/* Not sure what to do here, they ignore errors */
	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_mnomap;

	/* Only one open reference just call close and let it clean every thing up. */
	if (np->f_refcnt == 1) {
		error = smbfs_close(vp, np->f_mmapMode, ap->a_context);
		if (error)
			SMBWARNING("%s close failed with error = %d\n", np->n_name, error);
		goto out;
	} else
		np->f_refcnt--;
	/*
	 * We get passed the current context which may or may not be the the same as the one used
	 * in the open. So search the list and see if there are any mapped entries. Remember we only
	 * have one item mapped at a time.
	 */
	if (smbfs_findMappedFileRef(vp, &entry, NULL) == TRUE) {
		entry->mmapped = FALSE;
		if (entry->refcnt > 0)	/* This entry is still in use don't remove it yet. */
			entry->refcnt--;
		else /* Done with it remove it from the list */
		    smbfs_removeFileRef(vp, entry);
	}
out:
	np->f_mmapMode = 0;
	np->n_flag &= ~NISMAPPED;
	smbnode_unlock(np);
	return (error);
}

/*
 * smbfs_vnop_inactive - smbfs vnodeop entry point
 *	vnode_t a_vp;
 *	vfs_context_t a_context;
 */
static int smbfs_vnop_inactive(struct vnop_inactive_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct smbnode *np;
	int error = 0;
	int releaseLock = TRUE;

	(void) smbnode_lock(VTOSMB(vp), SMBFS_RECLAIM_LOCK);
	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_inactive;

	/* Node went inactive clear the ACL cache */
	if (!vnode_isnamedstream(vp))
		smbfs_clear_acl_cache(np);

    /*
	 * Before we take the lock, someone could jump in and do an open and start using this vnode again.
	 * We now check for that and just skip out if it happens. We will get another inactive later, if 
	 * this volume is not being force unmount. So check here to see if the vnode is in use and the
	 * volume is not being forced unmounted. Note that Kqueue opens will not be found by vnode_isinuse.
	 */
    if ((vnode_isinuse(vp, 0)) && !(vfs_isforce(vnode_mount(vp))))
        goto out;
	
	if (vnode_isdir(vp)) {
		smbfs_closedirlookup(np, ap->a_context);
		np->d_refcnt = 0;
		if (np->d_kqrefcnt)
			smbfs_stop_change_notify(np, TRUE, ap->a_context, &releaseLock);
		goto out;
	}
		
	/* its not in use are they don't care about it close it */
	if (np->f_refcnt) {
		np->f_refcnt = 1;
		error = smbfs_close(vp, FREAD, ap->a_context);
		if (error) {
			SMBDEBUG("error %d closing fid %d file %s\n", error,  np->f_fid, np->n_name);
		}
	}
	
	/*
	 * Does the file need to be deleted on close. Make one more check here
	 * just in case. 
	 */ 
	if (np->n_flag & NDELETEONCLOSE) {
		error = smbfs_smb_delete(np, ap->a_context, NULL, 0, 0);
		if (error)
			SMBWARNING("error %d deleting silly rename file %s\n", error, np->n_name);
		else np->n_flag &= ~NDELETEONCLOSE;
	}
#ifdef SMB_DEBUG
	/* If the file is not in use then it should be closed. */ 
	DBG_ASSERT((np->f_refcnt == 0));
	DBG_ASSERT((np->f_openDenyList == NULL));
	DBG_ASSERT((np->f_smbflock == NULL));
#endif // SMB_DEBUG

out:
	if (releaseLock)
		smbnode_unlock(np);
	return (0);
}

/*
 * Free smbnode, and give vnode back to system
 *		struct vnodeop_desc *a_desc;
 *		vnode_t a_vp;
 *		vfs_context_t a_context;
 */
static int smbfs_vnop_reclaim(struct vnop_reclaim_args *ap)
{
	vnode_t vp = ap->a_vp;
	vnode_t dvp;
	struct smbnode *np = NULL;
	struct smbmount *smp = NULL;
	
	(void) smbnode_lock(VTOSMB(vp), SMBFS_RECLAIM_LOCK);
	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_reclaim;
	smp = VTOSMBFS(vp);
#ifdef SMB_DEBUG
	/* We should never have a file open at this point */
	if (vnode_isreg(vp)) {
		DBG_ASSERT((np->f_refcnt == 0));	
	} else if (vnode_isdir(vp)) {
		DBG_ASSERT((np->d_kqrefcnt == 0));	
		DBG_ASSERT((np->d_fctx == NULL));	
	}
#endif // SMB_DEBUG

	SET(np->n_flag, NTRANSIT);
	
	/* If this is a named stream node then NREFPARENT won't be set */
	dvp = (np->n_parent && (np->n_flag & NREFPARENT)) ?
	    np->n_parent->n_vnode : NULL;

	smb_vhashrem(np);

	cache_purge(vp);
	if (smp->sm_rvp == vp) {
		SMBVDEBUG("root vnode\n");
		smp->sm_rvp = NULL;
	}
	/* Destroy the lock used for the open state, open deny list and resource size/timer */
	if (!vnode_isdir(vp)) {
		lck_mtx_destroy(&np->f_openDenyListLock, smbfs_mutex_group);
		lck_mtx_destroy(&np->f_openStateLock, smbfs_mutex_group);
		if (!vnode_isnamedstream(vp))
			lck_mtx_destroy(&np->rfrkMetaLock, smbfs_mutex_group);
	}

	/* We are done with the node clear the acl cache and destroy the acl cache lock  */
	if (!vnode_isnamedstream(vp)) {
		smbfs_clear_acl_cache(np);
		lck_mtx_destroy(&np->f_ACLCacheLock, smbfs_mutex_group);
	}
	
	/* Free up both names before we unlock the node */
	if (np->n_name)
		smbfs_name_free(np->n_name);
	if (np->n_sname)
		smbfs_name_free(np->n_sname);
	np->n_name = NULL;
	np->n_sname = NULL;
	
	smbnode_unlock(np);

	vnode_clearfsnode(vp);

	CLR(np->n_flag, (NALLOC|NTRANSIT));
	if (ISSET(np->n_flag, NWALLOC) || ISSET(np->n_flag, NWTRANSIT)) {
		CLR(np->n_flag, (NWALLOC|NWTRANSIT));
		wakeup(np);
	}
	lck_rw_destroy(&np->n_rwlock, smbfs_rwlock_group);
	lck_rw_destroy(&np->n_name_rwlock, smbfs_rwlock_group);
	FREE(np, M_SMBNODE);
	if (dvp) {
		if (vnode_get(dvp) == 0) {
			vnode_rele(dvp);
			vnode_put(dvp);
		}
	}

	return 0;
}

/*
 * smbfs_getattr call from vfs.
 */
static int smbfs_getattr(vnode_t vp, struct vnode_attr *vap, vfs_context_t context)
{
	struct smbnode *np = VTOSMB(vp);
	struct smbmount *smp = VTOSMBFS(vp);
	struct smb_share *ssp = smp->sm_share;

	if (ssp->ss_attributes & FILE_PERSISTENT_ACLS &&
	    (VATTR_IS_ACTIVE(vap, va_acl) || VATTR_IS_ACTIVE(vap, va_guuid) ||
	     VATTR_IS_ACTIVE(vap, va_uuuid))) {
			DBG_ASSERT(!vnode_isnamedstream(vp));
			(void)smbfs_getsecurity(np, vap, context);
	}
	return smbfs_update_cache(vp, vap, context);
}

static int smbfs_vnop_getattr(struct vnop_getattr_args *ap)
/*	struct vnop_getattr_args {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		struct vnode_attr *a_vap;
		vfs_context_t a_context;
	} *ap; */
{
	int32_t error = 0;
	
	if ((error = smbnode_lock(VTOSMB(ap->a_vp), SMBFS_SHARED_LOCK))) {
		return (error);
	}
	VTOSMB(ap->a_vp)->n_lastvop = smbfs_vnop_getattr;
	/* Before updating see if it needs to be reopened. */
	if ((!vnode_isdir(ap->a_vp)) && (VTOSMB(ap->a_vp)->f_openState == kNeedReopen))
		(void)smbfs_smb_reopen_file(VTOSMB(ap->a_vp), ap->a_context);

	error = smbfs_getattr (ap->a_vp, ap->a_vap, ap->a_context);
	
	smbnode_unlock(VTOSMB(ap->a_vp));
	return (error);
}

int smbfs_setattr(vnode_t vp, struct vnode_attr *vap, vfs_context_t context)
{
	struct smbnode *np = VTOSMB(vp);
	struct smbmount *smp = VTOSMBFS(vp);
	struct timespec *crtime, *mtime, *atime;
	struct smb_share *ssp = smp->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	u_quad_t tsize = 0;
	int error = 0, cerror, modified = 0;
	off_t newround;
	u_int16_t fid = 0;
	u_int32_t rights;

	/* If this is a stream then they can only set the size */
	if ((vnode_isnamedstream(vp)) && (vap->va_active & ~VNODE_ATTR_BIT(va_data_size))) {
		SMBDEBUG("Using stream node %s to set something besides the size?\n", np->n_name);
		error = ENOTSUP;
		goto out;
	}
	
	/*
	 * If our caller is trying to set multiple attributes, they
	 * can make no assumption about what order they are done in.
	 * Here we try to do them in order of decreasing likelihood
	 * of failure, just to minimize the chance we'll wind up
	 * with a partially complete request.
	 */

	if (ssp->ss_attributes & FILE_PERSISTENT_ACLS &&
	    (VATTR_IS_ACTIVE(vap, va_acl) || VATTR_IS_ACTIVE(vap, va_guuid) ||
	     VATTR_IS_ACTIVE(vap, va_uuuid))) {
		error = smbfs_setsecurity(vp, vap, context);
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
	 * If the server supports the new UNIX extensions, then we can support changing
	 * the uid, gid, mode, and va_flags. Currently the uid and gid don't make any sense,
	 * but in the future we may add this support. Even if the server doesn't support the 
	 * extensions, we still claim to have made the changes, so that the vfs above us 
	 * won't use a fallback strategy of generating dot-underscore files to keep these 
	 * attributes. This is the way it was done before adding the UNIX extensions, may want
	 * to look at this again in post Leopard time frame.
	 *
	 * %%% - Should we try do time here, post Leopard yes.
	 *
	 * The old code would check the users creditials here. There is no need for that in
	 * our case. The lower level will make sure the correct local user is using the vc
	 * and the server should protect us for any other case.
	 */
	
	if (VATTR_IS_ACTIVE(vap, va_uid))
		VATTR_SET_SUPPORTED(vap, va_uid);
	if (VATTR_IS_ACTIVE(vap, va_gid))
		VATTR_SET_SUPPORTED(vap, va_gid);

	if ((VATTR_IS_ACTIVE(vap, va_mode)) ||(VATTR_IS_ACTIVE(vap, va_flags))) {
		int unix_extensions = ((UNIX_CAPS(vcp) & CIFS_UNIX_POSIX_PATH_OPERATIONS_CAP)) ? TRUE : FALSE;
		int dosattr = np->n_dosattr;
		u_int32_t vaflags = 0;
		u_int32_t vaflags_mask = SMB_FLAGS_NO_CHANGE;
		u_int64_t vamode = SMB_MODE_NO_CHANGE;
		
		if (VATTR_IS_ACTIVE(vap, va_flags)) {
			/*
			 * Here we are strict, stricter than ufs in not allowing users to attempt 
			 * to set SF_SETTABLE bits or anyone to set unsupported bits.  However, we 
			 * ignore attempts to set ATTR_ARCHIVE for directories `cp -pr' from a more
			 * sensible file system attempts it a lot.
			 */
			if (vap->va_flags & ~(SF_ARCHIVED | SF_IMMUTABLE | UF_IMMUTABLE | UF_HIDDEN))
			{
				error = EINVAL;
				goto out;
			}
			/* Only set items we can change and the server supports */
			vaflags_mask = np->n_flags_mask & (EXT_IMMUTABLE | EXT_HIDDEN | EXT_DO_NOT_BACKUP);

			/*
			 * Remember that SMB_FA_ARCHIVE means the items needs to be 
			 * archive and SF_ARCHIVED means the item has been archive.
			 */
			if (vap->va_flags & SF_ARCHIVED) {
				dosattr &= ~SMB_FA_ARCHIVE;				
				vaflags |= EXT_DO_NOT_BACKUP;
			} else
				dosattr |= SMB_FA_ARCHIVE;	
			
			/*
			 * SMB_FA_RDONLY ~ UF_IMMUTABLE
			 *
			 * We treat the SMB_FA_RDONLY as the immutable flag. This allows
			 * us to support the finder lock bit and makes us follow the 
			 * MSDOS code model. See msdosfs project.
			 *
			 * NOTE: The ready-only flags does not exactly follow the lock/immutable bit
			 * also note the for directories its advisory only.
			 *
			 * We do not support the setting the read-only bit for folders if the server
			 * does not support the new UNIX extensions.
			 * 
			 * See Radar 5582956 for more details.
			 */
			if (unix_extensions || (! vnode_isdir(vp))) {
				if (vap->va_flags & (SF_IMMUTABLE | UF_IMMUTABLE)) {
					dosattr |= SMB_FA_RDONLY;				
					vaflags |= EXT_IMMUTABLE;				
				} else
					dosattr &= ~SMB_FA_RDONLY;
			}
			/*
			 * NOTE: Windows does not set ATTR_ARCHIVE bit for directories. 
			 */
			if ((! unix_extensions) && (vnode_isdir(vp)))
				dosattr &= ~SMB_FA_ARCHIVE;
			
			/* Now deal with the new Hidden bit */
			if (vap->va_flags & UF_HIDDEN) {
				dosattr |= SMB_FA_HIDDEN;				
				vaflags |= EXT_HIDDEN;
			} else
				dosattr &= ~SMB_FA_HIDDEN;
		}

		/* 
		 * Currently we do not allow setting the uid, gid, or sticky bits. Also chmod on
		 * a symbolic link doesn't really make sense. BSD allows this with the lchmod and
		 * on create, but Samba does support this because its not POSIX. If we try to 
		 * chmod here it will get set on the target which would be bad. So ignore the fact
		 * that they made this request.
		 */
		if (unix_extensions && (!vnode_islnk(vp)) && (VATTR_IS_ACTIVE(vap, va_mode))) {
			vamode = vap->va_mode & ACCESSPERMS;
		} else if (dosattr == np->n_dosattr)
			vaflags_mask = 0; /* Nothing really changes, no need to make the call */ 
		
		if (vaflags_mask || (vamode != SMB_MODE_NO_CHANGE)) {
			if (unix_extensions)
				error = smbfs_set_unix_info2(np, NULL, NULL, NULL, SMB_SIZE_NO_CHANGE, vamode, vaflags, vaflags_mask, context);
			else
				error = smbfs_smb_setpattr(np, NULL, 0, dosattr, NULL, context);
			if (error)
				goto out;
		}
		/* Everything work update the local cache and mark that we did the work */
		if (VATTR_IS_ACTIVE(vap, va_mode)) {
	    	if (vamode != SMB_MODE_NO_CHANGE)
				np->n_mode = vamode;
			VATTR_SET_SUPPORTED(vap, va_mode);
		}
		if (VATTR_IS_ACTIVE(vap, va_flags)) {
			np->n_dosattr = dosattr;
			VATTR_SET_SUPPORTED(vap, va_flags);
		}
	}
	
	if (VATTR_IS_ACTIVE(vap, va_data_size) && (!vnode_islnk(vp))) {
 		tsize = np->n_size;
		newround = round_page_64((off_t)vap->va_data_size);
		if ((off_t)tsize > newround) {
			error = ubc_msync(vp, newround, (off_t)tsize, NULL, UBC_INVALIDATE);
			if (error) {
				SMBERROR("ubc_msync failed! %d\n", error);
				goto out;
			}
		}
		/*
		 * np->f_rights holds different values depending on 
		 * SMB_CAP_NT_SMBS. For NT systems we need the file open for 
		 * write and append data. 
		 *
		 * Windows 98 just needs the file open for write. So either 
		 * write access or all access will work. We corrected tmpopen
		 * to work correctly now. So just ask for the NT access.
		 */
		rights = SA_RIGHT_FILE_WRITE_DATA | SA_RIGHT_FILE_APPEND_DATA;
		
		error = smbfs_smb_tmpopen(np, rights, context, &fid);
			
		/* zero fill if needed, ignore any errors will catch them on the seteof call */
		if ((!error) && (tsize < vap->va_data_size))
			smbfs_0extend(vp, fid, tsize, vap->va_data_size, context, SMBWRTTIMO);
		/* Set the eof on the server */
		if (!error)
			error = smbfs_smb_setfsize(np, fid, vap->va_data_size, context);			
		
		cerror = smbfs_smb_tmpclose(np, fid, context);
		if (cerror)
			SMBWARNING("error %d closing fid %d file %s\n", cerror, fid, np->n_name);
		if (error) {
			smbfs_setsize(vp, (off_t)tsize);
			goto out;
		} else
			smbfs_setsize(vp, (off_t)vap->va_data_size);
		
		smp->sm_statfstime = 0;	/* blow away statfs cache */
		VATTR_SET_SUPPORTED(vap, va_data_size);
		/* Tell the stream's parent that something has changed */
		if (vnode_isnamedstream(vp)) {
			vnode_t parent_vp = smb_update_rsrc_and_getparent(vp, TRUE);
			if (parent_vp)	/* We cannot always update the parents meta cache timer, so don't even try here */
				vnode_put(parent_vp);
		}
		
		modified = 1;
  	}

	/*
	 * Note that it's up to the caller to provide (or not) a fallback for
	 * backup_time, as we don't support them.
	 *
	 */
	crtime = VATTR_IS_ACTIVE(vap, va_create_time) ? &vap->va_create_time : NULL;
	mtime = VATTR_IS_ACTIVE(vap, va_modify_time) ? &vap->va_modify_time : NULL;
	atime = VATTR_IS_ACTIVE(vap, va_access_time) ? &vap->va_access_time : NULL;

	/* 
	 * If they are just setting the time to the same value then just say we made the
	 * call. This will not hurt anything and will protect us from badly written applications.
	 * Here is what was happening in the case of the finder copy.
	 * The file gets copied, and the modify time and create time have been set to
	 * the current time by the server. The application is using utimes to set the
	 * modify time to the original file's modify time. This time is before the create time.
	 * So we set both the create and modify time to the same value. See the HFS note
	 * below. Now the applications wants to set the create time to be the same as the
	 * orignal file. In this case the original file has the same modify and create time. So
	 * we end up setting the create time twice to the same value. Even with this code the
	 * copy engine needs to be fixed, looking into that now. Looks like this will get fix
	 * with Radar 4385758. We should retest once that radar is completed.
	 */
	if (crtime && (crtime->tv_sec == np->n_crtime.tv_sec)) {
		VATTR_SET_SUPPORTED(vap, va_create_time);
		crtime = NULL;
	}
	if (mtime && (mtime->tv_sec == np->n_mtime.tv_sec)) {
		VATTR_SET_SUPPORTED(vap, va_modify_time);		
		mtime = NULL;
	}
	if (atime && (atime->tv_sec == np->n_atime.tv_sec)) {
		VATTR_SET_SUPPORTED(vap, va_access_time);
		atime = NULL;
	}
	 
	/*
	 * We sometimes get sent a zero access time. Did some testing and found
	 * out the following:
	 *
	 *	MSDOS	- The date gets set to Dec 30 17:31:44 1969
	 *	SMB FAT	- The date gets set to Jan  1 00:00:00 1980
	 *	UFS	- The date gets set to Dec 31 16:00:00 1969
	 *	SMB NTFS- The date gets set to Dec 31 16:00:00 1969
	 *	HFS	- The date displayed from ls is Dec 31 16:00:00 1969
	 *	HFS	- The getattrlist date is <no value>
	 *
	 * I believe this is from a utimes call where they are setting the
	 * modify time, but leaving the access time set to zero. We seem to be 
	 * doing the same thing as everyone else so let them do it.
	 */
	/*
	 * The following comment came from the HFS code.
	 * The utimes system call can reset the modification time but it doesn't
	 * know about create times. So we need to ensure that the creation time 
	 * is always at least as old as the modification time.
	 *
	 * The HFS code also checks to make sure it was not the root vnode. Don 
	 * Brady said that the SMB code should not use that part of the check.
	 */
	if (!crtime && mtime && mtime->tv_sec < np->n_crtime.tv_sec)
		crtime = mtime;

	if (crtime || mtime || atime) {
		/*
		 * Windows 95/98/Me do not allow you to open a directory. 
		 * The previous code just punted on Windows 95/98/Me, 
		 * so we are going to do the same. 
		 */
		if ((vcp->vc_flags & SMBV_WIN98) && vnode_isdir(vp)) {
			error = 0;
			goto timedone;
		}
		
		rights = SA_RIGHT_FILE_WRITE_ATTRIBUTES;
		/*
		 * For Windows 95/98/Me/NT4 and all old dialects we must have 
		 * the item open before we can set the date and time. For all 
		 * other systems; if the item is already open then make sure it 
		 * has the correct open mode.
		 *
		 * We currently never do a NT style open with write attributes.
		 * So for all systems except NT4 that spport the NTCreateAndX 
		 * call we will fall through and just use the set path method.
		 * In the future we may decide to add open deny support. So if
		 * we decide to add write atributes access then this code will
		 * work without any other changes.  
		 */ 
		if ((vcp->vc_flags & (SMBV_WIN98 | SMBV_NT4)) || 
			(smp->sm_flags & kRequiresFileInfoTime) ||
			((!vnode_isdir(vp)) && np->f_refcnt && (np->f_rights & rights))) {
			
			/*
			 * np->f_rights holds different values depending on 
			 * SMB_CAP_NT_SMBS. For NT systems we need the file open
			 * for write attributes. 
			 *
			 * Windows 98 just needs the file open for write. So 
			 * either write access or all access will work. If they 
			 * already have it open for all access just use that, 
			 * otherwise use write. We corrected tmpopen
			 * to work correctly now. So just ask for the NT access.
			 */
		
			error = smbfs_smb_tmpopen(np, rights, context, &fid);
			if (error)
				goto out;
			
			error = smbfs_smb_setfattrNT(np, np->n_dosattr, fid, crtime, mtime, atime, NULL, context);
			cerror = smbfs_smb_tmpclose(np, fid, context);
			if (cerror)
				SMBERROR("error %d closing fid %d file %s\n", cerror, fid, np->n_name);

		}
		else {
			error = smbfs_smb_setpattrNT(np, np->n_dosattr, crtime, mtime, atime, NULL, context);
			/* They don't support this call, we need to fallback to the old method; stupid NetApp */
			if (error == ENOTSUP) {
				SMBWARNING("Server does not support setting time by path, fallback to old method\n"); 
				smp->sm_flags |= kRequiresFileInfoTime;	/* Never go down this path again */
				error = smbfs_smb_tmpopen(np, rights, context, &fid);
				if (!error) {
					error = smbfs_smb_setfattrNT(np, np->n_dosattr, fid, crtime, mtime, atime, NULL, context);
					(void)smbfs_smb_tmpclose(np, fid, context);				
				}
			}
		}
timedone:
		if (error)
			goto out;
		if (crtime) {
			VATTR_SET_SUPPORTED(vap, va_create_time);
			np->n_crtime.tv_sec = crtime->tv_sec;
			np->n_crtime.tv_nsec = crtime->tv_nsec;
		}
		if (mtime) {
			VATTR_SET_SUPPORTED(vap, va_modify_time);
			np->n_mtime.tv_sec = mtime->tv_sec;
			np->n_mtime.tv_nsec = mtime->tv_nsec;
		}
		if (atime) {
			VATTR_SET_SUPPORTED(vap, va_access_time);
			np->n_atime.tv_sec = atime->tv_sec;
			np->n_atime.tv_nsec = atime->tv_nsec;
		}
		/* Update the change time */
		if (crtime || mtime || atime)
			nanotime(&np->n_chtime); /* Need current date/time, so use nanotime */
	}
out:
	if (modified) {
		/*
		 * Invalidate attribute cache in case if server doesn't set
		 * required attributes.
		 */
		np->attribute_cache_timer = 0;	/* invalidate cache */
	}
	return (error);
}

static int smbfs_vnop_setattr(struct vnop_setattr_args *ap)
/* struct vnop_setattr_args {
		struct vnode *a_vp;
		struct vnode_attr *a_vap;
		vfs_context_t a_context;
	} *ap; */
{
	int32_t error = 0;

	if ((error = smbnode_lock(VTOSMB(ap->a_vp), SMBFS_EXCLUSIVE_LOCK))) {
		return (error);
	}
	VTOSMB(ap->a_vp)->n_lastvop = smbfs_vnop_setattr;
	
	error = smbfs_setattr (ap->a_vp, ap->a_vap, ap->a_context);
	
	smbnode_unlock(VTOSMB(ap->a_vp));
	/* 
	 * Special case here, if this is a stream try to update the parents meta cache timer. The 
	 * smb_clear_parent_cache_timer routine checks to make sure we got the parent. The child is
	 * now unlocked so we can update the parent now.
	 */
	if (vnode_isnamedstream(ap->a_vp))
		smb_clear_parent_cache_timer(vnode_getparent(ap->a_vp));

	return (error);
}

static void smbfs_ubc_upl_abort(upl_t upl, int32_t npages)
{
	int abort_type = UPL_ABORT_DUMP_PAGES;
	upl_page_info_t *pl = ubc_upl_pageinfo(upl);
	int ii, error;
	
	for (ii=0; ii < npages; ii++)
		if (upl_valid_page(pl, ii) && upl_dirty_page(pl, ii)) {
			abort_type = UPL_ABORT_FREE_ON_EMPTY;
			break;
		}

	error = ubc_upl_abort(upl, abort_type);
	if (error)
		SMBWARNING("ubc_upl_abort error = %d", error);
}

/*
 * smbfs_vnop_read
 *
 *	vnode_t a_vp;
 *	uio_t a_uio;
 *	int a_ioflag;
 *	vfs_context_t a_context;
 */
static int smbfs_vnop_read(struct vnop_read_args *ap)
{
	vnode_t 	vp = ap->a_vp;
	uio_t uio = ap->a_uio;
	off_t soff, eoff;
	upl_t upl;
	int error;
	user_ssize_t remaining, xfersize;
	struct smbnode *np = NULL;
	u_int16_t fid = 0;
	struct smb_share *ssp;
	pid_t	pid = proc_pid(vfs_context_proc(ap->a_context));

	if (!vnode_isreg(vp) && !vnode_islnk(vp))
		return (EPERM);		/* can only read regular files or symlinks */
		
	if (uio_resid(uio) == 0)
		return (0);		/* Nothing left to do */
		
	if (uio_offset(uio) < 0)
		return (EINVAL);	/* cant read from a negative offset */

	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_SHARED_LOCK)))
		return (error);

	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_read;
	ssp = np->n_mount->sm_share;

	/*
	 * History: FreeBSD vs Darwin VFS difference; we can get VNOP_READ without
 	 * preceeding open via the exec path, so do it implicitly.  VNOP_INACTIVE  
	 * closes the extra network file handle, and decrements the open count.
	 *
	 * If we are in reconnect mode calling smbfs_open will handle any reconnect
	 * issues. So only if we have a f_refcnt do we call smbfs_smb_reopen_file.
 	 */
 	if (!np->f_refcnt) {
 		error = smbfs_open(vp, FREAD, ap->a_context);
 		if (error)
 			goto exit;
		else np->f_needClose = 1;
 	}
	else {
		error = smbfs_smb_reopen_file(np, ap->a_context);
		if (error) {
			SMBDEBUG(" %s waiting to be revoked\n", np->n_name);
			goto exit;
		}
	}

	if (smbfs_findFileRef(vp, pid, kAccessRead, kCheckDenyOrLocks, uio_offset(uio), uio_resid(uio), NULL, &fid)) {
		/* No matches or no pid to match, so just use the generic shared fork */
		fid = np->f_fid;	/* We should always have something at this point */
	}
	DBG_ASSERT(fid);	 
	/*
	 * In order to maintain some synchronisation between memory-mapped access and reads from 
	 * a file, we build a upl covering the range we're about to read, and once the read
	 * completes, dump all the pages.
	 *
	 * Loop reading chunks small enough to be covered by a upl.
	 */
	while (!error && uio_resid(uio) > 0) {
		int ubc_error;

		remaining = uio_resid(uio);
		if (uio_offset(uio) & (PAGE_SIZE-1))
			xfersize = MIN(remaining, (user_ssize_t)(SMB_IOMAX - PAGE_SIZE));
		else
			xfersize = MIN(remaining, (user_ssize_t)SMB_IOMAX);
		
		soff = trunc_page_64(uio_offset(uio));
		eoff = round_page_64(uio_offset(uio) + xfersize);
		/* 
		 * First we need to push out any mmap-dirtied pages. Ignore any errors, since 
		 * there is nothing we could do about them anyways. The vnode lock is held, 
		 * so we shouldn't need to lock down all pages across the whole vop.
		 */
		ubc_msync(vp, soff, eoff, NULL, UBC_PUSHDIRTY | UBC_INVALIDATE | UBC_SYNC);
		
		/* create a upl for this range, if we get an error don't fail the read */
		ubc_error = ubc_create_upl(vp, soff, (int32_t)(eoff - soff), &upl, NULL, 0);
		
		uio_setresid(uio, xfersize);
		if (ssp->ss_flags & SMBS_RECONNECTING)
			smbfs_smb_reopen_file(np, ap->a_context);
		/* do the wire transaction */
		error = smbfs_doread(vp, uio, ap->a_context, fid);
		/* We got an error, did it happen on a reconnect */
		if (error && ((error = smbfs_io_reopen(vp, uio, kAccessRead, &fid, error, ap->a_context)) == 0))
			error = smbfs_doread(vp, uio, ap->a_context, fid);
		/*
		 * We succeed with the ubc_create_upl, so check to see if any pages 
		 * became dirty while we were doing the read. If none are dirty then
		 * have them dump the pages.
		 */
		if (!ubc_error)
			smbfs_ubc_upl_abort(upl, (int32_t)((eoff - soff) / PAGE_SIZE));

		uio_setresid(uio, (uio_resid(uio) + (remaining - xfersize)));
		if (uio_resid(uio) == remaining) /* nothing transferred? */
			break;
	}

exit:
	smbnode_unlock(np);
	return (error);
}

/*
 * smbfs_vnop_write
 *
 *	vnode_t a_vp;
 *	uio_t a_uio;
 *	int a_ioflag;
 *	vfs_context_t a_context;
 */
static int smbfs_vnop_write(struct vnop_write_args *ap)
{
	vnode_t 	vp = ap->a_vp;
	struct smbnode *np = NULL;
	uio_t		uio = ap->a_uio;
	off_t		soff, eoff;
	upl_t		upl;
	int			error;
	user_ssize_t	remaining, xfersize;
	int		timo = SMBWRTTIMO;
	u_int16_t	fid = 0;
	int		pid = vfs_context_pid(ap->a_context);
	struct smb_share *ssp;
	vnode_t parent_vp = NULL;	/* Always null unless this is a stream node */
	u_quad_t starting_eof;	
	
	if ( !vnode_isreg(vp))
		return (EPERM);
		
	if (uio_offset(uio) < 0)
		return (EINVAL);

	if (uio_resid(uio) == 0)
		return (0);

	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);

	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_write;
	ssp = np->n_mount->sm_share;
	
	/* Before trying the write see if the file needs to be reopened */
	error = smbfs_smb_reopen_file(np, ap->a_context);
	if (error) {
		SMBDEBUG(" %s waiting to be revoked\n", np->n_name);
		goto exit;
	}
	
	if (smbfs_findFileRef(vp, pid, kAccessWrite, kCheckDenyOrLocks, uio_offset(uio), uio_resid(uio), NULL, &fid)) {
		/* No matches or no pid to match, so just use the generic shared fork */
		fid = np->f_fid;	/* We should always have something at this point */
	}
	DBG_ASSERT(fid);
	starting_eof = np->n_size;	/* Save off the orignial end of file */
	/*
	 * Note that since our lower layers take the uio directly,
	 * we don't copy it into these pages; we're going to 
	 * invalidate them all when we're done anyway.
	 *
	 * Loop writing chunks small enough to be covered by a upl.
	 */
	while (!error && uio_resid(uio) > 0) {
		int ubc_error;	
		
		remaining = uio_resid(uio);
		if (uio_offset(uio) & (PAGE_SIZE-1))
			xfersize = MIN(remaining, (user_ssize_t)(SMB_IOMAX - PAGE_SIZE));
		else
			xfersize = MIN(remaining, (user_ssize_t)SMB_IOMAX);

		soff = trunc_page_64(uio_offset(uio));
		eoff = round_page_64(uio_offset(uio) + xfersize);
		
		/* 
		 * First we need to push out any mmap-dirtied pages. Ignore any errors, since 
		 * there is nothing we could do about them anyways. The vnode lock is held, 
		 * so we shouldn't need to lock down all pages across the whole vop.
		 */
		ubc_msync(vp, soff, eoff, NULL, UBC_PUSHDIRTY | UBC_INVALIDATE | UBC_SYNC);
		
		/* create a upl for this range, if we get an error don't fail the read */
		ubc_error = ubc_create_upl(vp, soff, (int32_t)(eoff - soff), &upl, NULL, 0);
		
		uio_setresid(uio, xfersize);
		if (ssp->ss_flags & SMBS_RECONNECTING)
			smbfs_smb_reopen_file(np, ap->a_context);
		/* do the wire transaction */
		error = smbfs_dowrite(vp, uio, fid, ap->a_context, ap->a_ioflag, timo);
		/*
		 * %%%  Radar 4573627
		 * We should resend the write if we failed because of a reconnect.
		 */
		timo = 0;
		/*
		 * We succeed with the ubc_create_upl, so check to see if any pages 
		 * became dirty while we were doing the read. If none are dirty then
		 * have them dump the pages.
		 */
		if (!ubc_error)
			smbfs_ubc_upl_abort(upl, (int32_t)((eoff - soff) / PAGE_SIZE));

		uio_setresid(uio, (uio_resid(uio) + (remaining - xfersize)));
		if (uio_resid(uio) == remaining) /* nothing transferred? */
			break;
	}
	
	if (!error) {
		struct smb_vc *vcp = SSTOVC(ssp);

		VTOSMBFS(vp)->sm_statfstime = 0;	/* if success, blow away statfs cache */
		/* 
		 * Windows servers do not handle writing pass the end of file the 
		 * same as Unix servers. If we do a directory lookup before the file 
		 * is close then the server may return the old size. Setting the end of file
		 * here will prevent that from happening. Unix servers do not seem to have this 
		 * problem so there is no reason to make this call in that case. So if the file
		 * size has changed and this is not a Unix server then set the eof of file to the
		 * new value.
		 *
		 * %%% - If this is a exclusive open then we could cache this information. Maybe 
		 * when we add cluster support we could also add support for caching the end of file. 
		 *
		 * We could have a readdir happening while we are waiting on the set eof call to complete. The 
		 * readdir will block waiting on us to unlock this vnode. We need to make sure that the readdir
		 * does update the file size because it doesn't have the correct information. Resetting our
		 * n_sizetime timer will protect us in this case.
		 */
		if ((!UNIX_SERVER(vcp)) && (starting_eof < np->n_size)) {
			int seteof_error = smbfs_smb_setfsize(np, fid, np->n_size, ap->a_context);	
			if (seteof_error)
				SMBERROR("%s wrote pass the eof setting it to %lld failed with error of %d\n", 
						 np->n_name, np->n_size, seteof_error);

			nanouptime(&np->n_sizetime);
		}
	}
	
exit:
	/* Tell the stream's parent that something has changed */
	if (vnode_isnamedstream(vp))
		parent_vp = smb_update_rsrc_and_getparent(vp, FALSE);

	smbnode_unlock(VTOSMB(vp));
	/* We have the parent vnode, so reset its meta data cache timer. This routine will release the parent vnode */
	if (parent_vp)
		smb_clear_parent_cache_timer(parent_vp);
	
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
		     vfs_context_t context)
{
	u_int32_t j;
	int error = 0;
	struct kauth_ace *acep;

	*fixed = 0;	/* we'll set this if we modify (fix) va_acl */
	VATTR_INIT(vap);
	VATTR_WANTED(vap, va_acl);
	VATTR_WANTED(vap, va_uuuid);
	error = smbfs_getattr(vp, vap, context);
	if (error) {
		SMBERROR("smbfs_getattr %d, ignored\n", error);
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
smbfs_composeacl(struct vnode_attr *vap, vnode_t vp, vfs_context_t context)
{
	struct smbnode *np = VTOSMB(vp);	
	struct smb_vc *vcp = SSTOVC(np->n_mount->sm_share);
	struct vnode_attr svrva;
	kauth_acl_t newacl;
	kauth_acl_t savedacl = NULL;
	int error, fixed;
	u_int32_t j;
	int32_t entries, dupstart, allocated;
	struct kauth_ace *acep;

	/* 
	 * Here is the deal. This routine is called by smbfs_mkdir and smbfs_create to set the vnode_attr
	 * passed into those routines. The problem comes in with the vfs layer. It wants to set things
	 * that our server will set for us. So to stop taking a performance hit turn off those items that
	 * vfs turn on. See vnode_authattr_new for what is getting set.
	 */
	if (VATTR_IS_ACTIVE(vap, va_flags))
		VATTR_SET_SUPPORTED(vap, va_flags);
	if (VATTR_IS_ACTIVE(vap, va_create_time))
		VATTR_SET_SUPPORTED(vap, va_create_time);
	if (VATTR_IS_ACTIVE(vap, va_modify_time))
		VATTR_SET_SUPPORTED(vap, va_modify_time);
	if (VATTR_IS_ACTIVE(vap, va_access_time))
		VATTR_SET_SUPPORTED(vap, va_access_time);
	/* This will be zero if vnode_authattr_new set it. Do not change this on create. */
	if (vap->va_flags == 0)
		VATTR_CLEAR_ACTIVE(vap, va_flags);
	VATTR_CLEAR_ACTIVE(vap, va_create_time);
	VATTR_CLEAR_ACTIVE(vap, va_modify_time);
	VATTR_CLEAR_ACTIVE(vap, va_access_time);
	VATTR_CLEAR_ACTIVE(vap, va_change_time);
	/*
	 * %%% - In the future couldn't we delay all of this until after the open? See Radar 5199099 for more details.
	 *
	 * This routine gets called from create. If this is a regular file they could be setting the posix
	 * file modes to something that doesn't allow them to open the file for the permissions they requested. 
	 *		Example: open(path, O_WRONLY | O_CREAT | O_EXCL,  0400)
	 * We only care about this if the server supports setting/getting posix permissions. So if they are not giving the
	 * owner read/write access then save the settings until after the open. We will just pretend to set them here.
	 */
	if (vnode_isreg(vp)  && (VATTR_IS_ACTIVE(vap, va_mode)) && 
		((vap->va_mode & (S_IRUSR | S_IWUSR)) !=  (S_IRUSR | S_IWUSR)) &&
		(UNIX_CAPS(vcp) & CIFS_UNIX_POSIX_PATH_OPERATIONS_CAP)) {
		
		np->create_va_mode = vap->va_mode;
		np->set_create_va_mode = TRUE;
		/* The server should always give us read/write on create */
		VATTR_SET_SUPPORTED(vap, va_mode);
		VATTR_CLEAR_ACTIVE(vap, va_mode);
	 }
	 

	/*
	 * To ensure no EA-fallback-ACL gets made by vnode_setattr_fallback...
	 */
	if (VATTR_IS_ACTIVE(vap, va_acl))
		VATTR_SET_SUPPORTED(vap, va_acl);
	/*
	 * Get the newly created ACL, and fix potentially unset
	 * "inherited" bits.
	 */
	error = smbfs_fixinheritance(vp, &svrva, &fixed, context);
	if (error)
		goto out;

	/*
	 * No filesec being set on this create/mkdir?
	 */
	if (!VATTR_IS_ACTIVE(vap, va_acl)) {
		if (fixed) {
			VATTR_SET(vap, va_acl, NULL);
		} else {
			error = smbfs_setattr(vp, vap, context);
			if (error)
				SMBERROR("smbfs_setattr %d, ignored\n", error);
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
			error = smbfs_setattr(vp, vap, context);
			vap->va_acl = savedacl;
		} else {
			VATTR_CLEAR_ACTIVE(vap, va_acl);
			error = smbfs_setattr(vp, vap, context);
			VATTR_SET_ACTIVE(vap, va_acl);
		}
		if (error)
			SMBERROR("smbfs_setattr+1 %d, ignored\n", error);
		goto out;
	}
	/*
	 * if none were created we just slam in the requested ACEs
	 */
	if (!VATTR_IS_SUPPORTED(&svrva, va_acl) || svrva.va_acl == NULL ||
	    svrva.va_acl->acl_entrycount == 0) {
		error = smbfs_setattr(vp, vap, context);
		if (error)
			SMBERROR("smbfs_setattr+2, error %d ignored\n", error);
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
	error = smbfs_setattr(vp, vap, context);
	if (error)
		SMBERROR("smbfs_setattr+3, error %d ignored\n", error);
	kauth_acl_free(vap->va_acl);
	vap->va_acl = savedacl;
out:
	if (VATTR_IS_SUPPORTED(&svrva, va_acl) && svrva.va_acl != NULL)
		kauth_acl_free(svrva.va_acl);
	return;
}

/*
 * Create a regular file or a "symlink". In the symlink case we will have a target. Depending
 * on the sytle of symlink the target may be just what we set or we may need to encode it into
 * that wacky windows data 
 */
static int smbfs_create(struct vnop_create_args *ap, char *target, size_t targetlen)
{
	vnode_t				dvp = ap->a_dvp;
	struct vnode_attr *vap = ap->a_vap;
	vnode_t			*vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct smbnode *dnp = VTOSMB(dvp);
	struct smbmount *smp = VTOSMBFS(dvp);
	struct smb_vc *vcp = SSTOVC(dnp->n_mount->sm_share);
	vnode_t 	vp;
	struct smbfattr fattr;
	const char *name = cnp->cn_nameptr;
	size_t nmlen = cnp->cn_namelen;
	int error, cerror;
	uio_t uio;
	u_int16_t fid = 0;
	struct timespec ts;
	int8_t UnixSymlnk = (UNIX_CAPS(vcp) & CIFS_UNIX_POSIX_PATH_OPERATIONS_CAP);

	*vpp = NULL;
	if (vap->va_type != VREG && vap->va_type != VLNK)
		return (ENOTSUP);
	
	/* UNIX style smylinks are created with a set path info call */
	if ((vap->va_type == VLNK) && UnixSymlnk)
		error = smbfs_smb_create_symlink(dnp, name, nmlen, target, targetlen, ap->a_context);
	else
		error = smbfs_smb_create(dnp, name, nmlen, SA_RIGHT_FILE_WRITE_DATA, ap->a_context, 
								 &fid, NTCREATEX_DISP_CREATE, 0, &fattr);
	if (error)
		return (error);
	/* Must be doing one of those wacky fake Windows symlinks. */ 
	if ((vap->va_type == VLNK) && !UnixSymlnk) {
		u_int32_t wlen = 0;
		char *wdata = smbfs_create_windows_symlink_data(target, targetlen, &wlen);
		
		uio = uio_create(1, 0, UIO_SYSSPACE, UIO_WRITE);
		uio_addiov(uio, CAST_USER_ADDR_T(wdata), wlen);
		error = smb_write(smp->sm_share, fid, uio, ap->a_context, SMBWRTTIMO);
		uio_free(uio);
		if (!error)	/* We just changed the size of the file */
			fattr.fa_size = wlen;
		FREE(wdata, M_TEMP);
	}
	/* If we have a fid then close the file we are done */
	if (fid) {
		cerror = smbfs_smb_close(smp->sm_share, fid, ap->a_context);
		if (cerror)
			SMBWARNING("error %d closing \"%s/%s\"\n", cerror, dnp->n_name, name);		
	}
	if (error)
		return (error);
 	/* Old style create call (Windows 98). No attributes returned on the create */
	if ((vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS) != SMB_CAP_NT_SMBS) {
		error = smbfs_smb_lookup(dnp, &name, &nmlen, &fattr, ap->a_context);
		if (error)
			return (error);
	}
	smbfs_attr_touchdir(smp->sm_share, dnp);
	/* 
	 * %%%
	 * We have smbfs_nget returning a lock so we need to unlock it when we 
	 * are done with it. Would really like to clean this code up in the future. 
	 * The whole create, mkdir and smblink create code should use the same code path.
	 */
	fattr.fa_vtype = vap->va_type;
	error = smbfs_nget(VTOVFS(dvp), dvp, name, nmlen, &fattr, &vp, cnp->cn_flags, ap->a_context);
	if (error)
		goto bad;

	/* 
	 * We just create the file, so we have no finder info and the resource fork
	 * should be empty. So set our cache timers to reflect this information
	 */
	nanouptime(&ts);
	VTOSMB(vp)->finfo_cache = ts.tv_sec;
	VTOSMB(vp)->rfrk_cache_timer = ts.tv_sec;
	
	smbfs_composeacl(vap, vp, ap->a_context);

	if (vap->va_type == VLNK) {
		VTOSMB(vp)->n_size = targetlen;	/* Set it to the correct size */
		if (!UnixSymlnk)	/* Mark it as a Windows symlink */
			VTOSMB(vp)->n_flag |= NWINDOWSYMLNK;
	}
	*vpp = vp;
	error = 0;
	smbnode_unlock(VTOSMB(vp));	/* Done with the smbnode unlock it. */
	
	/* Remove any negative cache entries. */
	if (dnp->n_flag & NNEGNCENTRIES) {
		dnp->n_flag &= ~NNEGNCENTRIES;
		cache_purge_negatives(dvp);
	}
	
bad:
	if (name != cnp->cn_nameptr)
		smbfs_name_free((u_char *)name);
	/* if success, blow away statfs cache */
	if (!error)
		smp->sm_statfstime = 0;
	return (error);
}

/*
 * smbfs_vnop_create
 *
 * vnode_t a_dvp;
 * vnode_t *a_vpp;
 * struct componentname *a_cnp;
 * struct vnode_attr *a_vap;
 * vfs_context_t a_context;
 */
static int smbfs_vnop_create(struct vnop_create_args *ap)
{
	vnode_t 	dvp = ap->a_dvp;
	int			error;
	struct smbnode *dnp;

		/* Make sure we lock the parent before calling smbfs_create */
	if ((error = smbnode_lock(VTOSMB(dvp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);
	
	dnp = VTOSMB(dvp);
	dnp->n_lastvop = smbfs_vnop_create;
	
	error = smbfs_create(ap, NULL, 0);
	smbnode_unlock(dnp);
	
	return (error);
}

static int smbfs_remove(vnode_t dvp, vnode_t vp, struct componentname *cnp, int flags, vfs_context_t context)
{
#pragma unused(cnp)
	struct smbnode *dnp = VTOSMB(dvp);
	proc_t p = vfs_context_proc(context);
	struct smbnode *np = VTOSMB(vp);
	struct smbmount *smp = VTOSMBFS(vp);
	struct smb_vc *vcp = SSTOVC(smp->sm_share);
	int error;

	DBG_ASSERT((!vnode_isnamedstream(vp)))
	cache_purge(vp);
	
	/*
	 * Carbon semantics prohibit deleting busy files.
	 * (enforced when NODELETEBUSY is requested) We just return 
	 * EBUSY here. Do not print an error to system log in this
	 * case.
	 *
	 * NOTE: Kqueue opens will not be found by vnode_isinuse
	 */
	if ((vnode_isinuse(vp, 0)) && (flags & VNODE_REMOVE_NODELETEBUSY))
		return (EBUSY); /* Do not print an error in this case */
	/*
	 * Did we open this in our read routine. Then we should close it.
	 */
	if ((np->f_refcnt == 1) && np->f_needClose) {
		error = smbfs_close(vp, FREAD, context);
		if (error)
			SMBWARNING("error %d closing %s\n", error, np->n_name);
	}
	/*
	 * The old code would check vnode_isinuse to see if the file was open,
	 * but if the file was open by Kqueue then vnode_isinuse will not find it.
	 * So at this point if the file is open then do the silly rename delete
	 * trick if the server supports it.
	 */
	if (np->f_refcnt) {
		/* 
		 * If the remote server supports eiher renaming or deleting an
		 * open file do it here. Currenly we know that XP, Windows 2000,
		 * Windows 2003 and SAMBA support these calls. We know that
		 * Windows 95/98/Me/NT do not support these calls.
		 */
		if ( (vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS) && (vcp->vc_sopt.sv_caps & SMB_CAP_INFOLEVEL_PASSTHRU) )
			error =  smbfs_delete_openfile(dnp, np, context);
		else 
			error = EBUSY;
		if (! error)
			return(0);
		goto out;
	}
	error = smbfs_smb_delete(np, context, NULL, 0, 0);
	if (error)	/* Nothing else we can do at this point */
		goto out;
	
	smb_vhashrem(np);
	smbfs_attr_touchdir(smp->sm_share, dnp);
	
	/* Remove any negative cache entries. */
	if (dnp->n_flag & NNEGNCENTRIES) {
		dnp->n_flag &= ~NNEGNCENTRIES;
		cache_purge_negatives(dvp);
	}
	
out:
	if (error == EBUSY) {
		char errbuf[32];

		(void)proc_name(proc_pid(p), &errbuf[0], 32);
		SMBWARNING("warning: pid %d(%.*s) unlink open file(%s)\n", proc_pid(p), 32, &errbuf[0], np->n_name);
	}
	if (!error) {
		/* Not sure why we do this here. Leave it for not. */
		(void) vnode_recycle(vp);
		/* if success, blow away statfs cache */
		smp->sm_statfstime = 0;
	}
	return (error);
}

static int smbfs_vnop_remove(struct vnop_remove_args *ap)
/*	struct vnop_remove_args {
		struct vnodeop_desc *a_desc;
		vnode_t  a_dvp;
		vnode_t  a_vp;
		struct componentname * a_cnp;
		int a_flags;
		vfs_context_t a_context;
	} *ap; */
{
	vnode_t dvp = ap->a_dvp;
	vnode_t vp = ap->a_vp;
	int32_t error;

	if (dvp == vp) 
		return (EINVAL);

	/* Always put in the order of parent then child */
	if ((error = smbnode_lockpair(VTOSMB(dvp), VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);

	VTOSMB(dvp)->n_lastvop = smbfs_vnop_remove;
	VTOSMB(vp)->n_lastvop = smbfs_vnop_remove;

	error = smbfs_remove(dvp, vp, ap->a_cnp, ap->a_flags, ap->a_context);

	smbnode_unlockpair(VTOSMB(dvp), VTOSMB(vp));
	return (error);

}

/*
 * smbfs remove directory call
 */
static int smbfs_rmdir(vnode_t dvp, vnode_t vp, struct componentname *cnp, vfs_context_t context)
{
#pragma unused(cnp)
	struct smbmount *smp = VTOSMBFS(vp);
	struct smbnode *dnp = VTOSMB(dvp);
	struct smbnode *np = VTOSMB(vp);
	int error;
	u_int16_t		fid;

	/* XXX other OSX fs test fs nodes here, not vnodes. Why? */
	if (dvp == vp) {
		error = EINVAL;
		goto bad;
	}

	cache_purge(vp);
	error = smbfs_smb_rmdir(np, context);
	if (error)
	    goto bad;
	smbfs_attr_touchdir(smp->sm_share, dnp);
	smb_vhashrem(np);
	/* 
	 * We may still have a change notify on this node, close it so
	 * the item will get delete on the server. Mark it not to be 
	 * reopened first, then save off the fid, clear the node fid
	 * now close the file descriptor.
	 */
	np->d_needReopen = FALSE;
	fid = np->d_fid;
	np->d_fid = 0;
	if (fid) {
		(void)smbfs_smb_tmpclose(np, fid, context);
	}
	
	/* Remove any negative cache entries. */
	if (dnp->n_flag & NNEGNCENTRIES) {
		dnp->n_flag &= ~NNEGNCENTRIES;
		cache_purge_negatives(dvp);
	}
	
bad:
	/* if success, blow away statfs cache */
	if (!error) {
		smp->sm_statfstime = 0;
		(void) vnode_recycle(vp);
	}
	return (error);
}

static int smbfs_vnop_rmdir(struct vnop_rmdir_args *ap)
/*	struct vnop_rmdir_args {
		struct vnodeop_desc *a_desc;
		vnode_t a_dvp;
		vnode_t a_vp;
		struct componentname *a_cnp;
		vfs_context_t a_context;
	} *ap; */
{
	vnode_t dvp = ap->a_dvp;
	vnode_t vp = ap->a_vp;
	int32_t error;

	if (!vnode_isdir(vp))
		return (ENOTDIR);

	if (dvp == vp)
		return (EINVAL);

	/* Always put in the order of parent then child */
	if ((error = smbnode_lockpair(VTOSMB(dvp), VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);

	VTOSMB(dvp)->n_lastvop = smbfs_vnop_rmdir;
	VTOSMB(vp)->n_lastvop = smbfs_vnop_rmdir;

	error = smbfs_rmdir(dvp, vp, ap->a_cnp, ap->a_context);

	smbnode_unlockpair(VTOSMB(dvp), VTOSMB(vp));

	return (error);
}

/*
 * smbfs_file rename call
 */
static int smbfs_vnop_rename(struct vnop_rename_args *ap)
/*	struct vnop_rename_args {
		struct vnodeop_desc *a_desc;
		vnode_t a_fdvp;
		vnode_t a_fvp;
		struct componentname *a_fcnp;
		vnode_t a_tdvp;
		vnode_t a_tvp;
		struct componentname *a_tcnp;
		vfs_context_t a_context;
	} *ap; */
{
	vnode_t 	fvp = ap->a_fvp;
	vnode_t 	tvp = ap->a_tvp;
	vnode_t 	fdvp = ap->a_fdvp;
	vnode_t 	tdvp = ap->a_tdvp;
	struct smbmount *smp = VFSTOSMBFS(vnode_mount(fvp));
	struct smb_share *ssp = smp->sm_share;
	struct componentname *tcnp = ap->a_tcnp;
	struct componentname *fcnp = ap->a_fcnp;
	proc_t p = vfs_context_proc(ap->a_context);
	int error=0;
	int hiderr;
	struct smbnode *fnp = NULL;
	struct smbnode *tdnp = NULL;
	struct smbnode *fdnp = NULL;
	int vtype;
	struct smbnode * lock_order[4] = {NULL};
	int	lock_cnt = 0;
	int ii;
	int unix_extensions = ((UNIX_CAPS(SSTOVC(ssp)) & CIFS_UNIX_POSIX_PATH_OPERATIONS_CAP)) ? TRUE : FALSE;
	
	/* Check for cross-device rename */
	if ( (vnode_mount(fvp) != vnode_mount(tdvp)) || (tvp && (vnode_mount(fvp) != vnode_mount(tvp))) )
		return (EXDEV);
	
	vtype = vnode_vtype(fvp);
	if ( (vtype != VDIR) && (vtype != VREG) && (vtype != VLNK) )
		return (EINVAL);

	/* We can remove this some day in the future. */
	DBG_ASSERT(!vnode_isnamedstream(fvp));
	if (tvp) {
		DBG_ASSERT(!vnode_isnamedstream(tvp));
	}
	
	/*
	 * First lets deal with the parents. If they are the same only lock the from vnode, otherwise
	 * see if one is the parent of the other. We always want to lock in parent child order if we
	 * can. If they are not the parent of each other then lock in address order.
	 */
	lck_mtx_lock(&smp->sm_renamelock);
	if (fdvp == tdvp)
		lock_order[lock_cnt++] = VTOSMB(fdvp);
	else if (VTOSMB(fdvp)->n_parent && (VTOSMB(fdvp)->n_parent == VTOSMB(tdvp))) {
		lock_order[lock_cnt++] = VTOSMB(tdvp);
		lock_order[lock_cnt++] = VTOSMB(fdvp);			
	} else if (VTOSMB(tdvp)->n_parent && (VTOSMB(tdvp)->n_parent == VTOSMB(fdvp))) {
		lock_order[lock_cnt++] = VTOSMB(fdvp);			
		lock_order[lock_cnt++] = VTOSMB(tdvp);				
	} else if (VTOSMB(fdvp) < VTOSMB(tdvp)) {
		lock_order[lock_cnt++] = VTOSMB(fdvp);
		lock_order[lock_cnt++] = VTOSMB(tdvp);
	} else {
		lock_order[lock_cnt++] = VTOSMB(tdvp);
		lock_order[lock_cnt++] = VTOSMB(fdvp);				
	}
	/*
	 * Now lets deal with the children. If any of the following is true then just lock
	 * the from vnode:
	 *		1. The to vnode doesn't exist
	 *		2. The to vnode and from vnodes are the same
	 *		3. The to vnode and the from parent vnodes are the same, I know it's strange but can happen.
	 * Otherwise, lock in address order
	 */
	if ((tvp == NULL) || (tvp == fvp) || (tvp == fdvp))
		lock_order[lock_cnt++] = VTOSMB(fvp);
	else {
		if (VTOSMB(fvp) < VTOSMB(tvp)) {
			lock_order[lock_cnt++] = VTOSMB(fvp);
			lock_order[lock_cnt++] = VTOSMB(tvp);
		} else {
			lock_order[lock_cnt++] = VTOSMB(tvp);
			lock_order[lock_cnt++] = VTOSMB(fvp);				
		}
	}
	/* Make sure there are now duplicates, this would be a design flaw */
	DBG_LOCKLIST_ASSERT(lock_cnt, lock_order);

	lck_mtx_unlock(&smp->sm_renamelock);
		
	for (ii=0; ii<lock_cnt; ii++) {
		if (error)
			lock_order[ii] = NULL;
		else if ((error = smbnode_lock(lock_order[ii], SMBFS_EXCLUSIVE_LOCK)))
			lock_order[ii] = NULL;
	}
	if (error)
		goto out;

	fdnp = VTOSMB(fdvp);
	fnp = VTOSMB(fvp);
	tdnp = VTOSMB(tdvp);

	fdnp->n_lastvop = smbfs_vnop_rename;
	fnp->n_lastvop = smbfs_vnop_rename;
	tdnp->n_lastvop = smbfs_vnop_rename;
	if (tvp != NULL)
		VTOSMB(tvp)->n_lastvop = smbfs_vnop_rename;

	/*
	 * %%%
	 * Should we check to see if fvp is being deleted? HFS and AFP 
	 * check for this condition. See the HFS code for more information.
	 */

	/*
	 * Check to see if the SMB_FA_RDONLY/IMMUTABLE are set. If they are set
	 * then do not allow the rename. See HFS and AFP code.
	 */
	if ((unix_extensions || (!vnode_isdir(fvp))) && (fnp->n_dosattr & SMB_FA_RDONLY)) {
		SMBWARNING( "Delete a locked file: Permissions error\n");
		error = EPERM;
		goto out;
	}
	 
	/*
	 * Since there are no hard links (from our client point of view)
	 * fvp==tvp means the arguments are case-variants.  (If they
	 * were identical the rename syscall doesnt call us.)
	 */
	if (fvp == tvp)
		tvp = NULL;
	
	/*
	 * The problem we have here is some servers will not return the correct
	 * case of the file name in the lookup. This can cause to have two vnodes
	 * that point to the same item. Very bad but nothing we can do about that
	 * in smb. So if the target exist, the parents are the same and the name is 
	 * the same except for case, then don't delete the target.
	 *
	 * Note with smb 2 we have real inode numbers to help us here, plus with smb2
	 * we can force the server to handle this issue.
	 */
	if (tvp && (fdvp == tdvp) &&  (fnp->n_nmlen == VTOSMB(tvp)->n_nmlen) && 
		(strncasecmp((char *)fnp->n_name, (char *)VTOSMB(tvp)->n_name, fnp->n_nmlen) == 0)) {
		SMBWARNING("Not removing target, same file. %s ==> %s\n", fnp->n_name, VTOSMB(tvp)->n_name);
		smb_vhashrem(VTOSMB(tvp)); /* Remove it from our hash so it can't be found */
		(void) vnode_recycle(tvp);
		tvp = NULL;
	}

	/*
	 * If we are not doing Named Streams, they gave us a target to delete, and the
	 * source is a dot underscore file then make sure the source exist before
	 * deleting the target.
	 *
	 * This problem happens when going Mac to Mac and the share is FAT or some
	 * share that doesn't support streams. The remote VNOP_RENAME will rename
	 * the dot underscore file underneath the client. So when the cleint tries 
	 * to rename the dot underscore it thinkis the target exist and needs to be 
	 * deleted. So never delete the target if the source doesn't exist.
	 */
	if ((tvp) && (!(ssp->ss_attributes & FILE_NAMED_STREAMS)) &&
		(fnp->n_nmlen > 2) && (fnp->n_name[0] == '.') && (fnp->n_name[1] == '_')) {
		const char *name = (const char *)fnp->n_name;
		size_t nmlen = fnp->n_nmlen;
		struct smbfattr fattr;
		
		error = smbfs_smb_lookup(fdnp, &name, &nmlen, &fattr, ap->a_context);
		/* Should we check for any error or just ENOENT */
		if (error == ENOENT)
			tvp = NULL;
		/* smbfs_smb_lookup could have replace the name, free it if did */
		if (name != (char *)fnp->n_name)
			smbfs_name_free((u_char *)name);
		error = 0;
	}
		
	/*
	 * If the destination exists then it needs to be removed.
	 */
	if (tvp) {
		if (vnode_isdir(tvp)) {
			/* 
			 * From the man 2 rename
			 *
			 * CONFORMANCE
			 * The restriction on renaming a directory whose permissions disallow writing is based on the fact that UFS
			 * directories contain a ".." entry.  If renaming a directory would move it to another parent directory,
			 * this entry needs to be changed.
			 * 
			 * This restriction has been generalized to disallow renaming of any write-disabled directory, even when
			 * this would not require a change to the ".." entry.  For consistency, HFS+ directories emulate this
			 * behavior.
			 * 
			 * So if you are renaming a dir to an existing dir, you must have write access on the existing dir. Seems
			 * if the user is a super user then we can delete the existing directory.
			 *
			 * Shouldn't we let the server handle this check? Well for sure it when we are doing ACLS! In the future if we
			 * have a real vnop_access call we could use it.
			 */
			if (ssp->ss_attributes & FILE_PERSISTENT_ACLS)
				error = smbfs_rmdir(tdvp, tvp, tcnp, ap->a_context);
			else if ((vfs_context_suser(ap->a_context) == 0) || (smb_check_posix_access(ap->a_context, VTOSMB(tvp), S_IWOTH)))
				error = smbfs_rmdir(tdvp, tvp, tcnp, ap->a_context);
			else
				error = EPERM;
		} else {
			error = smbfs_remove(tdvp, tvp, tcnp, 0, ap->a_context);
		}
		if (error)
			goto out;
	}

	cache_purge(fvp);

	/* Did we open this in our read routine. Then we should close it. */
	if ((!vnode_isdir(fvp)) && (!vnode_isinuse(fvp, 0)) && (fnp->f_refcnt == 1) && fnp->f_needClose) {
		error = smbfs_close(fvp, FREAD, ap->a_context);
		if (error)
			SMBWARNING("error %d closing %s\n", error, fnp->n_name);
	}
	
	/* 
	 * Try to rename the file, this may fail if the file is open. Some 
	 * SAMBA systems allow us to rename an open file, so try this case
	 * first. 
	 *
	 * FYI: While working on Radar 4532498 I notice that you can move/rename
	 * an open file if it is open for read-only. Only tested this on Windows 2003.
	 */  
	error = smbfs_smb_rename(fnp, tdnp, tcnp->cn_nameptr, tcnp->cn_namelen, ap->a_context);
	/* 
	 * The file could be open so lets try again. This call does not work on
	 * Windows 95/98/Me/NT4 systems. Since this call only allows you to
	 * rename in place do not even try it if they are moving the item.
	 */
	if ( (fdvp == tdvp) && error )
		error = smbfs_smb_t2rename(fnp, tcnp->cn_nameptr, tcnp->cn_namelen, ap->a_context, 1, NULL);

	/*
	 * We should really print a better description of the error to the 
	 * system log. Thing we could print in the system log.
	 * 1. This server does not support renaming of an open file.
	 * 2. This server only supports renaming of an open file in place.
	 * 3. Renaming open file failed.
	 * 
	 * Written up as Radar 4381236
	 */
	if (!error) {
		u_char *new_name, *old_name;
		u_int32_t hashval;
		u_int32_t orig_flag = fnp->n_flag;
		
		/* 
		 * At this point we still have both parents and the children locked if they exist. Since
		 * the parents are locked we know that a lookup of the children cannot happen over the
		 * network. So we are safe to play with both names and the hash node entries.The old code 
		 * would just remove the node from the hash table and then just change the nodes name, this
		 * was very bad and could cause the following to happen:
		 *
		 * RENAME HAPPENS ON VP1: vp1->np1->n_name = file1 gets renamed to vp1->np1->n_name = file2
		 *	1. vp1 is no longer in the name cache.
		 *  2. np1 is no longer in the hash table.
		 *  3. vp1 still has a ref taken on it and can still be used.
		 * 
		 * LOOKUP HAPPNES ON file2: Which will cause a new vnode and smbnode to get created
		 *  1. vp1 is not found because its not in the name cache.
		 *  2. np1 is not found because its not in the hash table.
		 *  3. vp2 and np2 get created and vp2->np2->n_name = file2
		 *
		 * RENAME HAPPENS ON VP2: vp2->np2->n_name = file2 gets renamed to vp2->np2->n_name = file3
		 *	1. vp1 no longer has the correct name.
		 *	2. vp2 is no longer in the name cache.
		 *  3. np2 is no longer in the hash table.
		 *  4. vp2 still has a ref taken on it and can still be used.
		 *	
		 * SOME OTHER OPERATION HAPPENS ON VP1: It will fail because VP1 now has the wrong name.
		 *	1. Now the whole thing can repeat, very bad! 
		 *
		 */		
		smb_vhashrem(fnp);	/* Remove it from the hash, it doesn't exist under that name anymore */
		/* Radar 4540844 will remove the need for the following code. */
		if (tdvp && (fdvp != tdvp)) {
			/* Take a ref count on the new parent */
			if (!vnode_isvroot(tdvp)) {
				if (vnode_get(tdvp) == 0) {
					if (vnode_ref(tdvp) == 0)
						fnp->n_flag |= NREFPARENT;
					else 
						fnp->n_flag &= ~NREFPARENT;
					vnode_put(tdvp);
				}
			}
			else 
				fnp->n_flag &= ~NREFPARENT;
			/* Remove the ref count off the old parent */
			if ((!vnode_isvroot(fdvp)) && (orig_flag & NREFPARENT)) {
				if (vnode_get(fdvp) == 0) {
					vnode_rele(fdvp);
					vnode_put(fdvp);
				}
			}
			fnp->n_parent = VTOSMB(tdvp);
		}
		
		/* 
		 * Now reset the name so path lookups will work and add the node back 
		 * into the hash table, so other lookups can this node. 
		 */
		new_name = smbfs_name_alloc((const u_char *)tcnp->cn_nameptr, tcnp->cn_namelen);
		old_name = fnp->n_name;
		lck_rw_lock_exclusive(&fnp->n_name_rwlock);
		fnp->n_name = new_name;
		fnp->n_nmlen = tcnp->cn_namelen;
		lck_rw_unlock_exclusive(&fnp->n_name_rwlock);
		hashval = smbfs_hash((const u_char *)fnp->n_name, fnp->n_nmlen);
		smb_vhashadd(fnp, hashval);
		/* Now its safe to free the old name */
		if (old_name)
			smbfs_name_free(old_name);
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
		if ( (hiderr = smbfs_smb_hideit(tdnp, tcnp->cn_nameptr, tcnp->cn_namelen, ap->a_context)) )
			SMBERROR("hiderr %d\n", hiderr);
	} else if ( !error && (tcnp->cn_nameptr[0] != '.') && (fcnp->cn_nameptr[0] == '.') ) {
		if ( (hiderr = smbfs_smb_unhideit(tdnp, tcnp->cn_nameptr, tcnp->cn_namelen, ap->a_context)) )
			SMBERROR("(un)hiderr %d\n", hiderr);
	}

	if (vnode_isdir(fvp)) {
		if ( (tvp != NULL) && (vnode_isdir(tvp)) )
			cache_purge(tdvp);
		cache_purge(fdvp);
	}
	if (error == EBUSY) {
		char errbuf[32];

		proc_name(proc_pid(p), &errbuf[0], 32);
		SMBERROR("warning: pid %d(%.*s) rename open file(%s)\n", proc_pid(p), 32, &errbuf[0], fnp->n_name);
	}
	smbfs_attr_touchdir(smp->sm_share, fdnp);
	if (tdvp != fdvp)
		smbfs_attr_touchdir(smp->sm_share, tdnp);
	/* if success, blow away statfs cache */
	if (!error) {
		smp->sm_statfstime = 0;
		if (tdnp->n_flag & NNEGNCENTRIES) {
			tdnp->n_flag &= ~NNEGNCENTRIES;
			cache_purge_negatives(tdvp);
		}
	}
	
out:
	if (error == EBUSY) {
		char errbuf[32];

		proc_name(proc_pid(p), &errbuf[0], 32);
		SMBWARNING("warning: pid %d(%.*s) rename open file(%s)\n", proc_pid(p), 32, &errbuf[0], fnp->n_name);
	}
	for (ii=0; ii<lock_cnt; ii++)
		if (lock_order[ii])
			smbnode_unlock(lock_order[ii]);
				
	return (error);
}

/*
 * sometime it will come true...
 */
static int smbfs_vnop_link(struct vnop_link_args *ap)
/*	struct vnop_link_args {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		vnode_t a_tdvp;
		struct componentname *a_cnp;
		vfs_context_t a_context;
	} *ap; */
{
	proc_t p = vfs_context_proc(ap->a_context);
	struct smbnode *np = VTOSMB(ap->a_vp);
	char errbuf[32];

	proc_name(proc_pid(p), &errbuf[0], 32);
	SMBERROR("warning: pid %d(%.*s) hardlink(%s)\n", proc_pid(p), 32, &errbuf[0], np->n_name);
	return (err_link(ap));
}

/*
 * smbfs_vnop_symlink link create call.
 * 
 * vnode_t a_dvp;
 * vnode_t *a_vpp;
 * struct componentname *a_cnp;
 * struct vnode_attr *a_vap;
 *  char *a_target;
 *  vfs_context_t a_context;
 */
static int smbfs_vnop_symlink(struct vnop_symlink_args *ap)
{
	int error;
	struct vnop_create_args a;
	vnode_t 	dvp = ap->a_dvp;
	struct smbnode *dnp;
	
	/* Make sure we lock the parent before calling smbfs_create */
	if ((error = smbnode_lock(VTOSMB(dvp), SMBFS_EXCLUSIVE_LOCK)))
		return error;
	
	dnp = VTOSMB(dvp);
	dnp->n_lastvop = smbfs_vnop_symlink;
	a.a_dvp = dvp;
	a.a_vpp = ap->a_vpp;
	a.a_cnp = ap->a_cnp;
	a.a_vap = ap->a_vap;
	a.a_context = ap->a_context;
	/* 
	 * We use PATH_MAX because we have no way currently to find out what is the 
	 * max path on the server.
	 */
	error = smbfs_create(&a, ap->a_target, strnlen(ap->a_target, PATH_MAX+1));
	smbnode_unlock(dnp);
	return (error);
}

/*
 * smbfs_vnop_readlink read symlink call.
 * 
 * vnode_t *a_vpp;
 * struct componentname *a_cnp;
 * uio_t a_uio; 
 *  vfs_context_t a_context;
 */
static int smbfs_vnop_readlink(struct vnop_readlink_args *ap)
{
	vnode_t 	vp = ap->a_vp;
	struct smbnode *np = NULL;
	struct smbmount *smp = NULL;
	int error;
	struct smb_share *ssp = NULL;

	if (vnode_vtype(vp) != VLNK)
		return (EINVAL);

	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);
	
	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_readlink;
	smp = VTOSMBFS(vp);
	ssp = smp->sm_share;
		
	if (np->n_flag & NWINDOWSYMLNK)
		error = smbfs_windows_readlink(ssp, np, ap->a_uio, ap->a_context);
	else 
		error = smbfs_smb_read_symlink(ssp, np, ap->a_uio, ap->a_context);
	
	smbnode_unlock(np);
	return (error);
}

static int smbfs_vnop_mknod(struct vnop_mknod_args *ap) 
/*	struct vnop_mknod_args {
		struct vnodeop_desc *a_desc;
		vnode_t a_dvp;
		vnode_t *a_vpp;
		struct componentname *a_cnp;
		struct vnode_attr *a_vap;
		vfs_context_t a_context;
	} *ap; */
{
	proc_t p = vfs_context_proc(ap->a_context);
	char errbuf[32];

	proc_name(proc_pid(p), &errbuf[0], 32);
	SMBERROR("warning: pid %d(%.*s) mknod(%s)\n", proc_pid(p), 32 , &errbuf[0], ap->a_cnp->cn_nameptr);
	return (err_mknod(ap));
}

static int smbfs_vnop_mkdir(struct vnop_mkdir_args *ap)
/*	struct vnop_mkdir_args {
		struct vnodeop_desc *a_desc;
		vnode_t a_dvp;
		vnode_t *a_vpp;
		struct componentname *a_cnp;
		struct vnode_attr *a_vap;
		vfs_context_t a_context;
	} *ap; */
{
	vnode_t 	dvp = ap->a_dvp;
	struct vnode_attr *vap = ap->a_vap;
	vnode_t 	vp;
	struct componentname *cnp = ap->a_cnp;
	struct smbnode *dnp = NULL;
	struct smbmount *smp = NULL;
	struct smbfattr fattr;
	const char *name = cnp->cn_nameptr;
	size_t len = cnp->cn_namelen;
	int error;
	struct smb_vc *vcp;
	struct timespec ts;

	if (name[0] == '.' && (len == 1 || (len == 2 && name[1] == '.')))
		return (EEXIST);

	if ((error = smbnode_lock(VTOSMB(dvp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);
	dnp = VTOSMB(dvp);
	dnp->n_lastvop = smbfs_vnop_mkdir;
	smp = VTOSMBFS(dvp);
	vcp = SSTOVC(smp->sm_share);
	
	error = smbfs_smb_mkdir(dnp, name, len, ap->a_context, &fattr);
	if (error)
		goto exit;
 	/* Old style create call (Windows 98). No attributes returned on the create */
	if ((vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS) != SMB_CAP_NT_SMBS) {
		error = smbfs_smb_lookup(dnp, &name, &len, &fattr, ap->a_context);
		if (error)
			goto exit;
	}
	smbfs_attr_touchdir(smp->sm_share, dnp);
	/* 
	 * %%%
	 * Would really like to clean this code up in the future. The whole create, mkdir
	 * and smblink create code should use the same code path. 
	 */
	error = smbfs_nget(VTOVFS(dvp), dvp, name, len, &fattr, &vp, cnp->cn_flags, ap->a_context);
	if (error)
		goto bad;

	/* We created it, so set the uid and gid here */
	VTOSMB(vp)->n_uid = smp->sm_args.uid;
	VTOSMB(vp)->n_gid = smp->sm_args.gid;
	/* 
	 * We just create the directory, so we have no finder info. So set our cache 
	 * timer to reflect this information
	 */
	nanouptime(&ts);
	VTOSMB(vp)->finfo_cache = ts.tv_sec;
	
	/* 
	 * If we create the directory using the NTCreateAndX then it will already be 
	 * marked hidden. So check to see if the the directory is arleady hidden before
	 * making it hidden. Remember that smbfs_nget will set the n_dosattr field to what 
	 * is passed in with fattr. So by now we have the latest attributes return by the
	 * server. Also ignore the error nothing we can do about it anyways.
	 */
	if (((VTOSMB(vp)->n_dosattr & SMB_FA_HIDDEN) != SMB_FA_HIDDEN) && (name[0] == '.'))
		(void)smbfs_smb_hideit(VTOSMB(vp), NULL, 0, ap->a_context);
			
	smbfs_composeacl(vap, vp, ap->a_context);
	*ap->a_vpp = vp;
	smbnode_unlock(VTOSMB(vp));	/* Done with the smbnode unlock it. */
	error = 0;
	if (dnp->n_flag & NNEGNCENTRIES) {
		dnp->n_flag &= ~NNEGNCENTRIES;
		cache_purge_negatives(dvp);
	}
	
bad:
	if (name != cnp->cn_nameptr)
		smbfs_name_free((u_char *)name);
	/* if success, blow away statfs cache */
	smp->sm_statfstime = 0;
exit:
	smbnode_unlock(dnp);
	return (error);
}

/*
 * smbfs_vnop_readdir call
 */
static int smbfs_vnop_readdir(struct vnop_readdir_args *ap)
/*	struct vnop_readdir_args {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		struct uio *a_uio;
		int a_flags;
		int *a_eofflag;
		int *a_numdirent;
		vfs_context_t a_context;
 } *ap; */
{
	vnode_t	vp = ap->a_vp;
	uio_t uio = ap->a_uio;
	int error;
	int32_t numdirent = 0;

	if (uio_offset(uio) < 0)
		return (EINVAL);

	if (!vnode_isdir(vp))
		return (EPERM);
	
	if (ap->a_eofflag)
		*ap->a_eofflag = 0;
	
	if (uio_resid(uio) == 0)
		return (0);
	
	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);
	VTOSMB(vp)->n_lastvop = smbfs_vnop_readdir;
	
	error = smbfs_readvdir(vp, uio, ap->a_context, ap->a_flags, &numdirent);
	if (error == ENOENT) {
		/* We have reached the end of the search */
		if (ap->a_eofflag)
			*ap->a_eofflag = 1;
		error = 0;
	}
	/* Return the number of entries from this lookup */ 
	if (ap->a_numdirent)
		*ap->a_numdirent = numdirent;
	smbnode_unlock(VTOSMB(vp));
	return (error);
}

int32_t smbfs_fsync(vnode_t vp, int waitfor, int ubc_flags, vfs_context_t context)
{
#pragma unused(waitfor)
	int error;
	off_t size;

	size = smb_ubc_getsize(vp);	
	if (size > 0) {
		error = ubc_msync(vp, 0, size, NULL, UBC_PUSHALL | UBC_SYNC | ubc_flags);
		if (error)	/* The below code will reset error, for now we only print the error out */ 
			SMBERROR("ubc_msync failed! %d\n", error);
	}
	error = smbfs_smb_flush(VTOSMB(vp), context);
	if (!error)
		VTOSMBFS(vp)->sm_statfstime = 0;
	return (error);
}


static int32_t smbfs_vnop_fsync(struct vnop_fsync_args *ap)
/*	struct vnop_fsync_args {
		vnode_t a_vp;
		int32_t a_waitfor;
		vfs_context_t a_context;
	} *ap; */
{
	int32_t error;

	error = smbnode_lock(VTOSMB(ap->a_vp), SMBFS_EXCLUSIVE_LOCK);
	if (error)
		return (0);
	VTOSMB(ap->a_vp)->n_lastvop = smbfs_vnop_fsync;

	error = smbfs_fsync(ap->a_vp, ap->a_waitfor, 0, ap->a_context);

	smbnode_unlock(VTOSMB(ap->a_vp));
	return (error);
}


static int smbfs_vnop_pathconf (struct vnop_pathconf_args *ap)
/*	struct vnop_pathconf_args {
		struct vnodeop_desc *a_desc;
		vnode_t  a_vp;
		int a_name;
		register_t *a_retval;
		vfs_context_t a_context;
	} *ap; */
{
	struct smb_share *ssp = VTOSMBFS(ap->a_vp)->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	int32_t *retval = ap->a_retval;
	int error = 0;
	
	switch (ap->a_name) {
	    case _PC_LINK_MAX:	/* Hard Link Support */
		*retval = 0;	/* May support in the future. Depends on the server */
		break;
	    case _PC_NAME_MAX:
		*retval = ssp->ss_maxfilenamelen;
		break;
	    case _PC_PATH_MAX:
			/*
			 * XXX
			 * Most Windows system have a 255 limit, but you can configure
			 * them to support 1024. It would be nice if we could figure out
			 * the real value for this field.
			 */
			*retval = PATH_MAX;
			break;
	    case _PC_CHOWN_RESTRICTED:
			*retval = 1;
			break;
	    case _PC_NO_TRUNC:
			*retval = 0;
			break;
	    case _PC_NAME_CHARS_MAX:
			*retval = ssp->ss_maxfilenamelen;
			break;
	    case _PC_CASE_SENSITIVE:
		/*
		 * Thought about using FILE_CASE_SENSITIVE_SEARCH, but this
		 * really just means they will search by case. It does not mean
		 * this is a case sensitive file system. Currently we have no
		 * way of determining if this is a case sensitive or 
		 * insensitive file system. We need to return a consistent
		 * answer between pathconf and vfs_getattr. We do not know the real 
		 * answer for case sensitive, but lets default to what 90% of the 
		 * servers have set.  Also remeber this fixes Radar 4057391 and 3530751. 
		 */
		*retval = 0;
		break;
	    case _PC_CASE_PRESERVING:
		if (ssp->ss_attributes & FILE_CASE_PRESERVED_NAMES)
			*retval = 1;
		else *retval = 0;
		break;
	    /* 
	     * Handle by vn_pathconf.
	     *
	     * case _PC_EXTENDED_SECURITY_NP:
	     *		*retval = vfs_extendedsecurity(vnode_mount(vp)) ? 1 : 0;
	     *		break;
	     * case _PC_AUTH_OPAQUE_NP:
	     *		*retval = vfs_authopaque(vnode_mount(vp));
	     *		break;
	     * case _PC_2_SYMLINKS:
	     *		*retval = 1;
	     *		break;
	     * case _PC_ALLOC_SIZE_MIN:
	     *		*retval = 1;    // XXX lie: 1 byte
	     *		break;
	     * case _PC_ASYNC_IO:     // unistd.h: _POSIX_ASYNCHRONUS_IO
	     *		*retval = 1;    // [AIO] option is supported
	     *		break;
	     */
	    case _PC_FILESIZEBITS:
		if (vcp->vc_sopt.sv_caps & SMB_CAP_LARGE_FILES)
			*retval = 64;	/* The server supports 64 bit offsets */
		else *retval = 32;	/* The server supports 32 bit offsets */
		break;
	    /* 
	     * Handle by vn_pathconf.
	     *
	     * case _PC_PRIO_IO:       // unistd.h: _POSIX_PRIORITIZED_IO
	     *		 *retval = 0;    // [PIO] option is not supported
	     *		break;
	     * case _PC_REC_INCR_XFER_SIZE:
	     *		*retval = 4096; // XXX go from MIN to MAX 4K at a time
	     *		break;
	     * case _PC_REC_MIN_XFER_SIZE:
	     *		*retval = 4096; // XXX recommend 4K minimum reads/writes
	     *		break;
	     * case _PC_REC_MAX_XFER_SIZE: // Should we use SMB_IOMAX
	     *		*retval = 65536; // XXX recommend 64K maximum reads/writes
	     *		break;
	     * case _PC_REC_XFER_ALIGN:
	     *		*retval = 4096; // XXX recommend page aligned buffers
	     *		break;
	     * case _PC_SYMLINK_MAX:
	     *		*retval = 255;  // Minimum acceptable POSIX value
	     *		break;
	     * case _PC_SYNC_IO:       // unistd.h: _POSIX_SYNCHRONIZED_IO
	     *		*retval = 0;    // [SIO] option is not supported
	     *		break;
	     */
	    default:
			error = EINVAL;
	}
	return (error);
}

#ifdef USE_SIDEBAND_CHANNEL_RPC

static int32_t memcpy_ToUser(user_addr_t dest, int8_t* source, int32_t len, struct proc *a_p)
{
    uio_t 	auio;
    int32_t	result;

	auio =  uio_create(1, 0, proc_is64bit(a_p) ? UIO_USERSPACE64 : UIO_USERSPACE32, UIO_READ);
	uio_addiov(auio, dest, len);
    result = uiomove((caddr_t) source, len, auio);
	uio_free(auio);
    
    return result;
}
#endif // USE_SIDEBAND_CHANNEL_RPC


/*
 * smbfs_vnop_ioctl - smbfs vnodeop entry point
 *	vnode_t a_vp;
 *	int32_t  a_command;
 *	caddr_t  a_data;
 *	int32_t  a_fflag;
 *	vfs_context_t context;
 */
static int32_t smbfs_vnop_ioctl(struct vnop_ioctl_args *ap)
{
    vnode_t		vp = ap->a_vp;
    struct smbnode	*np;
    int32_t			error = 0;
#ifdef USE_SIDEBAND_CHANNEL_RPC
    proc_t			p = vfs_context_proc(ap->a_context);
#endif // USE_SIDEBAND_CHANNEL_RPC
	pid_t			pid = proc_pid(vfs_context_proc(ap->a_context));


	error = smbnode_lock(VTOSMB(ap->a_vp), SMBFS_EXCLUSIVE_LOCK);
	if (error)
		return (error);

	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_ioctl;

	/*
	 * %%% 
	 * Remove the IOCBASECMD case lines (OLD) when fsctl code in vfs_syscalls.c 
	 * gets fixed to not use IOCBASECMD before calling VOP_IOCTL.
	 *
	 * The smbfsByteRangeLock2FSCTL call was made to support classic. We do 
	 * not support classic, but the file manager will only make the smbfsByteRangeLock2FSCTL
	 * call. So for now support all the commands, but treat them all the 
	 * same.
	 *
	 */
	switch (ap->a_command) {
	case smbfsUniqueShareIDFSCTL:
	case smbfsUniqueShareIDFSCTL_BASECMD: {
		struct UniqueSMBShareID *uniqueptr = (struct UniqueSMBShareID *)ap->a_data;
		struct smbmount *smp = VTOSMBFS(vp);
		
		uniqueptr->error = 0;
		if ((uniqueptr->flags & SMBFS_GET_ACCESS_INFO) ||
			((uniqueptr->unique_id_len == smp->sm_args.unique_id_len) && 
			(bcmp(smp->sm_args.unique_id, uniqueptr->unique_id, uniqueptr->unique_id_len) == 0))) {
				struct smb_vc *vcp = SSTOVC(smp->sm_share);	
				
			uniqueptr->user[0] = 0;	/* Just in case we have no user name */
			if (vcp->vc_flags & SMBV_GUEST_ACCESS) {
				uniqueptr->connection_type = kConnectedByGuest;
				strlcpy(uniqueptr->user, kGuestAccountName, SMB_MAXUSERNAMELEN + 1);
			}
			else if (vcp->vc_username) {
				uniqueptr->connection_type = kConnectedByUser;
				strlcpy(uniqueptr->user, vcp->vc_username, SMB_MAXUSERNAMELEN + 1);
			} else {
				uniqueptr->connection_type = kConnectedByKerberos;
				smb_get_username_from_kcpn(vcp, uniqueptr->user, sizeof(uniqueptr->user));
			}
			uniqueptr->error = EEXIST;
		}
	}
	break;
	case smbfsByteRangeLock2FSCTL:
	case smbfsByteRangeLock2FSCTL_BASECMD:
	case smbfsByteRangeLockFSCTL:
	case smbfsByteRangeLockFSCTL_BASECMD: {
		struct ByteRangeLockPB *pb = (struct ByteRangeLockPB *) ap->a_data;
		struct ByteRangeLockPB2 *pb2 = (struct ByteRangeLockPB2 *) ap->a_data;
		u_int32_t lck_pid;
		u_int32_t timo;
		u_int16_t fid = 0;
		struct fileRefEntry *fndEntry = NULL;
		u_int16_t accessMode = 0;
		int8_t flags;

		/* make sure its a file */
		if (vnode_isdir(vp)) {
			error = EISDIR;
			goto exit;
		}

		/* Before trying the lock see if the file needs to be reopened */
		error = smbfs_smb_reopen_file(np, ap->a_context);
		if (error) {
			SMBDEBUG(" %s waiting to be revoked\n", np->n_name);
			goto exit;
		}
		
		/* 
		 * If adding/removing a ByteRangeLock, thus this vnode should NEVER 
		 * be cacheable since the page in/out may overlap a lock and get 
		 * an error.
		 */
		if (! vnode_isnocache(vp)) {
			ubc_msync (vp, 0, ubc_getsize(vp), NULL, UBC_PUSHDIRTY | UBC_SYNC);
			ubc_msync (vp, 0, ubc_getsize(vp), NULL, UBC_INVALIDATE);
			vnode_setnocache(vp);
		}
		
		accessMode = np->f_accessMode;
		
		/* Since we never get a smbfsByteRangeLockFSCTL call we could skip this check, but for now leave it. */
		if ( (ap->a_command == smbfsByteRangeLock2FSCTL) || (ap->a_command == smbfsByteRangeLock2FSCTL_BASECMD) ) {
			int32_t openMode = 0;
                
			/* 
			 * Not just for classic any more, could get this from Carbon.
			 * 
			 * They will be using an extended BRL call that also passes in the open mode used
			 * that was used to open the file.  I will use the open access mode and pid to find 
			 * the fork ref to do the lock on.
			 * Scenarios:
			 *	Open (R/W), BRL, Close (R/W)
			 *	Open (R/W/dW), BRL, Close (R/W/dW)
			 *	Open1 (R/W), Open2 (R), BRL1, BRL2, Close1 (R/W), Close2 (R)
			 *	Open1 (R/W/dW), Open2 (R), BRL1, BRL2, Close1 (R/W/dW), Close2 (R)
			 *	Open1 (R/dW), Open2 (R/dW), BRL1, BRL2, Close1 (R/dW), Close2 (R/dW) 
			 */
			if ( (error = file_flags(pb2->fd, &openMode)) ) {
				error = EBADF;
				goto exit;
			}

			if (openMode & FREAD) {
				accessMode |= kAccessRead;
			}
			if (openMode & FWRITE) {
				accessMode |= kAccessWrite;
			}
               
			/* See if we can find a matching fork that has byte range locks or denyModes */
			if (openMode & FHASLOCK) {
				/* 
				 * NOTE:  FHASLOCK can be set by open with O_EXCLUSIVE or O_SHARED which maps 
				 * to my deny modes or FHASLOCK could also have been set/cleared by calling flock directly. 
				 * I assume that if they are using byte range locks, then they are Carbon and unlikely to be
				 * using flock.
				 * 
				 * Assume it was opened with denyRead/denyWrite or just denyWrite.
				 * Try denyWrite first, if not found, try with denyRead and denyWrite 
				 */
				accessMode |= kDenyWrite;
				error = smbfs_findFileRef(vp, pid, accessMode, kExactMatch, 0, 0, &fndEntry, &fid);
				if (error != 0) {
					accessMode |= kDenyRead;
					error = smbfs_findFileRef (vp, pid, accessMode, kExactMatch, 0, 0, &fndEntry, &fid);
				}
				if (error != 0) {
					/* deny modes were used, but the fork ref cant be found, return error */
					error = EBADF;
					goto exit;
				}
			}
			else {
				/* no deny modes used, look for any forks opened previously for BRL */
				error = smbfs_findFileRef(vp, pid, accessMode, kExactMatch, 0, 0, &fndEntry, &fid);
			}
		}
		else
			error = smbfs_findFileRef (vp, pid, kAccessRead | kAccessWrite, kAnyMatch, 0, 0, &fndEntry, &fid);

		/* 
		 * The process was not found or no list found
		 * Either case, we need to
		 *	1)  Open a new fork
		 *	2)  create a new OpenForkRefEntry entry and add it into the list 
		 */
		if (error) {
			 u_int32_t rights = 0; 
			 u_int32_t shareMode = NTCREATEX_SHARE_ACCESS_ALL;
			 proc_t 
			 p = vfs_context_proc(ap->a_context);
			 
			if (np->f_refcnt <= 0) {
				/* This is wrong, someone has to call open() before doing a byterange lock */
				error = EBADF;
				goto exit;
			}

			if (pb->unLockFlag == 1) {
				/* We must find a matching lock in order to succeed at unlock */
				error = EINVAL;
				goto exit;
			}
			/* Need to open the file here */
			if (accessMode & kAccessRead)
				rights |= SA_RIGHT_FILE_READ_DATA;
			if (accessMode & kAccessWrite)
				rights |= SA_RIGHT_FILE_APPEND_DATA | SA_RIGHT_FILE_WRITE_DATA;
				
			if (accessMode & kDenyWrite)
				shareMode &= ~NTCREATEX_SHARE_ACCESS_WRITE; /* Remove the wr shared access */
			if (accessMode & kDenyRead)
				shareMode &= ~NTCREATEX_SHARE_ACCESS_READ; /* Remove the wr shared access */
		
			error = smbfs_smb_open(np, rights, shareMode, ap->a_context, &fid);
			if (error != 0)
				goto exit;
			
			smbfs_addFileRef(vp, p, accessMode, rights, fid, &fndEntry);
		}

		/* Now I can do the ByteRangeLock */
		if (pb->startEndFlag) {
			/* 
			 * SMB only allows you to lock relative to the begining of the
			 * file.  So, I need to convert offset base on the begining
			 * of the file.  
			 */
			u_int64_t fileSize = np->n_size;

			pb->offset += fileSize;
			pb->startEndFlag = 0;
		}
            
		flags = 0;
		if (pb->unLockFlag)
			flags |= SMB_LOCK_RELEASE;
		else flags |= SMB_LOCK_EXCL;
		/* The problem here is that the lock pid must match the SMB Header PID. Some
		 * day it would be nice to pass a better value here. But for now always
		 * use the same value.
		 */
		lck_pid = 1;
		/*
		 * -1 infinite wait
		 * 0  no wait
		 * any other number is the number of milliseconds to wait.
		 */
		timo = 0;

		error = smbfs_smb_lock(np, flags, fid, lck_pid, pb->offset, pb->length, ap->a_context, timo);
		if (error == 0) {
			/* Save/remove lock info for use in read/write to determine what fork to use */
			smbfs_AddRemoveLockEntry (fndEntry, pb->offset, pb->length, pb->unLockFlag, lck_pid);
			/* return the offset to the first byte of the lock */
			pb->retRangeStart = pb->offset;
		} else if ((!pb->unLockFlag) && (error == EACCES)) {
			/* 
			 * Need to see if we are locking against ourself, so we can return 
			 * the correct error.
			 */
			if (smbfs_FindLockEntry(fndEntry, pb->offset, pb->length, lck_pid))
				error = EAGAIN;
		}
	}
	break;

#ifdef USE_SIDEBAND_CHANNEL_RPC
	case smbfsSpotLightSideBandRPC:
	case smbfsSpotLightSideBandRPC_BASECMD: {
		ucred_t cred = vfs_context_ucred(ap->a_context);
		struct smb_share *ssp = np->n_mount->sm_share;
		struct smb_vc *vcp = SSTOVC(ssp);
		struct user_SpotLightSideBandInfoPB *pb = (struct user_SpotLightSideBandInfoPB *) ap->a_data;
		struct SpotLightSideBandInfoPB *pb32 = (struct SpotLightSideBandInfoPB*) ap->a_data;
		//int32_t retval;
		//int32_t actualReadSize = 0;
		int8_t	*replyBuffer = NULL;
		int8_t	*cptr = NULL;
		struct networkAddress {
			u_int8_t	netAddrCount;
			u_int8_t	len;
			u_int8_t	tag;
			struct	in_addr IPv4Addr;
		} myNetAddress;
		struct networkAddress6 {
			u_int8_t	netAddrCount;
			u_int8_t	len;
			u_int8_t	tag;
			struct	in6_addr IPv6Addr;
		} myNetAddress6;
		struct sockaddr_in *ipv4;
		struct sockaddr_in6 *ipv6;
		//u_int32_t *lptr;
		u_int32_t actualLen = 0;
		int8_t *addrPtr;
		u_int32_t addrSize = 0;
		enum {
		  kAFPTagTypeIP                 = 0x01, /* 4 byte IP address (MSB first)            */
		  kAFPTagTypeIPv6				= 0x06, /* 4 byte IP address, 2 byte port (MSB first)     */
		};
		size_t osize;
		u_int32_t command = 1;

		enum {
		  kAFPTagLengthIP               = 0x06,
		  kAFPTagLengthIPv6             = 0x12
		};
				 

		/* for security, we will only allow root to make this call */ 
		if ((error = vfs_context_suser(ap->a_context))) {
			SMBDEBUG("SideBand failed uid 0x%x\n", kauth_cred_getuid (cred));
			error = EPERM;
			goto exit;
		}

		/* Only an IPv4 or IPv6 address is allowed.
		Note:  an alternate port for the SideBandServer can be returned by the server in the virtual auth data */
		ipv4 = (struct sockaddr_in *) &vcp->vc_saddr->sa_data[2];
		if (ipv4->sin_family == AF_INET) {
			memset (&myNetAddress, 0, sizeof (myNetAddress));
			myNetAddress.netAddrCount = 1;
			//ipv4 = (struct sockaddr_in *) (&vcp->vc_saddr->sa_data[2]);
			myNetAddress.len = kAFPTagLengthIP;
			myNetAddress.tag = kAFPTagTypeIP;
			myNetAddress.IPv4Addr = ipv4->sin_addr;
			addrPtr = (int8_t *) &myNetAddress;
			addrSize = sizeof (myNetAddress);
		}
		else {
			if (vcp->vc_saddr->sa_family == AF_INET6) {
				memset (&myNetAddress6, 0, sizeof (myNetAddress6));
				myNetAddress6.netAddrCount = 1;
				ipv6 = (struct sockaddr_in6 *)(&vcp->vc_saddr->sa_data[6]);
				myNetAddress6.len = kAFPTagLengthIPv6;
				myNetAddress6.tag = kAFPTagTypeIPv6;
				myNetAddress6.IPv6Addr = ipv6->sin6_addr;
				addrPtr = (int8_t *) &myNetAddress6;
				addrSize = sizeof (myNetAddress6);
			}
			else {
				SMBDEBUG("SideBand - unknown server address family %d\n", vcp->vc_paddr->sa_family);
				error = EIO;
				goto exit;
			}
		}
		/* Fill out the return data */
		if (proc_is64bit (p)) {
			osize = pb->authFileDataLen;
			
			pb->version = 1;	
			pb->flags = 0;	
			pb->networkProtocol = 'smbm';	/* return network protocol */
			
			if (pb->srvrAddrDataLen < addrSize) {
				SMBDEBUG("SideBand - Server Address buffer too small\n");
				error = ENOSPC;
				goto exit;
			}
			error = memcpy_ToUser(pb->srvrAddrData, (int8_t*) addrPtr, addrSize, p);
			if (error) {
				SMBDEBUG("SideBand - memcpy_ToUser failed %d\n", error);
				goto exit;
			}
			pb->srvrAddrDataLen = addrSize;
		} 
		else {
			osize = pb32->authFileDataLen;
			
			pb32->version = 1;	
			pb32->flags = 0;	
			pb32->networkProtocol = 'smbm';	/* return network protocol */
			
			if (pb32->srvrAddrDataLen < addrSize) {
				SMBDEBUG("SideBand - Server Address buffer too small\n");
				error = ENOSPC;
				goto exit;
			}
			error = memcpy_ToUser(CAST_USER_ADDR_T (pb32->srvrAddrData), addrPtr, addrSize, p);
			if (error) {
				SMBDEBUG("SideBand - memcpy_ToUser failed %d\n", error);
				goto exit;
			}
			pb32->srvrAddrDataLen = addrSize;
		}
		
		/* Read the VirtualAuthFile (which is unique to each user) from the sharepoint to get.  
		   The file data is encrypted, but the client can decrypt it using its own key.  The
		   actual format of the file data is opaque to the client.
		*/
		MALLOC (replyBuffer, int8_t *, osize, M_TEMP, M_WAITOK);
		DBG_ASSERT(replyBuffer != NULL);
		bzero (replyBuffer, osize);
			
		error = smbfs_spotlight(np, ap->a_context, &command, replyBuffer, sizeof (command), &osize);
		if (error != 0) {
			FREE(replyBuffer, M_TEMP);			
			goto exit;
		}

		/* <bms> %%% to do, decrypt the data here */
		cptr = replyBuffer;
		actualLen = osize;

		if (proc_is64bit (p)) {
			error = memcpy_ToUser(pb->authFileData, cptr, actualLen, p);
			if (error) {
				SMBDEBUG("SideBand memcpy_ToUser of Auth File Data failed %d\n", error);
				FREE(replyBuffer, M_TEMP);
				goto exit;
			}
			pb->authFileDataLen = actualLen;
		}
		else {
			error = memcpy_ToUser(CAST_USER_ADDR_T(pb32->authFileData), cptr, actualLen, p);
			if (error) {
				SMBDEBUG("SideBand memcpy_ToUser of Auth File Data failed %d\n", error);
				FREE(replyBuffer, M_TEMP);
				goto exit;
			}
			pb32->authFileDataLen = actualLen;
		}
		FREE(replyBuffer, M_TEMP);			
	}
	break;
	
	case smbfsGSSDProxy:
	case smbfsGSSDProxy_BASECMD: {
		ucred_t cred = vfs_context_ucred(ap->a_context);
		//struct smb_share *ssp = np->n_mount->sm_share;
		//struct smb_vc *vcp = SSTOVC(ssp);
		struct user_gssdProxyPB *pb = NULL;
		struct user_gssdProxyPB new;
		struct gssdProxyPB *old = (struct gssdProxyPB*) ap->a_data;
		

		/* for security, we will only allow root to make this call */ 
		if ((error = vfs_context_suser(ap->a_context))) {
			SMBDEBUG("gssdProxy failed uid 0x%x\n", kauth_cred_getuid (cred));
			error = EPERM;
			goto exit;
		}

		memset (&new, 0, sizeof (new));

		if (proc_is64bit (p)) {
			pb = (struct user_gssdProxyPB*) ap->a_data;
		}
		else {
			/* convert from old 32 bit struct to new 64 bit struct */
			pb = (struct user_gssdProxyPB*) &new;
			
			new.mechtype = old->mechtype;
			new.intoken = CAST_USER_ADDR_T (old->intoken);
			new.intokenLen = old->intokenLen;
			new.uid = old->uid;
			new.svc_namestr = CAST_USER_ADDR_T (old->svc_namestr);
			new.verifier = old->verifier;
			new.context = old->context;
			new.cred_handle = old->cred_handle;
			new.outtoken = CAST_USER_ADDR_T (old->outtoken);
			new.outtokenLen = old->outtokenLen;
			new.major_stat = old->major_stat;
			new.minor_stat = old->minor_stat;
		}
		
		/* DoGSSDProxyCall() Notes:
		1)  It will ALWAYS be passed in a pointer to a user_gssdProxyPB which has user_addr_t instead of pointers.
		2)  It will need to uiomove FROM user space the intoken and the svc_namestr into system space.
		3)  Then, it should call the appropriate gssd calls and get back the results.
		4)  Results should be filled into the user_gssdProxyPB that was passed in except for the outtoken.
		5)  It will need to uiomove TO user space the outtoken
		*/
		//error = DoGSSDProxyCall(pb, p);		/* %%% IMPLEMENT THIS CODE */

		/* Fill out the rest of the return data */
		if (proc_is64bit (p)) {
			/* nothing to do here since we filled out the 64 bit ap->a_data in DoGSSDProxyCall() already */
		} 
		else {
			/* convert to the old 32 user struct to return */
			old->mechtype = new.mechtype;
			old->intokenLen = new.intokenLen;
			old->uid = new.uid;
			old->verifier = new.verifier;
			old->context = new.context;
			old->cred_handle = new.cred_handle;
			old->outtokenLen = new.outtokenLen;
			old->major_stat = new.major_stat;
			old->minor_stat = new.minor_stat;
		}
		
	}
	break;
#endif // USE_SIDEBAND_CHANNEL_RPC

	default:
		error = ENOTSUP;
		goto exit;
    }
    
exit:
	smbnode_unlock(np);
	return (error);
}

/*
 * SMB locks do not map to POSIX.1 advisory locks in several ways:
 * 1 - SMB provides no way to find an existing lock on the server.
 *     So, the F_GETLK operation can only report locks created by processes
 *     on this client. 
 * 2 - SMB locks cannot overlap an existing locked region of a file. So,
 *     F_SETLK/F_SETLKW operations that establish locks cannot extend an
 *     existing lock.
 * 3 - When unlocking a SMB locked region, the region to unlock must correspond
 *     exactly to an existing locked region. So, F_SETLK F_UNLCK operations
 *     cannot split an existing lock or unlock more than was locked (this is
 *     especially important because whne files are closed, we receive a request
 *     to unlock the entire file: l_whence and l_start point to the beginning
 *     of the file, and l_len is zero).
 *
 * The result... SMB cannot support POSIX.1 advisory locks. It can however
 * support BSD flock() locks, so that's what this implementation will allow. 
 *
 * Since we now support open deny modes we will only support flocks on files that
 * have no files open w
 *
 *		vnode_t a_vp;
 *		caddr_t  a_id;
 *		int  a_op;
 *		struct flock *a_fl;
 *		int  a_flags;
 *		vfs_context_t a_context;
 */
static int32_t smbfs_vnop_advlock(struct vnop_advlock_args *ap)
{
	int		flags = ap->a_flags;
	vnode_t vp = ap->a_vp;
	struct smbmount *smp = VTOSMBFS(vp);
	struct smb_vc *vcp = SSTOVC(smp->sm_share);
	int operation = ap->a_op;
	struct smbnode *np;
	int error = 0;
	u_int32_t timo;
	struct flock *fl = ap->a_fl;
	off_t start = 0;
	u_int64_t len = -1;
	u_int32_t lck_pid;

	/* Only regular files can have locks */
	if ( !vnode_isreg(vp))
		return (EISDIR);

	if ((flags & F_POSIX) && ((vcp->vc_sopt.sv_unix_caps & CIFS_UNIX_FCNTL_LOCKS_CAP) == 0))
		return(err_advlock(ap));
		
	if ((flags & (F_FLOCK | F_POSIX)) == 0) {
		SMBWARNING("Lock flag we do not understand %x\n", flags);
		return(err_advlock(ap));
	}

	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);

	/* If we got here it must be a flock. Remember flocks lock the whole file. */
	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_advlock;

	/* 
	 * This vnode has a file open with open deny modes, so the file is really 
	 * already locked. Remember that vn_open and vn_close will also call us here 
	 * so to make them work for now, return no err. If the opened it for 
	 * Open Deny then no one else should be allowed to use it. We could check
	 * the pid here, but the open call should have handled that for us.
	 */
	if (np->f_openDenyList) {
		error = 0;
		goto exit;
	}
	
	/* Before trying the lock see if the file needs to be reopened */
	error = smbfs_smb_reopen_file(np, ap->a_context);
	if (error) {
		SMBDEBUG(" %s waiting to be revoked\n", np->n_name);
		goto exit;
	}
	
	/* 
	 * So if we got to this point we have a normal flock happening. We can have
	 * the following flags passed to us.
	 *
	 *	LOCK_SH - shared lock
	 *	LOCK_EX - exclusive lock
	 *	LOCK_NB - don't block when locking
	 *	LOCK_UN - unlock
	 *
	 * Currently we always allow the server handle blocking. We may want to 
	 * re-look at this later. What if we have a lock that is blocked and
	 * the server goes down how long to we wait.
	 *
	 * The locking mechanism allows two types of locks: shared locks and
	 * exclusive locks.  At any time multiple shared locks may be applied to a
	 * file, but at no time are multiple exclusive, or both shared and exclusive,
	 * locks allowed simultaneously on a file.
	 *
	 * A shared lock may be upgraded to an exclusive lock, and vice versa, sim-
	 * ply by specifying the appropriate lock type; this results in the previous
	 * lock being released and the new lock applied (possibly after other processes
	 * have gained and released the lock).
	 *
	 * We currently treat LOCK_EX and LOCK_SH the same except we do not allow
	 * you to have more that one LOCK_EX.
	*/
	timo = (flags & F_WAIT) ? -1 : 0;
	/* The problem here is that the lock pid must match the SMB Header PID. Some
	 * day it would be nice to pass a better value here. But for now always
	 * use the same value.
	 */
	lck_pid = 1;
	/* Remember that we are always using the share open file at this point */
	switch(operation) {
	case F_SETLK:
		if (! np->f_smbflock) {
			error = smbfs_smb_lock(np, SMB_LOCK_EXCL, np->f_fid, lck_pid, start, len, ap->a_context, timo);
			if (error)
				goto exit;
			MALLOC(np->f_smbflock, struct smbfs_flock *, sizeof *np->f_smbflock, M_LOCKF, M_WAITOK);
			np->f_smbflock->refcnt = 1;
			np->f_smbflock->fl_type = fl->l_type;	
			np->f_smbflock->lck_pid = lck_pid;
			np->f_smbflock->start = start;
			np->f_smbflock->len = len;
			np->f_smbflock->flck_pid = proc_pid(vfs_context_proc(ap->a_context));
		} else if (np->f_smbflock->flck_pid == (u_int32_t)proc_pid(vfs_context_proc(ap->a_context))) {
			/* First see if this is a upgrade or downgrade */
			if ((np->f_smbflock->refcnt == 1) && (np->f_smbflock->fl_type != fl->l_type)) {
					np->f_smbflock->fl_type = fl->l_type;
					goto exit;
			}
			/* Trying to mismatch two different style of locks with the same process id bad! */
			if (np->f_smbflock->fl_type != fl->l_type) {
				error = ENOTSUP;
				goto exit;
			}
			/* 
			 * We know they have the same lock style from above, but they are 
			 * asking for two exclusive. So from Terry comments it looks like
			 * this is ok.  Here's the issue: because what they are doing is 
			 * upgrading an exclusive lock to an exclusive lock in the process 
			 * that holds the previous lock, there are _not_ multiple locks 
			 * involved; there's only the first lock and the lock that replaces 
			 * it.
			 */
			if (np->f_smbflock->fl_type != F_WRLCK) {
				/* 
				 * At no time are multiple exclusive locks allowed simultaneously
				 * on a file. So we can have only one refcnt. This is an upgrade
				 * not another lock.
				 */
			} else
				np->f_smbflock->refcnt++;
		} else {
			/*
			 * Radar 5572840
			 * F_WAIT is set we should sleep until the other flock
			 * gets free then to an upgrade or down grade. Not support
			 * with SMB yet.
			 */
			error = EWOULDBLOCK;
			goto exit;
		}
		break;
	case F_UNLCK:
		error = 0;
		if (! np->f_smbflock)	/* Got an  unlock and had no lock ignore */
			break;
		np->f_smbflock->refcnt--;
		/* remove the lock on the network and  */
		if (np->f_smbflock->refcnt <= 0) {
			error = smbfs_smb_lock(np, SMB_LOCK_RELEASE, np->f_fid, lck_pid, start, len, ap->a_context, timo);
			if (error == 0) {
				FREE(np->f_smbflock, M_LOCKF);
				np->f_smbflock = NULL;
			}
		}
		break;
	default:
		error = EINVAL;
		break;
	}
exit:
	smbnode_unlock(np);
	return (error);
}

static int
smbfs_pathcheck(struct smb_share *ssp, const char *name, size_t nmlen, uint32_t nameiop)
{
	struct smb_vc *vcp = SSTOVC(ssp);
	const char *cp, *endp;
	int error;

	/*
	 * We need to check the file name length. We now use ss_maxfilenamelen 
	 * since that gives us a more accurate max file name length. If the 
	 * server supports UNICODE we should do more checking. Since UTF8 can
	 * have three bytes per character just checking the length is not enough.
	 * We should convert it to UTF16 and see if the length is twice 
	 * ss_maxfilenamelen.
	 *
	 */
	if ((u_int32_t)nmlen > ssp->ss_maxfilenamelen) {
		if (SMB_UNICODE_STRINGS(vcp)) {
			u_int16_t *convbuf;
			size_t ntwrk_len;
			/*
			 * smb_strtouni needs an output buffer that is twice 
			 * as large as the input buffer (name).
			 */
			convbuf= malloc(nmlen * 2, M_SMBNODENAME, M_WAITOK);
			if (! convbuf)
				return ENAMETOOLONG;
			ntwrk_len = smb_strtouni(convbuf, name,  nmlen, 
						UTF_PRECOMPOSED|UTF_NO_NULL_TERM);
			free(convbuf, M_SMBNODENAME);
			if (ntwrk_len > (ssp->ss_maxfilenamelen * 2))
				return ENAMETOOLONG;
		}
		else 
			return ENAMETOOLONG;
	}
	else if (! nmlen)
		return ENAMETOOLONG;
	
	/* Check name only if CREATE, DELETE, or RENAME */
	if (nameiop == LOOKUP)
		return (0);

	/* 
	 * Winodws systems do not allow items that begin with "con" to be created. 
	 * If this is not a UNIX server then stop the user from trying
	 * to create this file or folder. When trying to create a "con" folder or 
	 * "con.xxx" file a windows system will report the following error:
	 * Cannot create or replace file: The filename you specified is too long.
	 * Specify a different filename.
	 *
	 * From my testing any name that matches "con" or begins with "con."
	 * should not be create.
	 *
	 * Should we be like windows and return ENAMETOOLONG or EACCES
	 */
	if ((! UNIX_SERVER(SSTOVC(ssp))) && CON_FILENAME(name, nmlen)) {
		if ((nmlen == 3) || ((nmlen > 3) && (*(name+3) == '.')))
			return (ENAMETOOLONG); 
	}
	
	/* If the server supports UNICODE then we are done checking the name. */
	if (SMB_UNICODE_STRINGS(vcp))
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
	 
	/* check for illegal characters, if the server does not support UNICODE */
	for (cp = name, endp = name + nmlen; cp < endp; ++cp) {
		/*
		 * The set of illegal characters in long names is the same as
		 * 8.3 except the characters 0x20, 0x2b, 0x2c, 0x3b, 0x3d, 0x5b,
		 * and 0x5d are now legal, and the restrictions on periods was
		 * removed.
		 */
		switch (*cp) {
			case 0x20:	/* space */
			case 0x2B:	/* +     */
			case 0x2C:	/* ,     */
			case 0x3B:	/* ;     */
			case 0x3D:	/* =     */
			case 0x5B:	/* [     */
			case 0x5D:	/* ]     */
				break;
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
	return (0);
}

/*
 * Things go even weird without fixed inode numbers...
 */
static int smbfs_vnop_lookup(struct vnop_lookup_args *ap)
/*	struct vnop_lookup_args {
		struct vnodeop_desc *a_desc;
		vnode_t a_dvp;
		vnode_t *a_vpp;
		struct componentname *a_cnp;
		vfs_context_t a_context;
	} *ap; */
{
	struct componentname *cnp = ap->a_cnp;
	vnode_t dvp = ap->a_dvp;
	vnode_t *vpp = ap->a_vpp;
	vnode_t vp;
	struct smbmount *smp;
	struct mount *mp = vnode_mount(dvp);
	struct smbnode *dnp = NULL;
	struct smbfattr fattr, *fap = NULL;
	const char *name = cnp->cn_nameptr;
	uint32_t flags = cnp->cn_flags;
	uint32_t nameiop = cnp->cn_nameiop;
	size_t nmlen = cnp->cn_namelen;
	int wantparent, error, islastcn, isdot = FALSE;
	int parent_locked = FALSE;

	smp = VFSTOSMBFS(mp);
	
	/* 
	 * We may want to move smbfs_pathcheck here, but we really should never
	 * find a bad name in the name cache lookup.
	 */	
	if (!vnode_isdir(dvp))
		return (ENOTDIR);
	if ((flags & ISDOTDOT) && vnode_isvroot(dvp)) {
		SMBFSERR("invalid '..'\n");
		return (EIO);
	}
	islastcn = (flags & ISLASTCN) ? TRUE : FALSE;
	if (islastcn && vfs_isrdonly(mp) && nameiop != LOOKUP)
		return (EROFS);
	wantparent = (flags & (LOCKPARENT|WANTPARENT)) ? TRUE : FALSE;

	/*
	 * We need to make sure the negative name cache gets updated if 
	 * needed. So if the parents cache has expired, then update the
	 * the parent's cache. This will cause the negative name cache to
	 * be flush if the parent's modify time has changed.
	 */
	if (smbnode_lock(VTOSMB(dvp), SMBFS_EXCLUSIVE_LOCK) == 0) {
		VTOSMB(dvp)->n_lastvop = smbfs_vnop_lookup;
		if (VTOSMB(dvp)->n_flag & NNEGNCENTRIES)
			error =  smbfs_update_cache(dvp, NULL, ap->a_context);
		smbnode_unlock(VTOSMB(dvp));	/* Release the smbnode lock */
	}
	
	*vpp = NULLVP;
	error = cache_lookup(dvp, vpp, cnp);
	switch (error) {
		case ENOENT:	/* negative cache entry */
			goto skipLookup;
		case 0:		/* cache miss */
			break;
		case -1:	/* cache hit */
			/*
			 * On CREATE we can't trust a cache hit as if it is stale
			 * and the object doesn't exist on the server returning zero
			 * here would cause the vfs layer to, for instance, EEXIST
			 * the mkdir.
			 */
			if (nameiop != CREATE) {
				error = 0;
				/* Check to see it the node's meta cache needs to be update */
				if (smbnode_lock(VTOSMB(*vpp), SMBFS_EXCLUSIVE_LOCK) == 0) {
					VTOSMB(*vpp)->n_lastvop = smbfs_vnop_lookup;
					error =  smbfs_update_cache(*vpp, NULL, ap->a_context);
					smbnode_unlock(VTOSMB(*vpp));	/* Release the smbnode lock */
				}
				/* At this point we only care if it exist or not so any other error gets ignored */
				if (error != ENOENT)
					return 0;
				/* The item we had, no longer exists so fall through and see if it exist as a different item */
			}
			if (*vpp) {
				cache_purge(*vpp);
				vnode_put(*vpp);
				*vpp = NULLVP;
			}
			break;
		default:	/* unknown & unexpected! */
			SMBWARNING("cache_enter error=%d\n", error);
			return (error);
	}
	/* 
	 * entry is not in the name cache
	 *
	 * validate syntax of name.  ENAMETOOLONG makes it clear the name
	 * is the problem
	 */
	error = smbfs_pathcheck(smp->sm_share, cnp->cn_nameptr, cnp->cn_namelen, nameiop);
	if (error) {
		SMBWARNING("warning: bad filename %s\n", name);
		return (ENAMETOOLONG);
	}
	dnp = VTOSMB(dvp);

	/* lock the parent while we go look for the item on server */
	if (smbnode_lock(dnp, SMBFS_EXCLUSIVE_LOCK) != 0) {
		error = ENOENT;
		goto skipLookup;
	}
	parent_locked = TRUE;
	dnp->n_lastvop = smbfs_vnop_lookup;

	isdot = (nmlen == 1 && name[0] == '.');
	error = 0;
	fap = &fattr;
	/* 
	 * This can allocate a new "name" do not return before the end of the
	 * routine from here on.
	 */
	if (flags & ISDOTDOT)
		error = smbfs_smb_lookup(dnp->n_parent, NULL, NULL, fap, ap->a_context);
	else
		error = smbfs_smb_lookup(dnp, &name, &nmlen, fap, ap->a_context);

	/* 
	 * We use to unlock the parent here, but we really need it locked
	 * until after we do the smbfs_nget calls.
	 */
	/*
	 * We didn't find it and this is not a CREATE or RENAME operation so
	 * add it to the negative name cache.
	 */
	if ((error == ENOENT) && (cnp->cn_flags & MAKEENTRY) && 
		(!(((nameiop == CREATE) || (nameiop == RENAME)) && islastcn))) {
		/* add a negative entry in the name cache */
		cache_enter(dvp, NULL, cnp);
		dnp->n_flag |= NNEGNCENTRIES;
	}
	
skipLookup:
	if (error) {
		/*
		 * note the EJUSTRETURN code in lookup()
		 */
		if (((nameiop == CREATE) || (nameiop == RENAME)) && (error == ENOENT) && islastcn)
			error = EJUSTRETURN;
	} else if ((nameiop == RENAME) && islastcn && wantparent) {
		if (isdot) {
			error = EISDIR;
		} else {
			error = smbfs_nget(mp, dvp, name, nmlen, fap, &vp, 0, ap->a_context);
			if (!error) {
				smbnode_unlock(VTOSMB(vp));	/* Release the smbnode lock */
				*vpp = vp;
			}
		}
	} else if ((nameiop == DELETE) && islastcn) {
		if (isdot) {
			error = vnode_get(dvp);
			if (!error)
				*vpp = dvp;
		} else {
			error = smbfs_nget(mp, dvp, name, nmlen, fap, &vp, 0, ap->a_context);
			if (!error) {
				smbnode_unlock(VTOSMB(vp));	/* Release the smbnode lock */
				*vpp = vp;
			}
		}
	} else if (flags & ISDOTDOT) {
		if (dvp && VTOSMB(dvp)->n_parent) {
			vp = VTOSMB(dvp)->n_parent->n_vnode;
			error = vnode_get(vp);
			if (!error)
				*vpp = vp;
		}
	} else if (isdot) {
		error = vnode_get(dvp);
		if (!error)
			*vpp = dvp;
	} else {
		error = smbfs_nget(mp, dvp, name, nmlen, fap, &vp, cnp->cn_flags, ap->a_context);
		if (!error) {
			smbnode_unlock(VTOSMB(vp));	/* Release the smbnode lock */
			*vpp = vp;
		}
	}
	if (name != cnp->cn_nameptr)
		smbfs_name_free((u_char *)name);
	/* If the parent node is still lock then unlock it here. */
	if (parent_locked && dnp)
		smbnode_unlock(dnp);
		
	return (error);
}


/* offtoblk converts a file offset to a logical block number */
static int smbfs_vnop_offtoblk(struct vnop_offtoblk_args *ap)
/*	struct vnop_offtoblk_args {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		off_t a_offset;
		daddr64_t *a_lblkno;
		vfs_context_t a_context;
	} *ap; */
{
	*ap->a_lblkno = ap->a_offset / PAGE_SIZE_64;
	return (0);
}


/* blktooff converts a logical block number to a file offset */
static int smbfs_vnop_blktooff(struct vnop_blktooff_args *ap)
/*	struct vnop_blktooff_args {   
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		daddr64_t a_lblkno;
		off_t *a_offset;
		vfs_context_t a_context;
	} *ap; */
{	
	*ap->a_offset = (off_t)ap->a_lblkno * PAGE_SIZE_64;
	return (0);
}

/*
 * smbfs_vnop_pagein
 *
 *  vnode_t 	a_vp,
 *  upl_t		a_pl,
 *  vm_offset_t	a_pl_offset,
 *  off_t		a_f_offset, 
 *  size_t		a_size,
 *  int			a_flags
 *  vfs_context_t a_context;
 *
 * NOTE: We no longer take a node lock in this routine.
 */
static int smbfs_vnop_pagein(struct vnop_pagein_args *ap)
{       
	vnode_t vp = ap->a_vp;
	upl_t pl = ap->a_pl;
	size_t size = ap->a_size;
	off_t f_offset = ap->a_f_offset;
	vm_offset_t pl_offset = ap->a_pl_offset;
	vm_offset_t ioaddr;
	int error, flags = ap->a_flags; 
	struct smbnode *np;
	struct smbmount *smp;
	uio_t uio;
	kern_return_t   kret;
	u_int16_t fid = 0;

	np = VTOSMB(vp);
	smp = VTOSMBFS(vp);
	
	if ((size <= 0) || (f_offset < 0) || (f_offset >= (off_t)np->n_size) || 
		(f_offset & PAGE_MASK_64) || (size & PAGE_MASK)) {
		error = EINVAL;
		goto exit;
	}
	
	/* Before trying the read see if the file needs to be reopened */
	error = smbfs_smb_reopen_file(np, ap->a_context);
	if (error) {
		SMBDEBUG(" %s waiting to be revoked\n", np->n_name);
		goto exit;
	}
	kret = ubc_upl_map(pl, &ioaddr);
	if (kret != KERN_SUCCESS)
		panic("smbfs_vnop_pagein: ubc_upl_map %d!", kret);

	uio = uio_create(1, f_offset, UIO_SYSSPACE, UIO_READ);
	/* Stop at the EOF */
	if ((f_offset + (ssize_t)size) > (off_t)np->n_size) {
		size -= PAGE_SIZE;
		size += (size_t)(np->n_size & PAGE_MASK_64);
	}
	uio_addiov(uio, CAST_USER_ADDR_T(ioaddr + pl_offset), size);
	/*
	 * See if any of the open deny mode entries have been mmapped. Remember that
	 * the context we are passed in here may or may not be the same used in open.
	 * We need to use the mapped fid here. If we have no mapped fid in the file 
	 * reference list then we need to use the shared posix open fid. We can't just
	 * use any fid here, because our node isn't locked and someone could close
	 * it in the middle of us using it. So we should always use the mmapped fid.
	 */
	if (smbfs_findMappedFileRef(vp, NULL, &fid) == FALSE) {
		/* No matches or no pid to match, so just use the generic shared fork */
		fid = np->f_fid;	/* We should always have something at this point */
	}
	DBG_ASSERT(fid);
	error = smb_read(smp->sm_share, fid, uio, ap->a_context);
	/* We got an error, did it happen on a reconnect */
	if (error && ((error = smbfs_io_reopen(vp, uio, kAccessRead, &fid, error, ap->a_context)) == 0))
		error = smb_read(smp->sm_share, fid, uio, ap->a_context);
	if (!error && uio_resid(uio))
		error = EFAULT;
	uio_free(uio);
	if (!error && size != ap->a_size)
		bzero((caddr_t)(ioaddr + pl_offset) + size, ap->a_size - size);
	kret = ubc_upl_unmap(pl);
	if (kret != KERN_SUCCESS)
		panic("smbfs_vnop_pagein: ubc_upl_unmap %d", kret);
exit:;
	if (error)
		SMBERROR("%s read error=%d\n", np->n_name, error);
	if ((flags & UPL_NOCOMMIT) != UPL_NOCOMMIT) {		
		/* %%% Once they do the header clean up changes this code needs to be changed */
		DBG_ASSERT(pl_offset <= 0x0ffffffff);
		DBG_ASSERT(ap->a_size <= 0x0ffffffff);
		if (error)
			(void)ubc_upl_abort_range(pl, (upl_offset_t)pl_offset, (upl_size_t)ap->a_size, 
									  UPL_ABORT_ERROR | UPL_ABORT_FREE_ON_EMPTY);
		else
			(void)ubc_upl_commit_range(pl, (upl_offset_t)pl_offset, (upl_size_t)ap->a_size, 
									   UPL_COMMIT_CLEAR_DIRTY | UPL_COMMIT_FREE_ON_EMPTY);
	}
	return (error);
}

/*
 * smbfs_vnop_pageout
 *
 *  vnode_t 	a_vp,
 *  upl_t		a_pl,
 *  vm_offset_t	a_pl_offset,
 *  off_t		a_f_offset, 
 *  size_t		a_size,
 *  int			a_flags
 *  vfs_context_t a_context;
 *
 * NOTE: We no longer take a node lock in this routine.
 *
 */
static int smbfs_vnop_pageout(struct vnop_pageout_args *ap) 
{       
	vnode_t vp = ap->a_vp;
	upl_t pl = ap->a_pl;
	size_t size = ap->a_size;
	off_t f_offset = ap->a_f_offset;
	vm_offset_t pl_offset = ap->a_pl_offset;
	vm_offset_t ioaddr;
	int error, flags = ap->a_flags; 
	struct smbnode *np;
	struct smbmount *smp;
	uio_t uio;
	kern_return_t   kret;
	u_int16_t fid = 0;
	vnode_t parent_vp = NULL;	/* Always null unless this is a stream node */

	if (vnode_vfsisrdonly(vp))
		return(EROFS);
	
	np = VTOSMB(vp);
	smp = VTOSMBFS(vp);
	
	if (pl == (upl_t)NULL)
		panic("smbfs_vnop_pageout: no upl");

	if ((size <= 0) || (f_offset < 0) || (f_offset >= (off_t)np->n_size) ||
	    (f_offset & PAGE_MASK_64) || (size & PAGE_MASK)) {
		error = EINVAL;
		goto exit;
	}
	
	/* Before trying the write see if the file needs to be reopened */
	error = smbfs_smb_reopen_file(np, ap->a_context);
	if (error) {
		SMBDEBUG(" %s waiting to be revoked\n", np->n_name);
		goto exit;
	}

	kret = ubc_upl_map(pl, &ioaddr);
	if (kret != KERN_SUCCESS)
		panic("smbfs_vnop_pageout: ubc_upl_map %d!", kret);

	uio = uio_create(1, f_offset, UIO_SYSSPACE, UIO_WRITE);
	/* Stop at the EOF */
	if ((u_quad_t)(f_offset + size) > np->n_size) {
		size -= PAGE_SIZE;
		size += (size_t)(np->n_size & PAGE_MASK_64);
	}
	uio_addiov(uio, CAST_USER_ADDR_T(ioaddr + pl_offset), size);
	/*
	 * See if any of the open deny mode entries have been mmapped. Remember that
	 * the context we are passed in here may or may not be the same used in open.
	 * We need to use the mapped fid here. If we have no mapped fid in the file 
	 * reference list then we need to use the shared posix open fid. We can't just
	 * use any fid here, because our node isn't locked and someone could close
	 * it in the middle of us using it. So we should always use the mmapped fid.
	 */
	if (smbfs_findMappedFileRef(vp, NULL, &fid) == FALSE) {
		/* No matches or no pid to match, so just use the generic shared fork */
		fid = np->f_fid;	/* We should always have something at this point */
	}
	DBG_ASSERT(fid);
	error = smb_write(smp->sm_share, fid, uio, ap->a_context, SMBWRTTIMO);
	/* We got an error, did it happen on a reconnect */
	if (error && ((error = smbfs_io_reopen(vp, uio, kAccessWrite, &fid, error, ap->a_context)) == 0)) {
		uio_reset(uio, f_offset, UIO_SYSSPACE, UIO_WRITE);
		uio_addiov(uio, CAST_USER_ADDR_T(ioaddr + pl_offset), size);
		error = smb_write(smp->sm_share, fid, uio, ap->a_context, SMBWRTTIMO);
	}
	np->n_flag |= (NFLUSHWIRE | NATTRCHANGED);
	
	uio_free(uio);

	kret = ubc_upl_unmap(pl);
	if (kret != KERN_SUCCESS)
		panic("smbfs_vnop_pageout: ubc_upl_unmap %d", kret);
exit:
		
	/* 
	 * Tell the stream's parent that something has changed. In this case we do nothing in 
	 * smb_update_rsrc_and_getparent that requires the node lock. So if this is a resource stream we need to
	 * update the resource fork size cache timer. We can do that here because it has its own lock.
	 */
	if (vnode_isnamedstream(vp))
		parent_vp = smb_update_rsrc_and_getparent(vp, FALSE);
	
	if (error)
		SMBERROR("%s write error=%d\n", np->n_name, error);
	else /* if success, blow away statfs cache */
		smp->sm_statfstime = 0;

	if ((flags & UPL_NOCOMMIT) != UPL_NOCOMMIT) {
		/* %%% Once they clean up the header this code needs to be changed */
		DBG_ASSERT(pl_offset <= 0x0ffffffff);
		DBG_ASSERT(ap->a_size <= 0x0ffffffff);
		if (error)
			(void)ubc_upl_abort_range(pl, (upl_offset_t)pl_offset, (upl_size_t)ap->a_size, 
									  UPL_ABORT_DUMP_PAGES | UPL_ABORT_FREE_ON_EMPTY);
		else
			(void)ubc_upl_commit_range(pl, (upl_offset_t)pl_offset, (upl_size_t)ap->a_size, 
									   UPL_COMMIT_CLEAR_DIRTY | UPL_COMMIT_FREE_ON_EMPTY);
	}
	/* We have the parent vnode, so reset its meta data cache timer. This routine will release the parent vnode */
	if (parent_vp)
		smb_clear_parent_cache_timer(parent_vp);

	return (error);
}

/*
 * GOALS:
 *
 * We now have a method for turning streams support on and off. This same method will
 * turn on and off extended attribute support.
 *
 * 1. If the share has a file on the top level called "com.apple.smb.streams.on" and the
 * sever supports stream then we will do streams.
 *
 * 2. If the share has a file on the top level called "com.apple.smb.streams.off" we will turn 
 * off streams support.
 *
 * 3. Some server application needs to allow the OS X Server admin to turn steams on or off for all clients?
 * This option will be overwritten by 1 and 2.
 *
 * 4. The ~/Library/Preferences/nsmb.conf or /etc/nsmb.conf files will have an option streams="yes". 
 * This option defaults always to be off in Leopard. This option will be overwritten by 1, 2, or 3.
 *
 * %%%
 * 1. How should we handle extended attributes (Performance issues)? 
 *
 */
static u_int32_t emptyfinfo[8] = {0};

/*
 * DefaultFillAfpInfo
 *
 * Given a buffer fill in the default AfpInfo values.
 */
static void DefaultFillAfpInfo(u_int8_t *afpinfo)
{
	int ii = 0;
	bzero(afpinfo, AFP_INFO_SIZE);
		/* Signature is a DWORD. Must be *(PDWORDD)"AFP" */
	afpinfo[ii++] = 'A';
	afpinfo[ii++] = 'F';
	afpinfo[ii++] = 'P';
	afpinfo[ii++] = 0;
		/* Version is a DWORD. Must be 0x00010000 (byte swapped) */
	afpinfo[ii++] = 0;
	afpinfo[ii++] = 0;
	afpinfo[ii++] = 0x01;
	afpinfo[ii++] = 0;
		/* Reserved1 is a DWORD */
	ii += 4;
		/* Backup time is a DWORD. Backup time for the file/dir. Not set equals 0x80010000 (byte swapped) */
	afpinfo[ii++] = 0;
	afpinfo[ii++] = 0;
	afpinfo[ii++] = 0;
	afpinfo[ii++] = 0x80;
		/* Finder Info is 32 bytes. Calling process fills this in */
	ii += 32;
		/* ProDos Info is 6 bytes. Leave set to zero? */
	ii += 6;
		/* Reserved2 is 6 bytes */
	ii += 6;
}

/* 
 * xattr2sfm
 *
 * See if this xattr is really the resource fork or the finder info stream. If so
 * return the correct streams name otherwise just return the name passed to us.
 */
static const char * xattr2sfm(const char *xa, enum stream_types *stype)
{
	/* Never let them use the SFM Stream Names */
	if (!bcmp(xa, SFM_RESOURCEFORK_NAME, sizeof(SFM_RESOURCEFORK_NAME))) {
		return(NULL);
	}
	if (!bcmp(xa, SFM_FINDERINFO_NAME, sizeof(SFM_FINDERINFO_NAME))) {
		return(NULL);
	}
	if (!bcmp(xa, SFM_DESKTOP_NAME, sizeof(SFM_DESKTOP_NAME))) {
		return(NULL);
	}
	if (!bcmp(xa, SFM_IDINDEX_NAME, sizeof(SFM_IDINDEX_NAME))) {
		return(NULL);
	}
	
	if (!bcmp(xa, XATTR_RESOURCEFORK_NAME, sizeof(XATTR_RESOURCEFORK_NAME))) {
		*stype = kResourceFrk;
		return (SFM_RESOURCEFORK_NAME);
	}
	if (!bcmp(xa, XATTR_FINDERINFO_NAME, sizeof(XATTR_FINDERINFO_NAME))) {
		*stype = kFinderInfo;
		return (SFM_FINDERINFO_NAME);
	}
	*stype = kExtendedAttr;
	return (xa);
}

/*
 * smbfs_vnop_setxattr
 *
 *	vnode_t a_vp;
 *	int8_t * a_name;
 *	uio_t a_uio;
 *	int32_t a_options;
 *	vfs_context_t a_context;
 */
static int smbfs_vnop_setxattr(struct vnop_setxattr_args *ap)
{
	vnode_t vp = ap->a_vp;
	const char *sfmname;
	int error = 0;
	u_int16_t fid = 0;
	u_int32_t rights = SA_RIGHT_FILE_WRITE_DATA;
	struct smbnode *np = NULL;
	struct smb_share *ssp = NULL;
	enum stream_types stype = kNoStream;
	u_int32_t	open_disp = 0;
	uio_t		afp_uio = NULL;
	u_int8_t	afpinfo[60];
	struct smbfattr fattr;
	
	DBG_ASSERT(!vnode_isnamedstream(vp));
	
	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);
	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_setxattr;
	ssp = VTOSMBFS(vp)->sm_share;

	/*
	 * FILE_NAMED_STREAMS tells us the server supports streams. 
	 * NOTE: This flag may have been overwriten by us in smbfs_mount. The default
	 * is for streams to be turn off. See the smbfs_mount for more details.
	 */ 
	if (!(ssp->ss_attributes & FILE_NAMED_STREAMS)) {
		error = ENOTSUP;
		goto exit;
	}

	/* You cant have both of these set at the same time. */
	if ( (ap->a_options & XATTR_CREATE) && (ap->a_options & XATTR_REPLACE) ) {
		error = EINVAL;	
		goto exit;
	}

	sfmname = xattr2sfm(ap->a_name, &stype);
	if (!sfmname) {
		error = EINVAL;	
		goto exit;		
	}
	
	/* 
	 * Need to add write attributes if we want to reset the modify time. We never do this
	 * for the resource fork. The file manager expects the modify time to change if the 
	 * resource fork changes.
	 */
	if ((stype & kResourceFrk) != kResourceFrk)
		rights |= SA_RIGHT_FILE_WRITE_ATTRIBUTES;

	/* 
	 * We treat finder info differently than any other EA/Stream. Because of SFM we need to do things
	 * a little different. Remember the AFPInfo stream has more information in it than just the finder 
	 * info. WARNING: SFM can get very confused if you do not handle this correctly!   
	 */
	if (stype & kFinderInfo) {
		u_int8_t finfo[FINDERINFOSIZE];
		time_t attrtimeo;
		struct timespec ts;
		size_t sizep;
		int len = (int)uio_resid(ap->a_uio);

		/* Can't be larger that 32 bytes */
		if (len > FINDERINFOSIZE) {
			error = EINVAL;
			goto exit;
		}
		error = uiomove((void *)finfo, len, ap->a_uio);
		if (error)
			goto exit;
		SMB_CACHE_TIME(ts, np, attrtimeo);
		/* 
		 * The Finder Info cache hasn't expired so check to see if they are setting
		 * it to something different or the same. If the same then skip setting it,
		 * this is what AFP does.
		 */
		if ((ts.tv_sec - np->finfo_cache) <= attrtimeo) {
			if (bcmp(np->finfo, finfo, sizeof(finfo)) == 0)
				goto exit;				
		}
		/* We want to read also in this case.  */
		rights |= SA_RIGHT_FILE_READ_DATA;
		/* Create a dummy uio structure */
		afp_uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
		if (afp_uio) 
			error = uio_addiov( afp_uio, CAST_USER_ADDR_T(afpinfo), sizeof(afpinfo));
		else 
			error = ENOMEM;
		if (error)
			goto exit;
		/* Now set the default afp info buffer */
		DefaultFillAfpInfo(afpinfo);	
		/* Open and read the data in, if an empty file we will still get an fid */
		error = smbfs_smb_openread(np, &fid, rights, afp_uio, &sizep, sfmname, &ts, ap->a_context);
		/* Replace the finder info with the data that was passed down. */
		if (!error) {
			bcopy((void *)finfo, (void *)&afpinfo[AFP_INFO_FINDER_OFFSET], len);
			uio_reset(afp_uio, 0, UIO_SYSSPACE, UIO_WRITE );
			error = uio_addiov( afp_uio, CAST_USER_ADDR_T(afpinfo), sizeof(afpinfo));			
		}
		if (error)
			goto out;
		/* Truncate the stream if there is anything in it, this will wake up SFM */
		if (sizep && (ssp->ss_flags & SMBS_SFM_VOLUME))	
			(void)smbfs_smb_seteof(ssp, fid, 0, ap->a_context); /* Ignore any errors, write will catch them */
		/* Now we can write the afp info back out with the new finder information */
		if (!error)
			error = smb_write(ssp, fid, afp_uio, ap->a_context, SMBWRTTIMO);
		/* 
		 * Try to set the modify time back to the original time, ignore any errors. Since we are using
		 * the open stream file descriptor to change the time remove the directory attribute 
		 * bit if set.
		 */
		(void)smbfs_smb_setfattrNT(np, (np->n_dosattr & ~SMB_FA_DIR), fid, NULL, &ts, NULL, NULL, ap->a_context);
		/* Reset our cache timer and copy the new data into our cache */
		if (!error) {
			nanouptime(&ts);
			np->finfo_cache = ts.tv_sec;
			bcopy((void *)&afpinfo[AFP_INFO_FINDER_OFFSET], np->finfo, sizeof(np->finfo));
		}
		goto out;
	}
	
	switch(ap->a_options & (XATTR_CREATE | XATTR_REPLACE)) {
		case XATTR_CREATE:	/* set the value, fail if attr already exists */
				/* if exists fail else create it */
			open_disp = NTCREATEX_DISP_CREATE;
			break;
		case XATTR_REPLACE:	/* set the value, fail if attr does not exist */
				/* if exists overwrite item else fail */
			open_disp = NTCREATEX_DISP_OVERWRITE;
			break;
		default:		/* if exists open it else create it */
			open_disp = NTCREATEX_DISP_OPEN_IF;
			break;
	}
	/* Open/create the stream */
	error = smbfs_smb_create(np, sfmname, strnlen(sfmname, ssp->ss_maxfilenamelen+1), 
							 rights, ap->a_context, &fid, open_disp, 1, &fattr);
	if (error)
		goto exit;
	/* Now write out the stream data */
	error = smb_write(ssp, fid, ap->a_uio, ap->a_context, SMBWRTTIMO);
	/* 
	 * %%% 
	 * Should we reset the modify time back to the original time? Never for the resource 
	 * fork, but what about EAs? Could be a performance issue, really need a clearer message from
	 * the rest of the file system team.
	 *
	 * For now try to set the modify time back to the original time, ignore any errors. Since we are using
	 * the open stream file descriptor to change the time remove the directory attribute 
	 * bit if set.	 
	 */
	if ((stype & kResourceFrk) != kResourceFrk)
		(void)smbfs_smb_setfattrNT(np, (np->n_dosattr & ~SMB_FA_DIR), fid, NULL, &np->n_mtime, NULL, NULL, ap->a_context);
	
out:
	if (fid)
		(void)smbfs_smb_close(ssp, fid, ap->a_context);
	
exit:
	if (afp_uio)
		uio_free(afp_uio);
		
	if (error == ENOENT)
		error = ENOATTR;

	/* Check to see if its a normal error */
	if (error && (error != ENOTSUP) && (error != ENOATTR)) {
		SMBWARNING("error %d %s:%s\n", error, np->n_name, ap->a_name);
		/* Always make sure its a legit error, see man listxattr */
		if ((error != EROFS) && (error != EPERM) && (error != EINVAL) && 
			(error != ENOTDIR) && (error != EACCES) && (error != ELOOP) && 
			(error != EFAULT) && (error != EIO) && (error != ENAMETOOLONG) &&
			(error != EEXIST) && (error != ERANGE) && (error != E2BIG) && (error != ENOSPC))
			error = EIO;	/* Not sure what else to do here */
	}
	
	smbnode_unlock(np);
	return (error);
}

/*
 * smbfs_vnop_listxattr
 *
 *	vnode_t a_vp;
 *	uio_t a_uio;
 *	size_t *a_size;
 *	int32_t a_options;
 *	vfs_context_t a_context;
 */
static int smbfs_vnop_listxattr(struct vnop_listxattr_args *ap)
{
	vnode_t vp = ap->a_vp;
	uio_t uio = ap->a_uio;
	size_t *sizep = ap->a_size;
	struct smbnode *np = NULL;
	struct smb_share *ssp = NULL;
	int error = 0;

	DBG_ASSERT(!vnode_isnamedstream(vp));
	
	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);
	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_listxattr;
	ssp = VTOSMBFS(vp)->sm_share;

	/*
	 * FILE_NAMED_STREAMS tells us the server supports streams. 
	 * NOTE: This flag may have been overwriten by us in smbfs_mount. The default
	 * is for streams to be turn off. See the smbfs_mount for more details.
	 */ 
	if (!(ssp->ss_attributes & FILE_NAMED_STREAMS)) {
		error = ENOTSUP;
		goto exit;
	}

	error = smbfs_smb_qstreaminfo(np, ap->a_context, uio, sizep, NULL, NULL);
	/*
	 * From the man pages: If no accessible extended attributes are associated 
	 * with the given path or fd, the function returns zero.
	*/
	if (error == ENOATTR)
		error = 0;
	
exit:
	/* Check to see if its a normal error */
	if (error && (error != ENOTSUP)) {
		SMBWARNING("error %d %s\n", error, np->n_name);
		/* Always make sure its a legit error, see man listxattr */
		if ((error != ERANGE) && (error != EPERM) && (error != EINVAL) && 
			(error != ENOTDIR) && (error != EACCES) && (error != ELOOP) && 
			(error != EFAULT) && (error != EIO))
			error = 0;	/* Just pretend it doesn't exist */
	}

	smbnode_unlock(np);
	return (error);
}

/*
 * vnop_removexattr_args
 *
 *	vnode_t a_vp;
 *	int8_t * a_name;
 *	int32_t a_options;
 *	vfs_context_t a_context;
 */
static int smbfs_vnop_removexattr(struct vnop_removexattr_args *ap)
{
	vnode_t vp = ap->a_vp;
	const char *sfmname;
	int error = 0;
	struct smbnode *np = NULL;
	struct smb_share *ssp = NULL;
	enum stream_types stype = kNoStream;
	uio_t afp_uio = NULL;
	u_int16_t	fid = 0;
	u_int8_t	afpinfo[60];
	struct timespec ts;

	DBG_ASSERT(!vnode_isnamedstream(vp));
	
	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);
	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_removexattr;
	ssp = VTOSMBFS(vp)->sm_share;

	/*
	 * FILE_NAMED_STREAMS tells us the server supports streams. 
	 * NOTE: This flag may have been overwriten by us in smbfs_mount. The default
	 * is for streams to be turn off. See the smbfs_mount for more details.
	 */ 
	if (!(ssp->ss_attributes & FILE_NAMED_STREAMS)) {
		error = ENOTSUP;
		goto exit;
	}

	sfmname = xattr2sfm(ap->a_name, &stype);
	if (!sfmname) {
		error = EINVAL;	
		goto exit;		
	}
	
	/* 
	 * We do not allow them to remove the finder info stream on SFM Volume. It could hold other
	 * information used by SFM. We just zero out the finder info data. If the volume is
	 * just a normal NTFS Volume then deleting the stream is ok.
	 */
	if ((stype & kFinderInfo) && (ssp->ss_flags & SMBS_SFM_VOLUME)) {
		u_int32_t	rights = SA_RIGHT_FILE_WRITE_DATA | SA_RIGHT_FILE_READ_DATA | SA_RIGHT_FILE_WRITE_ATTRIBUTES;	
		
		afp_uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
		if (afp_uio) 
			error = uio_addiov( afp_uio, CAST_USER_ADDR_T(afpinfo), sizeof(afpinfo));
		else error = ENOMEM;
		
		if (error)
			goto exit;
		
		/* open and read the data */
		error = smbfs_smb_openread(np, &fid, rights, afp_uio, NULL, sfmname, &ts, ap->a_context);
		/* clear out the finder info data */
		bzero(&afpinfo[AFP_INFO_FINDER_OFFSET], FINDERINFOSIZE);
		
		if (!error)	/* truncate the stream, this will wake up SFM */
			error = smbfs_smb_seteof(ssp, fid, 0, ap->a_context); 
		/* Reset our uio */
		if (!error) {
			uio_reset(afp_uio, 0, UIO_SYSSPACE, UIO_WRITE );
			error = uio_addiov( afp_uio, CAST_USER_ADDR_T(afpinfo), sizeof(afpinfo));
		}	
		if (!error)
			error = smb_write(ssp, fid, afp_uio,ap->a_context, SMBWRTTIMO);
		/* Try to set the modify time back to the original time, ignore any errors */
		(void)smbfs_smb_setfattrNT(np, np->n_dosattr, fid, NULL, &ts, NULL, NULL, ap->a_context);
	}
	else {
		error = smbfs_smb_delete(np, ap->a_context, sfmname, strnlen(sfmname, ssp->ss_maxfilenamelen+1), 1);
	}

	/* If Finder info then reset our cache timer and zero out our cached finder info */
	if ((stype & kFinderInfo) && !error) {
		nanouptime(&ts);
		np->finfo_cache = ts.tv_sec;
		bzero(np->finfo, sizeof(np->finfo));
	}
	
exit:
	if (afp_uio)
		uio_free(afp_uio);
	
	if (fid)
		(void)smbfs_smb_close(ssp, fid, ap->a_context);
	
	if (error == ENOENT)
		error = ENOATTR;
	
	/* Check to see if its a normal error */
	if (error && (error != ENOTSUP) && (error != ENOATTR)) {
		SMBWARNING("error %d %s:%s\n", error, np->n_name, ap->a_name);
		/* Always make sure its a legit error, see man listxattr */
		if ((error != EROFS) && (error != EPERM) && (error != EINVAL) && 
			(error != ENOTDIR) && (error != EACCES) && (error != ELOOP) && 
			(error != EFAULT) && (error != EIO) && (error != ENAMETOOLONG))
			error = ENOATTR;	/* Not sure what else to do here */
	}
	
	smbnode_unlock(np);
	return (error);
}

/*
 * smbfs_vnop_getxattr
 *
 *	vnode_t a_vp;
 *	int8_t * a_name;
 *	uio_t a_uio;
 *	size_t *a_size;
 *	int32_t a_options;
 *	vfs_context_t a_context;
 */
static int smbfs_vnop_getxattr(struct vnop_getxattr_args *ap)
{
	vnode_t vp = ap->a_vp;
	const char *sfmname;
	uio_t uio = ap->a_uio;
	size_t *sizep = ap->a_size;
	u_int16_t fid = 0;
	int error = 0;
	struct smbnode *np = NULL;
	struct smb_share *ssp = NULL;
	size_t rq_resid = (uio) ? (size_t)uio_resid(uio) : 0;
	uio_t afp_uio = NULL;
	enum stream_types stype = kNoStream;
	struct timespec ts;
	time_t attrtimeo;

	DBG_ASSERT(!vnode_isnamedstream(vp));
	
	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);
	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_getxattr;
	ssp = VTOSMBFS(vp)->sm_share;

	/*
	 * FILE_NAMED_STREAMS tells us the server supports streams. 
	 * NOTE: This flag may have been overwriten by us in smbfs_mount. The default
	 *	 is for streams to be turn off. See the smbfs_mount for more details.
	 */ 
	if (!(ssp->ss_attributes & FILE_NAMED_STREAMS)) {
		error = ENOTSUP;
		goto exit;
	}

	sfmname = xattr2sfm(ap->a_name, &stype);
	if (!sfmname) {
		error = EINVAL;	
		goto exit;		
	}
	
	/* 
	 * They just want the size of the stream. We will handle the finder info down below. */
	if ((uio == NULL) && !(stype & kFinderInfo)) {
		u_int64_t strmsize = 0;
		
		if (stype & kResourceFrk) {
			error = smb_get_rsrcfrk_size(vp, ap->a_context);
			lck_mtx_lock(&np->rfrkMetaLock);
			strmsize = np->rfrk_size; /* The node's resource fork size will have the correct value at this point */
			lck_mtx_unlock(&np->rfrkMetaLock);
		} else
			error = smbfs_smb_qstreaminfo(np, ap->a_context, NULL, NULL, sfmname, &strmsize);

		if (sizep)
			*sizep = (size_t)strmsize;
		if (error)
			error = ENOATTR;
		goto exit;
	}
	
	/* 
	 * We treat finder info differently than any other EA/Stream. Because of SFM we need to do things
	 * a little different. Remember the AFPInfo stream has more information in it than just the finder 
	 * info. WARNING: SFM can get very confused if you do not handle this correctly!   
	 */
	if (stype & kFinderInfo) {
		SMB_CACHE_TIME(ts, np, attrtimeo);
		/* Cache has expired go get the finder information. */
		if ((ts.tv_sec - np->finfo_cache) > attrtimeo) {
			size_t afpsize = 0;
			u_int8_t	afpinfo[60];
			
			afp_uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
			if (afp_uio) 
				error = uio_addiov( afp_uio, CAST_USER_ADDR_T(afpinfo), sizeof(afpinfo));
			else error = ENOMEM;
			
			if (error)
				goto exit;
			
			uio_setoffset(afp_uio, 0);
			/* open and read the data */
			error = smbfs_smb_openread(np, &fid, SA_RIGHT_FILE_READ_DATA, afp_uio, &afpsize, sfmname, NULL, ap->a_context);
			/* Should never happen but just in case */
			if (afpsize != AFP_INFO_SIZE)
				error = ENOENT;

			if (error == ENOENT)
				bzero(np->finfo, sizeof(np->finfo));
			else bcopy((void *)&afpinfo[AFP_INFO_FINDER_OFFSET], np->finfo, sizeof(np->finfo));
			
			if (vnode_isreg(vp) && (bcmp(np->finfo, "brokMACS", 8) == 0)) {
				np->finfo_cache = 0;
				SMBDEBUG("Don't cache finder info, we have a finder copy in progress\n");
			} else {
				nanouptime(&ts);
				np->finfo_cache = ts.tv_sec;		
			}
		}
		/* If the finder info is all zero hide it, except if its a SFM volume */ 
		if ((!(ssp->ss_flags & SMBS_SFM_VOLUME)) && (bcmp(np->finfo, emptyfinfo, sizeof(emptyfinfo)) == 0))
				error = ENOENT;

		if (uio && !error)
			error = uiomove((const char *)np->finfo, (int)sizeof(np->finfo), ap->a_uio);
		if (sizep && !error) 
			*sizep = FINDERINFOSIZE; 
	}
	else 
		error = smbfs_smb_openread(np, &fid, SA_RIGHT_FILE_READ_DATA, uio, sizep, sfmname, NULL, ap->a_context);
		
	 /* If ENOTSUP support is returned then do the open and read in two transactions.  */
	if (error != ENOTSUP)
		goto out;
		
	/* 
	 * May need to add an oplock to this open call, if this is a finder info open.
	 * Not sure I remember the exact details, something about deletes.
	 */
	error = smbfs_smb_open_xattr(np, SA_RIGHT_FILE_READ_DATA, NTCREATEX_SHARE_ACCESS_ALL, 
								 ap->a_context, &fid, sfmname, sizep);
	if (error)
		goto exit;
		
	/*
	 * When reading finder-info, munge the uio so we read at offset 16 where the actual 
	 * finder info is located. Also ensure we don't read past the 32 bytes, of finder
	 * info. Since we are just reading we really don't care about the rest of the data.
	 *
	 * This is only here in case a server does not support the chain message above. We
	 * do not cache in this case. Should never happen, but just to be safe.
	 */
	if (stype & kFinderInfo) {
		user_ssize_t r;

		if (sizep)
			*sizep = FINDERINFOSIZE;
		/* Just wanted the size get out */
		if (uio == NULL)
			goto out;

		r = uio_resid(uio);
		if (uio_offset(uio) >= FINDERINFOSIZE) {
			uio_setresid(uio, 0);
		} else if (uio_offset(uio) + r > FINDERINFOSIZE)
		uio_setresid(uio, FINDERINFOSIZE - uio_offset(uio));
		r = r - uio_resid(uio);
		uio_setoffset(uio, uio_offset(uio) + 4*4);
		
		error = smb_read(ssp, fid, uio, ap->a_context);
		
		uio_setoffset(uio, uio_offset(uio) - 4*4);
		uio_setresid(uio, uio_resid(uio) + r);
	}
	else error = smb_read(ssp, fid, uio, ap->a_context);

out:;
	if (uio && sizep && (*sizep > rq_resid))
			error = ERANGE;
		
	if (fid)	/* Even an error can leave the file open. */
		(void)smbfs_smb_close(ssp, fid, ap->a_context);
exit:
	/* 
	 * So ENOENT just means ENOATTR. 
	 * Note: SAMBA 4 will reutrn EISDIR for folders which is legit, but not 
	 * expected by the finder 
	 */
	if ((error == ENOENT) || ((error == EISDIR) && (stype & kFinderInfo)))
		error = ENOATTR;
	
	if (afp_uio)
		uio_free(afp_uio);

	/* Check to see if its a normal error */
	if (error && (error != ENOTSUP) && (error != ENOATTR)) {
		SMBWARNING("error %d %s:%s\n", error, np->n_name, ap->a_name);
		/* Nope make sure its a legit error, see man getxattr */
		if ((error != ERANGE) && (error != EPERM) && (error != EINVAL) && 
			(error != EISDIR) && (error != ENOTDIR) && (error != EACCES) &&
			(error != ELOOP) && (error != EFAULT) && (error != EIO))
			error = ENOATTR;		
	}
	
	smbnode_unlock(np);
	return (error);
}

/*
 * smbfs_vnop_getnamedstream - Obtain the vnode for a stream.
 *
 *	vnode_t a_vp;
 *	vnode_t *a_svpp;
 *	const char *a_name;
	enum nsoperation a_operation; (NS_OPEN, NS_CREATE, NS_DELETE)
 *	vfs_context_t a_context;
 *
 */
static int smbfs_vnop_getnamedstream(struct vnop_getnamedstream_args* ap)
{
	struct smbmount *smp;
	vnode_t vp = ap->a_vp;
	vnode_t *svpp = ap->a_svpp;
	const char * streamname = ap->a_name;
	const char * sname = ap->a_name;
	struct smbnode *np = NULL;
	int error = 0;
	u_int64_t strmsize = 0;
	struct smbfattr fattr;
	struct vnode_attr vap;
	struct timespec ts;
	time_t attrtimeo;
	struct timespec reqtime;

	/* Lock the parent while we look for the stream */
	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);
	
	nanouptime(&reqtime);
	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_getnamedstream;
	smp = VTOSMBFS(vp);
	
	*svpp = NULL;
	/* Currently we only support the "com.apple.ResourceFork" stream. */
	if (bcmp(streamname, XATTR_RESOURCEFORK_NAME, sizeof(XATTR_RESOURCEFORK_NAME)) != 0) {
		SMBDEBUG("Wrong stream %s:$%s\n", np->n_name, streamname);
		error = ENOATTR;
		goto exit;
	} else
	    sname = SFM_RESOURCEFORK_NAME;	/* This is the resource stream use the SFM name */

	if ( !vnode_isreg(vp) ) {
		SMBDEBUG("%s not a file (EPERM)\n",  np->n_name);
		error = EPERM;
		goto exit;
	}

	/*
	 * %%%
	 * Since we have the parent node update its meta cache. Remember that smbfs_getattr
	 * will check to see if the cache has expired. May want to look at this and see 
	 * how it affects performance.
	 */
	VATTR_INIT(&vap);	/* Really don't care about the vap */
	error = smbfs_getattr(vp, &vap, ap->a_context);
	if (error) {
		SMBERROR("%s lookup failed %d\n", np->n_name, error);		
		goto exit;
	}

	/*
	 * If we already have the stream vnode in our hash table and its cache timer
	 * has not expired then just return we are done.
	 */
	if ((*svpp = smbfs_find_vgetstrm(smp, np, sname)) != NULL) {		
		VTOSMB(*svpp)->n_mtime = np->n_mtime;	/* update the modify time */
		SMB_CACHE_TIME(ts, VTOSMB(*svpp), attrtimeo);
		if ((ts.tv_sec - VTOSMB(*svpp)->attribute_cache_timer) <= attrtimeo)
			goto exit;			/* The cache is up to date, we are done */
	}
	
	/*
	 * Lookup the stream and get its size. This call will fail if the server tells us the stream
	 * does not exist. 
	 *
	 * NOTE1: If this is the resource stream then making this call will update the the data fork 
	 * node's resource size and its resource cache timer. 
	 *
	 * NOTE2: SAMBA will not return the resource stream if the size is zero. 
	 *
	 * NOTE3: We always try to create the stream on an open. Because of note two.
	 *
	 * If smbfs_smb_qstreaminfo returns an error and we do not have the stream node in our hash table then it doesn't 
	 * exist and they will have to create it.
	 *
	 * If smbfs_smb_qstreaminfo returns an error and we do  have the stream node in our hash table then it could exist 
	 * so just pretend that it does for now. If they try to open it and it doesn't exist the open will create it.
	 *
	 * If smbfs_smb_qstreaminfo returns no error and we do have the stream node in our hash table then just update its
	 * size and cache timers.
	 *
	 * If smbfs_smb_qstreaminfo returns no error and we do not have the stream node in our hash table then create the 
	 * stream node, using the data node to fill in all information except the size.
	 */
	if ((smbfs_smb_qstreaminfo(np, ap->a_context, NULL, NULL, sname, &strmsize)) && (*svpp == NULL)) {
		error = ENOATTR;
		goto exit;		
	}
	/*
	 * We already have the stream vnode. If it doesn't exist we will attempt to create it on
	 * the open. In the SMB open you can say create it if it does not exist. Reset the size
	 * if the above called failed then set the size to zero.
	 */
	if (*svpp) {		
		if (smbfs_update_size(VTOSMB(*svpp), &reqtime, strmsize) == TRUE) {
			/* Remember the only attribute for a stream is its size */
			nanouptime(&ts);
			VTOSMB(*svpp)->attribute_cache_timer = ts.tv_sec;			
		}
		goto exit;	/* We have everything we need, so we are done */
	}
	
	
	bzero(&fattr, sizeof(fattr));
	fattr.fa_vtype = VREG;		/* Streams are always regular files */
	fattr.fa_size = strmsize;	/* Fill in the stream size */
	fattr.fa_data_alloc = 0;	/* %%% not sure this really matters */	
	/* Now for the rest of the information we just use the data node information */
	fattr.fa_attr = np->n_dosattr;
	fattr.fa_atime = np->n_atime;	/* Access Time */
	fattr.fa_chtime = np->n_chtime;	/* Change Time */
	fattr.fa_mtime = np->n_mtime;	/* Modify Time */
	fattr.fa_crtime = np->n_crtime;	/* Create Time */
	nanouptime(&fattr.fa_reqtime);
	error = smbfs_vgetstrm(smp, vp, svpp, &fattr, sname);

exit:;
	if (*svpp)
		smbnode_unlock(VTOSMB(*svpp));	/* We are done with the smbnode unlock it. */

	if (error && (error != ENOATTR))
		SMBWARNING(" %s:$%s error = %d\n", np->n_name, streamname, error);
	smbnode_unlock(np);
	
	return (error);
}

/*
 * smbfs_vnop_makenamedstream - Create a stream.
 *
 *	vnode_t a_vp;
 *	vnode_t *a_svpp;
 *	const char *a_name;
 *	vfs_context_t a_context;
 */
static int smbfs_vnop_makenamedstream(struct vnop_makenamedstream_args* ap)
{
	struct smbmount *smp;
	vnode_t vp = ap->a_vp;
	vnode_t *svpp = ap->a_svpp;
	const char * streamname = ap->a_name;
	struct smbnode *np = NULL;
	int error = 0;
	struct smbfattr fattr;
	u_int16_t fid;
	struct timespec ts;
	int rsrcfrk = FALSE;
	size_t max_name_len;
	
	/* Lock the parent while we create the stream */
	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);
	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_makenamedstream;
	smp = VTOSMBFS(vp);
	
	*svpp = NULL;
	/* Currently we only support the "com.apple.ResourceFork" stream. */
	if (bcmp(streamname, XATTR_RESOURCEFORK_NAME, sizeof(XATTR_RESOURCEFORK_NAME)) != 0) {
		SMBDEBUG("Wrong stream %s:$%s\n", np->n_name, streamname);
		max_name_len = 0; /* strnlen(streamname, ssp->ss_maxfilenamelen+1) */
		error = ENOATTR;
		goto exit;
	} else {
		max_name_len = sizeof(XATTR_RESOURCEFORK_NAME);
	    streamname = SFM_RESOURCEFORK_NAME;	/* This is the resource stream use the SFM name */		
	}
	
	if ( !vnode_isreg(vp) ) {
		SMBDEBUG("%s not a file (EPERM)\n",  np->n_name);
		error = EPERM;
		goto exit;
	}
	
	/* Now create the stream */
	error = smbfs_smb_create(np, streamname, max_name_len, SA_RIGHT_FILE_WRITE_DATA, 
							 ap->a_context, &fid, NTCREATEX_DISP_OPEN_IF, 1, &fattr);
	if (error)
		goto exit;
	(void)smbfs_smb_close(smp->sm_share, fid, ap->a_context);
	
	error = smbfs_vgetstrm(smp, vp, svpp, &fattr, streamname);
	if (error == 0) {
		if (rsrcfrk) /* Update the data nodes resource size */ {
			lck_mtx_lock(&np->rfrkMetaLock);
			np->rfrk_size = fattr.fa_size;
			nanouptime(&ts);
			np->rfrk_cache_timer = ts.tv_sec;			
			lck_mtx_unlock(&np->rfrkMetaLock);
		}
		smbnode_unlock(VTOSMB(*svpp));	/* Done with the smbnode unlock it. */		
	}
	
exit:;
	if (error)
		SMBWARNING(" %s:$%s error = %d\n", np->n_name, streamname, error);
	smbnode_unlock(np);
	
	return (error);
}

/*
 * smbfs_vnop_removenamedstream - Remove a stream.
 *
 *	vnode_t a_vp;
 *	vnode_t a_svpp;
 *	const char *a_name;
 *	vfs_context_t a_context;
 */
static int smbfs_vnop_removenamedstream(struct vnop_removenamedstream_args* ap)
{
	vnode_t vp = ap->a_vp;
	vnode_t svp = ap->a_svp;
	const char * streamname = ap->a_name;
	struct smbnode *np = NULL;
	int error = 0;
	size_t max_name_len;
		
	
	/* Lock the parent and stream while we delete the stream*/
	if ((error = smbnode_lockpair(VTOSMB(vp), VTOSMB(svp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);
	np = VTOSMB(svp);
	np->n_lastvop = smbfs_vnop_removenamedstream;

	/* Currently we only support the "com.apple.ResourceFork" stream. */
	if (bcmp(streamname, XATTR_RESOURCEFORK_NAME, sizeof(XATTR_RESOURCEFORK_NAME)) != 0) {
		SMBDEBUG("Wrong stream %s:$%s\n", np->n_name, streamname);
		max_name_len = 0; /* strnlen(streamname, ssp->ss_maxfilenamelen+1) */
		error = ENOATTR;
		goto exit;
	} else {
		max_name_len = sizeof(XATTR_RESOURCEFORK_NAME);
	    streamname = SFM_RESOURCEFORK_NAME;	/* This is the resource stream use the SFM name */
	}
	
	if ( !vnode_isreg(vp) ) {
		SMBDEBUG("%s not a file (EPERM)\n",  np->n_name);
		error = EPERM;
		goto exit;
	}
	
	error = smbfs_smb_delete(np, ap->a_context, streamname, max_name_len, TRUE);
	if (!error) 
		smb_vhashrem(np);
exit:;	
	if (error)
		SMBWARNING(" %s:$%s error = %d\n", np->n_name, streamname, error);
	smbnode_unlockpair(VTOSMB(vp), VTOSMB(svp));

	return (error);
}

/*
 * smbfs_vnop_monitor - Monitor an item.
 *
 *	vnode_t a_vp;
 *  uint32_t a_unused_events;	- not used currently
 *  uint32_t a_flags;
 *				VNODE_MONITOR_BEGIN - setup notfication
 *				VNODE_MONITOR_END	- remove notfication
 *				VNODE_MONITOR_UPDATE	- change 
 *	void *a_handle;
 *				struct knote *
 *  vfs_context_t a_context;
 *
 */
static int smbfs_vnop_monitor(struct vnop_monitor_args *ap)
{
	struct smbnode *np;
	int error = 0;
	int releaseLock = TRUE;
	
	/* Currently we only support directories */
	if (! vnode_isdir(ap->a_vp))	{
		SMBDEBUG("%s is not a directory (ENOTSUP): node type = 0x%0x a_events = 0x%x, a_flags = 0x%x, a_handle = %p\n",
				 VTOSMB(ap->a_vp)->n_name, vnode_vtype(ap->a_vp), ap->a_events, ap->a_flags, ap->a_handle);
		return ENOTSUP;
	} 
	
	if ((error = smbnode_lock(VTOSMB(ap->a_vp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);
	
	np = VTOSMB(ap->a_vp);
	np->n_lastvop = smbfs_vnop_monitor;
	SMBDEBUG("%s a_events = 0x%x, a_flags = 0x%x, a_handle = %p\n", np->n_name, ap->a_events, ap->a_flags, ap->a_handle);
	
	switch (ap->a_flags) {
		case VNODE_MONITOR_BEGIN:
			error = smbfs_start_change_notify(np, ap->a_context, &releaseLock);
			break;
		case VNODE_MONITOR_END:
			error = smbfs_stop_change_notify(np, FALSE, ap->a_context, &releaseLock);
			break;
		case VNODE_MONITOR_UPDATE: /* We no longer get called to update */
		default:
			error = ENOTSUP;
			break;
	}
	if (releaseLock)
		smbnode_unlock(VTOSMB(ap->a_vp));
	return error;
}

vnop_t **smbfs_vnodeop_p;
static struct vnodeopv_entry_desc smbfs_vnodeop_entries[] = {
	{ &vnop_default_desc,		(vnop_t *) vn_default_error },
	{ &vnop_advlock_desc,		(vnop_t *) smbfs_vnop_advlock },
	{ &vnop_close_desc,			(vnop_t *) smbfs_vnop_close },
	{ &vnop_create_desc,		(vnop_t *) smbfs_vnop_create },
	{ &vnop_fsync_desc,			(vnop_t *) smbfs_vnop_fsync },
	{ &vnop_getattr_desc,		(vnop_t *) smbfs_vnop_getattr },
	{ &vnop_pagein_desc,		(vnop_t *) smbfs_vnop_pagein },
	{ &vnop_inactive_desc,		(vnop_t *) smbfs_vnop_inactive },
	{ &vnop_ioctl_desc,			(vnop_t *) smbfs_vnop_ioctl },
	{ &vnop_link_desc,			(vnop_t *) smbfs_vnop_link },
	{ &vnop_lookup_desc,		(vnop_t *) smbfs_vnop_lookup },
	{ &vnop_mkdir_desc,			(vnop_t *) smbfs_vnop_mkdir },
	{ &vnop_mknod_desc,			(vnop_t *) smbfs_vnop_mknod },
	{ &vnop_mmap_desc,			(vnop_t *) smbfs_vnop_mmap },
	{ &vnop_mnomap_desc,		(vnop_t *) smbfs_vnop_mnomap },
	{ &vnop_open_desc,			(vnop_t *) smbfs_vnop_open },
	{ &vnop_pathconf_desc,		(vnop_t *) smbfs_vnop_pathconf },
	{ &vnop_pageout_desc,		(vnop_t *) smbfs_vnop_pageout },
	{ &vnop_read_desc,			(vnop_t *) smbfs_vnop_read },
	{ &vnop_readdir_desc,		(vnop_t *) smbfs_vnop_readdir },
	{ &vnop_readlink_desc,		(vnop_t *) smbfs_vnop_readlink },
	{ &vnop_reclaim_desc,		(vnop_t *) smbfs_vnop_reclaim },
	{ &vnop_remove_desc,		(vnop_t *) smbfs_vnop_remove },
	{ &vnop_rename_desc,		(vnop_t *) smbfs_vnop_rename },
	{ &vnop_rmdir_desc,			(vnop_t *) smbfs_vnop_rmdir },
	{ &vnop_setattr_desc,		(vnop_t *) smbfs_vnop_setattr },
	{ &vnop_symlink_desc,		(vnop_t *) smbfs_vnop_symlink },
	{ &vnop_write_desc,			(vnop_t *) smbfs_vnop_write },
	{ &vnop_searchfs_desc,		(vnop_t *) err_searchfs },
	{ &vnop_offtoblk_desc,		(vnop_t *) smbfs_vnop_offtoblk },
	{ &vnop_blktooff_desc,		(vnop_t *) smbfs_vnop_blktooff },
	{ &vnop_getxattr_desc,		(vnop_t *) smbfs_vnop_getxattr },
	{ &vnop_setxattr_desc,		(vnop_t *) smbfs_vnop_setxattr },
	{ &vnop_removexattr_desc,	(vnop_t *) smbfs_vnop_removexattr },
	{ &vnop_listxattr_desc,		(vnop_t *) smbfs_vnop_listxattr },
	{ &vnop_monitor_desc,		(vnop_t *) smbfs_vnop_monitor},
	{ &vnop_getnamedstream_desc, (vnop_t *) smbfs_vnop_getnamedstream },
    { &vnop_makenamedstream_desc, (vnop_t *) smbfs_vnop_makenamedstream },
    { &vnop_removenamedstream_desc, (vnop_t *) smbfs_vnop_removenamedstream },
	{ NULL, NULL }
};

struct vnodeopv_desc smbfs_vnodeop_opv_desc =
	{ &smbfs_vnodeop_p, smbfs_vnodeop_entries };
