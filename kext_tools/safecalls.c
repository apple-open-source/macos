/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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
/*
 * FILE: safecalls.c
 * AUTH: Soren Spies (sspies)
 * DATE: 16 June 2006 (Copyright Apple Computer, Inc)
 * DESC: picky/safe syscalls
 *
 * Security functions
 * the first argument limits the scope of the operation
 *
 * Pretty much every function is implemented as
 * savedir = open(".", O_RDONLY);
 * schdirparent()->sopen()->spolicy()
 * <operation>(child)
 * fchdir(savedir)
 * 
 */

#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <libgen.h>
#include <limits.h>
#include <sys/mount.h>
#include <sys/param.h>  // MAXBSIZE
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>      // rename(2)?
#include <stdlib.h>     // malloc(3)
#include <sys/types.h>
#include <unistd.h>
#include <sys/ucred.h>

#include <IOKit/kext/kextmanager_types.h>

#include <IOKit/kext/OSKextPrivate.h>
#ifndef kOSKextLogCacheFlag
#define kOSKextLogCacheFlag kOSKextLogArchiveFlag
#endif  // no kOSKextLogCacheFlag

#define STRICT_SAFETY 0     // since our wrappers need to call the real calls
#include "safecalls.h"      // w/o STRICT_SAFETY, will #define mkdir, etc
#include "kext_tools_util.h"

#define RESTOREDIR(savedir) do { if (savedir != -1 && restoredir(savedir))  \
                 OSKextLog(/* kext */ NULL, \
                     kOSKextLogErrorLevel | kOSKextLogCacheFlag, \
                     "%s: ALERT: couldn't restore CWD", __func__); \
    } while(0)

// given that we call this function twice on an error path, it is tempting
// to use getmntinfo(3) but it's not threadsafe ... :P
static int findmnt(dev_t devid, char mntpt[MNAMELEN])
{
    int rval = ELAST + 1;
    int i, nmnts = getfsstat(NULL, 0, MNT_NOWAIT);
    size_t bufsz;
    struct statfs *mounts = NULL;

    if (nmnts <= 0)     goto finish;

    bufsz = nmnts * sizeof(struct statfs);
    if (!(mounts = malloc(bufsz)))                  goto finish;
    if (-1 == getfsstat(mounts, bufsz, MNT_NOWAIT)) goto finish;

    // loop looking for dev_t in the statfs structs
    for (i = 0; i < nmnts; i++) {
        struct statfs *sfs = &mounts[i];
        
        if (sfs->f_fsid.val[0] == devid) {
            if (strlcpy(mntpt, sfs->f_mntonname, MNAMELEN) >= MNAMELEN) {
                goto finish;
            }
            rval = 0;
            break;
        }
    }

finish:
    if (mounts)     free(mounts);
    return rval;
}

// currently checks to make sure on same volume
// other checks could include:
// - "really owned by <foo> on root/<foo>-mounted volume"
static int spolicy(int scopefd, int candfd)
{
    int bsderr = -1;
    struct stat candsb, scopesb;
    char path[PATH_MAX] = "<unknown>";

    if ((bsderr = fstat(candfd, &candsb)))    goto finish;  // trusty fstat()
    if ((bsderr = fstat(scopefd, &scopesb)))  goto finish;  // still there?

    // make sure st_dev matches
    if (candsb.st_dev != scopesb.st_dev ) {
        bsderr = -1;
        errno = EPERM;
        char scopemnt[MNAMELEN];

        if (findmnt(scopesb.st_dev, scopemnt) == 0) {
            (void)fcntl(candfd, F_GETPATH, path);

            OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogCacheFlag | kOSKextLogFileAccessFlag,
            "ALERT: %s does not appear to be on %s.", path, scopemnt);
        } else {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogGeneralFlag | kOSKextLogFileAccessFlag,
                "ALERT: dev_t mismatch (%d != %d).",
            candsb.st_dev, scopesb.st_dev);
        }
        goto finish;
    }

    // warn about non-root owners
    // (.disk_label can be written while owners are ignored :P)
    if (candsb.st_uid != 0) {

        // could try to trim pathname to basename?
        (void)fcntl(candfd, F_GETPATH, path);

        OSKextLog(NULL, kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "WARNING: %s: owner not root!", path);
    }

finish:
    return bsderr;
}


int schdirparent(int fdvol, const char *path, int *olddir, char child[PATH_MAX])
{
    int bsderr = -1;
    int dirfd = -1, savedir = -1;
    char parent[PATH_MAX];

    if (olddir)     *olddir = -1;
    if (!path)      goto finish;

    // make a copy of path in case our dirname() ever modifies the buffer
    if (strlcpy(parent, path, PATH_MAX) >= PATH_MAX)    goto finish;
    if (strlcpy(parent, dirname(parent), PATH_MAX) >= PATH_MAX)    goto finish;

    // make sure parent is on specified volume
    if (-1 == (dirfd = open(parent, O_RDONLY, 0)))  goto finish;
    errno = 0;
    if (spolicy(fdvol, dirfd)) {
        if (errno == EPERM)
            OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag | kOSKextLogFileAccessFlag,
            "Policy violation opening %s.", parent);
        goto finish;
    }

    // output parameters
    if (child) {
        if (strlcpy(child, path, PATH_MAX) >= PATH_MAX)    goto finish;
        if (strlcpy(child, basename(child), PATH_MAX) >= PATH_MAX)
            goto finish;
    }
    if (olddir) {
        if (-1 == (savedir = open(".", O_RDONLY)))  goto finish;
        *olddir = savedir;
    }

    if ((bsderr = fchdir(dirfd)))               goto finish;

