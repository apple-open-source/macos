/* Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved */
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * The NEXTSTEP Software License Agreement specifies the terms
 * and conditions for redistribution.
 *
 *	@(#)webdav_vnops.c	8.8 (Berkeley) 1/21/94
 */

/*
 * webdav Filesystem
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/lock.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#include <sys/errno.h>
#include <vfs/vfs_support.h>
#include <mach/mach_types.h>
#include <mach/memory_object_types.h>
#include <mach/message.h>
#include <vm/vm_pageout.h>
#include <sys/uio.h>
#include <sys/ubc.h>

#include "webdav.h"
#include "vnops.h"

/*****************************************************************************/

#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
	#define RET_ERR(str, error) \
	{ \
		if (error) \
			log_vnop_error(str, error); \
		return(error); \
	}
	
	void log_vnop_error(str, error)
	char *str;
	int error;
	{
		printf("vnop %s error = %d\n", str, error);
	}
#else
	#define RET_ERR(str, error) return(error)
#endif

struct close_args {
		int		fd;
};
extern int close(struct proc *p, struct close_args *uap, register_t *retval);

/*****************************************************************************/

static int webdav_connect(so, so2, switch_funnel)
	struct socket *so;
	struct socket *so2;
	int switch_funnel;
{
	/* from unp_connect, bypassing the namei stuff... */
	struct socket *so3;
	struct unpcb *unp2;
	struct unpcb *unp3;
	int error;

	if (so2 == 0)
	{
		return (ECONNREFUSED);
	}

	if (so->so_type != so2->so_type)
	{
		return (EPROTOTYPE);
	}

	if ((so2->so_options & SO_ACCEPTCONN) == 0)
	{
		return (ECONNREFUSED);
	}

	if (switch_funnel)
	{
		thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);
	}

	so3 = sonewconn(so2, 0);
	if (so3 == 0)
	{
		if (switch_funnel)
		{
			thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
		}
		return (ECONNREFUSED);
	}

	unp2 = sotounpcb(so2);
	unp3 = sotounpcb(so3);
	if (unp2->unp_addr)
	{
		MALLOC(unp3->unp_addr, struct sockaddr_un *, sizeof(struct sockaddr_un), M_TEMP, M_WAITOK);
		bcopy(unp2->unp_addr, unp3->unp_addr, sizeof(unp2->unp_addr));

		/* BSD old networking code	
		  unp3->unp_addr = m_copy(unp2->unp_addr, 0, (int)M_COPYALL);
		*/
	}
	so2 = so3;

	error = unp_connect2(so, so2);
	if (switch_funnel)
	{
		thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
	}

	return (error);
}

/*****************************************************************************/

static void webdav_closefd(p, fd)
	struct proc *p;
	int fd;
{
	int error = 0;
	struct close_args ua;
	int rc;

	ua.fd = fd;
	*fdflags(p, fd) &= ~UF_RESERVED;
	error = close(p, &ua, &rc);
	/*
	 * We should never get an error, and there isn't anything
	 * we could do if we got one, so just print a message.
	 */
	if (error)
	{
		printf("webdav_closefd: error = %d\n", error);
	}
}

/*****************************************************************************/

/* Webdav_sendmsg */
/*
 * The WebDAV send messge routine is used to communicate with the
 * WebDAV server process.  The arguments are as follows:
 * vnop - the operation to be peformed (CREATE,DELETE, etc.)  Defined
 *		  constants for this are in vnops.h
 * whattouse - tells the server process how to identify the file upon which
 *			   the operation will be performed.	 There are three possibilities.
 *		   It can used a user supplied argument, the full url (contained in the
 *			   webdavnode) or the file handle (also in the webdav node)
 * pt- pointer to the webdavnode
 * a_pcred - pointer to a webdav credentials structure (contains uid etc.)
 * a_toarg - adderss of an argument (supplied when whattouse indicates it should be used otherwise null)
 * toargsize - size of object pointed to by a_toarg must be 0 if there is no argument
 * a_server_error - address of place to return errors sent by the server process
 * a_arg - address of buffer to hold data returned by server process (optional set to null)
 * argsize - size of buffer pointed to by a_arg (optional, can be set to 0)
 */

int webdav_sendmsg(vnop, whattouse, pt, a_pcred, fmp, p, a_toarg, toargsize, a_server_error,
	a_arg, argsize)
	int vnop;
	int whattouse;
	struct webdavnode *pt;
	struct webdav_cred *a_pcred;
	struct webdavmount *fmp;
	struct proc *p;
	void *a_toarg;
	int toargsize;
	int *a_server_error;
	void *a_arg;
	int argsize;
{
	int error = 0;
	struct socket *so = 0;
	int flags;									/* socket flags */
	struct uio auio;
	struct iovec aiov[3];
	int res, outres;
	struct mbuf *cm = 0;
	int len;
	struct webdav_cred pcred;

	/*
	 * Create a new socket.
	 */
	thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);

	error = socreate(AF_UNIX, &so, SOCK_STREAM, 0);
	if (error)
	{
		goto bad;
	}

	/*
	 * Reserve some buffer space. Make sure it is enought to handle the
	 * arguments that may come in or out.  Also add some slop
	 */

	res = pt->pt_size + sizeof(pcred);			/* XXX */
	if (a_toarg)
	{
		res += toargsize;
	}

	res += 512;									/* XXX slop */
	outres = 512;								/* slop for the out */

	if (a_arg)
	{
		outres += argsize;
	}

	error = soreserve(so, res, outres);
	if (error)
	{
		goto bad;
	}

	/*
	 * Kick off connection
	 */
	error = webdav_connect(so, (struct socket *)fmp->pm_server->f_data, 0);
	if (error)
	{
		goto bad;
	}

	/*
	 * Wait for connection to complete
	 */
	/*
	 * XXX: Since the mount point is holding a reference on the
	 * underlying server socket, it is not easy to find out whether
	 * the server process is still running.	 To handle this problem
	 * we loop waiting for the new socket to be connected (something
	 * which will only happen if the server is still running) or for
	 * the reference count on the server socket to drop to 1, which
	 * will happen if the server dies.	Sleep for 5 second intervals
	 * and keep polling the reference count.   XXX.
	 */

	/* In free bsd we would need to set the intterupt state with
	  s = splnet();
	*/

	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0)
	{
		if (fcount(fmp->pm_server) == 1)
		{
			error = ECONNREFUSED;
			/* In free bsd we would need to set the intterupt state with
			  splx(s);
			*/
			goto bad;
		}
		(void)tsleep((caddr_t) & so->so_timeo, PSOCK, "webdavcon", 5 * hz);
		/* *** check for errors *** */
	}

	/* In free bsd we would need to set the intterupt state with
	  splx(s);
	*/

	if (so->so_error)
	{
		error = so->so_error;
		goto bad;
	}

	/*
	 * Set miscellaneous flags
	 */

	so->so_rcv.sb_timeo = 0;
	so->so_snd.sb_timeo = 0;
	so->so_rcv.sb_flags |= SB_NOINTR;
	so->so_snd.sb_flags |= SB_NOINTR;

	aiov[0].iov_base = (caddr_t) & vnop;
	aiov[0].iov_len = sizeof(vnop);
	aiov[1].iov_base = (caddr_t)a_pcred;
	aiov[1].iov_len = sizeof(*a_pcred);

	switch (whattouse)
	{
		case WEBDAV_USE_URL:
			aiov[2].iov_base = pt->pt_arg;
			aiov[2].iov_len = pt->pt_size;
			break;

		case WEBDAV_USE_HANDLE:
			aiov[2].iov_base = (caddr_t) & pt->pt_file_handle;
			aiov[2].iov_len = sizeof(pt->pt_file_handle);
			break;

		case WEBDAV_USE_INPUT:
			aiov[2].iov_base = a_toarg;
			aiov[2].iov_len = toargsize;
			break;

		default:
			panic("Webdav invalid usepat arg ");
	}

	auio.uio_iov = aiov;
	auio.uio_iovcnt = 3;
	auio.uio_rw = UIO_WRITE;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_procp = p;
	auio.uio_offset = 0;
	auio.uio_resid = aiov[0].iov_len + aiov[1].iov_len + aiov[2].iov_len;

	error = sosend(so, (struct sockaddr *)0, &auio, (struct mbuf *)0, (struct mbuf *)0, 0);
	if (error)
	{
		goto bad;
	}

	if (auio.uio_resid)
	{
		error = 0;
#ifdef notdef
		error = EMSGSIZE;
		goto bad;
#endif
	}

	aiov[0].iov_base = (caddr_t)a_server_error;
	aiov[0].iov_len = sizeof(*a_server_error);
	aiov[1].iov_base = (caddr_t)a_arg;
	aiov[1].iov_len = argsize;
	auio.uio_iov = aiov;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_procp = p;
	auio.uio_offset = 0;
	len = auio.uio_resid = argsize + sizeof(*a_server_error);
	auio.uio_iovcnt = (a_arg == NULL ? 1 : 2);
	flags = MSG_WAITALL;
	error = soreceive(so, (struct sockaddr **)0, &auio, (struct mbuf **)0, &cm, &flags);
	/* fall through */

bad:
	
	if (cm)
	{
		m_freem(cm);
	}

	if (so)
	{
		soshutdown(so, 2);
		soclose(so);
	}

	thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
	return (error);
}

/*****************************************************************************/

int webdav_buildnewvnode(dvp, vpp, cnp, existing_node)
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
	int *existing_node;
{
	char *pname = cnp->cn_nameptr;
	struct webdavnode *pt = NULL,  *dpt = NULL;
	int error = 0;
	struct vnode *fvp = 0;
	char *path;
	int name_size, size;
	struct webdavmount *fmp;
	int depth = 0;
	int parent_depth = 0;
	int has_dotdot = 0;
	int multiple_elements = 0;
	int consumed_chars = 0;
	char *current_pos,  *delete_pos;
	int num_deleted = 0;
	*existing_node = FALSE;

	*vpp = NULLVP;

	if (cnp->cn_namelen == 1 && *pname == '.')
	{
		*vpp = dvp;
		VREF(dvp);
		/*VOP_LOCK(dvp);*/
		*existing_node = TRUE;
		return (0);
	}

	MALLOC(pt, void *, sizeof(struct webdavnode), M_TEMP, M_WAITOK);
	/* Initialize the cache_vnode to prevent people from
	 * thinking something is there when it is not */

	bzero(pt, sizeof(struct webdavnode));
	pt->pt_file_handle = -1;

	if (WEBDAV_CHECK_VNODE(dvp))
	{
		error = EINVAL;
		goto bad;
	}

	fmp = VFSTOWEBDAV(dvp->v_mount);

	/* get size of our new component */

	for (name_size = 0, path = pname; *path; path++)
	{
		name_size++;
	}

	/* Putting the name (partial URI) in the webdav node now
	  depends on the parent.	If it is a WebDAV directory we
	  can just append it's name to the last pathname component
	  and we're done.	 If it isn't then it has to be our mount
	  point and we append the mounted from name. */

	/* Start our dotdot processing.	 We screen the incoming
	  component for ".."  For each one we find we back up our depth
	  variable by two.	 If the depth combined with the depth of
	  the parent ever reaches -1, the component has instructed us
	  to go past our root.	 At that point we consume whatever part
	  of the name we could and return the parent vnode with the
	  part of the pathname requireing us to go back.  If the combined
	  depth does not go negative, we just preserve the depth in the
	  webdav node in case this guy becomes a parent directory. Start
	  with a check to see if we are starting the name with .. */

	if (dvp->v_tag == VT_WEBDAV)
	{
		parent_depth = ((struct webdavnode *)dvp->v_data)->pt_depth;
	}

	path = pname;

	while (*path)
	{
		/* we have a real pathname element so increment depth
		 * from the root by one.
		 */

		++depth;

		if (*path == '.')
		{
			if (path[1] == '/' || path[1] == '\0')
			{
				/* we have found a . entry in the path name.  Since this
				 * is not a real directory but rather a self reference, we need
				 * to back up the depth and consume the character
				 */
				--depth;
				++path;
				++consumed_chars;
			}
			else
			{
				if (path[1] == '.' && (path[2] == '/' || path[2] == '\0'))
				{
					/* We have found a .. entry in our path indicating that
					 * this is our parent.	In these cases, we need to back up
					 * depth by two to both reverse the inrement we did for
					 * this path element and indicate that we need to go back
					 * one more.  If we back up beyond the root, then we won't
					 * consume these characters and instead will return out
					 * to the caller for further processing through directories
					 * that are not part of our mount point
					 */

					depth -= 2;
					path += 2;
					has_dotdot = 1;

					if (depth + parent_depth < 0)
					{
						*vpp = fmp->pm_root;
						VREF(fmp->pm_root);
						cnp->cn_consume = consumed_chars - cnp->cn_namelen;
						/* if we have processed multiple path elements, we need
						  to take into account the slash character for the last
						  path element.  We will have marked it as consumed and we
						  need not to, so back up one */

						if (multiple_elements)
						{
							cnp->cn_consume--;
						}

						/*VOP_LOCK(dvp);*/
						goto returned_parent;
					}
					/* end of if depth went negative */
					else
					{
						consumed_chars += 2;
					}							/* end of if depth went negative */
				}
				/* end of if .. case */
			}									/* end of if else ./ case */
		}
		/* end of if . */
		/* If we are here one of three things is true:
		 * 1. This path did not start with .
		 * 2. We have consumed an isolated . chracter
		 * 3. We have consumed an isolated .. entry
		 * No matter which case it is the next thing to do is the same
		 * We will increment path to the null or just past the next / so that
		 * when we come around again we will either process the next entry or
		 * or be done
		 */

		while (*path && *path != '/')
		{
			++path;
			++consumed_chars;
		}

		while (*path == '/')
		{
			/* increment past all contiguous / characters
			 * so that the next time we come around
			 * we will either just stop at the null or we will
			 * process the next real element */
			++path;
			++consumed_chars;
			multiple_elements = 1;
		}										/* end while path has / chars */
	}											/* end while */



	pt->pt_depth = depth + parent_depth;

	if (depth + parent_depth == 0)
	{
		/* we backed up to the root of the volume so
		 * return the volume root and get out */
		*vpp = fmp->pm_root;
		VREF(fmp->pm_root);
		cnp->cn_consume = name_size - cnp->cn_namelen;
		goto returned_parent;
	}

	if (dvp->v_tag == VT_WEBDAV)
	{
		dpt = (struct webdavnode *)dvp->v_data;
		size = dpt->pt_size + name_size + 1;
		MALLOC(pt->pt_arg, caddr_t, size, M_TEMP, M_WAITOK);
		bcopy(dpt->pt_arg, pt->pt_arg, dpt->pt_size);
		pt->pt_arg[dpt->pt_size - 1] = '/';
		bcopy(pname, (pt->pt_arg + dpt->pt_size), name_size + 1);
		pt->pt_size = size;

	}
	else
	{

		/*Well, I must be looking up the root here. The component name
		  is all we need */

		MALLOC(pt->pt_arg, caddr_t, name_size + 1, M_TEMP, M_WAITOK);
		bcopy(pname, (pt->pt_arg), name_size + 1);
		pt->pt_size = name_size + 1;
		/* add our / byte for the root */
		pt->pt_arg[name_size] = '/';
	}

	/*	Let namei know we have consumed everything by setting consume
	  to the full name */

	cnp->cn_consume = name_size - cnp->cn_namelen;

	/* Now we may have dot of dotdot chacters in our path.	We
	 * are gauranteed not to have so many as to back up
	 * into the root or the parent but we need to get
	 * rid of them to make a valid url */

	/* Now filter out dot dot, dot, and extra slashes from what will
	 * be used as a url.  Note that even if the root url contains
	 * more than one path element, we will not alter it since any
	 * sequence of .. entries that would have takens us past the root
	 * will have caused an early exit from this routine.
	 */

	current_pos = pt->pt_arg;
	num_deleted = 0;

	while (*current_pos)
	{
		if (current_pos + 2 <= pt->pt_arg + pt->pt_size)
		{
			if ((*current_pos == '.') &&
				(((*(current_pos + 1) == '/' ||
					*(current_pos + 1) == '\0') ||
					((current_pos + 3 <= pt->pt_arg + pt->pt_size) &&
						(*(current_pos + 1) == '.') &&
						((*(current_pos + 2) == '/' ||
							*(current_pos + 2) == '\0'))))))
			{

				/* We found . or .. that we need to contract out */
				if (*(current_pos + 1) == '/')
				{
					/* it was . so skip 2 */
					delete_pos = current_pos - 1;
					current_pos += 1;
					num_deleted = 2;
				}
				else
				{
					/* it was .. so back up to the last / */
					delete_pos = current_pos - 2;
					current_pos += 2;
					num_deleted = 4;
					while (*delete_pos != '/')
					{
						--delete_pos;
						++num_deleted;
					}
				}

				bcopy(current_pos, delete_pos, (size_t)(pt->pt_arg + pt->pt_size - current_pos));
				pt->pt_size -= num_deleted;
				current_pos = delete_pos;
			}
		}

		/*	Consume any contiguous / characters and keep moving
		 *	up the path until we are done
		 */

		while (*current_pos && *current_pos != '/')
		{
			++current_pos;
		}

		delete_pos = current_pos;

		if (*current_pos == '/')
		{
			++current_pos;
			delete_pos = current_pos;
			num_deleted = 0;

			while (*current_pos == '/')
			{
				++current_pos;
				++num_deleted;
			}
		}

		if (current_pos != delete_pos)
		{
			/* we have some slashes to eliminate so go for it */
			bcopy(current_pos, delete_pos, (size_t)(pt->pt_arg + pt->pt_size - current_pos));
			pt->pt_size -= num_deleted;
			current_pos = delete_pos;
			num_deleted = 0;
		}
	}											/* end big while */

	/* If this is not the root than mask off any trailing / that
	 * may have come from the name.	 If we find one of those, it
	 * may lead to URL's sent to http servers with extra slashes
	 * in them.	 Experience says that's a problem.
	 */

	if (pt->pt_depth > 0)
	{
		if (pt->pt_arg[pt->pt_size - 2] == '/')
		{
			pt->pt_arg[pt->pt_size - 2] = '\0';
			--pt->pt_size;
		}
	}

	if (WEBDAV_CHECK_VNODE(dvp))
	{
		error = EINVAL;
		goto bad;
	}

	fvp = (webdav_hashget(pt->pt_depth, pt->pt_size, dvp->v_mount->mnt_stat.f_fsid.val[0],
		pt->pt_arg));
	if (fvp)
	{
		/* We found the appropriate vnode and hashlookup has already
		 * upped the ref count for us.	Nothing left to do but return it
		 * get rid of our now temporary pt and let the caller know that
		 * we found an existing node.  
		 */
		*vpp = fvp;
		*existing_node = TRUE;
		if (pt->pt_arg)
		{
			FREE((caddr_t)pt->pt_arg, M_TEMP);
			pt->pt_arg = 0;
		}
		FREE((caddr_t)pt, M_TEMP);
		pt = 0;

		/* FREE() may have blocked, check the state of the vnode */

		if (WEBDAV_CHECK_VNODE(fvp))
		{
			error = EINVAL;
			goto bad;
		}
		else
		{
			return (0);
		}
	}

	if (WEBDAV_CHECK_VNODE(dvp))
	{
		error = EINVAL;
		goto bad;
	}

	lockinit(&pt->pt_lock, PINOD, "webdavnode", 0, 0);

	error = getnewvnode(VT_WEBDAV, dvp->v_mount, webdav_vnodeop_p, &fvp);
	if (error)
	{
		goto bad;
	}
	fvp->v_data = pt;
	*vpp = fvp;

	/*VOP_LOCK(fvp);*/

bad:

	if (error)
	{
		/* release the vnode reference acquired by webdav_hashget() */
		if (fvp)
		{
			if (fvp->v_usecount)				/* XXX should not look at usecount */
			{
				vrele(fvp);
			}
		}

		if (pt && pt->pt_arg)
		{
			FREE((caddr_t)pt->pt_arg, M_TEMP);
			pt->pt_arg = 0;
			FREE((caddr_t)pt, M_TEMP);
			pt = 0;
		}
		*vpp = (struct vnode *)NULL;
	}

	RET_ERR("webdav_buildnewvnode", error);

returned_parent:

	if (pt->pt_arg)
	{
		FREE((caddr_t)pt->pt_arg, M_TEMP);
		pt->pt_arg = 0;
	}
	FREE((caddr_t)pt, M_TEMP);

	*existing_node = TRUE;
	RET_ERR("webdav_buildnewvnode", error);
}

