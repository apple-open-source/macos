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

/*
 * Search for WEBDAV_VNODE_LOCKING to find the functions that need
 * work on when the vnode locking effort is resumed.
 */
#define	WEBDAV_VNODE_LOCKING 0

/*****************************************************************************/

extern int prtactive;	/* 1 => print out reclaim of active vnodes */

/*****************************************************************************/

/* keep builds working with older headers where these macros were not defined */

#ifndef atop_32
	#define atop_32	atop
#endif

#ifndef ptoa_32
	#define	ptoa_32	ptoa
#endif

#ifndef round_page_32
	#define round_page_32 round_page
#endif

#ifndef trunc_page_32
	#define trunc_page_32 trunc_page
#endif

/*****************************************************************************/

#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
	#define RET_ERR(str, error) \
	{ \
		if (error) \
			log_vnop_error(str, error); \
		return(error); \
	}
	
	static void log_vnop_error(str, error)
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

/*
 * webdav_down is called when the mount_webdav daemon cannot communicate with
 * the remote WebDAV server. It uses vfs_event_signal() to tell interested
 * parties the connection with the server is down.
 */
static void
webdav_down(struct webdavmount *fmp)
{
	if ( fmp != NULL )
	{
		if ( !(fmp->status & WEBDAV_MOUNT_TIMEO) )
		{
#ifdef DEBUG
			printf("webdav_down: lost connection\n");
#endif
			vfs_event_signal(&fmp->pm_mountp->mnt_stat.f_fsid, VQ_NOTRESP, 0);
			fmp->status |= WEBDAV_MOUNT_TIMEO;
		}
	}
}

/*****************************************************************************/

/*
 * webdav_down is called when the mount_webdav daemon can communicate with
 * the remote WebDAV server. It uses vfs_event_signal() to tell interested
 * parties the connection is OK again if the connection was having problems.
 */
static void
webdav_up(struct webdavmount *fmp)
{
	if ( fmp != NULL )
	{
        if ( (fmp->status & WEBDAV_MOUNT_TIMEO) )
		{
#ifdef DEBUG
			printf("webdav_up: connection OK again\n");
#endif
			fmp->status &= ~WEBDAV_MOUNT_TIMEO;
			vfs_event_signal(&fmp->pm_mountp->mnt_stat.f_fsid, VQ_NOTRESP, 1);
        }
	}
}

/*****************************************************************************/

/* XXX To Do:
 *	*	This is ripped off from unp_connect. Is there a cleaner way?
 */
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

	error = ENXIO;	/* default error */
	
	if (so2 == 0)
	{
		goto bad;
	}

	if (so->so_type != so2->so_type)
	{
		goto bad;
	}

	if ((so2->so_options & SO_ACCEPTCONN) == 0)
	{
		goto bad;
	}

	if (switch_funnel)
	{
		thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);
	}

	so3 = sonewconn(so2, 0);
	if (so3 == 0)
	{
		goto switchbad;
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
	if ( error != 0 )
	{
		error = ENXIO;
	}

switchbad:

	if (switch_funnel)
	{
		thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
	}

bad:

	return (error);
}

/*****************************************************************************/

/* XXX To Do:
 *	*	PR-2549455 says to get rid of this?
 */
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

/*
 * webdav_sendmsg
 *
 * The WebDAV send message routine is used to communicate with the
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

__private_extern__
int webdav_sendmsg(vnop, whattouse, pt, a_pcred, fmp, p, a_toarg, toargsize, a_server_error,
	a_arg, argsize, vp)
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
	struct vnode *vp;
{
	int error;
	struct socket *so;
	int flags;									/* socket flags */
	struct uio auio;
	struct iovec aiov[3];
	int res, outres;
	struct mbuf *cm;
	int len;
	struct webdav_cred pcred;
	struct timeval tv;
	struct sockopt sopt;
	struct timeval lasttrytime;
	struct timeval currenttime;

	/* get current time */
	microtime(&currenttime);
	
retry:

	lasttrytime.tv_sec = currenttime.tv_sec;
	error = 0;
	so = 0;
	cm = 0;
	
	if ( (fmp == NULL) || (fmp->status & WEBDAV_MOUNT_FORCE) || WEBDAV_CHECK_VNODE(vp) )
	{
		/* We're doing a forced unmount, so don't send another request to mount_webdav */
		return (ENXIO);
	}
		
	/*
	 * Create a new socket.
	 */
	thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);

	error = socreate(AF_UNIX, &so, SOCK_STREAM, 0);
	if (error)
	{
		goto bad;
	}
	
	/* set the socket receive timeout */
	tv.tv_sec = WEBDAV_SO_RCVTIMEO_SECONDS;
	tv.tv_usec = 0;
	bzero(&sopt, sizeof sopt);
	sopt.sopt_dir = SOPT_SET;
	sopt.sopt_level = SOL_SOCKET;
	sopt.sopt_name = SO_RCVTIMEO;
	sopt.sopt_val = &tv;
	sopt.sopt_valsize = sizeof tv;
	error = sosetopt(so, &sopt);
	if (error != 0)
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

	/* make sure the server process is still running before attempting to connect */
	if ( (fmp == NULL) || (fmp->status & WEBDAV_MOUNT_FORCE) ||
		WEBDAV_CHECK_VNODE(vp) || (fcount(fmp->pm_server) == 1) )
	{
		error = ENXIO;
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
		if ( (fmp == NULL) || (fmp->status & WEBDAV_MOUNT_FORCE) ||
			WEBDAV_CHECK_VNODE(vp) || (fcount(fmp->pm_server) == 1) )
		{
			error = ENXIO;
			goto bad;
		}
		(void) tsleep((caddr_t) & so->so_timeo, PSOCK, "webdav_sendmsg", 5 * hz);
		/* *** check for errors *** */
	}

	if (so->so_error)
	{
		error = so->so_error;
		goto bad;
	}

	/*
	 * Set miscellaneous flags
	 */
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
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
			printf("webdav_sendmsg: invalid whattouse arg\n");
#endif
			error = EIO;
			goto bad;
	}

	auio.uio_iov = aiov;
	auio.uio_iovcnt = 3;
	auio.uio_rw = UIO_WRITE;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_procp = p;
	auio.uio_offset = 0;
	auio.uio_resid = aiov[0].iov_len + aiov[1].iov_len + aiov[2].iov_len;

	/* make sure the server process is still running before attempting to send */
	if ( (fmp == NULL) || (fmp->status & WEBDAV_MOUNT_FORCE) ||
		WEBDAV_CHECK_VNODE(vp) || (fcount(fmp->pm_server) == 1) )
	{
		error = ENXIO;
		goto bad;
	}
	
	error = sosend(so, (struct sockaddr *)0, &auio, (struct mbuf *)0, (struct mbuf *)0, 0);
	if (error)
	{
		error = ENXIO;
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
	
	while ( 1 )
	{
		/* make sure the server process is still running before attempting to receive */
		if ( (fmp == NULL) || (fmp->status & WEBDAV_MOUNT_FORCE) ||
			WEBDAV_CHECK_VNODE(vp) || (fcount(fmp->pm_server) == 1) )
		{
			error = ENXIO;
			goto bad;
		}
		
		error = soreceive(so, (struct sockaddr **)0, &auio, (struct mbuf **)0, &cm, &flags);
		
		/* did soreceive timeout? */
		if (error != EWOULDBLOCK)
		{
			/* soreceive did not time out */
			break;
		}
		
		if ( (fmp == NULL) || (fmp->status & WEBDAV_MOUNT_FORCE) || WEBDAV_CHECK_VNODE(vp) )
		{
			/* We're doing a forced unmount, so don't send another request to mount_webdav */
			error = ENXIO;
			goto bad;
		}
	}
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
	
	if ( error != 0 )
	{
		error = ENXIO;
	}
	else
	{
		if ( *a_server_error & WEBDAV_CONNECTION_DOWN_MASK )
		{
			/* communications with mount_webdav were OK, but the remote server is unreachable */
			webdav_down(fmp);
			*a_server_error &= ~WEBDAV_CONNECTION_DOWN_MASK;
			
			/* If this request failed because of the connection problem, retry */
			if ( *a_server_error == ENXIO )
			{
				/* get current time */
				microtime(&currenttime);
				if ( currenttime.tv_sec < (lasttrytime.tv_sec + 2) )
				{
					/* sleep for 2 sec before retrying again */
					(void) tsleep(&lbolt, PCATCH, "webdav_sendmsg", 200);
					microtime(&currenttime);
				}
				goto retry;
			}
		}
		else
		{
			webdav_up(fmp);
		}
		
	}
	
	return (error);
}

/*****************************************************************************/

#if WEBDAV_VNODE_LOCKING
	#error "Needs vnode locking work"
#endif
static int webdav_buildnewvnode(dvp, vpp, cnp, existing_node)
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

#if !WEBDAV_VNODE_LOCKING
	if (WEBDAV_CHECK_VNODE(dvp))
	{
		error = EINVAL;
		goto bad;
	}
#endif

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

#if !WEBDAV_VNODE_LOCKING
	if (WEBDAV_CHECK_VNODE(dvp))
	{
		error = EINVAL;
		goto bad;
	}
#endif

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

#if !WEBDAV_VNODE_LOCKING
		/* FREE() may have blocked, check the state of the vnode */
		if (WEBDAV_CHECK_VNODE(fvp))
		{
			error = EINVAL;
			fvp = NULL;
			goto bad;
		}
		else
		{
			return (0);
		}
#else
		return (0);
#endif
	}

#if !WEBDAV_VNODE_LOCKING
	if (WEBDAV_CHECK_VNODE(dvp))
	{
		error = EINVAL;
		goto bad;
	}
#endif

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

/*
 * webdav_lock
 *
 * Lock a webdavnode. If its already locked, set the WANT bit and sleep.
 */
static int webdav_lock(ap)
	struct vop_lock_args	/* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
		} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct webdavnode *pt = VTOWEBDAV(vp);
	int error;

	if ( pt == NULL)
	{
		panic("webdav_lock: webdavnode in vnode is NULL\n");
	}
	error = lockmgr(&pt->pt_lock, ap->a_flags, &vp->v_interlock, ap->a_p);

	RET_ERR("webdav_lock", error);
}

/*****************************************************************************/

/*
 * webdav_unlock
 *
 * Unlock a webdavnode.
 */
static int webdav_unlock(ap)
	struct vop_lock_args	/* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
		} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct webdavnode *pt = VTOWEBDAV(vp);
	int error;

	if ( pt == NULL)
	{
		panic("webdav_unlock: webdavnode in vnode is NULL\n");
	}
	error = lockmgr(&pt->pt_lock, ap->a_flags | LK_RELEASE, &vp->v_interlock, ap->a_p);

	RET_ERR("webdav_unlock", error);
}

/*****************************************************************************/

#if WEBDAV_VNODE_LOCKING
	#error "Needs vnode locking work"
#endif
/* Temporary workaround to get around simultaneous opens */
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

#if WEBDAV_VNODE_LOCKING
	#error "Needs vnode locking work"
#endif
/* Temporary workaround to get around simultaneous opens */
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

#if WEBDAV_VNODE_LOCKING
	#error "Needs vnode locking work"
#endif
/*
 * vp is the current namei directory
 * cnp is the name to locate in that directory...
 */

static int webdav_lookup(ap)
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

	error = webdav_temp_lock(dvp, p);
	if (error)
	{
		return (EINVAL);
	}

	bzero(&pcred, sizeof(pcred));
	
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
		vrele(fvp);
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
		0, &server_error, &vap, sizeof(vap), fvp);
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

#if !WEBDAV_VNODE_LOCKING
	/* webdav_sendmsg() may have blocked, check the state of the vnode */
	if (WEBDAV_CHECK_VNODE(fvp))
	{
		error = ENOENT;
		fvp = NULL;
#ifdef DEBUG
		printf("webdav_lookup: bad vnode following webdav_sendmsg\n");
#endif
		goto bad;
	}
#endif

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
		pt->pt_mtime = vap.va_mtime;
		pt->pt_ctime = vap.va_ctime;
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

#if !WEBDAV_VNODE_LOCKING
	/* ubc_info_init() may have blocked, check the state of the vnode */
	if (WEBDAV_CHECK_VNODE(fvp))
	{
		error = ENOENT;
		fvp = NULL;
#ifdef DEBUG
		printf("webdav_lookup: bad vnode after ubc_info_init\n");
#endif
		goto bad;
	}
#endif

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

