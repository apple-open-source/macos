/*
 * Copyright (c) 1999-2001 Ion Badulescu
 * Copyright (c) 1997-2002 Erez Zadok
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
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
 *    must display the following acknowledgment:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *
 * $Id: autofs_solaris_v1.c,v 1.1.1.1 2002/05/15 01:22:05 jkh Exp $
 *
 */

/****************************************************************
 ****************************************************************
 **************** THIS CODE IS NOT FUNCTIONAL!!!! ***************
 ****************************************************************
 ****************************************************************/

/*
 * Automounter filesystem
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

/*
 * KLUDGE: wrap whole file in HAVE_FS_AUTOFS, because
 * not all systems with an automounter file system are supported
 * by am-utils yet...
 */

#ifdef HAVE_FS_AUTOFS

/*
 * MACROS:
 */
#ifndef AUTOFS_NULL
# define AUTOFS_NULL	NULLPROC
#endif /* not AUTOFS_NULL */

/*
 * STRUCTURES:
 */

/*
 * VARIABLES:
 */

/* forward declarations */
# ifndef HAVE_XDR_MNTREQUEST
bool_t xdr_mntrequest(XDR *xdrs, mntrequest *objp);
# endif /* not HAVE_XDR_MNTREQUEST */
# ifndef HAVE_XDR_MNTRES
bool_t xdr_mntres(XDR *xdrs, mntres *objp);
# endif /* not HAVE_XDR_MNTRES */
# ifndef HAVE_XDR_UMNTREQUEST
bool_t xdr_umntrequest(XDR *xdrs, umntrequest *objp);
# endif /* not HAVE_XDR_UMNTREQUEST */
# ifndef HAVE_XDR_UMNTRES
bool_t xdr_umntres(XDR *xdrs, umntres *objp);
# endif /* not HAVE_XDR_UMNTRES */
static int mount_autofs(char *dir, char *opts);
static int autofs_mount_1_svc(struct mntrequest *mr, struct mntres *result, struct authunix_parms *cred);
static int autofs_unmount_1_svc(struct umntrequest *ur, struct umntres *result, struct authunix_parms *cred);

/****************************************************************************
 *** VARIABLES                                                            ***
 ****************************************************************************/

/****************************************************************************
 *** FUNCTIONS                                                            ***
 ****************************************************************************/

/*
 * AUTOFS XDR FUNCTIONS:
 */
# ifndef HAVE_XDR_MNTREQUEST
bool_t
xdr_mntrequest(XDR *xdrs, mntrequest *objp)
{
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_mntrequest:");
#endif /* DEBUG */

  if (!xdr_string(xdrs, &objp->name, A_MAXNAME))
    return (FALSE);

  if (!xdr_string(xdrs, &objp->map, A_MAXNAME))
    return (FALSE);

  if (!xdr_string(xdrs, &objp->opts, A_MAXOPTS))
    return (FALSE);

  if (!xdr_string(xdrs, &objp->path, A_MAXPATH))
    return (FALSE);

  return (TRUE);
}
# endif /* not HAVE_XDR_MNTREQUEST */


# ifndef HAVE_XDR_MNTRES
bool_t
xdr_mntres(XDR *xdrs, mntres *objp)
{
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_mntres:");
#endif /* DEBUG */

  if (!xdr_int(xdrs, &objp->status))
    return (FALSE);

  return (TRUE);
}
# endif /* not HAVE_XDR_MNTRES */


# ifndef HAVE_XDR_UMNTREQUEST
bool_t
xdr_umntrequest(XDR *xdrs, umntrequest *objp)
{
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_umntrequest:");
#endif /* DEBUG */

  if (!xdr_int(xdrs, &objp->isdirect))
    return (FALSE);

  if (!xdr_u_int(xdrs, (u_int *) &objp->devid))
    return (FALSE);

#ifdef HAVE_UMNTREQUEST_RDEVID
  if (!xdr_u_long(xdrs, &objp->rdevid))
    return (FALSE);
#endif /* HAVE_UMNTREQUEST_RDEVID */

  if (!xdr_pointer(xdrs, (char **) &objp->next, sizeof(umntrequest), (XDRPROC_T_TYPE) xdr_umntrequest))
    return (FALSE);

  return (TRUE);
}
# endif /* not HAVE_XDR_UMNTREQUEST */