/*****************************************************************************/

/* Temporary workaround to get around simultaneous opens */
static int webdav_lock(ap)
	struct vop_lock_args	/* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
		} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct webdavnode *pt = VTOWEBDAV(vp);
	int retval = 0;

	retval = lockmgr(&pt->pt_lock, ap->a_flags, &vp->v_interlock, ap->a_p);

	return (retval);
}

/*****************************************************************************/

static int webdav_unlock(ap)
	struct vop_lock_args	/* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
		} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct webdavnode *pt = VTOWEBDAV(vp);
	int retval = 0;

	retval = lockmgr(&pt->pt_lock, ap->a_flags | LK_RELEASE, &vp->v_interlock, ap->a_p);

	return (retval);
}

/*****************************************************************************/

static int webdav_temp_lock(vp, p)
	struct vnode *vp;
	struct proc *p;
{
	struct vop_lock_args vla;
	int retval = 0;

	if (vp->v_tag == VT_WEBDAV)
	{
		vla.a_vp = vp;
		vla.a_flags = LK_EXCLUSIVE | LK_RETRY;
		vla.a_p = p;

		retval = webdav_lock(&vla);
	}

	return (retval);
}

/*****************************************************************************/

static int webdav_temp_unlock(vp, p)
	struct vnode *vp;
	struct proc *p;
{
	struct vop_lock_args vla;
	int retval = 0;

	if (vp->v_tag == VT_WEBDAV)
	{
		vla.a_vp = vp;
		vla.a_flags = 0;
		vla.a_p = p;

		retval = webdav_unlock(&vla);
	}

	return (retval);
}

/*****************************************************************************/

/*
 * vp is the current namei directory
 * cnp is the name to locate in that directory...
 */

int webdav_lookup(ap)
	struct vop_lookup_args	/* {
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
		} */ *ap;
{
	struct componentname *cnp = ap->a_cnp;
	struct vnode **vpp = ap->a_vpp;
	struct vnode *dvp = ap->a_dvp;
	int error = 0;
	struct webdavnode *pt;
	int server_error = 0;
	struct vnode *fvp = 0;
	struct proc *p = current_proc();
	struct webdavmount *fmp;
	struct webdav_cred pcred;
	int vnop = WEBDAV_STAT;
	struct vattr vap;
	struct timeval tv;
	int existing_node = 0;

	*vpp = 0;
	bzero(&pcred, sizeof(pcred));

	error = webdav_temp_lock(dvp, p);
	if (error)
	{
		return (EINVAL);
	}

	error = webdav_buildnewvnode(dvp, &fvp, cnp, &existing_node);
	if (error)
	{
		(void)webdav_temp_unlock(dvp, p);
		RET_ERR("webdav_lookup buildnewvnode", error);
	}

	if (existing_node)
	{
		*vpp = fvp;
		(void)webdav_temp_unlock(dvp, p);
		RET_ERR("webdav_lookup existing node", 0);
	}

	/* If we are trying to delete, rename or create and this is a
	  readonly mount, error out now and save the network
	  traffic */

	if ((dvp->v_mount->mnt_flag & MNT_RDONLY) &&
		(cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME || cnp->cn_nameiop == CREATE))
	{
		(void)webdav_temp_unlock(dvp, p);
		RET_ERR("webdav_lookup read only", EROFS);
	}

	pt = VTOWEBDAV(fvp);
	fmp = VFSTOWEBDAV(fvp->v_mount);

	pcred.pcr_flag = 0;
	pcred.pcr_uid = p->p_ucred->cr_uid;
	pcred.pcr_ngroups = p->p_ucred->cr_ngroups;
	bcopy(p->p_ucred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));

	/* Now call up the user level to find out what kind of file
	  this is.	 We need to know if it is a VDIR or a VREG */

	error = webdav_sendmsg(vnop, WEBDAV_USE_URL, pt, &pcred, fmp, p, (void *)NULL,
		0, &server_error, &vap, sizeof(vap));
	if (error)
	{
#ifdef DEBUG
		printf("webdav_lookup: webdav_sendmsg returned error\n");
#endif

		goto bad;
	}
	if (server_error)
	{
		error = server_error;
		if (server_error == ENOENT && (cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME))
		{
			/*
			 * This is a special case.	We looked up the vnode and
			 * didn't find it but we are creating it so don't return
			 * an error, just set vnode to null
			 */
			error = EJUSTRETURN;
			cnp->cn_flags |= SAVENAME;
			*vpp = 0;
		}
#ifdef DEBUG
		if (error != EJUSTRETURN)
		{
			printf("webdav_lookup: server error\n");
		}
#endif

		goto bad;
	}

	/* webdav_sendmsg() may have blocked, check the state of the vnode */

	if (WEBDAV_CHECK_VNODE(fvp))
	{
		error = ENOENT;
#ifdef DEBUG
		printf("webdav_lookup: bad vnode following webdav_sendmsg\n");
#endif

		goto bad;
	}

	if (vap.va_atime.tv_sec == 0)
	{
		microtime(&tv);
		TIMEVAL_TO_TIMESPEC(&tv, &pt->pt_atime);
		pt->pt_mtime = pt->pt_atime;
		pt->pt_ctime = pt->pt_atime;
	}
	else
	{
		pt->pt_atime = vap.va_atime;
		pt->pt_mtime = pt->pt_atime;
		pt->pt_ctime = pt->pt_atime;
	}

	/* fill in the inode number we will have gotten from the server process */

	pt->pt_fileid = vap.va_fileid;

	fvp->v_type = vap.va_type;
	pt->pt_vnode = fvp;

	/* This is a new vnode which so set up the UBC info */

	if ((fvp->v_type == VREG) && (UBCINFOMISSING(fvp) || UBCINFORECLAIMED(fvp)))
	{
		error = ubc_info_init(fvp);
		if (error)
		{
#ifdef DEBUG
			printf("webdav_lookup: error from ubc_info_init\n");
#endif

			goto bad;
		}
	}

	/* ubc_info_init() may have blocked, check the state of the vnode */

	/* This is a new vnode which so set up the UBC info. Note that at
	  this point we have to be able to handle a getattr */
	if (WEBDAV_CHECK_VNODE(fvp))
	{
		error = ENOENT;
#ifdef DEBUG
		printf("webdav_lookup: bad vnode after ubc_info_init\n");
#endif

		goto bad;
	}

	/*
	 * Note that since we want all callers to have their own
	 * up to date copy of a directory so we won't put directories
	 * in the cache
	 */

	if (fvp->v_type == VREG)
	{
		webdav_hashins(pt);
	}

	*vpp = fvp;

	/*VOP_LOCK(fvp);*/

	/* Note, if you add code here, make sure you clean up
	  the ubc stuff (if necessary) on an error case before
	  exiting */

	/* fall through */

bad:

	(void)webdav_temp_unlock(dvp, p);

#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
	if (error && cnp)
	{
		printf("webdav_lookup: name %s\n", cnp->cn_nameptr);
	}
#endif

	if (error && fvp)
	{
		if (fvp->v_usecount)					/* XXX should not look at usecount */
		/* *** if we don't check usecount we would detect refcount
		  error and panic right here rather than later *** */
			vrele(fvp);
	}

	RET_ERR("webdav_lookup", error);
}

/*****************************************************************************/