#if WEBDAV_VNODE_LOCKING
	#error "Needs vnode locking work"
#endif
static int webdav_open(ap)
	struct vop_open_args	/* {
		struct vnode *a_vp;
		int	 a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
		} */ *ap;
{
	struct socket *so;
	struct webdavnode *pt;
	struct proc *p;
	struct vnode *vp;
	struct uio auio;
	struct iovec aiov[3];
	int res;
	struct mbuf *cm;
	struct cmsghdr *cmsg;
	int newfds;
	int *ip;
	int fd;
	int error;
	int len;
	struct webdavmount *fmp;
	struct file *fp;
	struct webdav_cred pcred;
	int vnop;
	int flags;									/* socket flags */
	struct timeval tv;
	struct sockopt sopt;
	int retryrequest;
	int opencollision;
	struct timeval lasttrytime;
	struct timeval currenttime;

	p = ap->a_p;
	vp = ap->a_vp;
	
	/* get current time */
	microtime(&currenttime);

retry:

	lasttrytime.tv_sec = currenttime.tv_sec;
	so = 0;
	cm = 0;
	fd = 0;
	error = 0;
	retryrequest = 0;
	opencollision = 0;
	
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

	if ( (fmp == NULL) || (fmp->status & WEBDAV_MOUNT_FORCE) || WEBDAV_CHECK_VNODE(vp) )
	{
		/* We're doing a forced unmount, so don't send another request to mount_webdav */
		return (ENXIO);
	}

	/* If it is already open then just ref the node
	 * and go on about our business. Make sure to set
	 * the write status if this is read/write open
	 */
	/* There is no vnode locking so more than one open can be going to the deamon at a time
	 * and so multiple cache files can be opened. The opencollision retry mechanism was added
	 * to work around this problem until vnode locking is in place.
	 */
	if (vp->v_usecount > 0 && pt->pt_cache_vnode)
	{
		return (0);
	}

	/* Set the vnop type to tell the user process if we
	 * are going to open a file or a directory
	 */
	if (vp->v_type == VREG)
	{
		vnop = WEBDAV_FILE_OPEN;
	}
	else if (vp->v_type == VDIR)
	{
		vnop = WEBDAV_DIR_OPEN;
	}
	else
	{
		/* This should never happen, but just in case */
		error = EFTYPE;
		goto bad;
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
	
	/* set the socket receive timeout */
	tv.tv_sec = WEBDAV_SO_RCVTIMEO_SECONDS;
	tv.tv_usec = 0;
	bzero(&sopt, sizeof sopt);
	sopt.sopt_dir = SOPT_SET;
	sopt.sopt_level = SOL_SOCKET;
	sopt.sopt_name = SO_RCVTIMEO;
	sopt.sopt_val = &tv;
	sopt.sopt_valsize = sizeof tv;
	error = sosetopt(so, &sopt);
	if (error != 0)
	{
		thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
#ifdef DEBUG
		printf("webdav_open: error setting SO_RCVTIMEO\n");
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

	/* make sure the server process is still running before attempting to connect */
	if ( (fmp == NULL) || (fmp->status & WEBDAV_MOUNT_FORCE) ||
		WEBDAV_CHECK_VNODE(vp) || (fcount(fmp->pm_server) == 1) )
	{
		error = ENXIO;
		thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
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
		if ( (fmp == NULL) || (fmp->status & WEBDAV_MOUNT_FORCE) ||
			WEBDAV_CHECK_VNODE(vp) || (fcount(fmp->pm_server) == 1) )
		{
			error = ENXIO;
			thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
			goto bad;
		}
		(void) tsleep((caddr_t) & so->so_timeo, PSOCK, "webdav_open", 5 * hz);
	}

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

#if !WEBDAV_VNODE_LOCKING
	/* We pended so check the state of the vnode */
	/* probably fine to look this under network funnel as vnode is locked */
	if (WEBDAV_CHECK_VNODE(vp))
	{
		error = EPERM;
		goto bad;
	}
#endif

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
	so->so_snd.sb_timeo = 0;
	so->so_rcv.sb_flags |= SB_NOINTR;
	so->so_snd.sb_flags |= SB_NOINTR;

	/* make sure the server process is still running before attempting to send */
	if ( (fmp == NULL) || (fmp->status & WEBDAV_MOUNT_FORCE) ||
		WEBDAV_CHECK_VNODE(vp) || (fcount(fmp->pm_server) == 1) )
	{
		error = ENXIO;
		thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
		goto bad;
	}
	
	error = sosend(so, (struct sockaddr *)0, &auio, (struct mbuf *)0, (struct mbuf *)0, 0);
	if (error)
	{
		thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
#ifdef DEBUG
		printf("webdav_open: error from sosend\n");
#endif
		error = ENXIO;
		goto bad;
	}

	len = auio.uio_resid = sizeof(int);
	do
	{
		struct mbuf *m = 0;
		flags = MSG_WAITALL;
		
		while ( 1 )
		{
			/* make sure the server process is still running before attempting to receive */
			if ( (fmp == NULL) || (fmp->status & WEBDAV_MOUNT_FORCE) ||
				WEBDAV_CHECK_VNODE(vp) || (fcount(fmp->pm_server) == 1) )
			{
				error = ENXIO;
				thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
				goto bad;
			}
			
			error = soreceive(so, (struct sockaddr **)0, &auio, &m, &cm, &flags);
			
			/* did soreceive timeout? */
			if (error != EWOULDBLOCK)
			{
				/* soreceive did not time out */
				break;
			}
			
			if ( (fmp == NULL) || (fmp->status & WEBDAV_MOUNT_FORCE) || WEBDAV_CHECK_VNODE(vp) )
			{
				/* We're doing a forced unmount, so don't send another request to mount_webdav */
				error = ENXIO;
				thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
				goto bad;
			}
		}
		
		if (error)
		{
			thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
#ifdef DEBUG
			printf("webdav_open: error from soreceive\n");
#endif
			error = ENXIO;
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
				/* this is where the error from mount_webdav is obtained */
				error = *(mtod(m, int *));
				m_freem(m);
				if ( error & WEBDAV_CONNECTION_DOWN_MASK )
				{
					webdav_down(fmp);
					error &= ~WEBDAV_CONNECTION_DOWN_MASK;
					
					/* If this request failed because of the connection problem, retry */
					if ( error == ENXIO )
					{
						retryrequest = 1;
					}
				}
				else
				{
					webdav_up(fmp);
					if ( error == EAGAIN )
					{
						/* If this request failed because another open was opened a cache file
						 * after this open was started, then retry.
						 */
						opencollision = 1;
						retryrequest = 1;
					}
				}
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
	
#if !WEBDAV_VNODE_LOCKING
	/* We pended so check the state of the vnode */
	if (WEBDAV_CHECK_VNODE(vp))
	{
		error = EPERM;
		goto bad;
	}
#endif

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
		/* default to not ask for and to not cache additional directory information */
		SET(vp->v_flag, VNOCACHE_DATA);
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

	while ( 1 )
	{
		/* make sure the server process is still running before attempting to receive */
		if ( (fmp == NULL) || (fmp->status & WEBDAV_MOUNT_FORCE) ||
			WEBDAV_CHECK_VNODE(vp) || (fcount(fmp->pm_server) == 1) )
		{
			error = ENXIO;
			thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
			goto bad;
		}
		
		error = soreceive(so, (struct sockaddr **)0, &auio, (struct mbuf **)0, &cm, &flags);
		
		/* did soreceive timeout? */
		if (error != EWOULDBLOCK)
		{
			/* soreceive did not time out */
			break;
		}
		
		if ( (fmp == NULL) || (fmp->status & WEBDAV_MOUNT_FORCE) || WEBDAV_CHECK_VNODE(vp) )
		{
			/* We're doing a forced unmount, so don't send another request to mount_webdav */
			error = ENXIO;
			thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
			goto bad;
		}
	}

	if (error)
	{
		error = ENXIO;
#ifdef DEBUG
		printf("webdav_open: error getting fd back from user land\n");
#endif
	}

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
	
	if ( error == 0 ) 
	{
		/* If this request failed because of the connection problem, retry */
		if ( retryrequest )
		{
			/* get current time */
			microtime(&currenttime);
			if ( !opencollision && (currenttime.tv_sec < (lasttrytime.tv_sec + 2)) )
			{
				/* sleep for 2 sec before retrying again */
				(void) tsleep(&lbolt, PCATCH, "webdav_open", 200);
				microtime(&currenttime);
			}
			goto retry;
		}
	}
	
	RET_ERR("webdav_open", error);
}

/*****************************************************************************/

/*
 * webdav_fsync
 *
 * webdav_fsync flushes dirty pages (if any) to the cache file and then if
 * the file is dirty, pushes it up to the server.
 *
 * results:
 *	0		Success.
 *	EIO		A physical I/O error has occurred, or this error was generated for
 *			implementation-defined reasons.
 *	ENOSPC	The server returned 507 Insufficient Storage (WebDAV)
 */
static int webdav_fsync(ap)
	struct vop_fsync_args	/* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int a_waitfor;
		struct proc *a_p;
		} */ *ap;
{
	struct webdavnode *pt;
	struct proc *p;
	struct vnode *vp;
	struct vnode *cachevp;
	int error, server_error;
	struct webdavmount *fmp;
	struct webdav_cred pcred;
	struct vattr attrbuf;
	
	vp = ap->a_vp;
	pt = VTOWEBDAV(vp);
	cachevp = pt->pt_cache_vnode;
	fmp = VFSTOWEBDAV(vp->v_mount);
	p = ap->a_p;
	error = server_error = 0;

	if ( (vp->v_type != VREG) ||
		 (pt->pt_file_handle == -1) )
	{
		/* If this isn't a file, or there is no file_handle, we have nothing to
		 * tell the server to sync so we're done.
		 */
		error = 0;
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
		if ((vp->v_type == VREG) && (pt->pt_file_handle == -1))
		{
			printf("webdav_fsync: no file handle\n");
		}
#endif
		goto done;
	}

	/*
	 * The ubc_pushdirty() call can be expensive.
	 * There is a fixed cpu cost involved that is directly proportional
	 * to the size of file.
	 * For a webdav vnode, we do not cache any file data in the VM unless
	 * the file is mmap()ed. So if the file was never mapped, there is
	 * no need to call ubc_pushdirty(). 
	 */
	if ( WEBDAVISMAPPED(vp) )
	{
		/* This is where we need to tell UBC to flush out all of
		 * its pages for this vnode. If we do that then our write
		 * and pageout routines will get called if anything needs to
		 * be written.  That will cause the status to be dirty if
		 * it needs to be marked as such.
		 */
		if ( ubc_pushdirty(vp) == 0 )	/* ubc_pushdirty() returns 0 on error */
		{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
			printf("webdav_fsync: ubc_pushdirty failed\n");
#endif
			error = EIO;
			goto done;
		}
	
		/* ubc_pushdirty is not a synchronous call. It returns to us
		 * after having started I/O on all the dirty pages but not after
		 * the I/O is complete. However, it does lock the pages and the
		 * pages are not unlocked until the I/O is done, so a second
		 * call to ubc_pushdirty will not return until all the pages from the
		 * first call have been written. It is forced to wait because it
		 * needs to lock the pages which will be held locked until the I/O
		 * is done. Thus calling it twice simulates a synchronous call.
		 * So, this is why we call it again after having just called it. 
		 */
		if ( ubc_pushdirty(vp) == 0 )	/* ubc_pushdirty() returns 0 on error */
		{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
			printf("webdav_fsync: 2nd ubc_pushdirty failed\n");
#endif
			error = EIO;
			goto done;
		}

#if !WEBDAV_VNODE_LOCKING
		/* ubc_pushdirty() may have blocked, check the state of the vnode */
		if (WEBDAV_CHECK_VNODE(vp))
		{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
			printf("webdav_fsync: 2nd WEBDAV_CHECK_VNODE failed\n");
#endif
			error = EIO;
			goto done;
		}
#endif
	}

	/* If the file isn't dirty, or has been deleted don't sync it */
	if ( (!(pt->pt_status & WEBDAV_DIRTY)) || (pt->pt_status & WEBDAV_DELETED) )
	{
		error = 0;
		goto done;
	}

	/* make sure the file is completely downloaded from the server */
	do
	{
		error = VOP_GETATTR(cachevp, &attrbuf, ap->a_cred, ap->a_p);
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
			(void) tsleep(&lbolt, PCATCH, "webdav_fsync", 1);
		}
		else
		{
			/* the file has been downloaded */
			break;
		}
		
#if !WEBDAV_VNODE_LOCKING
		/* We pended so check the state of the vnode */
		if (WEBDAV_CHECK_VNODE(vp))
		{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
			printf("webdav_fsync: 1st WEBDAV_CHECK_VNODE failed\n");
#endif
			error = EIO;
			goto done;
		}
#endif
	} while ( TRUE );


	if ( attrbuf.va_flags & UF_APPEND )
	{
		/* If the UF_APPEND flag is set, there was an error downloading the file from the
		 * server, so exit with an EIO result.
		 */
		error = EIO;
		goto done;
	}

	/* At this point, the file is completely downloaded into cachevp.
	 * Locking cachevp isn't needed because webdavfs vnops are only writer
	 * to cachevp after it is downloaded.
	 */
	
	bzero(&pcred, sizeof(pcred));
	if (ap->a_cred)
	{
		pcred.pcr_uid = ap->a_cred->cr_uid;
		pcred.pcr_ngroups = ap->a_cred->cr_ngroups;
		bcopy(ap->a_cred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));
	}
	
	/* clear the dirty flag before pushing this to the server */
	pt->pt_status &= ~WEBDAV_DIRTY;

	error = webdav_sendmsg(WEBDAV_FILE_FSYNC, WEBDAV_USE_HANDLE, pt, &pcred, fmp, p, (void *)NULL, 0,
		&server_error, (void *)NULL, 0, vp);
	if (error)
	{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
		printf("webdav_fsync: webdav_sendmsg: %d\n", error);
#endif
		error = EIO;
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

#if !WEBDAV_VNODE_LOCKING
	/* We pended so check the state of the vnode */
	if (WEBDAV_CHECK_VNODE(vp))
	{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
		printf("webdav_fsync: 3rd WEBDAV_CHECK_VNODE failed\n");
#endif
		error = EIO;
		goto done;
	}
#endif

done:

	RET_ERR("webdav_fsync", error);
}

/*****************************************************************************/

/*
	webdav_close_inactive is the common subroutine used by webdav_close and
	webdav_inactive to synchronize the file with the server if needed, and
	release the cache vnode if needed.
	results
		0	no error
		EIO	an I/O error occurred
	locks	in	out	error (with vnode locking, this will be true)
		vp	L	L	L
*/
static
int webdav_close_inactive(struct vnode *vp,
					  struct proc *p,
					  struct ucred *a_cred,
					  struct webdav_cred *pcred,
					  int last_close)
{
	int error;
	int server_error;
	int fsync_error;
	struct webdavnode *pt;
	struct vop_fsync_args fsync_args;

	pt = VTOWEBDAV(vp);
	
	/* no errors yet */
	error = server_error = fsync_error = 0;
	
	/* If there is no file_handle, we have nothing to
	 * tell the server to close.
	 */
	if ( pt->pt_file_handle != -1 )
	{
		if ( vp->v_type == VREG )
		{
			/* It's a file.
			 * synchronize the file with the server if needed.
			 */
			fsync_args.a_p = p;
			fsync_args.a_vp = vp;
			fsync_args.a_cred = a_cred;
			fsync_args.a_waitfor = TRUE;
			fsync_error = webdav_fsync(&fsync_args);
			
#if !WEBDAV_VNODE_LOCKING
			/* We pended so check the state of the vnode */
			if (WEBDAV_CHECK_VNODE(vp))
			{
				error = EIO;
				goto error_return;
			}
#endif
		}
		
		/* If this the last close, tell mount_webdav */
		if ( last_close )
		{
			error = webdav_sendmsg(WEBDAV_CLOSE, WEBDAV_USE_HANDLE, pt, pcred,
				VFSTOWEBDAV(vp->v_mount), p, (void *)NULL, 0,
				&server_error, (void *)NULL, 0, vp);
			if ( error != 0 )
			{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
				printf("webdav_close_inactive: webdav_sendmsg failed: %d\n", error);
#endif
				error = EIO;
			}
			
#if !WEBDAV_VNODE_LOCKING
			/* We pended so check the state of the vnode */
			if (WEBDAV_CHECK_VNODE(vp))
			{
				/* Ok to close */
				error = 0;
				goto error_return;
			}
#endif
		}
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
		else
		{
			printf("webdav_close_inactive: not the last closer\n");
		}
#endif
	}
	else
	{
		/* no file handle */
		if (vp->v_type == VREG)
		{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
			printf("webdav_close_inactive: no file handle\n");
#endif
		}
	}
	
	/* If this the last close, release the cache vnode
	 * (if there is one) and invalidate the file handle.
	 */
	if ( last_close )
	{
		if ( pt->pt_cache_vnode != NULL )
		{
			struct vnode *temp;
			
			/* zero out pt_cache_vnode and then release the cache vnode */
			temp = pt->pt_cache_vnode;
			pt->pt_cache_vnode = 0;
			vrele(temp);
		}
		pt->pt_file_handle = -1;
	}
	
	/* report any errors */
	if ( error == 0 )
	{
		if ( fsync_error != 0 )
		{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
			printf("webdav_close_inactive: webdav fsync error: %d\n", fsync_error);
#endif
			error = fsync_error;
		}
		else if ( server_error != 0 )
		{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
			printf("webdav_close_inactive: webdav server error: %d\n", server_error);
#endif
			error = EIO;
		}
	}
	
error_return:

	RET_ERR("webdav_close_inactive", error);
}

/*****************************************************************************/

/*
	webdav_close
*/
static int webdav_close(ap)
	struct vop_close_args	/* {
		struct vnode *a_vp;
		int fflag;
		struct ucred *a_cred;
		struct proc *a_p;
		} */ *ap;
{
	int error;
	struct vnode *vp;
	struct webdav_cred pcred;
	int last_close;	/* TRUE if this is the last close */

	vp = ap->a_vp;
	
	/* no errors yet */
	error = 0;
	
	/* VOP_CLOSE can be called with vp locked (from vclean).
	 * We check for this case using VOP_ISLOCKED and bail if locked.
	 * webdav_inactive() will get to do the work in that case.
	 */
	if ( !VOP_ISLOCKED(vp) )
	{
#if WEBDAV_VNODE_LOCKING
		/* lock the vnode */
		error = vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, ap->a_p);
		if (error)
		{
			goto exit;
		}
#endif		
		/* set up webdav_cred */
		bzero(&pcred, sizeof(pcred));
		if ( ap->a_cred != NULL)
		{
			pcred.pcr_uid = ap->a_cred->cr_uid;
			pcred.pcr_ngroups = ap->a_cred->cr_ngroups;
			bcopy(ap->a_cred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));
		}
		
		/* set up last_close */		
		if ( VTOWEBDAV(vp)->pt_file_handle != -1 )
		{
			if ( vp->v_type == VREG )
			{
				/* check for last close of file */
				last_close = !ubc_isinuse(vp, 1);
			}
			else
			{
				/* check for last close of directory */
				last_close = (vp->v_usecount < 2);
			}
		}
		else
		{
			last_close = TRUE;
		}
		
		/* do the work common to close and inactive */
		error = webdav_close_inactive(vp, ap->a_p, ap->a_cred, &pcred, last_close);

#if !WEBDAV_VNODE_LOCKING
		/* make sure we didn't lose vp in webdav_close_inactive */
		if ( WEBDAV_CHECK_VNODE(vp) )
		{
			error = EPERM;
			goto exit;
		}
#else
		/* unlock the vnode */
		VOP_UNLOCK(vp, 0, ap->a_p);
#endif
	}
	else
	{
		/* we're holding off on close */
		last_close = FALSE;
	}
	
	/*
	 * If we're holding off close and the file has been mapped, set the
	 * "do not cache" bit with ubc_uncache so that webdav_inactive() gets 
	 * called immediately after the last mmap reference is gone.
	 */
	if (!last_close && WEBDAVISMAPPED(vp))
	{
		(void) ubc_uncache(vp);
	}

exit:
	
	RET_ERR("webdav_close", error);
}

/*****************************************************************************/

/*
 * webdav_read_bytes
 *
 * webdav_read_bytes is called by webdav_read and webdav_pagein to
 * read bytes directly from the server when we haven't yet downloaded the
 * part of the file needed to retrieve the data. If this routine returns an
 * error, then the caller will just spin wait for the part of the file needed
 * to be downloaded.
 *
 * results:
 * 0	no error - bytes were read
 * !0	the bytes were not read and the caller must wait for the download
 *
 * To do:
 *		Pass in current file size so that this routine can compare against
 *		WEBDAV_WAIT_IF_WITHIN instead of the caller.
 */
static int webdav_read_bytes(struct vnode *vp, struct uio *a_uio, struct ucred *a_cred)
{
	int error;
	int server_error;
	int message_size;
	int data_size;
	struct webdavnode *pt;
	webdav_byte_read_header_t *read_header;
	void *buffer;
	struct webdav_cred pcred;
	struct webdavmount *fmp;
	struct proc *p;

	pt = VTOWEBDAV(vp);
	error = server_error = 0;
	p = current_proc();
	fmp = VFSTOWEBDAV((vp)->v_mount);

	/* Determine space needed for the read header info we will be sending to the
	 * user process and the data size.
	 */
	message_size = sizeof(webdav_byte_read_header_t) + pt->pt_size;
	data_size = a_uio->uio_resid;

	/* don't bother if the range starts with 0,
	 * or if the read is too big for an out-of-band read.
	 */
	if ( (a_uio->uio_offset == 0) ||
		 (data_size > WEBDAV_MAX_IO_BUFFER_SIZE) )
	{
		/* return an error so the caller will wait */
		error = EINVAL;
		goto done;
	}

	/* allocate space for the read header info */
	MALLOC(read_header, void *, message_size, M_TEMP, M_WAITOK);

	/* Now allocate the buffer that we are going to use to hold the data that
	 * comes back
	 */
	MALLOC(buffer, void *, data_size, M_TEMP, M_WAITOK);

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

	error = webdav_sendmsg(WEBDAV_BYTE_READ, WEBDAV_USE_INPUT, pt, &pcred, fmp, p, (void *)read_header, 
		message_size, &server_error, buffer, data_size, vp);
	if (error)
	{
		/* return an error so the caller will wait */
		goto dealloc_done;
	}

	if (server_error)
	{
		/* return an error so the caller will wait */
		error = server_error;
		goto dealloc_done;
	}

	error = uiomove((caddr_t)buffer, data_size, a_uio);

dealloc_done:

	FREE((void *)buffer, M_TEMP);
	FREE((void *)read_header, M_TEMP);

done:

	RET_ERR("webdav_read_bytes", error);
}

/*****************************************************************************/

/*
 * webdav_rdwr
 *
 * webdav_rdwr is called by webdav_read and webdav_write. What webdav_read and
 * webdav_write need to do is so similar that a common subroutine can be used
 * for both. The "reading" flag is used in the few places where read and write
 * code is different.
 *
 * results:
 *	0		Success.
 *	EISDIR	Tried to read a directory.
 *	EIO		A physical I/O error has occurred, or this error was generated for
 *			implementation-defined reasons.
 */
static int webdav_rdwr(ap)
	struct vop_read_args	/* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
		} */ *ap;
{
	struct webdavnode *pt;
	struct proc *p;
	struct vnode *cachevp;
	int error;
	upl_t upl;
	struct uio *in_uio;
	struct vattr attrbuf;
	off_t total_xfersize;
	kern_return_t kret;
	struct vnode *vp;
	int mapped_upl;
	int file_changed;
	int reading;
	int tried_bytes;

	vp = ap->a_vp;
	pt = VTOWEBDAV(vp);
	cachevp = pt->pt_cache_vnode;
	in_uio = ap->a_uio;
	reading = (in_uio->uio_rw == UIO_READ);
	p = current_proc();
	file_changed = FALSE;
	total_xfersize = 0;
	tried_bytes = FALSE;

	/* make sure this is not a directory */
	if ( vp->v_type == VDIR )
	{
		error = EISDIR;
		goto exit;
	}
	
	/* make sure there's a cache file vnode associated with the webdav vnode */
	if ( cachevp == NULLVP )
	{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
		printf("webdav_rdwr: about to %s a uncached vnode\n", (reading ? "read from" : "write to"));
#endif
		error = EIO;
		goto exit;
	}
	
	if ( reading )
	{
		/* we've access the file */
		pt->pt_status |= WEBDAV_ACCESSED;
	}

	/* Start the sleep loop to wait on the background download. We will know that the webdav user
	 * process is finished when it either clears the nodump flag or sets the append only flag
	 * (indicating an error)
	 */
	do
	{
		off_t rounded_iolength;
		
		/* lock the cache vnode */
		error = VOP_LOCK(cachevp, LK_EXCLUSIVE | LK_RETRY, p);
		if ( error )
		{
			goto exit;
		}
		
		/* get the cache file's size and va_flags */
		error = VOP_GETATTR(cachevp, &attrbuf, ap->a_cred, p);
		if ( error )
		{
			goto unlock_exit;
		}

#if !WEBDAV_VNODE_LOCKING
		/* XXX remove this when locking is added */
		if ( WEBDAV_CHECK_VNODE(vp) )
		{
			error = EPERM;
			goto unlock_exit;
		}
#endif
		
		/* Don't attempt I/O until either:
		 *    (a) the page containing the end of the I/O is in has been downloaded, or
		 *    (b) the entire file has been downloaded.
		 * This ensures we don't read partially downloaded data, or write into a
		 * a portion of the file that is still being downloaded.
		 */
		rounded_iolength = (off_t)round_page_64(in_uio->uio_offset + in_uio->uio_resid);

		if ( (attrbuf.va_flags & UF_NODUMP) && (rounded_iolength > (off_t)attrbuf.va_size) )
		{
			/* We are downloading the file and we haven't gotten to
			 * to the bytes we need so unlock, sleep, and check again.
			 */
			VOP_UNLOCK(cachevp, 0, p);
			
			/* if reading, we may be able to read the part of the file we need out-of-band */
			if ( reading )
			{
				if ( !tried_bytes )
				{
					if ( rounded_iolength > ((off_t)attrbuf.va_size + WEBDAV_WAIT_IF_WITHIN) )
					{
						/* We aren't close to getting to the part of the file that contains
						 * the data we want so try to ask the server for the bytes
						 * directly. If that does not work, wait until the stuff gets down.
						 */
						error = webdav_read_bytes(vp, in_uio, ap->a_cred);
						if ( !error )
						{
							/* we're done */
#if !WEBDAV_VNODE_LOCKING
							if ( WEBDAV_CHECK_VNODE(vp) )
							{
								error = EPERM;
							}
#endif
							goto exit;
						}
					}
					/* If we are here, we must have failed to get the bytes so return and
					* set tried_bytes so we won't attempt that again and sleep */
					tried_bytes = TRUE;
				}
			}
			
			/* sleep for a bit */
			(void) tsleep(&lbolt, PCATCH, "webdav_rdwr", 1);
			
#if !WEBDAV_VNODE_LOCKING
			/* After pending on tsleep, check the state of the vnode */
			/* XXX remove this when locking is added */
			if ( WEBDAV_CHECK_VNODE(vp) )
			{
				error = EPERM;
				goto exit;
			}
#endif
		}
		else
		{
			/* the part we need has been downloaded and cachevp is still VOP_LOCK'ed */
			break; /* out of while (TRUE) loop */
		}
	} while ( TRUE );

	if ( attrbuf.va_flags & UF_APPEND )
	{
		/* If the UF_APPEND flag is set, there was an error downloading the file from the
		 * server, so exit with an EIO result.
		 */
		error = EIO;
		goto unlock_exit;
	}

	/* At this point, cachevp is locked and either the file is completely downloaded into
	 * cachevp, or the page this I/O ends within has been completely downloaded into cachevp.
	 */
	
	/* Determine the total_xfersize. Reads must be within the current file;
	 * Writes can extend the file.
	 */
	if ( reading )
	{
		/* pin total_xfersize to EOF */
		if ( in_uio->uio_offset > (off_t)attrbuf.va_size )
		{
			total_xfersize = 0;
		}
		else
		{
			total_xfersize = MIN(in_uio->uio_resid, ((off_t)attrbuf.va_size - in_uio->uio_offset));
			/* make sure total_xfersize isn't negative */
			if ( total_xfersize < 0 )
			{
				total_xfersize = 0;
			}
		}
	}
	else
	{
		/* get total_xfersize and make sure it isn't negative */
		total_xfersize = (in_uio->uio_resid < 0) ? (0) : (in_uio->uio_resid);
	}
	
	/*
	 * For a webdav vnode, we do not cache any file data in the VM unless
	 * the file is mmap()ed. So if the file was never mapped, there is
	 * no need to create a upl, scan for valid pages, etc, and the VOP_READ/WRITE
	 * to cachevp can handle the request completely.
	 */
	if ( WEBDAVISMAPPED(vp) )
	{
		/* If the ubc info exists we may need to get some or all of the data
		 * from mapped pages. */
				
		/* loop until total_xfersize has been transferred or error occurs */
		while ( total_xfersize > 0 )
		{
			int currentPage;
			int pagecount;
			int pageOffset;
			int xfersize;
			vm_offset_t addr;
			upl_page_info_t *pl;
			
			/* Determine the offset into the first page and how much to transfer this time.
			 * xfersize will be total_xfersize or as much as possible ending on a page boundary.
			 */
			pageOffset = in_uio->uio_offset & PAGE_MASK;
			xfersize = MIN(total_xfersize, (MAX_UPL_TRANSFER * PAGE_SIZE) - pageOffset);
			
#if !WEBDAV_VNODE_LOCKING
			if ( WEBDAV_CHECK_VNODE(vp) )
			{
				error = EIO;
				goto unlock_exit;
			}
#endif
			/* create the upl so that we "own" the pages */
			kret = ubc_create_upl(vp,
				(vm_object_offset_t) trunc_page_64(in_uio->uio_offset),
				(vm_size_t) round_page_32(xfersize + pageOffset),
				&upl,
				&pl,
				UPL_FLAGS_NONE);
            if ( kret != KERN_SUCCESS )
			{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
				printf("webdav_rdwr: ubc_create_upl failed %d\n", kret);
#endif
                error = EIO;
                goto unlock_exit;
            }
			
			/* Scan pages looking for valid/invalid ranges of pages.
			 * uiomove() the ranges of valid pages; VOP_READ/WRITE the ranges of invalid pages.
			 */
			mapped_upl = FALSE;
			currentPage = 0;
			pagecount = atop_32(pageOffset + xfersize - 1) + 1;
			while ( currentPage < pagecount )
			{
				int firstPageOfRange;
				int lastPageOfRange;
				int rangeIsValid;
				int requestSize;
				
				firstPageOfRange = lastPageOfRange = currentPage;
				rangeIsValid = upl_valid_page(pl, firstPageOfRange);
				++currentPage;
				
				/* find last page with same state */
				while ( (currentPage < pagecount) && (upl_valid_page(pl, currentPage) == rangeIsValid) )
				{
					lastPageOfRange = currentPage;
					++currentPage;
				}
				
				/* determine how much to uiomove() or VOP_READ() for this range of pages */
				requestSize = MIN(xfersize, (int)(ptoa_32(lastPageOfRange - firstPageOfRange + 1) - pageOffset));
				
				if ( rangeIsValid )
				{
					/* range is valid, uiomove it */
					
					/* map the upl the first time we need it mapped */
					if ( !mapped_upl )
					{
						kret = ubc_upl_map(upl, &addr);
						if ( kret != KERN_SUCCESS )
						{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
							printf("webdav_rdwr: ubc_upl_map failed %d\n", kret);
#endif
							error = EIO;
							goto unmap_unlock_exit;
						}
						mapped_upl = TRUE;
					}
					
					/* uiomove the the range firstPageOfRange through firstPageOfRange */
					error = uiomove((caddr_t)(addr + ptoa_32(firstPageOfRange) + pageOffset),
						requestSize,
						in_uio);
					if ( error )
					{
						goto unmap_unlock_exit;
					}
				}
				else
				{
					/* range is invalid, VOP_READ/WRITE it from the the cache file */
					int remainingRequestSize;
					
					/* subtract requestSize from uio_resid and save */
					remainingRequestSize = in_uio->uio_resid - requestSize;
					
					/* adjust size of read */
					in_uio->uio_resid = requestSize;
					
					if ( reading )
					{
						/* read it from the cache file */
						error = VOP_READ(cachevp, in_uio, 0 /* no flags */, ap->a_cred);
					}
					else
					{
						/* write it to the cache file */
						error = VOP_WRITE(cachevp, in_uio, 0 /* no flags */, ap->a_cred);
					}
					
					if ( error || (in_uio->uio_resid != 0) )
					{
						goto unmap_unlock_exit;
					}
					
					/* set remaining uio_resid */
					in_uio->uio_resid = remainingRequestSize;
				}
				
				if ( !reading )
				{
					/* after the write to the cache file has been completed, mark the file dirty */
					pt->pt_status |= WEBDAV_DIRTY;
					file_changed = TRUE;
				}
				
				/* set pageOffset to zero (which it will be if we need to loop again)
				 * and decrement xfersize and total_xfersize by requestSize.
				 */
				pageOffset = 0;
				xfersize -= requestSize;
				total_xfersize -= requestSize;
			}
			
			/* unmap the upl if needed */
			if ( mapped_upl )
			{
				(void) ubc_upl_unmap(upl);
			}
			
			/* get rid of the upl */
			kret = ubc_upl_abort(upl, 0);
			if ( kret != KERN_SUCCESS )
			{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
				printf("webdav_rdwr: ubc_upl_map failed %d\n", kret);
#endif
				error = EIO;
                goto unlock_exit;
			}
			
		}	/* end while loop */
	}
	else
	{
		/* No UBC, or was never mapped */
		if ( reading )
		{
			/* pass the read along to the underlying cache file */
			error = VOP_READ(cachevp, in_uio, ap->a_ioflag, ap->a_cred);
		}
		else
		{
			/* pass the write along to the underlying cache file */
			error = VOP_WRITE(cachevp, in_uio, ap->a_ioflag, ap->a_cred);
	
			/* after the write to the cache file has been completed... */
			pt->pt_status |= WEBDAV_DIRTY;
			file_changed = TRUE;
		}
	}

unlock_exit:

	if ( !reading )
	{
#if !WEBDAV_VNODE_LOCKING
		if ( WEBDAV_CHECK_VNODE(vp) )
		{
			error = EIO;
			/* unlock the cache file */
			VOP_UNLOCK(cachevp, 0, p);
			goto exit;
		}
#endif

		/* Note: the sleep loop	at the top of this function ensures that the file can grow only
		 * if the file is completely downloaded.
		 */
		if ( file_changed &&							/* if the file changed */
			 UBCINFOEXISTS(vp) &&						/* and vp has ubc info */
			 (in_uio->uio_offset > (off_t)attrbuf.va_size) )	/* and the file grew */
		{
			/* make sure the cache file's size is correct */
			struct vattr vattr;
			
			/* set the size of the cache file */
			VATTR_NULL(&vattr);
			vattr.va_size = in_uio->uio_offset;
			error = VOP_SETATTR(cachevp, &vattr, ap->a_cred, p);
			
			/* unlock the cache file before calling ubc_setsize */
			VOP_UNLOCK(cachevp, 0, p);
			
#if !WEBDAV_VNODE_LOCKING
			if ( WEBDAV_CHECK_VNODE(vp) )
			{
				error = EIO;
				goto exit;
			}
#endif
			
			/* let the UBC know the new size */
			if ( error == 0 )
			{
				(void) ubc_setsize(vp, in_uio->uio_offset);
			}
		}
		else
		{
			/* unlock the cache file */
			VOP_UNLOCK(cachevp, 0, p);
		}
	}
	else
	{
		/* unlock the cache file */
		VOP_UNLOCK(cachevp, 0, p);
	}
	
exit:

	return ( error );

unmap_unlock_exit:

	/* unmap the upl if it's mapped */
	if ( mapped_upl )
	{
		(void) ubc_upl_unmap(upl);
	}

	/* get rid of the upl */
	kret = ubc_upl_abort(upl, UPL_ABORT_ERROR);
	if ( kret != KERN_SUCCESS )
	{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
		printf("webdav_rdwr: ubc_upl_abort failed %d\n", kret);
#endif
		if (!error)
		{
			error = EIO;
		}
	}
	
	goto unlock_exit;
}

/*****************************************************************************/

/*
 * webdav_read
 *
 * webdav_read calls webdav_rdwr to do the work.
 */
static int webdav_read(ap)
	struct vop_read_args	/* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
		} */ *ap;
{
	RET_ERR("webdav_read", webdav_rdwr(ap));
}

/*****************************************************************************/

/*
 * webdav_write
 *
 * webdav_write calls webdav_rdwr to do the work.
 */
static int webdav_write(ap)
	struct vop_read_args	/* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
		} */ *ap;
{
	RET_ERR("webdav_write", webdav_rdwr(ap));
}

/*****************************************************************************/

/*
 * webdav_access
 *
 * webdav_access checks the permissions of a file against the given credentials.
 * WebDAVFS doesn't do much in this routine at this time.
 *
 * results:
 *	0		Success.
 *	EPERM	The file was immutable.
 *	EROFS	The file system is read only.
 *	EACCES	Permission denied.
 */
static int webdav_access(ap)
	struct vop_access_args	/* {
		struct vnode *a_vp;
		int	 a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
		} */ *ap;
{
	/*
	 * Disallow write attempts on read-only file systems;
	 * webdav only supports VREG and VDIR file types so
	 * we don't bother checking the type.
	 */
	if ( (ap->a_mode & VWRITE) && (ap->a_vp->v_mount->mnt_flag & MNT_RDONLY) )
	{
		return ( EROFS );
	}
	else
	{
		/* Server mediates access so we'll just return 0 here */
		return ( 0 );
	}
}

/*****************************************************************************/

/*
 * webdav_getattr
 *
 * webdav_getattr returns the most up-to-date vattr information.
 *
 * results:
 *	0		Success.
 *	EIO		A physical I/O error has occurred, or this error was generated for
 *			implementation-defined reasons.
 */
static int webdav_getattr(ap)
	struct vop_getattr_args	/* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
		} */ *ap;
{
	struct vnode *vp;
	struct vnode *cachevp;
	struct vattr *vap;
	struct vattr server_vap;
	struct vattr cache_vap;
	struct timeval tv;
	struct webdavnode *pt;
	struct webdavmount *fmp;
	int error;
	int server_error;
	struct proc *p;
	struct webdav_cred pcred;
	int cache_vap_valid;
	int callServer;
	
	vp = ap->a_vp;
	vap = ap->a_vap;
	fmp = VFSTOWEBDAV(vp->v_mount);
	pt = VTOWEBDAV(vp);
	cachevp = pt->pt_cache_vnode;
	p = current_proc();
	error = server_error = 0;
	
	bzero(vap, sizeof(*vap));
	vattr_null(vap);
	
	/* get everything we need out of vp and related structures before
	 * making any blocking calls where vp could go away.
	 */
	vap->va_type = vp->v_type;
	/* full access for everyone - let the server decide what can really be done */
	vap->va_mode = S_IRUSR | S_IWUSR | S_IXUSR |	/* owner */
				   S_IRGRP | S_IWGRP | S_IXGRP |	/* group */
				   S_IROTH | S_IWOTH | S_IXOTH |	/* other */
				   ((vp->v_type == VDIR) ? S_IFDIR : S_IFREG);
	/* Why 1 for va_nlink?
	 * Getting the real link count for directories is expensive.
	 * Setting it to 1 lets FTS(3) (and other utilities that assume
	 * 1 means a file system doesn't support link counts) work.
	 */
	vap->va_nlink = 1;
	vap->va_uid = UNKNOWNUID;
	vap->va_gid = UNKNOWNUID;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_fileid = pt->pt_fileid;
	vap->va_atime = pt->pt_atime;
	vap->va_mtime = pt->pt_mtime;
	vap->va_ctime = pt->pt_ctime;
	vap->va_gen = 0;
	vap->va_flags = 0;
	vap->va_rdev = 0;
	vap->va_filerev = 0;
	
	if ( (vp->v_type == VREG) && (cachevp != NULLVP) )
	{
		/* vp is a file and there's a cache file.
		 * Get the cache file's information since it is the latest.
		 */
		error = VOP_GETATTR(cachevp, &cache_vap, ap->a_cred, ap->a_p);
		if (error)
		{
			goto bad;
		}
				
		cache_vap_valid = TRUE;
		
		/* if the cache file is not complete or the download failed, we still need to call the server */
		if ( (cache_vap.va_flags & UF_NODUMP) || (cache_vap.va_flags & UF_APPEND) )
		{
			callServer = TRUE;
			
			/* this is the only place vp could go bad before it is used again */
			/* NOTE: this is the case even with vnode locking! */
			if ( WEBDAV_CHECK_VNODE(vp) )
			{
				error = EPERM;
				goto bad;
			}
		}
		else
		{
			callServer = FALSE;
		}
	}
	else
	{
		cache_vap_valid = FALSE;
		callServer = TRUE;
	}
	
	if ( callServer )
	{
		/* get the server file's information */
		
		bzero(&server_vap, sizeof(server_vap));
		vattr_null(&server_vap);
		
		/* user level is ignoring the pcred anyway */
		pcred.pcr_flag = 0;
		pcred.pcr_uid = ap->a_cred->cr_uid;
		pcred.pcr_ngroups = ap->a_cred->cr_ngroups;
		bcopy(ap->a_cred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));
		
		error = webdav_sendmsg(WEBDAV_STAT, WEBDAV_USE_URL, pt, &pcred, fmp, p,
			(void *)NULL, 0, &server_error, (void *) &server_vap, sizeof(server_vap), vp);
		if (error)
		{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
			printf("webdav_getattr: webdav_sendmsg: %d\n", error);
#endif
			error = EIO;
			goto bad;
		}
		
		if (server_error)
		{
			error = server_error;
			goto bad;
		}
		
		/* use the server file's size info */
		vap->va_size = server_vap.va_size;
		vap->va_bytes = server_vap.va_bytes;
		vap->va_blocksize = WEBDAV_IOSIZE;
	}
	else
	{
		/* use the cache file's size info */
		vap->va_size = cache_vap.va_size;
		vap->va_bytes = cache_vap.va_bytes;
		vap->va_blocksize = cache_vap.va_blocksize;
	}
	
	if ( cache_vap_valid )
	{
		/* use the time stamps from the cache file if needed */
		if ( pt->pt_status & WEBDAV_DIRTY )
		{
			vap->va_atime = cache_vap.va_atime;
			vap->va_mtime = cache_vap.va_mtime;
			vap->va_ctime = cache_vap.va_ctime;
		}
		else if ( (pt->pt_status & WEBDAV_ACCESSED) )
		{
			/* Though we have not dirtied the file, we have accessed it so
			 * grab the cache file's access time.
			 */
			vap->va_atime = cache_vap.va_atime;
		}
	}
	else
	{
		/* no cache file, so use server_vap for times if possible */
		if ( server_vap.va_atime.tv_sec != 0 )
		{
			/* use the server times if they were returned (if the getlastmodified
			 * property isn't returned by the server, server_vap.va_atime will be 0)
			 */
			vap->va_atime = server_vap.va_atime;
			vap->va_mtime = server_vap.va_mtime;
			vap->va_ctime = server_vap.va_ctime;
		}
		else
		{
			/* otherwise, use the current time */
			microtime(&tv);
			TIMEVAL_TO_TIMESPEC(&tv, &vap->va_atime);
			vap->va_mtime = vap->va_ctime = vap->va_atime;
		}
	}

bad:

	RET_ERR("webdav_getattr", error);
}

