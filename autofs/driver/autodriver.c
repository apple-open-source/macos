/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * Created by Alfred Perlstein on Tue Feb 3 2004.
 *
 * $Id: autodriver.c,v 1.6 2004/03/27 06:11:05 alfred Exp $
 */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include "sysctl.h"
#include "autofs.h"

struct autoentry {
	char *ae_mnt, *ae_path, *ae_type, *ae_opts, *ae_rpath, *ae_free;
	int ae_line;
	struct autoentry *ae_next;
};

struct autoentry *entries;
const char *mount_prog = "mount";

void *
xmalloc(size_t size)
{
	void *ret;

	ret = malloc(size);
	if (ret == NULL)
		err(1, "malloc %d", (int) size);
	return (ret);
}

void
parsetab(void)
{
	FILE *fp;
	const char *tab = "autotab";
	char *cp, *p, *line;
	size_t len;
	struct autoentry *ent;
	int lineno = 0;

	fp = fopen(tab, "r");
	if (fp == NULL)
		err(1, "fopen %s", tab);

	while ((cp = fgetln(fp, &len)) != NULL) {
		lineno++;
		while (len > 0 && isspace(cp[len - 1]))
			len--;
		line = xmalloc(len + 1);
		bcopy(cp, line, len);
		line[len] = '\0';
		cp = line;
		if ((cp = strchr(line, '#')) != NULL)
			*cp = '\0';
		cp = line;
		while (isspace(*cp))
			cp++;
		if (*cp == '\0') {
			free(line);
			continue;
		}
		ent = xmalloc(sizeof(*ent));
		if ((p = strsep(&cp, " \t")) == NULL)
			goto bad;
		ent->ae_mnt = p;
		if ((p = strsep(&cp, " \t")) == NULL)
			goto bad;
		ent->ae_path = p;
		if ((p = strsep(&cp, " \t")) == NULL)
			goto bad;
		ent->ae_type = p;
		if ((p = strsep(&cp, " \t")) == NULL)
			goto bad;
		ent->ae_opts = p;
		if ((p = strsep(&cp, " \t")) == NULL)
			goto bad;
		ent->ae_rpath = p;

		if (strlen(ent->ae_mnt) + strlen(ent->ae_path) + 2 > PATH_MAX) {
			warnx("Error in file %s, line %d, "
			    "mount+path (%s/%s) exceeds PATH_MAX (%d)",
			    tab, lineno, ent->ae_mnt, ent->ae_path, PATH_MAX);
			goto bad2;
		}
		ent->ae_line = lineno;
		ent->ae_free = line;
		ent->ae_next = entries;
		entries = ent;
		continue;
bad:
		warnx("Parse error in file %s, line %d", tab, lineno);
bad2:
		free(line);
		free(ent);
	}
	if (ferror(fp))
		err(1, "error with file %s", tab);
}

void
populate_tab(void)
{
	struct autoentry *ent;
	char *path, *cmd;
	struct autofs_mounterreq mr;
	struct stat sb;
	struct statfs sfs;
	int error;

	path = cmd = NULL;
	bzero(&mr, sizeof(mr));
	mr.amu_pid = getpid();

	for (ent = entries; ent != NULL; ent = ent->ae_next) {
		free(path);
		free(cmd);
		error = asprintf(&path, "%s/%s", ent->ae_mnt, ent->ae_path);
		if (error == -1)
			err(1, "asprintf");
		error = asprintf(&cmd, "mkdir -p %s", path);
		if (error == -1)
			err(1, "asprintf");
		error = system(cmd);
		if (error) {
			warn("system: %s", cmd);
			continue;
		}
		if (stat(path, &sb) != 0) {
			warn("stat: %s", path);
			continue;
		}
		mr.amu_ino = sb.st_ino;
		if (statfs(path, &sfs) != 0) {
			warn("statfs: %s", path);
			continue;
		}
		if (strcmp(sfs.f_fstypename, "autofs") != 0) {
			warnx("config file line %d, entry for '%s' is not"
			    "an autofs filesystem f_fstypename = '%s'",
			    ent->ae_line, ent->ae_mnt, sfs.f_fstypename);
			continue;
		}
		error = sysctl_fsid(AUTOFS_CTL_MOUNTER, &sfs.f_fsid,
		    NULL, NULL, &mr, sizeof(mr));
		if (error) {
			warn("sysctl_fsid");
			continue;
		}
	}
	free(path);
	free(cmd);
}

/*
 * Process an autofs request, scan the list of entries in the config
 * looking for our node, if found mount it.
 */
