/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* 
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 *
 */
/*
 * HISTORY
 * Revision 2.3  88/08/08  13:47:07  rvb
 * Allocate buffers dynamically vs statically.
 * Now b[i] and i_fs and i_buf, are allocated dynamically.
 * boot_calloc(size) allocates and zeros a  buffer rounded to a NPG
 * boundary.
 * Generalize boot spec to allow, xx()/mach, xx(n,[a..h])/mach,
 * xx([a..h])/mach, ...
 * Also default "xx" if unspecified and alloc just "/mach",
 * where everything is defaulted
 * Add routine, ptol(), to parse partition letters.
 *
 */
 
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)sys.c	7.1 (Berkeley) 6/5/86
 */

#include "libsaio.h"

struct devsw {
    const char *  name;
    unsigned char biosdev;
};

static struct devsw devsw[] =
{
    { "sd", 0x80 },  /* DEV_SD */
    { "hd", 0x80 },  /* DEV_HD */
    { "fd", 0x00 },  /* DEV_FD */
    { "en", 0xE0 },  /* DEV_EN */
    { 0, 0 }
};

/*
 * Max number of file descriptors.
 */
#define NFILES  6

static struct iob iob[NFILES];

void * gFSLoadAddress = 0;

static BVRef getBootVolumeRef( const char * path, const char ** outPath );
static BVRef newBootVolumeRef( int biosdev, int partno );

//==========================================================================
// LoadFile - LOW-LEVEL FILESYSTEM FUNCTION.
//            Load the specified file to the load buffer at LOAD_ADDR.

long LoadFile(const char * fileSpec)
{
    const char * filePath;
    long         fileSize;
    BVRef        bvr;

    // Resolve the boot volume from the file spec.

    if ((bvr = getBootVolumeRef(fileSpec, &filePath)) == NULL)
        return -1;

    // Read file into load buffer. The data in the load buffer will be
    // overwritten by the next LoadFile() call.

    gFSLoadAddress = (void *) LOAD_ADDR;

    fileSize = bvr->fs_loadfile(bvr, (char *)filePath);

    // Return the size of the file, or -1 if load failed.

    return fileSize;
}

//==========================================================================
// GetDirEntry - LOW-LEVEL FILESYSTEM FUNCTION.
//               Fetch the next directory entry for the given directory.

long GetDirEntry(const char * dirSpec, long * dirIndex, const char ** name, 
                 long * flags, long * time)
{
    const char * dirPath;
    BVRef        bvr;

    // Resolve the boot volume from the dir spec.

    if ((bvr = getBootVolumeRef(dirSpec, &dirPath)) == NULL)
        return -1;

    // Return 0 on success, or -1 if there are no additional entries.

    return bvr->fs_getdirentry( bvr,
                /* dirPath */   (char *)dirPath,
                /* dirIndex */  dirIndex,
                /* dirEntry */  (char **)name, flags, time );
}

//==========================================================================
// GetFileInfo - LOW-LEVEL FILESYSTEM FUNCTION.
//               Get attributes for the specified file.

long GetFileInfo(const char * dirSpec, const char * name,
                 long * flags, long * time)
{
    long         index = 0;
    const char * entryName;

    while (GetDirEntry(dirSpec, &index, &entryName, flags, time) == 0)
    {
        if (strcmp(entryName, name) == 0)
            return 0;  // success
    }
    return -1;  // file not found
}

//==========================================================================
// iob_from_fdesc()
//
// Return a pointer to an allocated 'iob' based on the file descriptor
// provided. Returns NULL if the file descriptor given is invalid.

static struct iob * iob_from_fdesc(int fdesc)
{
    register struct iob * io;

    if (fdesc < 0 || fdesc >= NFILES ||
        ((io = &iob[fdesc])->i_flgs & F_ALLOC) == 0)
        return NULL;
    else
        return io;
}

//==========================================================================
// openmem()

int openmem(char * buf, int len)
{
    int          fdesc;
    struct iob * io;

    // Locate a free descriptor slot.

    for (fdesc = 0; fdesc < NFILES; fdesc++)
        if (iob[fdesc].i_flgs == 0)
            goto gotfile;

    stop("Out of file descriptors");

gotfile:
    io = &iob[fdesc];
    bzero(io, sizeof(*io));

    // Mark the descriptor as taken. Set the F_MEM flag to indicate
    // that the file buffer is provided by the caller.

    io->i_flgs     = F_ALLOC | F_MEM;
    io->i_buf      = buf;
    io->i_filesize = len;

    return fdesc;
}

//==========================================================================
// open() - Open the file specified by 'path' for reading.