/*****************************************************************************/

/*
 * webdav_remove
 *
 * webdav_remove removes a file.
 *
 * results:
 *	0		Success.
 *	EBUSY	Caller requested Carbon delete semantics and file was open.
 *	EIO		A physical I/O error has occurred, or this error was generated for
 *			implementation-defined reasons.
 */
static int webdav_remove(ap)
	struct vop_remove_args	/* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
		} */ *ap;
{
	struct vnode *vp;
	struct vnode *dvp;
	struct webdavnode *pt;
	struct webdavmount *fmp;
	int error;
	int server_error;
	struct proc *p;
	struct webdav_cred pcred;
	
	vp = ap->a_vp;
	dvp = ap->a_dvp;
	fmp = VFSTOWEBDAV(vp->v_mount);
	pt = VTOWEBDAV(vp);
	p = current_proc();
	error = server_error = 0;

	if ( ap->a_cnp->cn_flags & NODELETEBUSY )
	{
		/* Caller requested Carbon delete semantics */
		if ( (!UBCISVALID(vp) && vp->v_usecount > 1) ||
			 (UBCISVALID(vp) && ubc_isinuse(vp, 1)) )
		{
			error = EBUSY;
			goto bad;
		}
	}
	
	/* user level is ignoring the pcred anyway */
	pcred.pcr_flag = 0;
	pcred.pcr_uid = p->p_ucred->cr_uid;
	pcred.pcr_ngroups = p->p_ucred->cr_ngroups;
	bcopy(p->p_ucred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));

	error = webdav_sendmsg(WEBDAV_FILE_DELETE, WEBDAV_USE_URL, pt, &pcred, fmp, p,
		(void *)NULL, 0, &server_error, (void *)NULL, 0, vp);
	if (error)
	{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
		printf("webdav_remove: webdav_sendmsg: %d\n", error);
#endif
		error = EIO;
		goto bad;
	}

	if (server_error)
	{
		error = server_error;
		goto bad;
	}