int webdav_open(ap)
	struct vop_open_args	/* {
		struct vnode *a_vp;
		int	 a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
		} */ *ap;
{
	struct socket *so = 0;
	struct webdavnode *pt;
	struct proc *p = ap->a_p;
	struct vnode *vp = ap->a_vp;
	struct uio auio;
	struct iovec aiov[3];
	int res;
	struct mbuf *cm = 0;
	struct cmsghdr *cmsg;
	int newfds;
	int *ip;
	int fd = 0;
	int error = 0;
	int len;
	struct webdavmount *fmp;
	struct file *fp;
	struct webdav_cred pcred;
	int vnop;
	int flags;									/* socket flags */

	/*
	 * Previously this code:
	  if (p->p_dupfd >= 0)
	  RET_ERR("webdav_open", ENODEV);
	 * was included as part of a handshake in the VFS open layer, 
	 * however it's no longer needed.  In addition, the handshake is
	 * not thread-safe and causes the second WebDAV fd to be dup'ed 
	 * to fd 0 when open's on two threads happen at the same time.
	 */

	pt = VTOWEBDAV(vp);
	fmp = VFSTOWEBDAV(vp->v_mount);

	/* If it is already open then just ref the node
	 * and go on about our business. Make sure to set
	 * the write status if this is read/write open
	 */

	if (vp->v_usecount > 0 && pt->pt_cache_vnode)
	{
		return (0);
	}

	/* Set the vnop type to tell the user process if we
	  are going to open a file or a directory */

	if (vp->v_type == VREG)
	{
		vnop = WEBDAV_FILE_OPEN;
	}
	else
	{
		if (vp->v_type == VDIR)
		{
			vnop = WEBDAV_DIR_OPEN;
		}
		else
		{
			/* This should never happen but just in case */
			error = EFTYPE;
			goto bad;
		}
	}

	/*
	 * Create a new socket.
	 */
	thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);
	error = socreate(AF_UNIX, &so, SOCK_STREAM, 0);
	if (error)
	{
		thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
#ifdef DEBUG
		printf("webdav_open: error creating socket\n");
#endif

		goto bad;
	}

	/*
	 * Reserve some buffer space
	 */
	res = pt->pt_size + sizeof(pcred) + 512;	/* XXX */
	error = soreserve(so, res, res);
	if (error)
	{
		thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
#ifdef DEBUG
		printf("webdav_open: error from soreserve\n");
#endif

		goto bad;
	}

	/*
	 * Kick off connection
	 */
	error = webdav_connect(so, (struct socket *)fmp->pm_server->f_data, 0);
	if (error)
	{
		thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
#ifdef DEBUG
		printf("webdav_open: error connecting to daemon\n");
#endif

		goto bad;
	}

	/*
	 * Wait for connection to complete
	 */
	/*
	 * XXX: Since the mount point is holding a reference on the
	 * underlying server socket, it is not easy to find out whether
	 * the server process is still running.	 To handle this problem
	 * we loop waiting for the new socket to be connected (something
	 * which will only happen if the server is still running) or for
	 * the reference count on the server socket to drop to 1, which
	 * will happen if the server dies.	Sleep for 5 second intervals
	 * and keep polling the reference count.   XXX.
	 */

	/* In free bsd we would need to set the intterupt state with
	  s = splnet();
	*/

	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0)
	{
		if (fcount(fmp->pm_server) == 1)
		{
			error = ECONNREFUSED;
			/* In free bsd we would need to set the intterupt state with
			  splx(s);
			*/
			thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
			goto bad;
		}
		(void)tsleep((caddr_t) & so->so_timeo, PSOCK, "webdavcon", 5 * hz);
	}

	/* In free bsd we would need to set the intterupt state with
	  splx(s);
	*/

	if (so->so_error)
	{
		error = so->so_error;
		thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
#ifdef DEBUG
		printf("webdav_open: so_error %d\n", error);
#endif

		goto bad;
	}

	thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);

	/* We pended so check the state of the vnode */
	/* probably fine to look this under network funnel as vnode is locked */
	if (WEBDAV_CHECK_VNODE(vp))
	{
		error = EPERM;
		goto bad;
	}

	pcred.pcr_flag = ap->a_mode;
	pcred.pcr_uid = ap->a_cred->cr_uid;
	pcred.pcr_ngroups = ap->a_cred->cr_ngroups;
	bcopy(ap->a_cred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));
	aiov[0].iov_base = (caddr_t) & vnop;
	aiov[0].iov_len = sizeof(vnop);
	aiov[1].iov_base = (caddr_t) & pcred;
	aiov[1].iov_len = sizeof(pcred);
	aiov[2].iov_base = pt->pt_arg;
	aiov[2].iov_len = pt->pt_size;
	auio.uio_iov = aiov;
	auio.uio_iovcnt = 3;
	auio.uio_rw = UIO_WRITE;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_procp = p;
	auio.uio_offset = 0;
	auio.uio_resid = aiov[0].iov_len + aiov[1].iov_len + aiov[2].iov_len;

	thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);
	/*
	 * Set miscellaneous flags
	 */
	so->so_rcv.sb_timeo = 0;
	so->so_snd.sb_timeo = 0;
	so->so_rcv.sb_flags |= SB_NOINTR;
	so->so_snd.sb_flags |= SB_NOINTR;



	error = sosend(so, (struct sockaddr *)0, &auio, (struct mbuf *)0, (struct mbuf *)0, 0);
	if (error)
	{
		thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
#ifdef DEBUG
		printf("webdav_open: error from sosend\n");
#endif

		goto bad;
	}

	len = auio.uio_resid = sizeof(int);
	do
	{
		struct mbuf               *m = 0;
		flags = MSG_WAITALL;
		error = soreceive(so, (struct sockaddr **)0, &auio, &m, &cm, &flags);
		if (error)
		{
			thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
#ifdef DEBUG
			printf("webdav_open: error from soreceive\n");
#endif

			goto bad;
		}

		/*
		 * Grab an error code from the mbuf.
		 */
		if (m)
		{
			m = m_pullup(m, sizeof(int));		/* Needed? */
			if (m)
			{
				error = *(mtod(m, int *));
				m_freem(m);
			}
			else
			{
				error = EINVAL;
			}
		}
		else
		{
			if (cm == 0)
			{
				error = ECONNRESET;				/* XXX */
#ifdef notdef
				break;
#endif
			}
		}
	} while (cm == 0 && auio.uio_resid == len && !error);

	if (cm == 0)
	{
		thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
		goto bad;
	}

	if (auio.uio_resid)
	{
		error = 0;
#ifdef notdef
		error = EMSGSIZE;
		thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
		goto bad;
#endif

	}

	/*
	 * XXX: Break apart the control message, and retrieve the
	 * received file descriptor.  Note that more than one descriptor
	 * may have been received, or that the rights chain may have more
	 * than a single mbuf in it.  What to do?
	 */
	cmsg = mtod(cm, struct cmsghdr *);
	newfds = (cmsg->cmsg_len - sizeof(*cmsg)) / sizeof(int);
	if (newfds == 0)
	{
		error = ECONNREFUSED;
		thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
		goto bad;
	}

	thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
	/*
	 * At this point the rights message consists of a control message
	 * header, followed by a data region containing a vector of
	 * integer file descriptors.  The fds were allocated by the action
	 * of receiving the control message.
	 */
	ip = (int *)(cmsg + 1);
	fd = *ip++;
	if (newfds > 1)
	{
		/*
		 * Close extra fds.
		 */
		int i;
		printf("webdav_open: %d extra fds\n", newfds - 1);
		for (i = 1; i < newfds; i++)
		{
			webdav_closefd(p, *ip);
			ip++;
		}
	}

	/*
	 * Check that the mode the file is being opened for is a subset 
	 * of the mode of the existing descriptor.
	 */
	fp = *fdfile(p, fd);
	if (((ap->a_mode & (FREAD | FWRITE)) | fp->f_flag) != fp->f_flag)
	{
		error = EACCES;
		goto bad;
	}


	/*
	 * Get the vnode of the cached file from this descriptor and put it
	 * in our data so that we can now do I/O directly to the cache.	 Get
	 * a reference on it also so that it won't go away
	 */
	/* We pended so check the state of the vnode */

	if (WEBDAV_CHECK_VNODE(vp))
	{
		/* Ok to close */

		error = 0;
		goto bad;
	}

	pt->pt_cache_vnode = (struct vnode *)fp->f_data;
	vref(pt->pt_cache_vnode);

	/* Set the dir not loaded bit if this is a directory, that way
	 * readdir will know that it needs to force a directory download
	 * even if the first call turns out not to be in the middle of the
	 * directory
	 */

	if (vp->v_type == VDIR)
	{
		pt->pt_status |= WEBDAV_DIR_NOT_LOADED;
	}

	/*	Set the uio structure back up to receive the file handle from
	  user land */

	aiov[0].iov_base = (caddr_t) & (pt->pt_file_handle);
	aiov[0].iov_len = sizeof(webdav_filehandle_t);
	auio.uio_iov = aiov;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_procp = p;
	auio.uio_offset = 0;
	len = auio.uio_resid = sizeof(webdav_filehandle_t);
	auio.uio_iovcnt = 1;

	flags = MSG_WAITALL;

	thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);
	/* disgard the control message and reset cm to 0 so
	 * we'll know if there's anything to free after
	 * this call to soreceive */
	if (cm)
	{
		m_freem(cm);
		cm = 0;
	}
	error = soreceive(so, (struct sockaddr **)0, &auio, (struct mbuf **)0, &cm, &flags);

#ifdef DEBUG
	if (error)
	{
		printf("webdav_open: error getting fd back from user land\n");
	}
#endif

	thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
	/* fall through */
bad:

	if (fd)
	{
		webdav_closefd(p, fd);
	}
	/*
	 * And discard the control message.
	 */
	if (cm)
	{
		m_freem(cm);
	}

	if (so)
	{
		thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);
		soshutdown(so, 2);
		soclose(so);
		thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
	}
	RET_ERR("webdav_open", error);
}

/*****************************************************************************/

int webdav_fsync(ap)
	struct vop_fsync_args	/* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int a_waitfor;
		struct proc *a_p;
		} */ *ap;
{
	struct webdavnode *pt;
	struct proc *p = ap->a_p;
	struct vnode *vp = ap->a_vp;
	int error = 0, server_error = 0;
	struct webdavmount *fmp;
	int vnop = WEBDAV_FILE_FSYNC;
	struct webdav_cred pcred;
	int done = FALSE;
	struct vattr attrbuf;

	pt = VTOWEBDAV(vp);
	fmp = VFSTOWEBDAV(vp->v_mount);

	if (vp->v_type != VREG ||
		/* fsync only makes sense for files */
		pt->pt_file_handle == -1)
	{
		/* If there is no file_handle, we have nothing to
		  tell the server to sync so just get out of dodge */
		error = 0;
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
		if ((vp->v_type == VREG) && (pt->pt_file_handle == -1))
		{
			printf("webdav_fsync: no file handle\n");
		}
#endif

		goto done;
	}

	/* This is where we need to tell UBC to flush out all of
	  it's pages for this vnode.	 If we do that then our write
	  and pageout routines will get called if anything needs to
	  be written.  That will cause the status to be dirty if
	  it needs to be marked as such. */

	if (UBCINFOEXISTS(vp) && (ubc_pushdirty(vp) == 0))
	{
		/* ubc_pushdirty() returns 0 on error */
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
		printf("webdav_fsync: ubc_pushdirty failed\n");
#endif

		error = EIO;
		goto done;
	}

	/* ubc_pushdirty is not a synchronous call.	 It returns to us
	  after having strated I/O on all the dirty pages but not after
	  the I/O is complete.	 It does lock the pages, however, and the
	  pages are not unlocked until the I/O is done. Thus a second
	  call to pushdirty will not return until all the pages from the
	  first call have been written.  It is forced to wait because it
	  needs to lock the pages which will be held locked until the I/O
	  is done.	 Thus calling it twice simulates a synchronous call.
	  So, this is why we call it again after having just called it. 
	*/


	if (UBCINFOEXISTS(vp) && (ubc_pushdirty(vp) == 0))
	{
		/* ubc_pushdirty() returns 0 on error */
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
		printf("webdav_fsync: 2nd ubc_pushdirty failed\n");
#endif

		error = EIO;
		goto done;
	}

	/* ubc_pushdirty() may have blocked, check the state of the vnode */

	if (WEBDAV_CHECK_VNODE(vp))
	{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
		printf("webdav_fsync: 2nd WEBDAV_CHECK_VNODE failed\n");
#endif

		error = EIO;
		goto done;
	}

	/* If the file isn't dirty, or has been deleted don't sync it */

	if ((!(pt->pt_status & WEBDAV_DIRTY)) || (pt->pt_status & WEBDAV_DELETED))
	{
		error = 0;
		goto done;
	}

	do
	{
		error = VOP_GETATTR(pt->pt_cache_vnode, &attrbuf, ap->a_cred, ap->a_p);
		if (error)
		{
			goto done;
		}

		if (attrbuf.va_flags & UF_NODUMP)
		{
			/* We are downloading the file and we haven't finished
			 * since the user process is going push the entire file
			 * back to the server, we'll have to wait until we have
			 * gotten all of it.	 Otherwise we will have inadvertantly
			 * pushed back an incomplete file and wiped out the original
			 */
			error = tsleep(&lbolt, PCATCH, "webdavdownload", 10);
#ifdef DEBUG
			if (error && error != EWOULDBLOCK)
			{
				printf("webdav_fsync: tsleep returned %d\n", error);
			}
#endif

			error = 0;
		}
		else
		{
			done = TRUE;
		}

		/* We pended so check the state of the vnode */
		if (WEBDAV_CHECK_VNODE(vp))
		{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
			printf("webdav_fsync: 1st WEBDAV_CHECK_VNODE failed\n");
#endif

			error = EIO;
			goto done;
		}

	} while (!done);


	bzero(&pcred, sizeof(pcred));
	if (ap->a_cred)
	{
		pcred.pcr_uid = ap->a_cred->cr_uid;
		pcred.pcr_ngroups = ap->a_cred->cr_ngroups;
		bcopy(ap->a_cred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));
	}

	pt->pt_status &= ~WEBDAV_DIRTY;

	error = webdav_sendmsg(vnop, WEBDAV_USE_HANDLE, pt, &pcred, fmp, p, (void *)NULL, 0,
		&server_error, (void *)NULL, 0);
	if (error)
	{
		goto done;
	}

	if (server_error)
	{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
		printf("webdav_fsync: server error %d\n", server_error);
#endif

		error = server_error;
		goto done;
	}

	/* We pended so check the state of the vnode */

	if (WEBDAV_CHECK_VNODE(vp))
	{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
		printf("webdav_fsync: 3rd WEBDAV_CHECK_VNODE failed\n");
#endif

		error = EIO;
		goto done;
	}

done:

	RET_ERR("webdav_fsync", error);
}

/*****************************************************************************/

int webdav_close(ap)
	struct vop_close_args	/* {
		struct vnode *a_vp;
		int fflag;
		struct ucred *a_cred;
		struct proc *a_p;
		} */ *ap;
{
	struct webdavnode *pt;
	struct proc *p = ap->a_p;
	struct vnode *vp = ap->a_vp;
	int error = 0, server_error = 0, fsync_error = 0;
	struct webdavmount *fmp;
	int vnop = WEBDAV_CLOSE;
	int waitfor = TRUE;
	struct webdav_cred pcred;


	pt = VTOWEBDAV(vp);
	fmp = VFSTOWEBDAV(vp->v_mount);

	/* If there is no file_handle, we have nothing to
	  tell the server to close so just get out of dodge */

	if (pt->pt_file_handle == -1)
	{
		error = 0;
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
		if (vp->v_type == VREG)
		{
			printf("webdav_close: no file handle\n");
		}
#endif

		goto bad;
	}

	if (vp->v_type == VREG)
	{
		/* it's a file */
		struct vop_fsync_args fsync_args;

		/* synchronize the file before closing */
		fsync_args.a_p = p;
		fsync_args.a_vp = vp;
		fsync_args.a_cred = ap->a_cred;
		fsync_args.a_waitfor = waitfor;

		/* VOP_LOCK(vp) */

		fsync_error = webdav_fsync(&fsync_args);
		if (WEBDAV_CHECK_VNODE(vp))
		{
			error = EIO;
			goto error_return;
		}

		/* VOP_UNLOCK(vp) ? */

		/* is this the last close? */
		if ( ubc_isinuse(vp, 1) )
		{
			/* no, do nothing and return */
			error = 0;
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
			printf("webdav_close: not the last closer\n");
#endif
			goto error_return;
		}
	}
	else
	{
		/* it's a directory */
		
		/* is this the last close? */
		if (vp->v_usecount > 1)
		{
			/* no, do nothing and return */
			error = 0;
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
			printf("webdav_close: not the last closer\n");
#endif
			goto error_return;
		}
	}

	bzero(&pcred, sizeof(pcred));
	if (ap->a_cred)
	{
		pcred.pcr_uid = ap->a_cred->cr_uid;
		pcred.pcr_ngroups = ap->a_cred->cr_ngroups;
		bcopy(ap->a_cred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));
	}

	error = webdav_sendmsg(vnop, WEBDAV_USE_HANDLE, pt, &pcred, fmp, p, (void *)NULL, 0,
		&server_error, (void *)NULL, 0);

	/* We pended so check the state of the vnode */
	if (WEBDAV_CHECK_VNODE(vp))
	{
		/* Ok to close */
		error = 0;
		goto error_return;
	}

	/* fall through to "bad" */

#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
	if (error)
	{
		printf("webdav_close: webdav_sendmsg failed\n");
	}
#endif

bad:

	/* we're the last closer so clean up
	 * We don't want to talk to the server again so
	 * get rid of the file handle also
	 * if we have a cached node, time to release it, We
	 * have pended though so we have to make sure that
	 * someone didn't come along and open the file
	 * while we were waiting for the server process.
	 * check the use coungs again just to make sure.
	 */

	if (!error)
	{
		if (fsync_error)
		{
			error = fsync_error;
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
			printf("webdav_close: webdav fsync error: %d\n", error);
#endif

		}
		else
		{
			if (server_error)
			{
				error = server_error;
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
				printf("webdav_close: webdav server error: %d\n", error);
#endif

			}
		}
	}

	if (pt->pt_cache_vnode)
	{
		struct vnode *temp;
		
		if ((vp->v_type == VREG))
		{
			/* it's a file - is this the last close? */
			if (ubc_isinuse(vp, 1))
			{
				/* no, do nothing and return */
#if (defined(DEBUG) || defined(WEBDAV_TRACE))
				printf("webdav_close: someone else opened the file\n");
#endif
				error = 0;
				goto error_return;
			}
			else
			{
				/* yes, remove it from the hash */
				webdav_hashrem(pt);
			}
		}
		else
		{
			/* it's a directory - is this the last close? */
			if (vp->v_usecount > 1)
			{
				/* no, do nothing and return */
				error = 0;
				goto error_return;
			}
		}
		
		/* zero out pt_cache_vnode and then release the cache vnode */
		temp = pt->pt_cache_vnode;
		pt->pt_cache_vnode = 0;
		vrele(temp);
	}
	pt->pt_file_handle = -1;					/* clear this after last potential re-use */

	/* Now that we've cleaned up the file_handle (which will keep
	  webdav_inactive from getting confused).  Blow this puppy out
	  of the vm system if it was mapped.
	*/

	if (WEBDAVISMAPPED(vp))
	{
		(void) ubc_uncache(vp);
		/* WARNING vp may not be valid after this */
	}

error_return:

	RET_ERR("webdav_close", error);
}

/*****************************************************************************/

/* webdav_read_bytes
 * This is a utility routine called by webdav_read to read
 * bytes directly from the server when we haven't yet downloaded
 * the part of the file needed to retrieve the data
 */

