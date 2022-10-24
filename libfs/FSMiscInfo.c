/*
 * Copyright (c) 2022 Apple Computer, Inc.  All Rights Reserved.
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

#include <sys/param.h>
#include <sys/mount.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "FSPrivate.h"

static bool
check_mntfromname(const char *fstype)
{
    return strcmp(fstype, "lifs") == 0 ||
           strcmp(fstype, "fskit") == 0;
}

errno_t
_FSGetTypeInfoFromStatfs(const struct statfs *sfs, char *typenamebuf,
    size_t typenamebufsize, uint32_t *subtypep)
{
    /*
     * In the event that the caller passes sfs->f_fstypename as typenamebuf,
     * working in a local buffer will avoid having to deal with overlapping
     * copies.
     */
    char fstype[MFSTYPENAMELEN];

    if (check_mntfromname(sfs->f_fstypename)) {
        /*
         * The form that FSKit uses is a regular form like so:
         *
         *      fstype://location/volname
         *
         * N.B. This might look like a URL, **but it is not a URL**.
         */
        const char *cp = strstr(sfs->f_mntfromname, "://");
        if (cp == NULL) {
            /* Something is wrong ??? */
            return EINVAL;
        }
        ptrdiff_t len = cp - sfs->f_mntfromname;

        /* Clamp the name if necessary (N.B. should never be necessary). */
        if (len > sizeof(fstype) - 1) {
            len = sizeof(fstype) - 1;
        }
        memcpy(fstype, sfs->f_mntfromname, len);
        fstype[len] = '\0';
    } else {
        strlcpy(fstype, sfs->f_fstypename, sizeof(fstype));
    }

    /* Copy out the results. */
    if (typenamebuf != NULL) {
        strlcpy(typenamebuf, fstype, typenamebufsize);
    }
    if (subtypep != NULL) {
        *subtypep = sfs->f_fssubtype;
    }

    return 0;
}

errno_t
_FSGetTypeInfoForPath(const char *path, char *typenamebuf,
    size_t typenamebufsize, uint32_t *subtypep)
{
    struct statfs sfs;

    if (statfs(path, &sfs) == -1) {
        return errno;
    }
    return _FSGetTypeInfoFromStatfs(&sfs, typenamebuf, typenamebufsize,
                                    subtypep);
}

errno_t
_FSGetTypeInfoForFileDescriptor(int fd, char *typenamebuf,
    size_t typenamebufsize, uint32_t *subtypep)
{
    struct statfs sfs;

    if (fstatfs(fd, &sfs) == -1) {
        return errno;
    }
    return _FSGetTypeInfoFromStatfs(&sfs, typenamebuf, typenamebufsize,
                                    subtypep);
}

errno_t
_FSGetLocationFromStatfs(const struct statfs *sfs, char *locationbuf,
    size_t locationbufsize)
{
    /* This is 1KB, so don't use the stack. */
    char *locbuf = calloc(1, MNAMELEN);
    int error = 0;

    if (locbuf == NULL) {
        error = ENOMEM;
        goto out;
    }

    if (check_mntfromname(sfs->f_fstypename)) {
        /*
         * The form that FSKit uses is a regular form like so:
         *
         *      fstype://location/volname
         *
         * N.B. This might look like a URL, **but it is not a URL**.
         */
        const char *cp1 = strstr(sfs->f_mntfromname, "://");
        if (cp1 == NULL) {
            /* Something is wrong ??? */
            error = EINVAL;
            goto out;
        }
        /* Advance past type delimeter. */
        cp1 += 3;

        const char *cp2 = strchr(cp1, '/');
        if (cp2 == NULL) {
            /* Something is wrong ??? */
            error = EINVAL;
            goto out;
        }
        ptrdiff_t len = cp2 - cp1;

        /* Clamp the location if necessary (N.B. should never be necessary). */
        if (len > MNAMELEN - 1) {
            len = MNAMELEN - 1;
        }
        memcpy(locbuf, cp1, len);
        locbuf[len] = '\0';
    } else {
        const char *cp = sfs->f_mntfromname;

        /*
         * Special-case block devices, so that the return is consistent with
         * the FSKit case (which will be of the form diskNsM, no leading "/dev/").
         */
        if (strncmp(cp, "/dev/disk", strlen("/dev/disk")) == 0) {
            cp += 5;
        } else if (strncmp(cp, "/dev/rdisk", strlen("/dev/rdisk")) == 0) {
            /* This case should never happen but what the heck. */
            cp += 6;
        }
        strlcpy(locbuf, cp, MNAMELEN);
    }

    if (locationbuf != NULL) {
        strlcpy(locationbuf, locbuf, locationbufsize);
    }

 out:
    if (locbuf != NULL) {
        free(locbuf);
    }
    return error;
}

errno_t
_FSGetLocationForPath(const char *path, char *locationbuf,
    size_t locationbufsize)
{
    struct statfs sfs;

    if (statfs(path, &sfs) == -1) {
        return errno;
    }
    return _FSGetLocationFromStatfs(&sfs, locationbuf, locationbufsize);
}

errno_t
_FSGetLocationForFileDescriptor(int fd, char *locationbuf,
    size_t locationbufsize)
{
    struct statfs sfs;

    if (fstatfs(fd, &sfs) == -1) {
        return errno;
    }
    return _FSGetLocationFromStatfs(&sfs, locationbuf, locationbufsize);
}