#if !WEBDAV_VNODE_LOCKING
	/* We pended so check the state of the vnode */
	if (WEBDAV_CHECK_VNODE(vp))
	{
		error = EPERM;
		vp = NULL;
		goto bad;
	}
#endif
	
	/* Get the node off of the cache so that other lookups
	 * won't find it and think the file still exists
	 */
	pt->pt_status |= WEBDAV_DELETED;
	webdav_hashrem(pt);

	if ( dvp != vp )
	{
		VOP_UNLOCK(vp, 0, ap->a_cnp->cn_proc);
	}
	
	(void) ubc_uncache(vp); 

	vrele(vp);
	vput(dvp);

exit:

	RET_ERR("webdav_remove", error);

bad:

	if (vp)
	{
		if (dvp == vp)
		{
			vrele(vp);
		}
		else
		{
			vput(vp);
		}
	}
	vput(dvp);
	goto exit;
}

/*****************************************************************************/

/*
 * webdav_rmdir
 *
 * webdav_rmdir removes a directory.
 *
 * results:
 *	0		Success.
 *	ENOTEMPTY Directory was not empty.
 *	EIO		A physical I/O error has occurred, or this error was generated for
 *			implementation-defined reasons.
 */
static int webdav_rmdir(ap)
	struct vop_remove_args	/* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
		} */ *ap;
{
	struct vnode *vp;
	struct vnode *dvp;
	struct webdavnode *pt;
	struct webdavmount *fmp;
	int error;
	int server_error;
	struct proc *p;
	struct webdav_cred pcred;

	vp = ap->a_vp;
	dvp = ap->a_dvp;
	pt = VTOWEBDAV(vp);
	fmp = VFSTOWEBDAV(vp->v_mount);
	p = current_proc();
	error = server_error = 0;
	
	/* No rmdir "." please. */
	if ( pt == VTOWEBDAV(dvp) )
	{
		vrele(dvp);
		vput(vp);
		error = EINVAL;
		goto out;
	}
	
	/* user level is ignoring the pcred anyway */
	pcred.pcr_flag = 0;
	pcred.pcr_uid = p->p_ucred->cr_uid;
	pcred.pcr_ngroups = p->p_ucred->cr_ngroups;
	bcopy(p->p_ucred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));

	error = webdav_sendmsg(WEBDAV_DIR_DELETE, WEBDAV_USE_URL, pt, &pcred, fmp, p, (void *)NULL, 0,
		&server_error, (void *)NULL, 0, vp);
	if (error)
	{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
		printf("webdav_rmdir: webdav_sendmsg: %d\n", error);
#endif
		error = EIO;
		goto bad;
	}

	if (server_error)
	{
		error = server_error;
	}

	