int open(const char * path, int flags)
{
    int          fdesc, i;
    struct iob * io;
    const char * filePath;
    BVRef        bvr;

    // Locate a free descriptor slot.

    for (fdesc = 0; fdesc < NFILES; fdesc++)
        if (iob[fdesc].i_flgs == 0)
            goto gotfile;

    stop("Out of file descriptors");

gotfile:
    io = &iob[fdesc];
    bzero(io, sizeof(*io));

    // Mark the descriptor as taken.

    io->i_flgs = F_ALLOC;

    // Resolve the boot volume from the file spec.

    if ((bvr = getBootVolumeRef(path, &filePath)) == NULL)
        goto error;

    // Find the next available memory block in the download buffer.

    io->i_buf = (char *) LOAD_ADDR;
    for (i = 0; i < NFILES; i++)
    {
        if ((iob[i].i_flgs != F_ALLOC) || (i == fdesc)) continue;
        io->i_buf = max(iob[i].i_filesize + iob[i].i_buf, io->i_buf);
    }

    // Load entire file into memory. Unnecessary open() calls must
    // be avoided.

    gFSLoadAddress = io->i_buf;
    io->i_filesize = bvr->fs_loadfile(bvr, (char *)filePath);
    if (io->i_filesize < 0) goto error;

    return fdesc;

error:
    close(fdesc);
    return -1;
}

//==========================================================================
// close() - Close a file descriptor.

int close(int fdesc)
{
    struct iob * io;

    if ((io = iob_from_fdesc(fdesc)) == NULL)
        return (-1);

    io->i_flgs = 0;

    return 0;
}

//==========================================================================
// lseek() - Reposition the byte offset of the file descriptor from the
//           beginning of the file. Returns the relocated offset.

int b_lseek(int fdesc, int offset, int ptr)
{
    struct iob * io;

    if ((io = iob_from_fdesc(fdesc)) == NULL)
        return (-1);

    io->i_offset = offset;

    return offset;
}

//==========================================================================
// tell() - Returns the byte offset of the file descriptor.

int tell(int fdesc)
{
    struct iob * io;

    if ((io = iob_from_fdesc(fdesc)) == NULL)
        return 0;

    return io->i_offset;
}

//==========================================================================
// read() - Read up to 'count' bytes of data from the file descriptor
//          into the buffer pointed to by buf.

int read(int fdesc, char * buf, int count)
{
    struct iob * io;
    
    if ((io = iob_from_fdesc(fdesc)) == NULL)
        return (-1);

    if (io->i_offset + count > io->i_filesize)
        count = io->i_filesize - io->i_offset;

    if (count <= 0)
        return 0;  // end of file

    bcopy(io->i_buf + io->i_offset, buf, count);

    io->i_offset += count;

    return count;
}

//==========================================================================
// file_size() - Returns the size of the file described by the file
//               descriptor.

int file_size(int fdesc)
{
    struct iob * io;

    if ((io = iob_from_fdesc(fdesc)) == 0)
        return 0;

    return io->i_filesize;
}

//==========================================================================

struct dirstuff * opendir(const char * path)
{
    struct dirstuff * dirp = 0;
    const char *      dirPath;
    BVRef             bvr;

    if ((bvr = getBootVolumeRef(path, &dirPath)) == NULL)
        goto error;

    dirp = (struct dirstuff *) malloc(sizeof(struct dirstuff));
    if (dirp == NULL)
        goto error;

    dirp->dir_path = newString(dirPath);
    if (dirp->dir_path == NULL)
        goto error;

    dirp->dir_bvr = bvr;

    return dirp;

error:
    closedir(dirp);
    return NULL;
}

//==========================================================================

int closedir(struct dirstuff * dirp)
{
    if (dirp) {
        if (dirp->dir_path) free(dirp->dir_path);
        free(dirp);
    }
    return 0;
}

//==========================================================================

int readdir(struct dirstuff * dirp, const char ** name, long * flags,
            long * time)
{
    return dirp->dir_bvr->fs_getdirentry( dirp->dir_bvr,
                          /* dirPath */   dirp->dir_path,
                          /* dirIndex */  &dirp->dir_index,
                          /* dirEntry */  (char **)name, flags, time );
}

//==========================================================================

int currentdev()
{
    return kernBootStruct->kernDev;
}

//==========================================================================

int switchdev(int dev)
{
    kernBootStruct->kernDev = dev;
    return dev;
}

//==========================================================================

const char * usrDevices()
{
    return (B_TYPE(currentdev()) == DEV_EN) ? "" : "/private/Drivers/i386";
}

//==========================================================================