int webdav_read_bytes(vp, a_uio, a_cred)
	struct vnode *vp;
	struct uio *a_uio;
	struct ucred *a_cred;
{
	struct webdavnode *pt;
	int error = 0, server_error = 0;
	int vnop = WEBDAV_BYTE_READ;
	webdav_byte_read_header_t * read_header;
	void *buffer = 0;
	int message_size;
	int data_size;
	struct webdav_cred pcred;
	struct webdavmount *fmp;
	struct proc *p = current_proc();

	fmp = VFSTOWEBDAV((vp)->v_mount);
	pt = VTOWEBDAV(vp);

	/*
	 * Get enough space for the read header info we will be sending to the
	 * user process
	 */

	message_size = sizeof(webdav_byte_read_header_t) + pt->pt_size;
	data_size = a_uio->uio_resid;

#ifdef DEBUG
	printf("webdav_read_bytes: start %d, len %d, pid %d\n", (int)a_uio->uio_offset, 
		(int)data_size, (int)p->p_pid);
#endif

	if ((!a_uio->uio_offset) ||
		/* don't bother if the range starts with 0 */
		data_size > WEBDAV_MAX_IO_BUFFER_SIZE)
	{
		error = EINVAL;
		goto done;
	}

	MALLOC(read_header, void *, message_size, M_TEMP, M_WAITOK);

	/* Now allocate the bufer that we are going to use to hold the data that
	 * comes back
	 */

	MALLOC(buffer, void *, data_size, M_TEMP, M_WAITOK);

	/* We pended so check the state of the vnode */

	if (WEBDAV_CHECK_VNODE(vp))
	{
		error = EPERM;
		goto dealloc_done;
	}

	read_header->wd_byte_start = a_uio->uio_offset;
	read_header->wd_num_bytes = a_uio->uio_resid;
	read_header->wd_uri_size = pt->pt_size;
	bcopy(pt->pt_arg, ((char *)read_header) + sizeof(webdav_byte_read_header_t), pt->pt_size);

	bzero(&pcred, sizeof(pcred));
	if (a_cred)
	{
		pcred.pcr_uid = a_cred->cr_uid;
		pcred.pcr_ngroups = a_cred->cr_ngroups;
		bcopy(a_cred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));
	}

	error = webdav_sendmsg(vnop, WEBDAV_USE_INPUT, pt, &pcred, fmp, p, (void *)read_header, 
		message_size, &server_error, buffer, data_size);
	if (error)
	{
		goto dealloc_done;
	}

	if (server_error)
	{
		error = server_error;
		goto dealloc_done;
	}

	error = uiomove((caddr_t)buffer, data_size, a_uio);
	if (error)
	{
		goto dealloc_done;
	}

dealloc_done:

	if (buffer)
	{
		FREE((void *)buffer, M_TEMP);
	}

	FREE((void *)read_header, M_TEMP);

done:

	RET_ERR("webdav_read_bytes", error);
}

/*****************************************************************************/

int webdav_read(ap)
	struct vop_read_args	/* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
		} */ *ap;
{
	struct webdavnode *pt;
	struct proc *p = current_proc();
	struct vnode *cachevp;
	int error;
	upl_t upl;
	upl_page_info_t * pl;
	struct iovec aiov;
	struct uio uio;
	struct vattr attrbuf;
	off_t offset;
	off_t total_xfersize;
	off_t rounded_iolength;
	int xfersize;
	int bytes_to_zero = 0;
	int index = 0, current_page = 0;
	int pagecount = 0;
	vm_offset_t addr;
	kern_return_t kret;
	struct vnode *vp = ap->a_vp;
	int done = FALSE;
	int tried_bytes = FALSE;

	pt = VTOWEBDAV(vp);
	cachevp = pt->pt_cache_vnode;

	if (!cachevp)
	{
		panic("Webdav about to read from uncached vnode ");
	}

	pt->pt_status |= WEBDAV_ACCESSED;

	/* Check the size to make sure we aren't going past the end
	 * of the file */

	error = VOP_GETATTR(cachevp, &attrbuf, ap->a_cred, p);
	if (error)
	{
		goto exit;
	}

	/* We might have pended, check things over */

	if (WEBDAV_CHECK_VNODE(vp))
	{
		error = EPERM;
		goto exit;
	}

	/* XXX UBC	This needs to change to contact the VM system */
	/* to see if it has the pages we need read before consulting */
	/* the cache file */


	/* Ok, start the sleep loop to wait on the background download
	  We will know that the webdav user process is finished when it
	  either clears the nodump flag or sets the append only flag
	  (indicating an error) */

	do
	{
		error = VOP_LOCK(cachevp, LK_EXCLUSIVE | LK_RETRY, p);
		if (error)
		{
			goto exit;
		}

		if (WEBDAV_CHECK_VNODE(vp))
		{
			error = EPERM;
			goto exit;
		}

		error = VOP_GETATTR(cachevp, &attrbuf, ap->a_cred, p);
		if (error)
		{
			goto unlock_exit;
		}

		if (WEBDAV_CHECK_VNODE(vp))
		{
			error = EPERM;
			goto exit;
		}

		/* since we'll be bringing this data in one page at a time we have
		 * to ensure that we don't bring in a partial page which the VM
		 * system will remember but which does not contain all of the
		 * data which is intended for that part of the file.  All this to
		 * say that we will not go ahead and read in data to satisfy the
		 * request unless the full page of data is there
		 */

		rounded_iolength = (((ap->a_uio->uio_offset + ap->a_uio->uio_resid - 1) / PAGE_SIZE) + 1) *
			PAGE_SIZE;

		if ((attrbuf.va_flags & UF_NODUMP) && (rounded_iolength > attrbuf.va_size))
		{
			/* We are downloading the file and we haven't gotten to
			 * the bytes we need so try to ask the server for the bytes
			 * directly. If that does not work, wait until the stuff gets down.
			 */			
			VOP_UNLOCK(cachevp, 0, p);
			if (!tried_bytes)
			{
				if (rounded_iolength > (attrbuf.va_size + WEBDAV_WAIT_IF_WITHIN))
				{
					error = webdav_read_bytes(ap->a_vp, ap->a_uio, ap->a_cred);
					if (!error)
					{
						goto exit;
					}
				}
				/* If we are here, we must have failed to get the bytes so return an
				* set tried_bytes so we won't make this mistake again and sleep */

				tried_bytes = TRUE;
			}
			
			error = tsleep(&lbolt, PCATCH, "webdavdownload", 10);
#ifdef DEBUG
			if (error && error != EWOULDBLOCK)
			{
				printf("webdav_read: tsleep returned %d\n", error);
			}
#endif

			error = 0;

			/* After pending on tsleep, check the state of the vnode */
			if (WEBDAV_CHECK_VNODE(vp))
			{
				error = EPERM;
				goto exit;
			}
		}
		else
		{
			/* the part we need has been downloaded 
			  and cachevp is still VOP_LOCK'ed */
			done = TRUE;
		}
	} while (!done);

	if (attrbuf.va_flags & UF_APPEND)
	{
		error = EIO;
		goto unlock_exit;
	}

	if (UBCISVALID(vp))
	{
		/* If the UBC is valid we need to get the information
		 * from the vm cache. */
		
		total_xfersize = MIN(ap->a_uio->uio_resid, (attrbuf.va_size - ap->a_uio->uio_offset));

		if (ap->a_uio->uio_offset > attrbuf.va_size)
		{
			total_xfersize = 0;
		}

		offset = ap->a_uio->uio_offset;

		/* transfer it in up to MAXPHYSIO sized chunks */
		while (total_xfersize > 0)
		{
			/* transfer size this time? */
			xfersize = MIN(MAXPHYSIO, total_xfersize);

			pagecount = (((offset % PAGE_SIZE) + xfersize - 1) / PAGE_SIZE) + 1;
			
			/* create the upl */
			kret = ubc_create_upl(vp,
				(vm_object_offset_t) trunc_page(offset),
				(vm_size_t) round_page(xfersize + (offset % PAGE_SIZE)),
				&upl,
				&pl,
				UPL_FLAGS_NONE);

            if (kret != KERN_SUCCESS)
			{
                error = kret;
                goto unlock_exit;
            }
			
			/* map the upl */
			kret = ubc_upl_map(upl, &addr);
            if (kret != KERN_SUCCESS)
			{
				panic("webdav read: unable to map data");
			}

			/* read in the invalid pages */
			for (index = 0; index < pagecount; ++index)
			{
				if (!upl_valid_page(pl, index))
				{
					uio.uio_iov = &aiov;
					uio.uio_iovcnt = 1;
					uio.uio_offset = trunc_page(offset + (index * PAGE_SIZE));
					uio.uio_segflg = UIO_SYSSPACE;
					uio.uio_rw = UIO_READ;
					uio.uio_resid = PAGE_SIZE;
					uio.uio_procp = NULL;
					aiov.iov_base = (caddr_t)(addr + index * PAGE_SIZE);
					aiov.iov_len = PAGE_SIZE;

					error = VOP_READ(cachevp, &uio, 0/* no flags */ , ap->a_cred);
					if (error)
					{
						goto unmap_unlock_exit;
					}

					/* We have already checked to make sure that total_xfersize will
					 * not take us past the end of the file.  It is possible though that
					 * we will be reading into a full page that spans the eof of the file
					 * in that case we will need to zero the rest of the page because
					 * this read will make it a valid page in the vm cache and a truncate
					 * or write past the eof could come along and extend the file.	That will
					 * make the junk at the end of this page look like valid data in the middle
					 * of the newly extended file.	So how do we know if this is true ?	 Easy,
					 * uio_resid will be non-zero indicating that we tried to VOP_READ past the
					 * end of the file.	 Just to be sure we'll use the known end of the file
					 * to figure out what to zero
					 */

					if (uio.uio_resid)
					{
						if ((trunc_page(offset) + pagecount * PAGE_SIZE) > attrbuf.va_size)
						{
							bytes_to_zero = trunc_page(offset) + (pagecount * PAGE_SIZE) - 
								attrbuf.va_size;
							bzero((caddr_t)(addr + pagecount * PAGE_SIZE) - bytes_to_zero,
								bytes_to_zero);
						}
					}
				}
			}

			/* move the data */
			error = uiomove((caddr_t)(addr + (int)(offset % PAGE_SIZE)), xfersize, ap->a_uio);
			if (error)
			{
				goto unmap_unlock_exit;
			}
			
			/* unmap the upl */
			(void) ubc_upl_unmap(upl);

			/* The pages we just read in should not be left in memory
			 * because they weren't there to begin with. If they are needed
			 * again, we'll get them from the cache file (where they might be cached
			 * in memory). In this case, we'll dump them with ubc_upl_abort_range.
			 *
			 * The pages that were already in memory need to left in memory in
			 * whatever state (dirty or clean). In this case, we'll commit them.
			 */
			index = 0;
			while (index < pagecount)
			{
				current_page = index;

				/* Calculate the run of pages with the same state so that
				* we can commit or abort them together */
				while ((current_page + 1) < pagecount && 
					(upl_valid_page(pl, current_page) == upl_valid_page(pl, current_page + 1)))
				{
					++current_page;
				}

				if (!upl_valid_page(pl, index))
				{
					/* the run of pages was invalid so dump them with abort */
					kret = ubc_upl_abort_range(upl, (vm_offset_t)index * PAGE_SIZE,
						(vm_size_t)((current_page - index) + 1) * PAGE_SIZE,
						UPL_ABORT_DUMP_PAGES | UPL_ABORT_FREE_ON_EMPTY);
					if ( kret )
					{
						error = kret;	/* XXX Translate ? */
						goto unlock_exit;
					}
				}
				else
				{
					/* the run of pages was valid so just commit */
					kret = ubc_upl_commit_range(upl, (vm_offset_t)index * PAGE_SIZE,
						(vm_size_t)((current_page - index) + 1) * PAGE_SIZE,
						UPL_COMMIT_FREE_ON_EMPTY);
					if ( kret )
					{
						error = kret;	/* XXX Translate ? */
						goto unlock_exit;
					}
				}

				/* We updated some pages so bump up the index as we don't
				* want to recommit pages.
				*/
				index = current_page + 1;
			}	/* end while index < pagecount */
            
			total_xfersize -= xfersize;
			offset += xfersize;


		}	/* end while loop */

		VOP_UNLOCK(cachevp, 0, p);

	}
	else
	{
		/* No UBC so Just pass the read along to the underlying file
		 *	and return
		 */

		error = VOP_READ(cachevp, ap->a_uio, ap->a_ioflag, ap->a_cred);
		VOP_UNLOCK(cachevp, 0, p);
	}

exit:

	RET_ERR("webdav_read", error);

unmap_unlock_exit:

	(void) ubc_upl_unmap(upl);

	kret = ubc_upl_abort(upl, UPL_ABORT_ERROR);
	if ( kret )
	{
		if (!error)
		{
			error = kret;
		}
	}

unlock_exit:

	VOP_UNLOCK(cachevp, 0, p);
	goto exit;
}

/*****************************************************************************/