bad:

	if (dvp)
	{
		vput(dvp);
	}

#if !WEBDAV_VNODE_LOCKING
	if (WEBDAV_CHECK_VNODE(vp))
	{
		error = EPERM;
	}
	else
	{
		vput(vp);
	}
#else
	vput(vp);
#endif

out:

	RET_ERR("webdav_rmdir", error);
}												/* webdav_rmdir */

/*****************************************************************************/

#if WEBDAV_VNODE_LOCKING
	#error "Needs vnode locking work"
#endif
static int webdav_create(ap)
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
	/* user level is ignoring the pcred anyway */

	pcred.pcr_uid = cnp->cn_cred->cr_uid;
	pcred.pcr_ngroups = cnp->cn_cred->cr_ngroups;
	bcopy(cnp->cn_cred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));

	error = webdav_sendmsg(vnop, WEBDAV_USE_URL, pt, &pcred, fmp, p, (void *)NULL, 0,
		&server_error, (void *)NULL, 0, *vpp);
	if (error)
	{
		goto bad;
	}

	if (server_error)
	{
		error = server_error;
		goto bad;
	}

#if !WEBDAV_VNODE_LOCKING
	/* We pended so check the state of the vnode */
	if (WEBDAV_CHECK_VNODE(*vpp))
	{
		error = EIO;
		*vpp = NULL; /* so it won't be vrele'd */
		goto bad;
	}