finish:
    if (bsderr) {
        if (savedir != -1)              close(savedir);
        if (olddir && *olddir != -1)    close(*olddir);
    }
    if (dirfd != -1)    close(dirfd);

    return bsderr;
}

// have to rely on schdirparent so we don't accidentally O_CREAT
int sopen(int fdvol, const char *path, int flags, mode_t mode /*'...' fancier*/)
{
    int rfd = -1;
    int candfd = -1;
    char child[PATH_MAX];
    int savedir = -1;

    // omitting O_NOFOLLOW except when creating gives better errors
    // flags |= O_NOFOLLOW;

    // if creating, make sure it doesn't exist (O_NOFOLLOW for good measure)
    if (flags & O_CREAT)
        flags |= O_EXCL | O_NOFOLLOW;

    if (schdirparent(fdvol, path, &savedir, child))     goto finish;
    if (-1 == (candfd = open(child, flags, mode)))      goto finish;

    // schdirparent checked the parent; here we check the child (6393648)
    if (spolicy(fdvol, candfd))                         goto finish;

    rfd = candfd;

finish:
    if (candfd != -1 && rfd != candfd) {
        close(candfd);
    }
    RESTOREDIR(savedir);

    return rfd;
}

int schdir(int fdvol, const char *path, int *savedir)
{
    char cpath[PATH_MAX];

    // X could switch to snprintf()
    if (strlcpy(cpath, path, PATH_MAX) >= PATH_MAX ||
        strlcat(cpath, "/.", PATH_MAX) >= PATH_MAX)   return -1;

    return schdirparent(fdvol, cpath, savedir, NULL);
}

int restoredir(int savedir)
{
    int cherr = -1, clerr = -1;

    if (savedir != -1) {
        cherr = fchdir(savedir);
        clerr = close(savedir);
    }

    return cherr ? cherr : clerr;
}

int smkdir(int fdvol, const char *path, mode_t mode)
{
    int bsderr = -1;
    int savedir = -1;
    char child[PATH_MAX];

    if (schdirparent(fdvol, path, &savedir, child))  goto finish;
    if ((bsderr = mkdir(child, mode)))      goto finish;

finish:
    RESTOREDIR(savedir);
    return bsderr;
}

int srmdir(int fdvol, const char *path)
{
    int bsderr = -1;
    char child[PATH_MAX];
    int savedir = -1;

    if (schdirparent(fdvol, path, &savedir, child))  goto finish;

    bsderr = rmdir(child);

finish:
    RESTOREDIR(savedir);
    return bsderr;
}

int sunlink(int fdvol, const char *path)
{
    int bsderr = -1;
    char child[PATH_MAX];
    int savedir = -1;

    if (schdirparent(fdvol, path, &savedir, child))  goto finish;

    bsderr = unlink(child);

finish:
    RESTOREDIR(savedir);
    return bsderr;
}

// taking a path and a filename is sort of annoying for clients
// so we "auto-strip" newname if it happens to be a path
int srename(int fdvol, const char *oldpath, const char *newpath)
{
    int bsderr = -1;
    int savedir = -1;
    char oldname[PATH_MAX];
    char newname[PATH_MAX];

    // calculate netname first since schdirparent uses basename
    if (strlcpy(newname, newpath, PATH_MAX) >= PATH_MAX)    goto finish;
    if (strlcpy(newname, basename(newname), PATH_MAX) >= PATH_MAX)goto finish;
    if (schdirparent(fdvol, oldpath, &savedir, oldname))        goto finish;

    bsderr = rename(oldname, newname);

finish:
    RESTOREDIR(savedir);
    return bsderr;
}