# ifndef HAVE_XDR_UMNTRES
bool_t
xdr_umntres(XDR *xdrs, umntres *objp)
{
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_mntres:");
#endif /* DEBUG */

  if (!xdr_int(xdrs, &objp->status))
    return (FALSE);

  return (TRUE);
}
# endif /* not HAVE_XDR_UMNTRES */


/*
 * Mount an automounter directory.
 * The automounter is connected into the system
 * as a user-level NFS server.  mount_autofs constructs
 * the necessary NFS parameters to be given to the
 * kernel so that it will talk back to us.
 */
static int
mount_autofs(char *dir, char *opts)
{
  char fs_hostname[MAXHOSTNAMELEN + MAXPATHLEN + 1];
  char *map_opt, buf[MAXHOSTNAMELEN];
  int retry, error, flags;
  struct utsname utsname;
  mntent_t mnt;
  autofs_args_t autofs_args;
  MTYPE_TYPE type = MOUNT_TYPE_AUTOFS;

  memset((voidp) &autofs_args, 0, sizeof(autofs_args)); /* Paranoid */

  memset((voidp) &mnt, 0, sizeof(mnt));
  mnt.mnt_dir = dir;
  mnt.mnt_fsname = pid_fsname;
  mnt.mnt_opts = opts;
  mnt.mnt_type = type;

  retry = hasmntval(&mnt, "retry");
  if (retry <= 0)
    retry = 2;			/* XXX */

  /*
   * SET MOUNT ARGS
   */
  if (uname(&utsname) < 0) {
    strcpy(buf, "localhost.autofs");
  } else {
    strcpy(buf, utsname.nodename);
    strcat(buf, ".autofs");
  }
#ifdef HAVE_AUTOFS_ARGS_T_ADDR
  autofs_args.addr.buf = buf;
  autofs_args.addr.len = strlen(autofs_args.addr.buf);
  autofs_args.addr.maxlen = autofs_args.addr.len;
#endif /* HAVE_AUTOFS_ARGS_T_ADDR */

  autofs_args.path = dir;
  autofs_args.opts = opts;

  map_opt = hasmntopt(&mnt, "map");
  if (map_opt) {
    map_opt += sizeof("map="); /* skip the "map=" */
    if (map_opt == NULL) {
      plog(XLOG_WARNING, "map= has a null map name. reset to amd.unknown");
      map_opt = "amd.unknown";
    }
  }
  autofs_args.map = map_opt;

  /* XXX: these I set arbitrarily... */
  autofs_args.mount_to = 300;
  autofs_args.rpc_to = 60;
  autofs_args.direct = 0;

  /*
   * Make a ``hostname'' string for the kernel
   */
  sprintf(fs_hostname, "pid%ld@%s:%s",
	  get_server_pid(), am_get_hostname(), dir);

  /*
   * Most kernels have a name length restriction.
   */
  if (strlen(fs_hostname) >= MAXHOSTNAMELEN)
    strcpy(fs_hostname + MAXHOSTNAMELEN - 3, "..");

  /*
   * Finally we can compute the mount flags set above.
   */
  flags = compute_mount_flags(&mnt);

  /*
   * This is it!  Here we try to mount amd on its mount points.
   */
  error = mount_fs(&mnt, flags, (caddr_t) &autofs_args, retry, type, 0, NULL, mnttab_file_name);
  return error;
}