int webdav_write(ap)
	struct vop_read_args	/* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
		} */ *ap;
{
	struct webdavnode *pt;
	struct proc *p = current_proc();
	struct vnode *cachevp;
	int error = 0;
	upl_t upl;
	upl_page_info_t * pl;
	struct iovec aiov;
	struct uio uio;
	struct vattr attrbuf;
	off_t rounded_iolength;
	off_t total_xfersize, current_offset, start_offset, start_xfersize, len;
	int xfersize;
	int pagecount = 0;
	int done = FALSE;
	vm_offset_t addr;
	kern_return_t kret;
	struct vnode *vp = ap->a_vp;

	pt = VTOWEBDAV(vp);
	cachevp = pt->pt_cache_vnode;

	if (!cachevp)
	{
		panic("Webdav about to read from uncached vnode ");
	}

	/* Ok, start the sleep loop to wait on the background download
	  We will know that the webdav user process is finished when it
	  either clears the nodump flag or sets the append only flag
	  (indicating an error) */

	do
	{
		error = VOP_LOCK(cachevp, LK_EXCLUSIVE | LK_RETRY, p);
		if (error)
		{
			goto exit;
		}

		if (WEBDAV_CHECK_VNODE(vp))
		{
			error = EPERM;
			goto exit;
		}

		error = VOP_GETATTR(cachevp, &attrbuf, ap->a_cred, p);
		if (error)
		{
			goto unlock_exit;
		}

		if (WEBDAV_CHECK_VNODE(vp))
		{
			error = EPERM;
			goto exit;
		}

		/* since our read befoe write strategy will be bringing this data in 
		 * one page at a time we have to ensure that we don't write into
		 * a page that is only partially downloaded. The VM system will
		 * remember the page that is half done and commit our write as
		 * well as some zeroes that don't represent our actual file data.
		 * All this to say that we will not write our data into the page
		 * until the entire page is down from the server, or we have
		 * reached the end of the file. 
		 */

		rounded_iolength =
			(((ap->a_uio->uio_offset + ap->a_uio->uio_resid - 1) / PAGE_SIZE) + 1) * PAGE_SIZE;

		if ((attrbuf.va_flags & UF_NODUMP) && (rounded_iolength > attrbuf.va_size))
		{
			/* We are downloading the file and we haven't gotten to
			 * to the bytes we need so unlock, sleep, and try the whole
			 * thing again.
			 */

			VOP_UNLOCK(cachevp, 0, p);
			error = tsleep(&lbolt, PCATCH, "webdavdownload", 10);
#ifdef DEBUG
			if (error && error != EWOULDBLOCK)
			{
				printf("webdav_write: tsleep returned %d\n", error);
			}
#endif

			error = 0;

			/* After pending on tsleep, check the state of the vnode */
			if (WEBDAV_CHECK_VNODE(vp))
			{
				error = EPERM;
				goto exit;
			}

		}
		else
		{
			/* the part we need has been downloaded 
			  and cachevp is still VOP_LOCK'ed */
			done = TRUE;
		}
	} while (!done);

	if (attrbuf.va_flags & UF_APPEND)
	{
		error = EIO;
		goto unlock_exit;
	}

	if (UBCISVALID(vp))
	{
		/* If the UBC is valid we need to get the information
		 * from the vm cache.  Lock up the underlying cache file
		 * first
		 */

		/* Get the size so that we can determine if we have
		  extended the size of the file */

		error = VOP_GETATTR(cachevp, &attrbuf, ap->a_cred, p);
		if (error)
		{
			goto unlock_exit;
		}

		total_xfersize = start_xfersize = ap->a_uio->uio_resid;
		start_offset = ap->a_uio->uio_offset;

		if (total_xfersize <= 0)
		{
			total_xfersize = 0;
		}

		while (total_xfersize > 0)
		{
			vm_size_t	upl_size;
			
			xfersize = MIN(MAXPHYSIO, total_xfersize);
			current_offset = ap->a_uio->uio_offset;

			pagecount = (((current_offset % PAGE_SIZE) + xfersize - 1) / PAGE_SIZE) + 1;
			
			upl_size = (vm_size_t) round_page(xfersize + (current_offset % PAGE_SIZE));
			kret = ubc_create_upl(vp,
				(vm_object_offset_t) trunc_page(current_offset),
				upl_size,
				&upl, &pl, UPL_FLAGS_NONE);
			if (kret != KERN_SUCCESS)
			{
				error = kret;
				goto unlock_exit;
			}

			kret = ubc_upl_map(upl, &addr);
			if (kret != KERN_SUCCESS)
			{
				panic("webdav read: unable to map data");
			}

			/* If the write we are about to do is not aligned, read in the pages
			  necessary so that the right data will be there when we ship it
			  back to the file, provided of course that it is valid file data
			  We will zero out the page if it is not valid and beyond the
			  current file offset.*/

			if (current_offset % PAGE_SIZE)
			{
				if (trunc_page(current_offset) < attrbuf.va_size)
				{

					/* Write starts in the middle of the first page so read
					 * the whole page in if it was not in the cache. */

					if (!upl_valid_page(pl, 0))
					{
						uio.uio_iov = &aiov;
						uio.uio_iovcnt = 1;
						uio.uio_offset = trunc_page(current_offset);
						uio.uio_segflg = UIO_SYSSPACE;
						uio.uio_rw = UIO_READ;
						uio.uio_resid = PAGE_SIZE;
						uio.uio_procp = NULL;
						aiov.iov_base = (caddr_t)(addr);
						aiov.iov_len = PAGE_SIZE;

						/* Make our adjustements for the file's size.  If we are trying
						 * to do a read before write, make sure we are not going past
						 * the end of the file
						 */

						if ((trunc_page(current_offset) + PAGE_SIZE) > attrbuf.va_size)
						{
							if (trunc_page(current_offset) > attrbuf.va_size)
							{
								uio.uio_resid = 0;
							}
							else
							{
								uio.uio_resid = attrbuf.va_size - trunc_page(current_offset);
							}
						}

						/* Before reading the page in zero it out.	This way any stuff
						 * on the page of memory will not wind up associated with this file
						 * should it happen to grow later and this particular page get
						 * flushed out
						 */

						bzero((caddr_t)trunc_page(addr), PAGE_SIZE);

						error = VOP_READ(cachevp, &uio, 0/* no flags */ , ap->a_cred);
						if (error)
						{
							goto unmap_unlock_exit;
						}
					}
					/* Being here means that we had a valid page and we have
					 * absolutely no work to do.
					 */
				}
				else
				{
					/* So we are beyond the end of the file about to do a non
					 * page aligned write.	Zero the page so that the stuff
					 * in the beginnin won't be junk.
					 */

					bzero((caddr_t)trunc_page(addr), PAGE_SIZE);
				}
			}

			if (((current_offset + xfersize) % PAGE_SIZE) && 
				((pagecount > 1) || (current_offset % PAGE_SIZE == 0)))
			{
				if (trunc_page(current_offset + xfersize) < attrbuf.va_size)
				{

					/* Write ends in the middle of the last page and we are
					 * dealing with a multiple page write, or it is a single page
					 * write but the beginning was on a page boundry so the code above
					 * will not hvae read the page in.  So read
					 * the whole page in if it is not in the cache.
					 */

					if (!upl_valid_page(pl, (pagecount - 1)))
					{
						uio.uio_iov = &aiov;
						uio.uio_iovcnt = 1;
						uio.uio_offset = trunc_page(current_offset + xfersize);
						uio.uio_segflg = UIO_SYSSPACE;
						uio.uio_rw = UIO_READ;
						uio.uio_resid = PAGE_SIZE;
						uio.uio_procp = NULL;
						aiov.iov_base = (caddr_t)trunc_page(addr + (current_offset % PAGE_SIZE) + xfersize);
						aiov.iov_len = PAGE_SIZE;

						/* Make our adjustements for the file's size.  If we are trying
						 * to do a read before write, make sure we are not going past
						 * the end of the file */

						if ((trunc_page(current_offset) + PAGE_SIZE) > attrbuf.va_size)
						{
							if (trunc_page(current_offset) > attrbuf.va_size)
							{
								uio.uio_resid = 0;
							}
							else
							{
								uio.uio_resid = attrbuf.va_size - trunc_page(current_offset);
							}
						}

						/* Before reading the page in zero it out.	This way any stuff
						 * on the page of memory will not wind up associated with this file
						 * should it happen to grow later and this particular page get
						 * flushed out
						 */

						bzero((caddr_t)trunc_page(addr + (current_offset % PAGE_SIZE) + xfersize),
							PAGE_SIZE);

						error = VOP_READ(cachevp, &uio, 0/* no flags */ , ap->a_cred);
						if (error)
						{
							goto unmap_unlock_exit;
						}
					}
				}
				else
				{

					/* So we are beyond the end of the file about to do a non
					 * page aligned write.	Zero the page so that the stuff
					 * in the end won't be junk.
					 */

					bzero((caddr_t)trunc_page(addr + (current_offset % PAGE_SIZE) + xfersize), PAGE_SIZE);
				}
			}

			total_xfersize -= xfersize;

			error = uiomove((caddr_t)(addr + ((int)(current_offset % PAGE_SIZE))), xfersize, ap->a_uio);
			if (error)
			{
				goto unmap_unlock_exit;
			}

			/* Ok, now the user data is in our buffer so push it to the
			 * file.  We don't actually write the trailing data at the
			 * end of the users data which we read in from the file.  We
			 * read it into make sure the UBC has the right data in the
			 * page.  We don't rewrite the extra stuff at the beginning
			 * either */

			uio.uio_iov = &aiov;
			uio.uio_iovcnt = 1;
			uio.uio_offset = current_offset;
			uio.uio_segflg = UIO_SYSSPACE;
			uio.uio_rw = UIO_WRITE;
			uio.uio_resid = xfersize;
			uio.uio_procp = NULL;
			aiov.iov_base = (caddr_t)(addr + ((int)(current_offset % PAGE_SIZE)));
			aiov.iov_len = xfersize;

			error = VOP_WRITE(cachevp, &uio, 0/* no flags */ , ap->a_cred);
			if (error)
			{
				/* We didn't really write the data, so update the uio to
				 * let the user know */
				ap->a_uio->uio_resid += xfersize;
				ap->a_uio->uio_offset -= xfersize;
				goto unmap_unlock_exit;
			}

			(void) ubc_upl_unmap(upl);
			
			/* All pages in the range are now valid in the cache file so
			 * we don't need them in memory anymore. If they are needed
			 * again, we'll get them from the cache file (which might be
			 * cached in memory).
			 */
			kret = ubc_upl_abort_range(upl, 0, upl_size,
				UPL_ABORT_DUMP_PAGES | UPL_ABORT_FREE_ON_EMPTY);
			if (kret)
			{
				error = kret;					/* XXX Translate ? */
				goto unlock_exit;
			}
		}										/* end while loop */

		/* after the write to the cache file has been completed... */
		pt->pt_status |= WEBDAV_DIRTY;

		VOP_UNLOCK(cachevp, 0, p);

		/* if we grew the file then do a setsize to let
		 * the UBC know, but only if we weren't background
		 * downloading.	 Account for bytes we failed to write */

		len = start_offset + (start_xfersize - total_xfersize);
		if ((attrbuf.va_size < len) && !(attrbuf.va_flags & UF_NODUMP))
		{
			(void)ubc_setsize(vp, len);
		}

	}
	else
	{
		/* No UBC so Just pass the read along to the underlying file
		 *	and return
		 */

		error = VOP_WRITE(cachevp, ap->a_uio, ap->a_ioflag, ap->a_cred);

		/* after the write to the cache file has been completed... */
		pt->pt_status |= WEBDAV_DIRTY;

		VOP_UNLOCK(cachevp, 0, p);
	}

exit:

	RET_ERR("webdav_write", error);

unmap_unlock_exit:

	(void) ubc_upl_unmap(upl);

	kret = ubc_upl_abort (upl,UPL_ABORT_ERROR);
	if (kret)
	{
		if (!error)
		{
			error = kret;
		}
	}

unlock_exit:

	/* after the write to the cache file has been completed... */
	pt->pt_status |= WEBDAV_DIRTY;

	VOP_UNLOCK(cachevp, 0, p);
	goto exit;
}												/* webdav_write */

/*****************************************************************************/

int webdav_truncate(ap)
	struct vop_truncate_args	/* {
		struct vnode *a_vp;
		off_t a_length;
		int a_flags;
		struct ucred *a_cred;
		struct proc *a_p;
		} */ *ap;
{
	struct webdavnode *pt;
	int error = 0;
	int done = FALSE;
	struct vattr attrbuf;
	struct vnode *cachevp;

	pt = VTOWEBDAV(ap->a_vp);
	cachevp = pt->pt_cache_vnode;

	if (!cachevp)
	{
		panic("Webdav about to truncate with no cached vnode ");
	}

	do
	{
		error = VOP_LOCK(cachevp, LK_EXCLUSIVE | LK_RETRY, ap->a_p);
		if (error)
		{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
			printf("webdav_truncate: VOP_LOCK error\n");
#endif

			goto exit;
		}

		if (WEBDAV_CHECK_VNODE(ap->a_vp))
		{
			error = EPERM;
			goto exit;
		}

		error = VOP_GETATTR(cachevp, &attrbuf, ap->a_cred, ap->a_p);
		if (error)
		{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
			printf("webdav_truncate: VOP_GETATTR error\n");
#endif

			goto unlock_exit;
		}

		if (WEBDAV_CHECK_VNODE(ap->a_vp))
		{
			error = EPERM;
			goto exit;
		}

		if (attrbuf.va_flags & UF_NODUMP)
		{
			/* We are downloading the file and we haven't finished
			 * since the user process is going to extend the file with
			 * it's writes until it is done, we will just have to wait for
			 * it to finish.
			 */

			VOP_UNLOCK(cachevp, 0, ap->a_p);
			error = tsleep(&lbolt, PCATCH, "webdavdownload", 10);
#ifdef DEBUG
			if (error && error != EWOULDBLOCK)
			{
				printf("webdav_truncate: tsleep returned %d\n", error);
			}
#endif

			error = 0;

			/* After pending on tsleep, check the state of the vnode */
			if (WEBDAV_CHECK_VNODE(ap->a_vp))
			{
				error = EPERM;
				goto exit;
			}

		}
		else
		{
			done = TRUE;
		}
	} while (!done);

	if (attrbuf.va_flags & UF_APPEND)
	{
		error = EIO;
		goto unlock_exit;
	}

	/* if the UBC is valid for this guy, we need to tell it that our size
	 * now matches the size of the cache file underneath.
	 */

	if (UBCINFOEXISTS(ap->a_vp) && !error)
	{
		if (!ubc_setsize(ap->a_vp, (off_t)ap->a_length))
		{
			/* XXX Check Errors, and do what? XXX */
			printf("webdav_truncate: ubc_setsize error\n");
		}
	}

	error = VOP_TRUNCATE(cachevp, ap->a_length, ap->a_flags, ap->a_cred, ap->a_p);

unlock_exit:

	/* after the write to the cache file has been completed... */
	pt->pt_status |= WEBDAV_DIRTY;

	VOP_UNLOCK(cachevp, 0, ap->a_p);

exit:

	RET_ERR("webdav_truncate", error);
}

/*****************************************************************************/

int webdav_access(ap)
	struct vop_access_args	/* {
		struct vnode *a_vp;
		int	 a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
		} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	/*
	 * Disallow write attempts on read-only file systems;
	 * webdav only support VREG and VDIR file types so
	 * we don't bother checking the type.
	 */
	if ((ap->a_mode & VWRITE) && (vp->v_mount->mnt_flag & MNT_RDONLY))
	{
		return (EROFS);
	}

	/* Server mediates access so we'll just return 0 here */

	return (0);
}

/*****************************************************************************/

int webdav_getattr(ap)
	struct vop_getattr_args	/* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
		} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;
	struct vattr server_vap;
	struct timeval tv;
	struct webdavnode *pt;
	struct webdavmount *fmp;
	int error = 0;
	int server_error = 0;
	struct proc *p = current_proc();
	struct webdav_cred pcred;
	int vnop = WEBDAV_STAT;

	fmp = VFSTOWEBDAV(vp->v_mount);
	pt = VTOWEBDAV(vp);

	/* first check the cache file */

	if (pt->pt_cache_vnode && vp->v_type == VREG)
	{
		error = VOP_GETATTR(pt->pt_cache_vnode, vap, ap->a_cred, ap->a_p);
		if (error)
		{
			goto bad;
		}
		if (WEBDAV_CHECK_VNODE(vp))
		{
			error = EPERM;
			goto bad;
		}
		/* cache vnode may be a file but webdav node
		 * may represent a directory.  Set the type
		 */
		vap->va_type = vp->v_type;
		if (vap->va_type == VDIR)
		{
			vap->va_mode &= ~S_IFREG;
			vap->va_mode |= S_IFDIR;
		}


		if (!(pt->pt_status & WEBDAV_DIRTY))
		{
			/* If we have not dirtied the cache file, use the
			 * original mode and change times cached in the vnode
			 * so that local clients won't get confused.  The act
			 * of opening the file and caching it locally should not
			 * make it appear as if the file was modified
			 */

			vap->va_mtime = pt->pt_mtime;
			vap->va_ctime = pt->pt_ctime;

			/* Though we have not dirtied it we may have accessed it so
			 * we'll check for that. Only if we haven't accessed it also will
			 * we use the oiginal atime
			 */

			if (!(pt->pt_status & WEBDAV_ACCESSED))
			{
				vap->va_atime = pt->pt_atime;
			}
		}

		/* If the file is being downloaded or failed to download,
		 * we will need to get size from the server
		 */

		if ((vap->va_flags & UF_NODUMP) || (vap->va_flags & UF_APPEND))
		{
			bzero(&server_vap, sizeof(server_vap));
			vattr_null(&server_vap);

			pcred.pcr_flag = 0;
			/* user level is ingnoring the pcred anyway */

			pcred.pcr_uid = ap->a_cred->cr_uid;
			pcred.pcr_ngroups = ap->a_cred->cr_ngroups;
			bcopy(ap->a_cred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));

			error = webdav_sendmsg(vnop, WEBDAV_USE_URL, pt, &pcred, fmp, p,
				(void *)NULL, 0, &server_error, (void *) & server_vap, sizeof(server_vap));
			if (error)
			{
				goto bad;
			}

			if (server_error)
			{
				error = server_error;
				goto bad;
			}

			/* We pended so check the state of the vnode */

			if (WEBDAV_CHECK_VNODE(vp))
			{
				error = EPERM;
				goto bad;
			}

			vap->va_size = server_vap.va_size;
			vap->va_bytes = server_vap.va_bytes;
		}
	}
	else
	{

		bzero(vap, sizeof(*vap));
		vattr_null(vap);

		pcred.pcr_flag = 0;
		/* user level is ingnoring the pcred anyway */

		pcred.pcr_uid = ap->a_cred->cr_uid;
		pcred.pcr_ngroups = ap->a_cred->cr_ngroups;
		bcopy(ap->a_cred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));

		error = webdav_sendmsg(vnop, WEBDAV_USE_URL, pt, &pcred, fmp, p,
			(void *)NULL, 0, &server_error, (void *)vap, sizeof(*vap));
		if (error)
		{
			goto bad;
		}

		if (server_error)
		{
			error = server_error;
			goto bad;
		}

		/* We pended so check the state of the vnode */

		if (WEBDAV_CHECK_VNODE(vp))
		{
			error = EPERM;
			goto bad;
		}

		if (vap->va_atime.tv_sec == 0)
		{
			microtime(&tv);
			if (WEBDAV_CHECK_VNODE(vp))
			{
				error = EPERM;
				goto bad;
			}
			TIMEVAL_TO_TIMESPEC(&tv, &vap->va_atime);
			vap->va_mtime = vap->va_atime;
			vap->va_ctime = vap->va_ctime;
		}
		vap->va_blocksize = WEBDAV_BLOCK_SIZE;

	}

	/* These fields are the same no regardless of whether there
	 * is a cache file or not
	 */

	vap->va_uid = UNKNOWNUID;
	vap->va_gid = UNKNOWNUID;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];


	vap->va_gen = 0;
	vap->va_flags = 0;
	vap->va_rdev = 0;
	/* vap->va_qsize = 0; */
	if (vp->v_flag & VROOT)
	{
		vap->va_type = VDIR;
		vap->va_mode = S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP |
			S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH;
		vap->va_fileid = vap->va_fileid = WEBDAV_ROOTFILEID;
	}
	else
	{
		vap->va_type = vp->v_type;
		vap->va_mode = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP |
			S_IROTH | S_IWOTH | S_IXOTH;
		if (vap->va_type == VDIR)
		{
			/* This is a redundent setting of permissions but in
			 * case we change our minds it will be a handy reminder
			 */
			vap->va_mode |= S_IXUSR | S_IXGRP | S_IXOTH | S_IFDIR;
		}
		else
		{
			vap->va_mode |= S_IFREG;
		}

		if (!vap->va_fileid)
		{
			vap->va_fileid = VTOWEBDAV(vp)->pt_fileid;
		}
	}
	/* Getting the real link count for directories is expensive.
	 * This lets FTS(3) (and other utilities that assume 1 means
	 * a file system doesn't support link counts) work.
	 */
	vap->va_nlink = 1;
	
	/* fall through */