BVRef scanBootVolumes( int biosdev, int * count )
{
    BVRef bvr = 0;

    switch ( BIOS_DEV_TYPE( biosdev ) )
    {
        case kBIOSDevTypeFloppy:
        case kBIOSDevTypeHardDrive:
            bvr = diskScanBootVolumes( biosdev, count );
            break;
        case kBIOSDevTypeNetwork:
            bvr = nbpScanBootVolumes( biosdev, count );
            break;
    }
    return bvr;
}

//==========================================================================

void getBootVolumeDescription( BVRef bvr, char * str, long strMaxLen )
{
    bvr->description( bvr, str, strMaxLen );
}

//==========================================================================

BVRef selectBootVolume( BVRef chain )
{
    BVRef bvr, bvr1 = 0, bvr2 = 0;

    for ( bvr = chain; bvr; bvr = bvr->next )
    {
        if ( bvr->flags & kBVFlagNativeBoot ) bvr1 = bvr;
        if ( bvr->flags & kBVFlagPrimary )    bvr2 = bvr;
    }

    bvr = bvr1 ? bvr1 :
          bvr2 ? bvr2 : chain;

    return bvr;
}

//==========================================================================

#define LP '('
#define RP ')'
extern int gBIOSDev;

static BVRef getBootVolumeRef( const char * path, const char ** outPath )
{
    const char * cp;
    BVRef        bvr;
    int          type = B_TYPE( kernBootStruct->kernDev );
    int          unit = B_UNIT( kernBootStruct->kernDev );
    int          part = B_PARTITION( kernBootStruct->kernDev );
    int          biosdev = gBIOSDev;
    static BVRef lastBVR = 0;
    static int   lastKernDev;

    // Search for left parenthesis in the path specification.

    for (cp = path; *cp; cp++) {
        if (*cp == LP || *cp == '/') break;
    }

    if (*cp != LP)  // no left paren found
    {
        cp = path;
        if ( lastBVR && lastKernDev == kernBootStruct->kernDev )
        {
            bvr = lastBVR;
            goto quick_exit;
        }
    }
    else if ((cp - path) == 2)  // found "xx("
    {
        const struct devsw * dp;
        const char * xp = path;
        int          i;

        cp++;

        // Check the 2 character device name pointed by 'xp'.

        for (dp = devsw; dp->name; dp++)
        {
            if ((xp[0] == dp->name[0]) && (xp[1] == dp->name[1]))
                break;  // found matching entry
        }
        if (dp->name == NULL)
        {
            error("Unknown device '%c%c'\n", xp[0], xp[1]);
            return NULL;
        }
        type = dp - devsw;  // kerndev type
        
        // Extract the optional unit number from the specification.
        // hd(unit) or hd(unit, part).

        i = 0;
        while (*cp >= '0' && *cp <= '9')
        {
            i = i * 10 + *cp++ - '0';
            unit = i;
        }

        // Extract the optional partition number from the specification.

        if (*cp == ',')
            part = atoi(++cp);

        // Skip past the right paren.

        for ( ; *cp && *cp != RP; cp++) /* LOOP */;
        if (*cp == RP) cp++;
        
        biosdev = dp->biosdev;
    }
    else
    {
        // Bad device specifier, skip past the right paren.

        for ( cp++; *cp && *cp != RP; cp++) /* LOOP */;
        if (*cp == RP) cp++;
    }

    biosdev += (unit & kBIOSDevUnitMask);

    if ((bvr = newBootVolumeRef(biosdev, part)) == NULL)
    {
        // error("newBootVolumeRef() error\n");
        return NULL;
    }

    // Record the most recent device parameters in the
    // KernBootStruct.

    kernBootStruct->kernDev = MAKEKERNDEV(type, unit, bvr->part_no);

    lastBVR = bvr;
    lastKernDev = kernBootStruct->kernDev;

quick_exit:
    // Returns the file path following the device spec.
    // e.g. 'hd(1,b)mach_kernel' is reduced to 'mach_kernel'.

    *outPath = cp;

    return bvr;
}

//==========================================================================

static BVRef newBootVolumeRef( int biosdev, int partno )
{
    BVRef bvr, bvr1, bvrChain;

    // Fetch the volume list from the device.

    bvrChain = scanBootVolumes( biosdev, NULL );

    // Look for a perfect match based on device and partition number.

    for ( bvr1 = NULL, bvr = bvrChain; bvr; bvr = bvr->next )
    {
        if ( ( bvr->flags & kBVFlagNativeBoot ) == 0 ) continue;
    
        bvr1 = bvr;
        if ( bvr->part_no == partno ) break;
    }

    return bvr ? bvr : bvr1;
}