#endif

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

#if !WEBDAV_VNODE_LOCKING
	/* ubc_info_init() may have blocked, check the state of the vnode */
	if (WEBDAV_CHECK_VNODE(*vpp))
	{
		error = EIO;
		*vpp = NULL; /* so it won't be vrele'd */
		goto bad;
	}
#endif

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

#if WEBDAV_VNODE_LOCKING
	#error "Needs vnode locking work"
#endif
static int webdav_rename(ap)
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
	struct vnode *fdvp = ap->a_fdvp;
	struct webdavnode *fpt;
	struct webdavnode *tpt;
	struct webdavmount *fmp;
	int error = 0, server_error = 0, created_object = 0, existing_object = 0;
	struct proc *p = current_proc();
	int vnop = WEBDAV_RENAME;
	webdav_rename_header_t * rename_header = NULL;
	int message_size;
	struct webdav_cred pcred;

	fmp = VFSTOWEBDAV((fvp)->v_mount);
	
	/* make sure the new parent directory is on the same WebDAV mount */
	if ( (tdvp->v_tag != VT_WEBDAV) || (VFSTOWEBDAV((tdvp)->v_mount) != fmp) )
	{
		error = EXDEV;
		goto done;
	}

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
#if !WEBDAV_VNODE_LOCKING
		/* ubc_info_init() may have blocked, check the state of the vnodes */
		if (WEBDAV_CHECK_VNODE(fdvp) ||
			WEBDAV_CHECK_VNODE(fvp)	 ||
			WEBDAV_CHECK_VNODE(tdvp) ||
			WEBDAV_CHECK_VNODE(tvp))
		{
			error = EIO;
			goto bailout;
		}
#endif
	}

	fpt = VTOWEBDAV(fvp);
	tpt = VTOWEBDAV(tvp);

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

#if !WEBDAV_VNODE_LOCKING
	/* We pended so check the state of the vnodes */
	if (WEBDAV_CHECK_VNODE(fdvp) ||
		WEBDAV_CHECK_VNODE(fvp)	 ||
		WEBDAV_CHECK_VNODE(tdvp) ||
		WEBDAV_CHECK_VNODE(tvp))
	{
		error = EIO;
		goto bailout;
	}
#endif

	rename_header->wd_first_uri_size = fpt->pt_size;
	rename_header->wd_second_uri_size = tpt->pt_size;

	bcopy(fpt->pt_arg, ((char *)rename_header) + sizeof(webdav_rename_header_t), fpt->pt_size);
	bcopy(tpt->pt_arg, ((char *)rename_header) + sizeof(webdav_rename_header_t) + fpt->pt_size,
		tpt->pt_size);

	pcred.pcr_flag = 0;
	/* user level is ignoring the pcred anyway */

	pcred.pcr_uid = fcnp->cn_cred->cr_uid;
	pcred.pcr_ngroups = fcnp->cn_cred->cr_ngroups;
	bcopy(fcnp->cn_cred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));

	error = webdav_sendmsg(vnop, WEBDAV_USE_INPUT, fpt, &pcred, fmp, p, (void *)rename_header,
		message_size, &server_error, (void *)NULL, 0, fvp);
	if (!error && server_error)
	{
		error = server_error;
	}

#if !WEBDAV_VNODE_LOCKING
	/* We pended so check the state of the vnodes */
	if (WEBDAV_CHECK_VNODE(fdvp) ||
		WEBDAV_CHECK_VNODE(fvp)	 ||
		WEBDAV_CHECK_VNODE(tdvp) ||
		WEBDAV_CHECK_VNODE(tvp))
	{
		error = EIO;
		goto bailout;
	}
#endif

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

	FREE((void *)rename_header, M_TEMP);

done:
	
#if !WEBDAV_VNODE_LOCKING
	/* We pended so check the state of the vnodes */
	if (WEBDAV_CHECK_VNODE(fdvp) ||
		WEBDAV_CHECK_VNODE(fvp)	 ||
		WEBDAV_CHECK_VNODE(tdvp))
	{
		error = EIO;
		goto bailout;
	}
	if (tvp != NULL)
	{
		if (WEBDAV_CHECK_VNODE(tvp))
		{
			error = EIO;
			goto bailout;
		}
	}
#endif

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
	vrele(fdvp);

exit:

	RET_ERR("webdav_rename", error);

#if !WEBDAV_VNODE_LOCKING
bailout:
	/* ugly cleanup when WEBDAV_CHECK_VNODE failed */
	if (!WEBDAV_CHECK_VNODE(fdvp))
	{
		vrele(fdvp);
	}
	if (!WEBDAV_CHECK_VNODE(fvp))
	{
		vrele(fvp);
	}
	if (!WEBDAV_CHECK_VNODE(tdvp))
	{
		if (!WEBDAV_CHECK_VNODE(tvp))
		{
			if (tvp == tdvp)
			{
				vrele(tdvp);
			}
			else
			{
				vput(tdvp);
			}
		}
		vput(tdvp);
	}
	if (rename_header)
	{
		FREE((void *)rename_header, M_TEMP);
	}
	goto exit;
#endif
}

/*****************************************************************************/

#if WEBDAV_VNODE_LOCKING
	#error "Needs vnode locking work"
#endif
static int webdav_mkdir(ap)
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
	/* user level is ignoring the pcred anyway */

	pcred.pcr_uid = cnp->cn_cred->cr_uid;
	pcred.pcr_ngroups = cnp->cn_cred->cr_ngroups;
	bcopy(cnp->cn_cred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));

	error = webdav_sendmsg(vnop, WEBDAV_USE_URL, pt, &pcred, fmp, p, (void *)NULL, 0,
		&server_error, (void *)NULL, 0, *vpp);

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

/*
 * webdav_setattr
 *
 * webdav_setattr set the attributes of a file.
 *
 * results:
 *	0		Success.
 *	EACCES	vp was VROOT.
 *	EINVAL	unsettable attribute
 *	EROFS	read-only file system
 *	EIO		A physical I/O error has occurred, or this error was generated for
 *			implementation-defined reasons.
 */
static int webdav_setattr(ap)
	struct vop_setattr_args	/* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
		} */ *ap;
{
	int error;
	struct vnode *vp;
	struct webdavnode *pt;
	struct vnode *cachevp;
	struct vattr *vap;
	struct vattr attrbuf;

	error = 0;
	vp = ap->a_vp;
	pt = VTOWEBDAV(vp);
	cachevp = pt->pt_cache_vnode;
	vap = ap->a_vap;
	
	/* Can't mess with the root vnode */
	if (vp->v_flag & VROOT)
	{
		error = EACCES;
		goto exit;
	}

	/* Check for unsettable attributes */
	if ( (vap->va_type != VNON) ||
		 (vap->va_nlink != VNOVAL) ||
		 (vap->va_fsid != VNOVAL) ||
		 (vap->va_fileid != VNOVAL) ||
		 (vap->va_blocksize != VNOVAL) ||
		 (vap->va_gen != (u_long)VNOVAL) ||
		 (vap->va_rdev != VNOVAL) ||
		 ((int)vap->va_bytes != VNOVAL) )
	{
		error = EINVAL;
		goto exit;
	}
	
	/* Check for attempts to change a read-only mount */
	if ( (vp->v_mount->mnt_flag & MNT_RDONLY) &&
		 ((vap->va_mode != (mode_t)VNOVAL) ||
		  (vap->va_uid != (uid_t)VNOVAL) ||
		  (vap->va_gid != (gid_t)VNOVAL) ||
		  (vap->va_size != (u_quad_t)VNOVAL) ||
		  (vap->va_atime.tv_sec != VNOVAL) ||
		  (vap->va_mtime.tv_sec != VNOVAL) || 
		  (vap->va_flags != (u_long)VNOVAL)) )
	{
		error = EROFS;
		goto exit;
	}

	/* If there is a local cache file, we'll allow setting.	We won't talk to the
	 * server, but we will honor the local file set. This will at least make fsx work.
	 */
	if ( cachevp != NULLVP )
	{
		/* lock the cache vnode */
		error = VOP_LOCK(cachevp, LK_EXCLUSIVE | LK_RETRY, ap->a_p);
		if (error)
		{
			goto exit;
		}
		
#if !WEBDAV_VNODE_LOCKING
		if (WEBDAV_CHECK_VNODE(vp))
		{
			error = EPERM;
			goto unlock_exit;
		}
#endif
		
		/* If we are changing the size, call ubc_setsize to fix things up
		 * with the UBC Also, make sure that we wait until the file is
		 * completely downloaded */
		if ((ap->a_vap->va_size != (u_quad_t)VNOVAL) && (vp->v_type == VREG))
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
					* writes until it is done, so unlock, sleep, and check again.
					*/
					VOP_UNLOCK(cachevp, 0, ap->a_p);
					
					(void) tsleep(&lbolt, PCATCH, "webdav_setattr", 1);
					
					/* lock the cache vnode */
					error = VOP_LOCK(cachevp, LK_EXCLUSIVE | LK_RETRY, ap->a_p);
					if ( error )
					{
						goto exit;
					}
					
#if !WEBDAV_VNODE_LOCKING
					/* After pending on tsleep and VOP_LOCK, check the state of the vnode */
					if (WEBDAV_CHECK_VNODE(vp))
					{
						error = EPERM;
						goto unlock_exit;
					}
#endif
				}
				else
				{
					/* the file has been downloaded and cachevp is still VOP_LOCK'ed */
					break; /* out of while (TRUE) loop */
				}
			} while ( TRUE );

			if (attrbuf.va_flags & UF_APPEND)
			{
				/* If the UF_APPEND flag is set, there was an error downloading the file from the
				 * server, so exit with an EIO result.
				 */
				error = EIO;
				goto unlock_exit;
			}

			/* At this point, cachevp is locked and the file is completely downloaded into cachevp */
	
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
			
			/* set the size and other attributes of the cache file */
			error = VOP_SETATTR(cachevp, ap->a_vap, ap->a_cred, ap->a_p);
			
			/* unlock the cache file before calling ubc_setsize */
			VOP_UNLOCK(cachevp, 0, ap->a_p);
			
#if !WEBDAV_VNODE_LOCKING
			if (WEBDAV_CHECK_VNODE(vp))
			{
				error = EPERM;
				goto exit;
			}
#endif
			
			if (UBCINFOEXISTS(vp))
			{
				/* let the UBC know the new size */
				(void) ubc_setsize(vp, (off_t)ap->a_vap->va_size);
			}
		}
		else
		{
			/* set the attributes of the cache file */
			error = VOP_SETATTR(cachevp, ap->a_vap, ap->a_cred, ap->a_p);
			
			VOP_UNLOCK(cachevp, 0, ap->a_p);
		}
	}

exit:

	RET_ERR("webdav_setattr", error);

unlock_exit:

	VOP_UNLOCK(cachevp, 0, ap->a_p);
	goto exit;
}

/*****************************************************************************/

/*
 * webdav_readdir
 *
 * webdav_readdir reads directory entries. We'll use the cache file for
 * the needed I/O.
 *
 * results:
 *	0		Success.
 *	ENOTDIR	attempt to use non-VDIR.
 *	EOPNOTSUPP a_ncookies was not zero.
 *	EINVAL	no cache file, or attempt to read from illegal offset in the directory.
 *	EIO		A physical I/O error has occurred, or this error was generated for
 *			implementation-defined reasons.
 */