bad:

	RET_ERR("webdav_getattr", error);
}

/*****************************************************************************/

int webdav_remove(ap)
	struct vop_remove_args	/* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
		} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct webdavnode *pt;
	struct webdavmount *fmp;
	int error = 0;
	int server_error = 0;
	struct proc *p = current_proc();
	int vnop = WEBDAV_FILE_DELETE;
	struct webdav_cred pcred;

	fmp = VFSTOWEBDAV(vp->v_mount);
	pt = VTOWEBDAV(vp);

	pcred.pcr_flag = 0;
	/* user level is ingnoring the pcred anyway */

	pcred.pcr_uid = p->p_ucred->cr_uid;
	pcred.pcr_ngroups = p->p_ucred->cr_ngroups;
	bcopy(p->p_ucred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));

	error = webdav_sendmsg(vnop, WEBDAV_USE_URL, pt, &pcred, fmp, p, (void *)NULL, 0,
		&server_error, (void *)NULL, 0);
	if (error)
	{
		goto bad;
	}

	if (server_error)
	{
		error = server_error;
		goto bad;
	}

	/* We pended so check the state of the vnode */

	if (WEBDAV_CHECK_VNODE(vp))
	{
		error = EPERM;
		goto bad;
	}

	/* Get the node off of the cache so that other lookups
	 * won't find it and think the file still exists
	 */

	pt->pt_status |= WEBDAV_DELETED;
	webdav_hashrem(pt);


bad:

	if (vp == ap->a_dvp)
	{
		vrele(vp);
	}
	else
	{
		vput(vp);
	}

	vput(ap->a_dvp);

	if (!error)
	{
		/* If we have an error, we have failed to delete the file so
		 * do not tell the UBC to give up on it.
		 */

		if (UBCINFOEXISTS(vp))
		{
			(void) ubc_uncache(vp);
			/* WARNING vp may not be valid after this */
		}
	}

	RET_ERR("webdav_remove", error);
}

/*****************************************************************************/

int webdav_rmdir(ap)
	struct vop_remove_args	/* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
		} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct webdavnode *pt;
	struct webdavmount *fmp;
	int error = 0;
	int server_error = 0;
	struct proc *p = current_proc();
	int vnop = WEBDAV_DIR_DELETE;
	struct webdav_cred pcred;

	fmp = VFSTOWEBDAV(vp->v_mount);
	pt = VTOWEBDAV(vp);

	pcred.pcr_flag = 0;
	/* user level is ingnoring the pcred anyway */

	pcred.pcr_uid = p->p_ucred->cr_uid;
	pcred.pcr_ngroups = p->p_ucred->cr_ngroups;
	bcopy(p->p_ucred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));

	error = webdav_sendmsg(vnop, WEBDAV_USE_URL, pt, &pcred, fmp, p, (void *)NULL, 0,
		&server_error, (void *)NULL, 0);
	if (error)
	{
		goto bad;
	}

	if (WEBDAV_CHECK_VNODE(vp))
	{
		error = EPERM;
	}
	else
	{
		if (server_error)
			error = server_error;
	}

	/* fall through to bad */

bad:

	if (ap->a_dvp)
	{
		vput(ap->a_dvp);
	}

	vput(vp);
	RET_ERR("webdav_rmdir", error);
}												/* webdav_rmdir */

/*****************************************************************************/

int webdav_create(ap)
struct vop_create_args	/* {
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
	struct vattr *a_vap;
	} */ *ap;
{
	struct componentname *cnp = ap->a_cnp;
	struct vnode **vpp = ap->a_vpp;
	struct vnode *dvp = ap->a_dvp;
	struct webdavnode *pt = NULL;
	struct webdavmount *fmp;
	int error = 0;
	int server_error = 0;
	struct proc *p = current_proc();
	struct timeval tv;
	int vnop = WEBDAV_FILE_CREATE;
	int existing_node;
	struct webdav_cred pcred;

	/* 
	 * Set *vpp to Null for error checking purposes
	 */

	*vpp = NULL;

	/*
	 *	We don't support special files so make sure this is a regular file
	 */

	if (ap->a_vap->va_type != VREG)
	{
		error = EOPNOTSUPP;
		goto bad;
	}

	error = webdav_temp_lock(dvp, p);
	if (error)
	{
		return (EINVAL);
	}

	/*
	 *	Make the vnode.	 Lookup should have taken care of us trying to move
	 *	out of the directory so if we wind up with an existing node we have
	 *	no idea what to do. Throw up our hands and go home. 
	 */

	error = webdav_buildnewvnode(dvp, vpp, cnp, &existing_node);
	if (error || existing_node)
	{
		if (!error)
		{
			error = EINVAL;
		}
		goto bad;
	}

	pt = VTOWEBDAV(*vpp);
	fmp = VFSTOWEBDAV((*vpp)->v_mount);
	(*vpp)->v_type = VREG;

	pcred.pcr_flag = 0;
	/* user level is ingnoring the pcred anyway */

	pcred.pcr_uid = cnp->cn_cred->cr_uid;
	pcred.pcr_ngroups = cnp->cn_cred->cr_ngroups;
	bcopy(cnp->cn_cred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));

	error = webdav_sendmsg(vnop, WEBDAV_USE_URL, pt, &pcred, fmp, p, (void *)NULL, 0,
		&server_error, (void *)NULL, 0);
	if (error)
	{
		goto bad;
	}

	if (server_error)
	{
		error = server_error;
		goto bad;
	}

	/* We pended so check the state of the vnode */

	if (WEBDAV_CHECK_VNODE(*vpp))
	{
		error = EIO;
		goto bad;
	}

	/* Fill in the times with the current time */

	microtime(&tv);
	TIMEVAL_TO_TIMESPEC(&tv, &pt->pt_atime);
	pt->pt_mtime = pt->pt_atime;
	pt->pt_ctime = pt->pt_atime;

	pt->pt_vnode = *vpp;

	/* This is a new vnode which so set up the UBC info */

	if (((*vpp)->v_type == VREG) && (UBCINFOMISSING(*vpp) || UBCINFORECLAIMED(*vpp)))
	{
		error = ubc_info_init(*vpp);
		if (error)
		{
			goto bad;
		}
	}

	/* ubc_info_init() may have blocked, check the state of the vnode */

	if (WEBDAV_CHECK_VNODE(*vpp))
	{
		error = EIO;
		goto bad;
	}

	/*
	 * Note that since we want all callers to have their own
	 * up to date copy of a directory so we won't put directories
	 * in the cache.  Right now, we only create VREG's so it's
	 * impossible not to suceed in this case but it never hurts to
	 * be paranoid.
	 */

	if ((*vpp)->v_type == VREG)
	{
		webdav_hashins(pt);
	}

	/* Note, if you add code here, make sure you clean up
	  the ubc stuff (if necessary) on an error case before
	  exiting */

bad:

	if ((cnp->cn_flags & (HASBUF | SAVESTART)) == HASBUF)
	{
		FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
	}
	
	vput(dvp);

	/* build new vnode will have returned a referenced vnode. That
	 * means if we are returning an error and we have obtained a
	 * vnode we have to dereference it or we won't be able to
	 * unmount.
	 */

	if (error && *vpp)
	{
		vrele(*vpp);
	}

	(void)webdav_temp_unlock(dvp, p);
	RET_ERR("webdav_create", error);
}

/*****************************************************************************/

int webdav_rename(ap)
	struct vop_rename_args	/* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
		} */ *ap;
{
	struct componentname *fcnp = ap->a_fcnp;
	struct componentname *tcnp = ap->a_tcnp;
	struct vnode *fvp = ap->a_fvp;
	struct vnode *tvp = ap->a_tvp;
	struct vnode *tdvp = ap->a_tdvp;
	struct webdavnode *fpt;
	struct webdavnode *tpt;
	struct webdavmount *fmp;
	int error = 0, server_error = 0, created_object = 0, existing_object = 0;
	struct proc *p = current_proc();
	int vnop = WEBDAV_RENAME;
	webdav_rename_header_t * rename_header;
	int message_size;
	struct webdav_cred pcred;

	/*
	 *	If tvp is null, Make the vnode.	 
	 */

	if (tvp == NULL)
	{
		error = webdav_buildnewvnode(tdvp, &tvp, tcnp, &existing_object);
		if (error)
		{
			goto done;
		}
		if (!existing_object)
		{

			/* if we are creating a new regular file, set the ubc info */
			if ((fvp->v_type == VREG) && (UBCINFOMISSING(tvp) || UBCINFORECLAIMED(tvp)))
			{
				error = ubc_info_init(tvp);
				if (error)
				{
					goto done;
				}
			}
			created_object = TRUE;
		}
	}

	/* ubc_info_init() may have blocked, check the state of the vnode */

	if (WEBDAV_CHECK_VNODE(tvp))
	{
		error = EIO;
		goto done;
	}

	fpt = VTOWEBDAV(fvp);
	tpt = VTOWEBDAV(tvp);
	fmp = VFSTOWEBDAV((fvp)->v_mount);

	/*
	 *	Check to if we are renaming something to itself.  If so
	 *	we should do nothing.  We had to check here becuase only
	 *	we know if the URL's actually match.  The vnode cache isn't being
	 *	used yet so every instantiation of a file has its own vnode at
	 *	the moment.
	 */

	if (fpt->pt_size == tpt->pt_size && !bcmp(fpt->pt_arg, tpt->pt_arg, fpt->pt_size))
	{
		error = 0;
		goto done;
	}

	/*
	 * Get enough space for the rename info we will be sending to the
	 * user process
	 */

	message_size = sizeof(webdav_rename_header_t) + fpt->pt_size + tpt->pt_size;

	MALLOC(rename_header, void *, message_size, M_TEMP, M_WAITOK);

	/* We pended so check the state of the vnode */

	if (WEBDAV_CHECK_VNODE(fvp))
	{
		error = ENOENT;
		goto dealloc_done;
	}
	if (WEBDAV_CHECK_VNODE(tvp))
	{
		error = EIO;
		goto dealloc_done;
	}

	rename_header->wd_first_uri_size = fpt->pt_size;
	rename_header->wd_second_uri_size = tpt->pt_size;

	bcopy(fpt->pt_arg, ((char *)rename_header) + sizeof(webdav_rename_header_t), fpt->pt_size);
	bcopy(tpt->pt_arg, ((char *)rename_header) + sizeof(webdav_rename_header_t) + fpt->pt_size,
		tpt->pt_size);

	pcred.pcr_flag = 0;
	/* user level is ingnoring the pcred anyway */

	pcred.pcr_uid = fcnp->cn_cred->cr_uid;
	pcred.pcr_ngroups = fcnp->cn_cred->cr_ngroups;
	bcopy(fcnp->cn_cred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));

	error = webdav_sendmsg(vnop, WEBDAV_USE_INPUT, fpt, &pcred, fmp, p, (void *)rename_header,
		message_size, &server_error, (void *)NULL, 0);

	if (WEBDAV_CHECK_VNODE(fvp))
	{
		error = ENOENT;
		goto dealloc_done;
	}
	if (WEBDAV_CHECK_VNODE(tvp))
	{
		error = EIO;
		goto dealloc_done;
	}

	if (!error && server_error)
	{
		error = server_error;
	}

	/* delete the source, and the destination (if the destination existed before)
	  from the cache */
	if (!error)
	{
		webdav_hashrem(fpt);
		if (tvp || existing_object)
		{
			tpt->pt_status |= WEBDAV_DELETED;
			webdav_hashrem(tpt);
		}
	}


	/* fall through */

dealloc_done:

	FREE((void *)rename_header, M_TEMP);

done:

	if (tvp == tdvp)
	{
		vrele(tdvp);
	}
	else
	{
		vput(tdvp);
	}

	if (tvp)
	{
		vput(tvp);
	}

	vrele(fvp);
	vrele(ap->a_fdvp);

	RET_ERR("webdav_rename", error);
}

/*****************************************************************************/

int webdav_mkdir(ap)
	struct vop_create_args	/* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		} */ *ap;
{
	struct componentname *cnp = ap->a_cnp;
	struct vnode **vpp = ap->a_vpp;
	struct vnode *dvp = ap->a_dvp;
	struct webdavnode *pt;
	struct webdavmount *fmp;
	int error = 0;
	int server_error = 0;
	struct proc *p = current_proc();
	int vnop = WEBDAV_DIR_CREATE;
	int existing_node;
	struct webdav_cred pcred;

	/*
	 * Set *vpp to Null for error checking purposes
	 */

	*vpp = NULL;

	/*
	 *	Make sure this is a directory
	 */

	if (ap->a_vap->va_type != VDIR)
	{
		RET_ERR("webdav_mkdir", ENOTDIR);
	}

	/*
	 *	Make the vnode.	 Lookup should have taken care of us trying to move
	 *	out of the directory so if we wind up with an existing node we have
	 *	no idea what to do. Throw up our hands and go home.
	 */
	error = webdav_buildnewvnode(dvp, vpp, cnp, &existing_node);
	if (error || existing_node)
	{
		if (error)
		{
			RET_ERR("webdav_mkdir", error);
		}
		else
		{
			RET_ERR("webdav_mkdir", EINVAL);
		}
	}

	pt = VTOWEBDAV(*vpp);
	fmp = VFSTOWEBDAV((*vpp)->v_mount);
	(*vpp)->v_type = VDIR;

	pcred.pcr_flag = 0;
	/* user level is ingnoring the pcred anyway */