/****************************************************************************/
/* autofs program dispatcher */
static void
autofs_program_1(struct svc_req *rqstp, SVCXPRT *transp)
{
  int ret;
  union {
    mntrequest autofs_mount_1_arg;
    umntrequest autofs_umount_1_arg;
  } argument;
  union {
    mntres mount_res;
    umntres umount_res;
  } result;

  bool_t (*xdr_argument)(), (*xdr_result)();
  int (*local)();

  switch (rqstp->rq_proc) {

  case AUTOFS_NULL:
    svc_sendreply(transp,
		  (XDRPROC_T_TYPE) xdr_void,
		  (SVC_IN_ARG_TYPE) NULL);
    return;

  case AUTOFS_MOUNT:
    xdr_argument = xdr_mntrequest;
    xdr_result = xdr_mntres;
    local = (int (*)()) autofs_mount_1_svc;
    break;

  case AUTOFS_UNMOUNT:
    xdr_argument = xdr_umntrequest;
    xdr_result = xdr_umntres;
    local = (int (*)()) autofs_unmount_1_svc;
    break;

  default:
    svcerr_noproc(transp);
    return;
  }

  memset((char *) &argument, 0, sizeof(argument));
  if (!svc_getargs(transp,
		   (XDRPROC_T_TYPE) xdr_argument,
		   (SVC_IN_ARG_TYPE) &argument)) {
    plog(XLOG_ERROR,
	 "AUTOFS xdr decode failed for %d %d %d",
	 (int) rqstp->rq_prog, (int) rqstp->rq_vers, (int) rqstp->rq_proc);
    svcerr_decode(transp);
    return;
  }

  ret = (*local) (&argument, &result, rqstp);
  if (!svc_sendreply(transp,
		     (XDRPROC_T_TYPE) xdr_result,
		     (SVC_IN_ARG_TYPE) &result)) {
    svcerr_systemerr(transp);
  }

  if (!svc_freeargs(transp,
		    (XDRPROC_T_TYPE) xdr_argument,
		    (SVC_IN_ARG_TYPE) &argument)) {
    plog(XLOG_FATAL, "unable to free rpc arguments in autofs_program_1");
    going_down(1);
  }
}


static int
autofs_mount_1_svc(struct mntrequest *mr, struct mntres *result, struct authunix_parms *cred)
{
  int err = 0;
  am_node *anp, *anp2;

  /* XXX: needs to be fixed? */
  plog(XLOG_INFO, "autofs_mount_1_svc: %s:%s:%s:%s",
       mr->map, mr->name, mr->opts, mr->path);

  /* look for map (eg. "/home") */
  anp = find_ap(mr->path);
  if (!anp) {
    plog(XLOG_ERROR, "map %s not found", mr->path);
    err = ENOENT;
    goto out;
  }
  /* turn on autofs in map flags */
  if (!(anp->am_flags & AMF_AUTOFS)) {
    plog(XLOG_INFO, "turning on AMF_AUTOFS for node %s", mr->path);
    anp->am_flags |= AMF_AUTOFS;
  }

  /*
   * Look for (and create if needed) the new node.
   *
   * If an error occurred, return it.  If a -1 was returned, that indicates
   * that a mount is in progress, so sleep a while (while the backgrounded
   * mount is happening), and then signal the autofs to retry the mount.
   *
   * There's something I don't understand.  I was thinking that this code
   * here is the one which will succeed eventually and will send an RPC
   * reply to the kernel, but apparently that happens somewhere else, not
   * here.  It works though, just that I don't know how.  Arg. -Erez.
   * */
  err = 0;
  anp2 = autofs_lookuppn(anp, mr->name, &err, VLOOK_CREATE);
  if (!anp2) {
    if (err == -1) {		/* then tell autofs to retry */
      sleep(1);
      err = EAGAIN;
    }
    goto out;
  }

out:
  result->status = err;
  return err;
}


static int
autofs_unmount_1_svc(struct umntrequest *ur, struct umntres *result, struct authunix_parms *cred)
{
  int err = 0;

  /* XXX: needs to be fixed? */
  plog(XLOG_INFO, "autofs_unmount_1_svc: %d:%lu:%lu:0x%lx",
       ur->isdirect, (unsigned long) ur->devid, (unsigned long) ur->rdevid,
       (unsigned long) ur->next);

  err = EINVAL;			/* XXX: not implemented yet */
  goto out;

out:
  result->status = err;
  return err;
}

#endif /* HAVE_FS_AUTOFS */
