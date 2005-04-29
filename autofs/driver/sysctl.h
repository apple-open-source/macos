/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * Created by Patrick Dirks on Mon Mar 17 2003.
 *
 * $Id: sysctl.h,v 1.1 2004/02/04 03:36:01 alfred Exp $
 */

#ifndef SYSCTL_H
#define SYSCTL_H

int	vfslist_get(fsid_t **, int *);
void	vfslist_free(fsid_t *);
int	sysctl_fsid(int, fsid_t *, void *, size_t *, void *, size_t);
u_int	vfsevent_wait(int, int);
int	vfsevent_init(void);
int	vfslist_get(fsid_t **, int *);
void	vfslist_free(fsid_t *);
int	sysctl_statfs(fsid_t *, struct statfs *, int);
int	sysctl_queryfs(fsid_t *, struct vfsquery *);
int	sysctl_unmount(fsid_t *, int);
int	sysctl_setfstimeout(fsid_t *, int);
int	sysctl_getfstimeout(fsid_t *, int *);

#endif /* !SYSCTL_H */