	pcred.pcr_uid = cnp->cn_cred->cr_uid;
	pcred.pcr_ngroups = cnp->cn_cred->cr_ngroups;
	bcopy(cnp->cn_cred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));

	error = webdav_sendmsg(vnop, WEBDAV_USE_URL, pt, &pcred, fmp, p, (void *)NULL, 0,
		&server_error, (void *)NULL, 0);

	if (!error && server_error)
	{
		error = server_error;
	}

	if ((cnp->cn_flags & (HASBUF | SAVESTART)) == HASBUF)
	{
		FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
	}

	vput(dvp);

	/* build new vnode will have returned a referenced vnode. That
	 * means if we are returning an error and we have obtained a
	 * vnode we have to dereference it or we won't be able to
	 * unmount.
	 */

	if (error && *vpp)
	{
		vrele(*vpp);
	}

	RET_ERR("webdav_mkdir", error);
}

/*****************************************************************************/

int webdav_setattr(ap)
	struct vop_setattr_args	/* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
		} */ *ap;
{

	int error = 0;
	struct webdavnode *pt;
	struct vnode *cachevp;
	int done = FALSE;
	struct vattr attrbuf;
	struct vattr *vap = ap->a_vap;

	pt = VTOWEBDAV(ap->a_vp);
	cachevp = pt->pt_cache_vnode;
	/*
	 * Can't mess with the root vnode
	 */
	if (ap->a_vp->v_flag & VROOT)
	{
		RET_ERR("webdav_setattr root vnode", EACCES);
	}

	/* Get out of dodge if this is a readonly mount */

	if (((vap->va_flags != VNOVAL) ||
		(vap->va_uid != (uid_t)VNOVAL) ||
		(vap->va_gid != (gid_t)VNOVAL) || 
		(vap->va_size != VNOVAL) || 
		(vap->va_atime.tv_sec != VNOVAL) || 
		(vap->va_mtime.tv_sec != VNOVAL) || 
		(vap->va_mode != (mode_t)VNOVAL)) && (ap->a_vp->v_mount->mnt_flag & MNT_RDONLY))
	{
		return (EROFS);
	}

	/* If there is a local cache file, we'll
	  allow setting.	We won't talk to the
	  server, but we will honor the local file
	  set.  This will at least make fsx work
	*/

	if (cachevp)
	{
		error = VOP_LOCK(cachevp, LK_EXCLUSIVE | LK_RETRY, ap->a_p);
		if (error)
		{
			RET_ERR("webdav_setattr lock", error);
		}

		if (WEBDAV_CHECK_VNODE(ap->a_vp))
		{
			error = EPERM;
			goto exit;
		}

		/* if we are changing the size, call ubcsetsize to fix things up
		 * with the UBC Also, make sure that we wait until the file is
		 * completely downloaded */

		if ((ap->a_vap->va_size != VNOVAL) && (ap->a_vp->v_type == VREG))
		{
			do
			{
				error = VOP_GETATTR(cachevp, &attrbuf, ap->a_cred, ap->a_p);
				if (error)
				{
					goto unlock_exit;
				}

				if (attrbuf.va_flags & UF_NODUMP)
				{
					/* We are downloading the file and we haven't finished
					 * since the user process is going to extend the file with
					 * it's writes until it is done, we will just have to wait for
					 * it to finish.
					 */

					VOP_UNLOCK(cachevp, 0, ap->a_p);
					error = tsleep(&lbolt, PCATCH, "webdavdownload", 10);
#ifdef DEBUG
					if (error && error != EWOULDBLOCK)
					{
						printf("webdav_setattr: tsleep returned %d\n", error);
					}
#endif

					error = 0;

					/* After pending on tsleep, check the state of the vnode */
					if (WEBDAV_CHECK_VNODE(ap->a_vp))
					{
						error = EPERM;
						goto exit;
					}

					error = VOP_LOCK(cachevp, LK_EXCLUSIVE | LK_RETRY, ap->a_p);
					if (error)
					{
						goto exit;
					}

					if (WEBDAV_CHECK_VNODE(ap->a_vp))
					{
						error = EPERM;
						goto exit;
					}

				}
				else
				{
					done = TRUE;
				}
			} while (!done);

			if (attrbuf.va_flags & UF_APPEND)
			{
				error = EIO;
				goto unlock_exit;
			}

			/* If the size of the file is changed, set WEBDAV_DIRTY.
			 * WEBDAV_DIRTY is not set for other cases of setattr
			 * because we don't actually save va_flags, va_mode,
			 * va_uid, va_gid, va_atime or va_mtime on the server.
			 *
			 * XXX -- Eventually, we need to add code to call
			 * webdav_lock() in webdav_file.c to make sure we have
			 * a LOCK on the WebDAV server, so we don't try to PUT
			 * with no lock. The way things work now, the truncate
			 * might not stick if the file on the server isn't open
			 * with write access and some other client of the server
			 * has a WebDAV LOCK on the file (in which case fsync will
			 * fail with EBUSY when it tries to PUT the file to the
			 * server).
			 */
			if ( ap->a_vap->va_size != attrbuf.va_size )
			{
				pt->pt_status |= WEBDAV_DIRTY;
			}
			
			if (UBCINFOEXISTS(ap->a_vp))
			{
				if (!ubc_setsize(ap->a_vp, (off_t)ap->a_vap->va_size))
				/* XXX Check Errors, and do what? XXX */
					printf("webdav_setattr: ubc_setsize error\n");
			}
		}

		error = VOP_SETATTR(cachevp, ap->a_vap, ap->a_cred, ap->a_p);

unlock_exit:

		/* after the write to the cache file has been completed... */
		VOP_UNLOCK(cachevp, 0, ap->a_p);

	}

exit:

	RET_ERR("webdav_setattr", error);
}

/*****************************************************************************/

/*
 * If we are doing readdir, we must have opened the
 * directory first so we'll use the cache file for
 * the needed I/O.
 */
int webdav_readdir(ap)
	struct vop_readdir_args	/* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		u_long *a_cookies;
		int a_ncookies;
		} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct vnode *cache_vp;
	struct webdavnode *pt;
	struct webdav_cred pcred;
	struct webdavmount *fmp;
	int server_error;
	register struct uio *uio = ap->a_uio;
	int error = 0;
	size_t count, lost;
	struct vattr vattr;
	struct proc *p = current_proc();

	/*
	 * We don't allow exporting webdav mounts, and currently local
	 * requests do not need cookies.
	 */

	if (ap->a_ncookies)
	{
		panic("webdav_readdir: not hungry");
	}

	/* First make sure it is a directory we are dealing with */

	if (vp->v_type != VDIR)
	{
		RET_ERR("webdav_readdir", ENOTDIR);
	}

	pt = VTOWEBDAV(vp);
	fmp = VFSTOWEBDAV(vp->v_mount);

	/* Are we starting from the begininng ?	 If so check the
	 * enumerated status.  If it is set, clear it and refresh
	 * the directory.  If it is not set, set  it so that the
	 * next time around we will do a refresh */

	if ((uio->uio_offset == 0) || (pt->pt_status & WEBDAV_DIR_NOT_LOADED))
	{
		bzero(&pcred, sizeof(pcred));
		if (ap->a_cred)
		{
			pcred.pcr_uid = ap->a_cred->cr_uid;
			pcred.pcr_ngroups = ap->a_cred->cr_ngroups;
			bcopy(ap->a_cred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));
		}

		error = webdav_sendmsg(WEBDAV_DIR_REFRESH, WEBDAV_USE_HANDLE, pt, &pcred, fmp,
			p, (void *)NULL, 0, &server_error, (void *)NULL, 0);
		if (error)
		{
			goto done;
		}

		if (server_error)
		{
			error = server_error;
			goto done;
		}

		/* We pended so check the state of the vnode */

		if (WEBDAV_CHECK_VNODE(vp))
		{
			/* Permissiond denied seems to be what comes back
			 * when you try to enumerate a no longer existing
			 * directory */

			error = EPERM;
			goto done;
		}


		/* We didn't get an error so turn off the dir not loaded bit */

		pt->pt_status &= ~WEBDAV_DIR_NOT_LOADED;

	}
	/* end if offset == 0 */

	/* Make sure we do have a cache file.  If not the call
	 * must be wrong some how */

	cache_vp = pt->pt_cache_vnode;
	if (!cache_vp)
	{
		RET_ERR("webdav_readdir", EINVAL);
	}

	/* Make sure we don't return partial entries. */
	if (uio->uio_offset % sizeof(struct dirent) || uio->uio_resid < sizeof(struct dirent))
	{
		RET_ERR("webdav_readdir", EINVAL);
	}

	count = uio->uio_resid;
	count -= (uio->uio_offset + count) % sizeof(struct dirent);
	if (count <= 0)
	{
		RET_ERR("webdav_readdir", EINVAL);
	}

	lost = uio->uio_resid - count;
	uio->uio_resid = count;
	uio->uio_iov->iov_len = count;

	error = VOP_LOCK(cache_vp, LK_SHARED | LK_RETRY, p);
	if (error)
	{
		RET_ERR("webdav_readdir", error);
	}

	if (WEBDAV_CHECK_VNODE(vp))
	{
		error = EPERM;
		goto done;
	}

	error = VOP_READ(cache_vp, uio, 0, ap->a_cred);
	VOP_UNLOCK(cache_vp, 0, p);
	uio->uio_resid += lost;
	if (ap->a_eofflag)
	{
		error = VOP_GETATTR(cache_vp, &vattr, ap->a_cred, p);
		if (error)
		{
			RET_ERR("webdav_readdir getattr", error);
		}
		*ap->a_eofflag = vattr.va_size <= uio->uio_offset;
	}

done:

	RET_ERR("webdav_readdir", error);
}

/*****************************************************************************/

int webdav_inactive(ap)
	struct vop_inactive_args	/* {
		struct vnode *a_vp;
		struct proc *a_p;
		} */ *ap;
{
	struct webdavnode *pt;
	struct proc *p = ap->a_p;
	struct vnode *vp = ap->a_vp;
	int error = 0, server_error = 0;
	struct webdavmount *fmp;
	struct webdav_cred pcred;
	int vnop = WEBDAV_CLOSE;

	webdav_hashrem(ap->a_vp->v_data);
	pt = VTOWEBDAV(vp);
	fmp = VFSTOWEBDAV(vp->v_mount);

	/* Now with UBC it may be possible that the "last close"
	 * isn't really seen by the file system until inactive
	 * is called.  Thus if we see a cache file and a file_handle
	 * for this vnode, we should tell the server process to close
	 * up.	This is because the call sequence of mmap is open()
	 * mmap(), close().	 After this point I/O can go on to
	 * the file and the vm system will be holding the reference.
	 * Not until the process dies and vm cleans up do the mappings
	 * go away and the file can be safely pushed back to the
	 * server.
	 */

	if (pt->pt_file_handle != -1)
	{
		pcred.pcr_flag = 0;
		pcred.pcr_uid = p->p_ucred->cr_uid;
		pcred.pcr_ngroups = p->p_ucred->cr_ngroups;
		bcopy(p->p_ucred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));

		error = webdav_sendmsg(vnop, WEBDAV_USE_HANDLE, pt, &pcred, fmp, p, (void *)NULL, 
			0, &server_error, (void *)NULL, 0);

		/* We pended so check the state of the vnode */

		if (WEBDAV_CHECK_VNODE(ap->a_vp))
		{
			/* OK if the vnode went bad when we were going to
			 * mark it inactive anyway */

			error = 0;
			goto exit;
		}

		if ((!error) && server_error)
		{
			error = server_error;
		}
		if (error)
		{
			goto unlock;
		}

		/* we're the last closer so clean up
		 * We don't want to talk to the server again so
		 * get rid of the file handle also
		 * if we have a cached node, time to release it */

		pt->pt_file_handle = -1;
		if (pt->pt_cache_vnode)
		{
			struct vnode *temp;
			
			/* zero out pt_cache_vnode and then release the cache vnode */
			temp = pt->pt_cache_vnode;
			pt->pt_cache_vnode = 0;
			vrele(temp);
		}
	}

unlock:

	VOP_UNLOCK(ap->a_vp, 0, ap->a_p);

exit:

	RET_ERR("webdav_inactive", error);
}

/*****************************************************************************/

int webdav_reclaim(ap)
	struct vop_reclaim_args	/* {
		struct vnode *a_vp;
		} */ *ap;
{
	struct webdavnode *pt = VTOWEBDAV(ap->a_vp);

	if (pt->pt_arg)
	{
		FREE((caddr_t)pt->pt_arg, M_TEMP);
		pt->pt_arg = 0;
	}
	FREE(ap->a_vp->v_data, M_TEMP);
	ap->a_vp->v_data = 0;

	return (0);
}

/*****************************************************************************/

/*
 * Return POSIX pathconf information applicable to special devices.
 */
int webdav_pathconf(ap)
	struct vop_pathconf_args	/* {
		struct vnode *a_vp;
		int a_name;
		int *a_retval;
		} */ *ap;
{

	switch (ap->a_name)
	{
		case _PC_LINK_MAX:
			*ap->a_retval = LINK_MAX;
			return (0);
			
		case _PC_MAX_CANON:
			*ap->a_retval = MAX_CANON;
			return (0);
			
		case _PC_MAX_INPUT:
			*ap->a_retval = MAX_INPUT;
			return (0);
			
		case _PC_NAME_MAX:
			*ap->a_retval = MAXPATHLEN;
			return (0);
			
		case _PC_PIPE_BUF:
			*ap->a_retval = PIPE_BUF;
			return (0);
			
		case _PC_CHOWN_RESTRICTED:
			*ap->a_retval = 1;
			return (0);
			
		case _PC_VDISABLE:
			*ap->a_retval = _POSIX_VDISABLE;
			return (0);
			
		default:
			RET_ERR("webdav_pathconf", EINVAL);
	}
	/* NOTREACHED */
}

/*****************************************************************************/

/*
 * Print out the contents of a webdav vnode.
 */
/* ARGSUSED */
int webdav_print(ap)
	struct vop_print_args	/* {
		struct vnode *a_vp;
		} */ *ap;
{
	printf("tag VT_WEBDAV, webdav vnode\n");
	return (0);
}

/*****************************************************************************/

/*void*/
int webdav_vfree(ap)
	struct vop_vfree_args	/* {
		struct vnode *a_pvp;
		ino_t a_ino;
		int a_mode;
		} */ *ap;
{
	return (0);
}

/*****************************************************************************/