static int webdav_readdir(ap)
	struct vop_readdir_args	/* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		u_long *a_cookies;
		int a_ncookies;
		} */ *ap;
{
	struct vnode *vp;
	struct vnode *cachevp;
	struct webdavnode *pt;
	struct webdav_cred pcred;
	struct webdavmount *fmp;
	int server_error;
	register struct uio *uio;
	int error;
	size_t count, lost;
	struct vattr vattr;
	struct proc *p;

	vp = ap->a_vp;
	uio = ap->a_uio;
	pt = VTOWEBDAV(vp);
	fmp = VFSTOWEBDAV(vp->v_mount);
	p = current_proc();
	error = 0;
	
	/* First make sure it is a directory we are dealing with */
	if (vp->v_type != VDIR)
	{
		error = ENOTDIR;
		goto done;
	}

	/*
	 * We don't allow exporting webdav mounts, and currently local
	 * requests do not need cookies.
	 */
	if (ap->a_ncookies)
	{
		error = EOPNOTSUPP;
		goto done;
	}

	/* Are we starting from the beginning? If so check the
	 * WEBDAV_DIR_NOT_LOADED status. If it is set, refresh
	 * the directory and clear it. If it is not set, set it so that the
	 * next time around we will do a refresh (XXX we don't set it today) */
	if ( (uio->uio_offset == 0) || (pt->pt_status & WEBDAV_DIR_NOT_LOADED) )
	{
		bzero(&pcred, sizeof(pcred));
		if (ap->a_cred)
		{
			pcred.pcr_uid = ap->a_cred->cr_uid;
			pcred.pcr_ngroups = ap->a_cred->cr_ngroups;
			bcopy(ap->a_cred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));
		}

		error = webdav_sendmsg(
			ISSET(vp->v_flag, VNOCACHE_DATA) ? WEBDAV_DIR_REFRESH : WEBDAV_DIR_REFRESH_CACHE,
			WEBDAV_USE_HANDLE, pt, &pcred, fmp,
			p, (void *)NULL, 0, &server_error, (void *)NULL, 0, vp);
		if (error)
		{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
			printf("webdav_readdir: webdav_sendmsg: %d\n", error);
#endif
			error = EIO;
			goto done;
		}

		if (server_error)
		{
			error = server_error;
			goto done;
		}

#if !WEBDAV_VNODE_LOCKING
		/* We pended so check the state of the vnode */
		if (WEBDAV_CHECK_VNODE(vp))
		{
			/* Permissiond denied seems to be what comes back
			 * when you try to enumerate a no longer existing
			 * directory */
			error = EPERM;
			goto done;
		}
#endif
		
		/* We didn't get an error so turn off the dir not loaded bit */
		pt->pt_status &= ~WEBDAV_DIR_NOT_LOADED;
	}

	/* Make sure we do have a cache file. If not the call
	 * must be wrong some how */
	cachevp = pt->pt_cache_vnode;
	if ( cachevp == NULLVP )
	{
		error = EINVAL;
		goto done;
	}

	/* Make sure we don't return partial entries. */
	if ( ((uio->uio_offset % sizeof(struct dirent)) != 0) ||
		 (uio->uio_resid < (int)sizeof(struct dirent)) )
	{
		error = EINVAL;
		goto done;
	}

	count = uio->uio_resid;
	count -= (uio->uio_offset + count) % sizeof(struct dirent);
	if (count <= 0)
	{
		error = EINVAL;
		goto done;
	}

	lost = uio->uio_resid - count;
	uio->uio_resid = count;
	uio->uio_iov->iov_len = count;

	error = VOP_LOCK(cachevp, LK_SHARED | LK_RETRY, p);
	if (error)
	{
		goto done;
	}

	error = VOP_READ(cachevp, uio, 0, ap->a_cred);
	
	VOP_UNLOCK(cachevp, 0, p);
	
	uio->uio_resid += lost;
	
	if (ap->a_eofflag)
	{
		error = VOP_GETATTR(cachevp, &vattr, ap->a_cred, p);
		if (error)
		{
			goto done;
		}
		*ap->a_eofflag = (off_t)vattr.va_size <= uio->uio_offset;
	}

#if !WEBDAV_VNODE_LOCKING
	if (WEBDAV_CHECK_VNODE(vp))
	{
		error = EPERM;
		goto done;
	}
#endif

done:

	RET_ERR("webdav_readdir", error);
}

/*****************************************************************************/

#if WEBDAV_VNODE_LOCKING
	#error "Needs vnode locking work"
#endif
static int webdav_inactive(ap)
	struct vop_inactive_args	/* {
		struct vnode *a_vp;
		struct proc *a_p;
		} */ *ap;
{
	int error;
	struct vnode *vp;
	struct proc *p;
	struct webdav_cred pcred;
	int last_close;	/* TRUE if this is the last close */
	
	vp = ap->a_vp;
	p = ap->a_p;
	
	/* no errors yet */
	error = 0;
	
	if ( prtactive && (vp->v_usecount != 0) )
	{
		vprint("webdav_inactive: pushing active", vp);
	}
	
	webdav_hashrem(vp->v_data);

	/* Now with UBC it may be possible that the "last close"
	 * isn't really seen by the file system until inactive
	 * is called. Thus if we see a cache file and a file_handle
	 * for this vnode, we should tell the server process to close
	 * up. This is because the call sequence of mmap is open()
	 * mmap(), close(). After this point I/O can go on to
	 * the file and the vm system will be holding the reference.
	 * Not until the process dies and vm cleans up do the mappings
	 * go away and the file can be safely pushed back to the
	 * server.
	 */

	/* set up webdav_cred */
	pcred.pcr_flag = 0;
	pcred.pcr_uid = p->p_ucred->cr_uid;
	pcred.pcr_ngroups = p->p_ucred->cr_ngroups;
	bcopy(p->p_ucred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));
	
	/* always the last_close */		
	last_close = TRUE;
	
	/* Note: errors are ignored by VFS */
	error = webdav_close_inactive(vp, p, NULL, &pcred, last_close);

#if !WEBDAV_VNODE_LOCKING
	if (WEBDAV_CHECK_VNODE(vp))
	{
		error = EPERM;
	}
	else
	{
		VOP_UNLOCK(vp, 0, ap->a_p);
	}
#else
	VOP_UNLOCK(vp, 0, ap->a_p);
#endif
	
	RET_ERR("webdav_inactive", error);
}

/*****************************************************************************/

#if WEBDAV_VNODE_LOCKING
	#error "Needs vnode locking work"
#endif
static int webdav_reclaim(ap)
	struct vop_reclaim_args	/* {
		struct vnode *a_vp;
		} */ *ap;
{
	struct webdavnode *pt = VTOWEBDAV(ap->a_vp);

	if ( prtactive && (ap->a_vp->v_usecount != 0) )
	{
		vprint("webdav_reclaim: pushing active", ap->a_vp);
	}

	if (pt->pt_arg)
	{
		FREE((caddr_t)pt->pt_arg, M_TEMP);
		pt->pt_arg = NULL;
	}
	FREE(ap->a_vp->v_data, M_TEMP);
	ap->a_vp->v_data = NULL;

	return ( 0 );
}

/*****************************************************************************/

/*
 * webdav_pathconf
 *
 * Return POSIX pathconf information.
 */
static int webdav_pathconf(ap)
	struct vop_pathconf_args	/* {
		struct vnode *a_vp;
		int a_name;
		int *a_retval;
		} */ *ap;
{
	int error;
	
	error = 0;
	
	switch (ap->a_name)
	{
		case _PC_LINK_MAX:
			/* 1 because we do not support link counts */
			*ap->a_retval = 1;
			break;
			
		case _PC_NAME_MAX:
			/* The maximum number of bytes in a file name */
			*ap->a_retval = NAME_MAX;
			break;
		
		case _PC_PATH_MAX:
			/* The maximum number of bytes in a pathname */
			*ap->a_retval = PATH_MAX;
			break;
		
		case _PC_CHOWN_RESTRICTED:
			/* Return 1 if appropriate privileges are required for the chown(2) */
			*ap->a_retval = _POSIX_CHOWN_RESTRICTED;
			break;
		
		case _PC_NO_TRUNC:
			/* Return 1 if file names longer than KERN_NAME_MAX are truncated */
			*ap->a_retval = _POSIX_NO_TRUNC;
			break;
		
		default:
			/* webdavfs doesn't support any other name arguments */
			*ap->a_retval = -1;
			error = EINVAL;
			break;
	}
	
	return ( error );
}

/*****************************************************************************/

/*
 * webdav_print
 *
 * Print out the contents of a webdav vnode.
 */
static int webdav_print(ap)
	struct vop_print_args	/* {
		struct vnode *a_vp;
		} */ *ap;
{
	struct webdavnode *pt = VTOWEBDAV(ap->a_vp);
	
	/* print a few things from the webdavnode */ 
	printf("tag VT_WEBDAV, webdav, id=%d, depth=%d, size=%d, path=%s\n",
		pt->pt_fileid, pt->pt_depth, pt->pt_size, pt->pt_arg);
	return (0);
}

/*****************************************************************************/

/*
 * webdav_pagein
 *
 * Page in (read) a page of a file into memory.
 *
 * To do:
 *		If UBC ever supports an efficient way to move pages from the cache file's
 *		UPL to the webdav UPL (a_pl), use that method instead of calling VOP_READ.
 */