// stolen with gratitude from TAOcommon's TAOCFURLDelete
int sdeepunlink(int fdvol, char *path)
{
    int             rval = ELAST + 1;

    char        *   const pathv[2] = { path, NULL };
    int             ftsoptions = 0;
    FTS         *   fts;
    FTSENT      *   fent;

    // opting for security, of course
    ftsoptions |= FTS_PHYSICAL;         // see symlinks
    ftsoptions |= FTS_XDEV;             // don't cross devices
    ftsoptions |= FTS_NOSTAT;           // fts_info tells us enough
//  ftsoptions |= FTS_COMFOLLOW;        // if 'path' is symlink, remove link
//  ftsoptions |= FTS_NOCHDIR;          // chdir is fine
//  ftsoptions |= FTS_SEEDOT;           // we don't need "."

    if ((fts = fts_open(pathv, ftsoptions, NULL)) == NULL)  goto finish;

    // and here we go (accumulating errors, though that usu ends in ENOTEMPTY)
    rval = 0;
    while ((fent = fts_read(fts)) /* && !rval ?? */) {
        switch (fent->fts_info) {
            case FTS_DC:        // directory that causes a cycle in the tree
            case FTS_D:         // directory being visited in pre-order
            case FTS_DOT:       // file named '.' or '..' (not requested)
                break;

            case FTS_DNR:       // directory which cannot be read
            case FTS_ERR:       // generic fcts_errno-borne error
            case FTS_NS:        // file for which stat(s) failed (not requested)
                rval |= fent->fts_errno;
                break;

            case FTS_SL:        // symbolic link
            case FTS_SLNONE:    // symbolic link with a non-existent target
            case FTS_DEFAULT:   // good file of type unknown to FTS (block? ;)
            case FTS_F:         // regular file
            case FTS_NSOK:      // no stat(2) requested (but not a dir?)
            default:            // in case FTS gets smarter in the future
                rval |= sunlink(fdvol, fent->fts_accpath);
                break;

            case FTS_DP:        // directory being visited in post-order
                rval |= srmdir(fdvol, fent->fts_accpath);
                break;
        } // switch
    } // while

    if (!rval)  rval = errno;   // fts_read() clears if all went well

    // close the iterator now
    if (fts_close(fts) < 0) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag | kOSKextLogFileAccessFlag,
            "fts_close failed - %s.", strerror(errno));
    }

finish:

    return rval;
}

int sdeepmkdir(int fdvol, const char *path, mode_t mode)
{
    int bsderr = -1;
    struct stat sb;
    char parent[PATH_MAX];

    if (strlen(path) == 0)      goto finish;        // protection?

    // trusting that stat(".") will always do the right thing
    if (0 == stat(path, &sb)) {
        if (sb.st_mode & S_IFDIR == 0) {
            bsderr = ENOTDIR;
            goto finish;
        } else {
            bsderr = 0;             // base case (dir exists) 
            goto finish;
        }
    } else if (errno != ENOENT) {
        goto finish;                // bsderr = -1 -> errno
    } else {
        if (strlcpy(parent, path, PATH_MAX) >= PATH_MAX)            goto finish;
        if (strlcpy(parent, dirname(parent), PATH_MAX) >= PATH_MAX) goto finish;

        // and recurse since it wasn't there
        if ((bsderr = sdeepmkdir(fdvol, parent, mode)))     goto finish;
    }

    // all parents made; top-level still needed
    bsderr = smkdir(fdvol, path, mode);

finish:
    return bsderr;
}

#define     min(a,b)        ((a) < (b) ? (a) : (b))
int scopyfile(int srcfdvol, const char *srcpath, int dstfdvol, const char *dstpath)
{
    int bsderr = -1;
    int srcfd = -1, dstfd = -1;
    struct stat srcsb;
    char dstparent[PATH_MAX];
    mode_t dirmode;
    void *buf = NULL;       // MAXBSIZE on the stack is a bad idea
    off_t bytesLeft, thisTime;

    // figure out directory mode
    if (-1 == (srcfd = sopen(srcfdvol, srcpath, O_RDONLY, 0)))    goto finish;
    if (fstat(srcfd, &srcsb))                       goto finish;
    dirmode = ((srcsb.st_mode&~S_IFMT) | S_IWUSR | S_IXUSR /* u+wx */);
    if (dirmode & S_IRGRP)      dirmode |= S_IXGRP;     // add conditional o+x
    if (dirmode & S_IROTH)      dirmode |= S_IXOTH;

    // and recursively create the parent directory
    if (strlcpy(dstparent, dstpath, PATH_MAX) >= PATH_MAX)          goto finish;
    if (strlcpy(dstparent, dirname(dstparent), PATH_MAX)>=PATH_MAX) goto finish;
    if ((sdeepmkdir(dstfdvol, dstparent, dirmode)))         goto finish;

    // nuke/open the destination
    (void)sunlink(dstfdvol, dstpath);
    dstfd = sopen(dstfdvol, dstpath, O_CREAT|O_WRONLY, srcsb.st_mode | S_IWUSR);
    if (dstfd == -1)        goto finish;

    // and loop with our handy buffer
    if (!(buf = malloc(MAXBSIZE)))      goto finish;;
    for (bytesLeft = srcsb.st_size; bytesLeft > 0; bytesLeft -= thisTime) {
        thisTime = min(bytesLeft, MAXBSIZE);

        if (read(srcfd, buf, thisTime) != thisTime)     goto finish;
        if (write(dstfd, buf, thisTime) != thisTime)    goto finish;
    }

    // apply final permissions
    if (bsderr = fchmod(dstfd, srcsb.st_mode))  goto finish;
    // kextcache doesn't currently look into the Apple_Boot, so we'll skip times

finish:
    if (srcfd != -1)    close(srcfd);
    if (dstfd != -1)    close(dstfd);

    if (buf)            free(buf);

    return bsderr;
}