int webdav_pagein(ap)
	struct vop_pagein_args	/* {
		struct vnode *a_vp,
		upl_t	a_pl,
		vm_offset_t	  a_pl_offset,
		off_t		  a_f_offset,
		size_t		  a_size,
		struct ucred *a_cred,
		int			  a_flags
		} */ *ap;
{
	vm_offset_t ioaddr;
	struct uio auio;
	struct proc *p = current_proc();
	struct vnode *cachevp;
	struct webdavnode *pt;
	struct iovec aiov;
	struct uio *uio = &auio;
	int nocommit = ap->a_flags & UPL_NOCOMMIT;
	int ioflag = (ap->a_flags & UPL_IOSYNC) ? IO_SYNC : 0;
	int bytes_to_zero;
	int error = 0;
	int done = FALSE;
	int tried_bytes = FALSE;
	struct vattr attrbuf;
	kern_return_t kret;

	pt = VTOWEBDAV(ap->a_vp);
	cachevp = pt->pt_cache_vnode;

	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = ap->a_f_offset;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_READ;
	auio.uio_resid = ap->a_size;
	auio.uio_procp = NULL;
	aiov.iov_len = ap->a_size;

	kret = ubc_upl_map(ap->a_pl, &ioaddr);
	if (kret != KERN_SUCCESS)
	{
		panic("webdav_pagein: ubc_upl_map() failed with (%d)", kret);
	}

	ioaddr += ap->a_pl_offset;	/* add a_pl_offset */
	aiov.iov_base = (caddr_t)ioaddr;

	/* Ok, start the sleep loop to wait on the background download
	  We will know that the webdav user process is finished when it
	  either clears the nodump flag or sets the append only flag
	  (indicating an error) */

	do
	{
		/* Set LK_CANRECURSE on the lock. webdav_setattr needs to lock the cache file
		 * during its entire operation. That includes the call to ubc_setsize which
		 * when truncating can force a pagein. Thus we can be called with
		 * ourselves holding the lock.
		 */
		error = VOP_LOCK(cachevp, LK_EXCLUSIVE | LK_RETRY | LK_CANRECURSE, p);
		if (error)
		{
			goto exit;
		}

		if (WEBDAV_CHECK_VNODE(ap->a_vp))
		{
			error = EPERM;
			goto exit;
		}

		error = VOP_GETATTR(cachevp, &attrbuf, ap->a_cred, p);
		if (error)
		{
			goto unlock_exit;
		}

		if (WEBDAV_CHECK_VNODE(ap->a_vp))
		{
			error = EPERM;
			goto exit;
		}

		if ((attrbuf.va_flags & UF_NODUMP) && (auio.uio_offset + auio.uio_resid) > attrbuf.va_size)
		{
			/* We are downloading the file and we haven't gotten to
			 * to the bytes we need so unlock, sleep, and try the whole
			 * thing again.	We will take one shot at trying to get the
			 * bytes out of the file directly if that part hasn't yet
			 * been downloaded.	This is a little iffy since the VM system
			 * may now be chaching data that could theoritically be out of
			 * sync with what's on the server.  That is the following sequence
			 * of operations could lead to strange results:
			 * 1. I start a read and begin a down load
			 * 2. Another client changes the file
			 * 3. I do a byte read of the end of the file and get the new data
			 * 4. The download finishes and the underlying cash file has
			 *	 the old data, possibly depending on how the server works.
			 */
			VOP_UNLOCK(cachevp, 0, p);
			if (!tried_bytes)
			{
				if ((auio.uio_offset + auio.uio_resid) > (attrbuf.va_size + WEBDAV_WAIT_IF_WITHIN))
				{
					error = webdav_read_bytes(ap->a_vp, &auio, ap->a_cred);
					if (!error)
					{
						if (!uio->uio_resid)
						{
							goto exit;
						}
						else
						{
							/* we did not get all the data we wanted, we don't
							* know why so we'll just give up on the byte access
							* and wait for the data to download.	 We need to reset
							* the uio in that case since the VM system is not going
							* to be happy with partial reads
							*/
							auio.uio_iov = &aiov;
							auio.uio_iovcnt = 1;
							auio.uio_offset = ap->a_f_offset;
							auio.uio_segflg = UIO_SYSSPACE;
							auio.uio_rw = UIO_READ;
							auio.uio_resid = ap->a_size;
							auio.uio_procp = NULL;
							aiov.iov_base = (caddr_t)ioaddr;
							aiov.iov_len = ap->a_size;
						}
					}
				}
				/* If we are here, we must have failed to get the bytes so set
				 * tried_bytes so we won't make this mistake again and sleep */

				tried_bytes = TRUE;
			}

			error = tsleep(&lbolt, PCATCH, "webdavdownload", 10);
#ifdef DEBUG
			if (error && error != EWOULDBLOCK)
			{
				printf("webdav_pagein: tsleep returned %d\n", error);
			}
#endif

			error = 0;

			/* After pending on tsleep, check the state of the vnode */
			if (WEBDAV_CHECK_VNODE(ap->a_vp))
			{
				error = EPERM;
				goto exit;
			}
		}
		else
		{
			/* the part we need has been downloaded 
			  and cachevp is still VOP_LOCK'ed */
			done = TRUE;
		}
	} while (!done);

	if (attrbuf.va_flags & UF_APPEND)
	{
		error = EIO;
		goto unlock_exit;
	}

	error = VOP_GETATTR(cachevp, &attrbuf, ap->a_cred, p);
	if (error)
	{
		goto unlock_exit;
	}

	if (ap->a_f_offset > attrbuf.va_size)
	{
		/* Trying to pagein data beyond the eof is a no no */
		error = EFAULT;
		goto unlock_exit;
	}

	error = VOP_READ(cachevp, uio, ioflag, ap->a_cred);

	if (uio->uio_resid)
	{
		/* If we were not able to read the entire page, check to
		 * see if we are at the end of the file, and if so, zero
		 * out the remaining part of the page
		 */

		if (attrbuf.va_size < ap->a_f_offset + ap->a_size)
		{
			bytes_to_zero = ap->a_f_offset + ap->a_size - attrbuf.va_size;
			bzero((caddr_t)(ioaddr + ap->a_size - bytes_to_zero), bytes_to_zero);
		}

	}

unlock_exit:

	VOP_UNLOCK(cachevp, 0, p);


exit:

	kret = ubc_upl_unmap(ap->a_pl);

	if (kret != KERN_SUCCESS)
	{
        panic("webdav_pagein: ubc_upl_unmap() failed with (%d)", kret);
	}

	if (!nocommit)
	{
		if (!error)
		{
			kret = ubc_upl_commit_range(ap->a_pl, ap->a_pl_offset, ap->a_size, UPL_COMMIT_FREE_ON_EMPTY);
		}
		else
		{
			kret = ubc_upl_abort_range(ap->a_pl, ap->a_pl_offset, ap->a_size, UPL_ABORT_ERROR | UPL_ABORT_FREE_ON_EMPTY);
		}
	}

	RET_ERR("webdav_pagein", error);
}

/*****************************************************************************/

int webdav_pageout(ap)
	struct vop_pageout_args	/* {
		struct vnode *a_vp,
		upl_t	a_pl,
		vm_offset_t	  a_pl_offset,
		off_t		  a_f_offset,
		size_t		  a_size,
		struct ucred *a_cred,
		int			  a_flags
		} */ *ap;
{
	vm_offset_t ioaddr;
	struct uio auio;
	struct proc *p = current_proc();
	struct vnode *cachevp;
	struct webdavnode *pt;
	struct iovec aiov;
	struct uio *uio = &auio;
	int nocommit = ap->a_flags & UPL_NOCOMMIT;
	int ioflag = (ap->a_flags & UPL_IOSYNC) ? IO_SYNC : 0;
	int error = 0;
	int done = FALSE;
	kern_return_t kret;
	struct vattr attrbuf;
	
	pt = VTOWEBDAV(ap->a_vp);
	cachevp = pt->pt_cache_vnode;

	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = ap->a_f_offset;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_WRITE;
	auio.uio_resid = ap->a_size;
	auio.uio_procp = NULL;
	aiov.iov_len = ap->a_size;

	kret = ubc_upl_map(ap->a_pl, &ioaddr);
	if (kret != KERN_SUCCESS)
	{
        panic("webdav_pageout: ubc_upl_map() failed with (%d)", kret);
	}

	ioaddr += ap->a_pl_offset;	/* add a_pl_offset */
	aiov.iov_base = (caddr_t)ioaddr;

	if (ap->a_vp->v_mount->mnt_flag & MNT_RDONLY)
	{
		error = EROFS;
		goto exit;
	}

	do
	{
		/* Set Can recurse on the lock.	WebDAV setattr needs to lock the cache file
		 * during it's entire operation.	 That includes the call to setsize which
		 * can invalidate a page and force a pageout. Thus we can be called with
		 * ourselves holding the lock.
		 */
		error = VOP_LOCK(cachevp, LK_EXCLUSIVE | LK_RETRY | LK_CANRECURSE, p);
		if (error)
		{
			goto exit;
		}

		if (WEBDAV_CHECK_VNODE(ap->a_vp))
		{
			error = EPERM;
			goto exit;
		}

		error = VOP_GETATTR(cachevp, &attrbuf, ap->a_cred, p);
		if (error)
		{
			goto unlock_exit;
		}

		if (WEBDAV_CHECK_VNODE(ap->a_vp))
		{
			error = EPERM;
			goto exit;
		}

		if ((attrbuf.va_flags & UF_NODUMP) && (auio.uio_offset + auio.uio_resid) > attrbuf.va_size)
		{
			/* We are downloading the file and we haven't gotten to
			 * to the bytes we need so unlock, sleep, and try the whole
			 * thing again.
			 */

			VOP_UNLOCK(cachevp, 0, p);
			error = tsleep(&lbolt, PCATCH, "webdavdownload", 10);
#ifdef DEBUG
			if (error && error != EWOULDBLOCK)
			{
				printf("webdav_pageout: tsleep returned %d\n", error);
			}
#endif

			error = 0;

			/* After pending on tsleep, check the state of the vnode */
			if (WEBDAV_CHECK_VNODE(ap->a_vp))
			{
				error = EPERM;
				goto exit;
			}

		}
		else
		{
			/* download has finished and cachevp is still VOP_LOCK'ed */
			done = TRUE;
		}
	} while (!done);

	if (attrbuf.va_flags & UF_APPEND)
	{
		error = EIO;
		goto unlock_exit;
	}

	/* We don't want to write past the end of the file so 
	 * truncate the write to the size.
	 */

	if (auio.uio_offset + auio.uio_resid > attrbuf.va_size)
	{
		if (auio.uio_offset < attrbuf.va_size)
		{
			auio.uio_resid = attrbuf.va_size - auio.uio_offset;
		}
		else
		{
			/* If we are here, someone probably truncated a file that
			 * some one else had mapped.  In any event we are not allowed
			 * to grow the file on a page out so return EFAULT as that is
			 * what VM is expecting.
			 */
			error = EFAULT;
			goto unlock_exit;
		}
	}

	error = VOP_WRITE(cachevp, uio, ioflag, ap->a_cred);


unlock_exit:

	/* after the write to the cache file has been completed... */
	pt->pt_status |= WEBDAV_DIRTY;

	VOP_UNLOCK(cachevp, 0, p);

exit:

	kret = ubc_upl_unmap(ap->a_pl);

	if (kret != KERN_SUCCESS)
	{
		panic("webdav_pageout: ubc_upl_unmap() failed with (%d)", kret);
	}

	if (!nocommit)
	{
		if (!error)
		{
			kret = ubc_upl_commit_range(ap->a_pl, ap->a_pl_offset, ap->a_size, UPL_COMMIT_FREE_ON_EMPTY);
		}
		else
		{
			kret = ubc_upl_abort_range(ap->a_pl, ap->a_pl_offset, ap->a_size, UPL_ABORT_ERROR | UPL_ABORT_FREE_ON_EMPTY);
		}
	}

	RET_ERR("webdav_pageout", error);
}

/*****************************************************************************/

/*
 * webdav vnode unsupported operation
 */
int webdav_enotsupp()
{
	RET_ERR("webdav_enotsupp", EOPNOTSUPP);
}

/*****************************************************************************/

/*
 * webdav "should never get here" operation
 */
int webdav_badop()
{
	panic("webdav: bad op");
	/* NOTREACHED */
	return (EOPNOTSUPP);						/* included to shut up the compiler warning */
}

/*****************************************************************************/

/*
 * webdav vnode null operation
 */
int webdav_nullop()
{
	return (0);
}

/*****************************************************************************/

#define VOPFUNC int (*)(void *)

int( **webdav_vnodeop_p)();

struct vnodeopv_entry_desc webdav_vnodeop_entries[] = {
	{&vop_default_desc, (VOPFUNC)vn_default_error},
	{&vop_lookup_desc, (VOPFUNC)webdav_lookup},			/* lookup */
	{&vop_create_desc, (VOPFUNC)webdav_create},			/* create */
	{&vop_mknod_desc, (VOPFUNC)err_mknod},				/* mknod */
	{&vop_mkcomplex_desc, (VOPFUNC)err_mkcomplex},		/* mkcompelx */
	{&vop_open_desc, (VOPFUNC)webdav_open},				/* open */
	{&vop_close_desc, (VOPFUNC)webdav_close},			/* close */
	{&vop_access_desc, (VOPFUNC)webdav_access},			/* access */
	{&vop_getattr_desc, (VOPFUNC)webdav_getattr},		/* getattr */
	{&vop_setattr_desc, (VOPFUNC)webdav_setattr},		/* setattr */
	{&vop_read_desc, (VOPFUNC)webdav_read},				/* read */
	{&vop_write_desc, (VOPFUNC)webdav_write},			/* write */
	{&vop_ioctl_desc, (VOPFUNC)err_ioctl},				/* ioctl */
	{&vop_select_desc, (VOPFUNC)err_select},			/* select */
	{&vop_mmap_desc, (VOPFUNC)err_mmap},				/* mmap */
	{&vop_revoke_desc, (VOPFUNC)err_revoke},			/* revoke */
	{&vop_fsync_desc, (VOPFUNC)webdav_fsync},			/* fsync */
	{&vop_seek_desc, (VOPFUNC)err_seek},				/* seek */
	{&vop_remove_desc, (VOPFUNC)webdav_remove},			/* remove */
	{&vop_link_desc, (VOPFUNC)err_link},				/* link */
	{&vop_rename_desc, (VOPFUNC)webdav_rename},			/* rename */
	{&vop_mkdir_desc, (VOPFUNC)webdav_mkdir},			/* mkdir */
	{&vop_rmdir_desc, (VOPFUNC)webdav_rmdir},			/* rmdir */
	{&vop_symlink_desc, (VOPFUNC)err_symlink},			/* symlink */
	{&vop_readdir_desc, (VOPFUNC)webdav_readdir},		/* readdir */
	{&vop_readlink_desc, (VOPFUNC)err_readlink},		/* readlink */
	{&vop_abortop_desc, (VOPFUNC)err_abortop},			/* abortop */
	{&vop_inactive_desc, (VOPFUNC)webdav_inactive},		/* inactive */
	{&vop_reclaim_desc, (VOPFUNC)webdav_reclaim},		/* reclaim */
	{&vop_lock_desc, (VOPFUNC)nop_lock},				/* lock */
	{&vop_unlock_desc, (VOPFUNC)nop_unlock},			/* unlock */
	{&vop_bmap_desc, (VOPFUNC)err_bmap},				/* bmap */
	{&vop_strategy_desc, (VOPFUNC)err_strategy},		/* strategy */
	{&vop_print_desc, (VOPFUNC)webdav_print},			/* print */
	{&vop_islocked_desc, (VOPFUNC)vop_noislocked},		/* islocked */
	{&vop_pathconf_desc, (VOPFUNC)webdav_pathconf},		/* pathconf */
	{&vop_advlock_desc, (VOPFUNC)err_advlock},			/* advlock */
	{&vop_blkatoff_desc, (VOPFUNC)err_blkatoff},		/* blkatoff */
	{&vop_valloc_desc, (VOPFUNC)err_valloc},			/* valloc */
	{&vop_vfree_desc, (VOPFUNC)webdav_vfree},			/* vfree */
	{&vop_truncate_desc, (VOPFUNC)webdav_truncate},		/* truncate */
	{&vop_update_desc, (VOPFUNC)err_update},			/* update */
	{&vop_bwrite_desc, (VOPFUNC)err_bwrite},			/* bwrite */
	{&vop_pagein_desc, (VOPFUNC)webdav_pagein},			/* Pagein */
	{&vop_pageout_desc, (VOPFUNC)webdav_pageout},		/* Pageout */
	{&vop_getattrlist_desc, (VOPFUNC)err_getattrlist},	/* Getattrlist */
	{&vop_setattrlist_desc, (VOPFUNC)err_setattrlist},	/* Getattrlist */
	{&vop_exchange_desc, (VOPFUNC)err_exchange},		/* Exchange Files */
	{&vop_blktooff_desc, (VOPFUNC)err_blktooff},		/* blktooff */
	{&vop_offtoblk_desc, (VOPFUNC)err_offtoblk},		/* blktooff */
	{&vop_cmap_desc, (VOPFUNC)err_cmap},				/* cmap*/
	{(struct vnodeop_desc *)NULL, (int( *)())NULL}};

struct vnodeopv_desc webdav_vnodeop_opv_desc = {
	&webdav_vnodeop_p, webdav_vnodeop_entries};

/*****************************************************************************/