static int webdav_pagein(ap)
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
	struct proc *p;
	struct vnode *cachevp;
	struct vnode *vp;
	struct webdavnode *pt;
	struct iovec aiov;
	int bytes_to_zero;
	int error;
	int tried_bytes;
	struct vattr attrbuf;
	kern_return_t kret;

	vp = ap->a_vp;
	pt = VTOWEBDAV(vp);
	cachevp = pt->pt_cache_vnode;
	p = current_proc();
	error = 0;

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
	tried_bytes = FALSE;
	do
	{
		error = VOP_LOCK(cachevp, LK_EXCLUSIVE | LK_RETRY, p);
		if (error)
		{
			goto exit;
		}

		error = VOP_GETATTR(cachevp, &attrbuf, ap->a_cred, p);
		if (error)
		{
			goto unlock_exit;
		}

#if !WEBDAV_VNODE_LOCKING
		if (WEBDAV_CHECK_VNODE(vp))
		{
			error = EPERM;
			goto unlock_exit;
		}
#endif

		if ((attrbuf.va_flags & UF_NODUMP) && (auio.uio_offset + auio.uio_resid) > (off_t)attrbuf.va_size)
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
			 * 4. The download finishes and the underlying cache file has
			 *	 the old data, possibly depending on how the server works.
			 */
			VOP_UNLOCK(cachevp, 0, p);
			
			if (!tried_bytes)
			{
				if ((auio.uio_offset + auio.uio_resid) > ((off_t)attrbuf.va_size + WEBDAV_WAIT_IF_WITHIN))
				{
					error = webdav_read_bytes(vp, &auio, ap->a_cred);
					if (!error)
					{
						if (!auio.uio_resid)
						{
#if !WEBDAV_VNODE_LOCKING
							if ( WEBDAV_CHECK_VNODE(vp) )
							{
								error = EPERM;
							}
#endif
							goto exit;
						}
						else
						{
							/* we did not get all the data we wanted, we don't
							* know why so we'll just give up on the byte access
							* and wait for the data to download. We need to reset
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

			(void) tsleep(&lbolt, PCATCH, "webdav_pagein", 1);
			
#if !WEBDAV_VNODE_LOCKING
			/* After pending on tsleep, check the state of the vnode */
			if (WEBDAV_CHECK_VNODE(vp))
			{
				error = EPERM;
				goto exit;
			}
#endif
		}
		else
		{
			/* the part we need has been downloaded 
			  and cachevp is still VOP_LOCK'ed */
			break;
		}
	} while ( TRUE );

	if (attrbuf.va_flags & UF_APPEND)
	{
		/* If the UF_APPEND flag is set, there was an error downloading the file from the
		 * server, so exit with an EIO result.
		 */
		error = EIO;
		goto unlock_exit;
	}

	/* At this point, cachevp is locked and either the file is completely downloaded into
	 * cachevp, or the page this I/O ends within has been completely downloaded into cachevp.
	 */
	
	if (ap->a_f_offset > (off_t)attrbuf.va_size)
	{
		/* Trying to pagein data beyond the eof is a no no */
		error = EFAULT;
		goto unlock_exit;
	}

	error = VOP_READ(cachevp, &auio, ((ap->a_flags & UPL_IOSYNC) ? IO_SYNC : 0), ap->a_cred);

	if (auio.uio_resid)
	{
		/* If we were not able to read the entire page, check to
		 * see if we are at the end of the file, and if so, zero
		 * out the remaining part of the page
		 */
		if ((off_t)attrbuf.va_size < ap->a_f_offset + ap->a_size)
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

	if ( (ap->a_flags & UPL_NOCOMMIT) == 0 )
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

/*
 * webdav_pageout
 *
 * Page out (write) a page of a file from memory.
 *
 * To do:
 *		If UBC ever supports an efficient way to move pages from the webdav
 *		UPL (a_pl) to the cache file's UPL, use that method instead of calling VOP_WRITE.
 */
static int webdav_pageout(ap)
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
	struct proc *p;
	struct vnode *cachevp;
	struct vnode *vp;
	struct webdavnode *pt;
	struct iovec aiov;
	int error;
	kern_return_t kret;
	struct vattr attrbuf;
	
	vp = ap->a_vp;
	pt = VTOWEBDAV(vp);
	cachevp = pt->pt_cache_vnode;
	p = current_proc();
	error = 0;

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

	if (vp->v_mount->mnt_flag & MNT_RDONLY)
	{
		error = EROFS;
		goto exit;
	}

	do
	{
		error = VOP_LOCK(cachevp, LK_EXCLUSIVE | LK_RETRY, p);
		if (error)
		{
			goto exit;
		}

		error = VOP_GETATTR(cachevp, &attrbuf, ap->a_cred, p);
		if (error)
		{
			goto unlock_exit;
		}

#if !WEBDAV_VNODE_LOCKING
		if (WEBDAV_CHECK_VNODE(vp))
		{
			error = EPERM;
			goto unlock_exit;
		}
#endif

		if ((attrbuf.va_flags & UF_NODUMP) && (auio.uio_offset + auio.uio_resid) > (off_t)attrbuf.va_size)
		{
			/* We are downloading the file and we haven't gotten to
			 * to the bytes we need so unlock, sleep, and try the whole
			 * thing again.
			 */
			VOP_UNLOCK(cachevp, 0, p);
			
			(void) tsleep(&lbolt, PCATCH, "webdav_pageout", 1);
		}
		else
		{
			/* the part we need has been downloaded 
			  and cachevp is still VOP_LOCK'ed */
			break;
		}
	} while ( TRUE );

	if (attrbuf.va_flags & UF_APPEND)
	{
		/* If the UF_APPEND flag is set, there was an error downloading the file from the
		 * server, so exit with an EIO result.
		 */
		error = EIO;
		goto unlock_exit;
	}

	/* We don't want to write past the end of the file so 
	 * truncate the write to the size.
	 */
	if (auio.uio_offset + auio.uio_resid > (off_t)attrbuf.va_size)
	{
		if (auio.uio_offset < (off_t)attrbuf.va_size)
		{
			auio.uio_resid = attrbuf.va_size - auio.uio_offset;
		}
		else
		{
			/* If we are here, someone probably truncated a file that
			 * someone else had mapped. In any event we are not allowed
			 * to grow the file on a page out so return EFAULT as that is
			 * what VM is expecting.
			 */
			error = EFAULT;
			goto unlock_exit;
		}
	}

	error = VOP_WRITE(cachevp, &auio, ((ap->a_flags & UPL_IOSYNC) ? IO_SYNC : 0), ap->a_cred);


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

	if ( (ap->a_flags & UPL_NOCOMMIT) == 0 )
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

static int webdav_ioctl(ap)
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		int  a_command;
		caddr_t  a_data;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
		} */ *ap;
{
	int error;
	struct vnode *vp;
	
	error = EINVAL;
	vp = ap->a_vp;
	
	switch (ap->a_command)
	{
	case WEBDAVINVALIDATECACHES:	/* invalidate all mount_webdav caches */
		{
			struct webdavnode *pt;
			struct webdav_cred pcred;
			struct webdavmount *fmp;
			struct proc *p;
			int server_error;
			
			/* Note: Since this command is coming through fsctl(), the vnode is locked. */
			
			/* set up the rest of the parameters needed to send a message */ 
			pt = VTOWEBDAV(vp);
			bzero(&pcred, sizeof(pcred));
			if (ap->a_cred)
			{
				pcred.pcr_uid = ap->a_cred->cr_uid;
				pcred.pcr_ngroups = ap->a_cred->cr_ngroups;
				bcopy(ap->a_cred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));
			}
			fmp = VFSTOWEBDAV((vp)->v_mount);
			p = current_proc();
			server_error = 0;
			error = webdav_sendmsg(WEBDAV_INVALIDATE_CACHES, WEBDAV_USE_INPUT, pt, &pcred, fmp, p,
				NULL, 0,	/* no input */
				&server_error,
				NULL, 0,	/* no output */
				vp);
			if (error)
			{
#if (defined(DEBUG) || defined(WEBDAV_TRACE) || defined(WEBDAV_ERROR))
				printf("webdav_ioctl: webdav_sendmsg(WEBDAV_INVALIDATE_CACHES): %d\n", error);
#endif
				error = EIO;
				break;
			}
	
			if (server_error)
			{
				error = server_error;
				break;
			}
		}
		break;
		
	default:
		error = EINVAL;
		break;
	}

	RET_ERR("webdav_ioctl", error);
}

/*****************************************************************************/

#define VOPFUNC int (*)(void *)

/* vnode operations mapped to standard error implementations */
#define webdav_whiteout		err_whiteout
#define webdav_mknod		err_mknod
#define webdav_mkcomplex	err_mkcomplex
#define webdav_setattrlist	err_setattrlist
#define webdav_lease		err_lease
#define webdav_select		err_select
#define webdav_exchange		err_exchange
#define webdav_revoke		err_revoke
#define webdav_mmap			err_mmap
#define webdav_seek			err_seek
#define webdav_link			err_link
#define webdav_symlink		err_symlink
#define webdav_readdirattr	err_readdirattr
#define webdav_readlink		err_readlink
#define webdav_abortop		err_abortop
#define webdav_bmap			err_bmap
#define webdav_strategy		err_strategy
#define webdav_advlock		err_advlock
#define webdav_blkatoff		err_blkatoff
#define webdav_valloc		err_valloc
#define webdav_reallocblks	err_reallocblks
#define webdav_vfree		err_vfree
#define webdav_truncate		err_truncate
#define webdav_allocate		err_allocate
#define webdav_update		err_update
#define webdav_pgrd			err_pgrd
#define webdav_pgwr			err_pgwr
#define webdav_bwrite		err_bwrite
#define webdav_devblocksize	err_devblocksize
#define webdav_searchfs		err_searchfs
#define webdav_copyfile		err_copyfile
#define webdav_blktooff		err_blktooff
#define webdav_offtoblk		err_offtoblk
#define webdav_cmap			err_cmap

/* vnode operations mapped to system routines */
#if WEBDAV_VNODE_LOCKING
	#error "Needs vnode locking work - these should not be nops"
#endif
#define webdav_nop_lock		nop_lock
#define webdav_nop_unlock	nop_unlock
#define webdav_nop_islocked	nop_islocked

int( **webdav_vnodeop_p)();

struct vnodeopv_entry_desc webdav_vnodeop_entries[] = {
	{&vop_default_desc, (VOPFUNC)vn_default_error},			/* default */
	{&vop_lookup_desc, (VOPFUNC)webdav_lookup},				/* lookup */
	{&vop_create_desc, (VOPFUNC)webdav_create},				/* create */
	{&vop_whiteout_desc, (VOPFUNC)webdav_whiteout},			/* whiteout */
	{&vop_mknod_desc, (VOPFUNC)webdav_mknod},				/* mknod */
	{&vop_mkcomplex_desc, (VOPFUNC)webdav_mkcomplex},		/* mkcompelx */
	{&vop_open_desc, (VOPFUNC)webdav_open},					/* open */
	{&vop_close_desc, (VOPFUNC)webdav_close},				/* close */
	{&vop_access_desc, (VOPFUNC)webdav_access},				/* access */
	{&vop_getattr_desc, (VOPFUNC)webdav_getattr},			/* getattr */
	{&vop_setattr_desc, (VOPFUNC)webdav_setattr},			/* setattr */
	{&vop_getattrlist_desc, (VOPFUNC)webdav_getattrlist},	/* getattrlist */
	{&vop_setattrlist_desc, (VOPFUNC)webdav_setattrlist},	/* setattrlist */
	{&vop_read_desc, (VOPFUNC)webdav_read},					/* read */
	{&vop_write_desc, (VOPFUNC)webdav_write},				/* write */
	{&vop_lease_desc, (VOPFUNC)webdav_lease},				/* lease */
	{&vop_ioctl_desc, (VOPFUNC)webdav_ioctl},				/* ioctl */
	{&vop_select_desc, (VOPFUNC)webdav_select},				/* select */
	{&vop_exchange_desc, (VOPFUNC)webdav_exchange},			/* exchange */
	{&vop_revoke_desc, (VOPFUNC)webdav_revoke},				/* revoke */
	{&vop_mmap_desc, (VOPFUNC)webdav_mmap},					/* mmap */
	{&vop_fsync_desc, (VOPFUNC)webdav_fsync},				/* fsync */
	{&vop_seek_desc, (VOPFUNC)webdav_seek},					/* seek */
	{&vop_remove_desc, (VOPFUNC)webdav_remove},				/* remove */
	{&vop_link_desc, (VOPFUNC)webdav_link},					/* link */
	{&vop_rename_desc, (VOPFUNC)webdav_rename},				/* rename */
	{&vop_mkdir_desc, (VOPFUNC)webdav_mkdir},				/* mkdir */
	{&vop_rmdir_desc, (VOPFUNC)webdav_rmdir},				/* rmdir */
	{&vop_symlink_desc, (VOPFUNC)webdav_symlink},			/* symlink */
	{&vop_readdir_desc, (VOPFUNC)webdav_readdir},			/* readdir */
	{&vop_readdirattr_desc, (VOPFUNC)webdav_readdirattr},	/* readdirattr */
	{&vop_readlink_desc, (VOPFUNC)webdav_readlink},			/* readlink */
	{&vop_abortop_desc, (VOPFUNC)webdav_abortop},			/* abortop */
	{&vop_inactive_desc, (VOPFUNC)webdav_inactive},			/* inactive */
	{&vop_reclaim_desc, (VOPFUNC)webdav_reclaim},			/* reclaim */
	{&vop_lock_desc, (VOPFUNC)webdav_nop_lock},				/* lock */
	{&vop_unlock_desc, (VOPFUNC)webdav_nop_unlock},			/* unlock */
	{&vop_bmap_desc, (VOPFUNC)webdav_bmap},					/* bmap */
	{&vop_strategy_desc, (VOPFUNC)webdav_strategy},			/* strategy */
	{&vop_print_desc, (VOPFUNC)webdav_print},				/* print */
	{&vop_islocked_desc, (VOPFUNC)webdav_nop_islocked},		/* islocked */
	{&vop_pathconf_desc, (VOPFUNC)webdav_pathconf},			/* pathconf */
	{&vop_advlock_desc, (VOPFUNC)webdav_advlock},			/* advlock */
	{&vop_blkatoff_desc, (VOPFUNC)webdav_blkatoff},			/* blkatoff */
	{&vop_valloc_desc, (VOPFUNC)webdav_valloc},				/* valloc */
	{&vop_reallocblks_desc, (VOPFUNC)webdav_reallocblks},	/* reallocblks */
	{&vop_vfree_desc, (VOPFUNC)webdav_vfree},				/* vfree */
	{&vop_truncate_desc, (VOPFUNC)webdav_truncate},			/* truncate */
	{&vop_allocate_desc, (VOPFUNC)webdav_allocate},			/* allocate */
	{&vop_update_desc, (VOPFUNC)webdav_update},				/* update */
	{&vop_pgrd_desc, (VOPFUNC)webdav_pgrd},					/* pgrd */
	{&vop_pgwr_desc, (VOPFUNC)webdav_pgwr},					/* pgwr */
	{&vop_bwrite_desc, (VOPFUNC)webdav_bwrite},				/* bwrite */
	{&vop_pagein_desc, (VOPFUNC)webdav_pagein},				/* Pagein */
	{&vop_pageout_desc, (VOPFUNC)webdav_pageout},			/* Pageout */
	{&vop_devblocksize_desc, (VOPFUNC)webdav_devblocksize},	/* devblocksize */
	{&vop_searchfs_desc, (VOPFUNC)webdav_searchfs},			/* searchfs */
	{&vop_copyfile_desc, (VOPFUNC)webdav_copyfile},			/* copyfile */
	{&vop_blktooff_desc, (VOPFUNC)webdav_blktooff},			/* blktooff */
	{&vop_offtoblk_desc, (VOPFUNC)webdav_offtoblk},			/* offtoblk */
	{&vop_cmap_desc, (VOPFUNC)webdav_cmap},					/* cmap */
	{(struct vnodeop_desc *)NULL, (int( *)())NULL}			/* end of table */
};

struct vnodeopv_desc webdav_vnodeop_opv_desc = {
	&webdav_vnodeop_p, webdav_vnodeop_entries};

/*****************************************************************************/