void
doreq(struct autofs_userreq *req, char *mnt, fsid_t *fsid)
{
	struct autoentry *ent;
	char path[PATH_MAX];
	int error;
	char *cp, *cmd;
	int mcmp, pcmp;

	for (ent = entries; ent != NULL; ent = ent->ae_next) {
		fprintf(stderr, "comparing {%s,%s} to {%s,%s}\n",
		    mnt, ent->ae_mnt, req->au_name, ent->ae_path);
		fprintf(stderr, "comparing {%d,%d} to {%d,%d}\n",
		    (int)strlen(mnt), (int)strlen( ent->ae_mnt),
		    (int)strlen( req->au_name), (int)strlen( ent->ae_path));
		mcmp = strcmp(mnt, ent->ae_mnt);
		pcmp = strcmp(req->au_name, ent->ae_path);
		if (mcmp == 0 && pcmp == 0) {
			fprintf(stderr, "entry found...\n");
			break;
		}
		fprintf(stderr, "mcmp = %d, pcmp = %d\n", mcmp, pcmp);
	}
	if (ent == NULL) {
		fprintf(stderr, "no entry found...\n");
		req->au_errno = ENOENT;
		goto out;
	}
	cp = path + strlen(ent->ae_mnt) + 1;
	sprintf(path, "%s/%s", ent->ae_mnt, ent->ae_path);
#if 0
	/* do the equiv of mkdir -p */
	do {
		ncp = strchr(cp, '/');
		if (ncp != NULL) {
			*ncp = '\0';
		}
		fprintf(stderr, "mkdir %s\n", path);
		error = mkdir(path, 0777);
		if (error == -1 && errno != EEXIST) {
			warn("mkdir(%s)", path);
			req->au_errno = errno;
			goto out;
		}
		if (ncp != NULL) {
			*ncp = '/';
			cp = ncp + 1;
		}
	} while (ncp != NULL);
#endif
	error = asprintf(&cmd, "%s -t %s -o %s %s %s", mount_prog,
	    ent->ae_type, ent->ae_opts, ent->ae_rpath, path);
	if (error == -1)
		err(1, "asprintf");
	fprintf(stderr, "running:\n\t%s\n", cmd);
	error = system(cmd);
	fprintf(stderr, "error = %d\n", error);
	free(cmd);
	if (error) {
		rmdir(path);
		req->au_errno = ENOENT;
		goto out;
	}
	req->au_flags = 1;
out:
	error = sysctl_fsid(AUTOFS_CTL_SERVREQ, fsid, NULL, NULL,
	    req, sizeof(*req));
	if (error == -1) {
		warn("AUTOFS_CTL_SERVREQ");
	}
}

/*
 * Ask the filesystem passed in if it has a pending request.
 * if so process them.
 */
void
dotheneedful(struct statfs *sfs)
{
	struct vfsquery vq;
	size_t olen;
	struct autofs_userreq *reqs;
	int cnt, error, i;
	int vers;

	vers = AUTOFS_PROTOVERS;

	fprintf(stderr, "querying '%s'\n", sfs->f_mntonname);
	bzero(&vq, sizeof(vq));
	if (sysctl_queryfs(&sfs->f_fsid, &vq) == -1) {
		warn("sysctl_queryfs %s", sfs->f_mntonname);
		return;
	}
	if ((vq.vq_flags & VQ_ASSIST) == 0) {
		fprintf(stderr, "reply empty from '%s'\n", sfs->f_mntonname);
		return;
	}

	olen = 0;
	error = sysctl_fsid(AUTOFS_CTL_GETREQS, &sfs->f_fsid, NULL, &olen,
	    &vers, sizeof(vers));
	if (error == -1) {
		warn("AUTOFS_CTL_GETREQS");
		return;
	}
	fprintf(stderr, "qeuried size = %d for '%s'\n",
	    (int)olen, sfs->f_mntonname);

	reqs = xmalloc(olen);
	error = sysctl_fsid(AUTOFS_CTL_GETREQS, &sfs->f_fsid, reqs, &olen,
	    &vers, sizeof(vers));
	if (error == -1) {
		free(reqs);
		warn("AUTOFS_CTL_GETREQS");
		return;
	}
	cnt = olen / sizeof(*reqs);
	fprintf(stderr, "processing %d (size = %d) requests for '%s'\n",
		    cnt, (int)olen, sfs->f_mntonname);
	for (i = 0; i < cnt; i++) {
		fprintf(stderr, "processing request for '%s' '%s'\n",
		    sfs->f_mntonname, reqs[i].au_name);
		doreq(&reqs[i], sfs->f_mntonname, &sfs->f_fsid);
	}
	free(reqs);
}

void
eventloop(void)
{
	int kq;
	u_int waitret;
	struct statfs *mntbufp;
	int cnt, i;
	const char *fstype = "autofs";

	fprintf(stderr, "starting event loop...\n");
	kq = vfsevent_init();
	for ( ;; ) {
		fprintf(stderr, "waiting for event...\n");
		waitret = vfsevent_wait(kq, 0);
		fprintf(stderr, "getmntinfo...\n");
		cnt = getmntinfo(&mntbufp, MNT_NOWAIT);
		if (cnt == 0)
			err(1, "getmntinfo");
		for (i = 0; i < cnt; i++) {
			if (strcmp(fstype, mntbufp[i].f_fstypename) != 0)
				continue;
			dotheneedful(&mntbufp[i]);
		}
	}
}

int
main(int argc, char **argv)
{

	parsetab();
	populate_tab();
	eventloop();
	return (0);
}
