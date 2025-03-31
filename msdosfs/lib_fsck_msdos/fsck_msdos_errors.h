/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
 *
 * lib_fsck_msdos
 * fsck_msdos_errors.h
 *
 * This file contain error numbers that are returned from checkfilesys.
 * We want the error codes to be unique so we can recognize each error case by
 * the error code we get from telemetry.
 */

#ifndef _FSCK_MSDOS_ERRORS_H
#define _FSCK_MSDOS_ERRORS_H

enum fsck_msdos_errors {
    /*
     * Starting from 100 to not collide with POSIX error codes which may be
     * returned from pread/pwrite during fsck run (to keep our error codes unique).
     */
    fsckErrQuickCheckDirty = 200,   /* File system was found dirty in quick check. */
    fsckErrBootRegionInvalid,       /* Couldn't read the boot region, or it contains a non-recoverable corruption. */
    fsckErrCouldNotInitFAT,         /* Got a fatal error while initializing the FAT structure. */
    fsckErrCouldNotInitRootDir,     /* Got a fatal error while initializing the root dir structure. */
    fsckErrCouldNotScanDirs,        /* Got a fatal error while scanning the volume's directory hierarchy. */
    fsckErrCouldNotFreeUnused,      /* Got a fatal error while freeing unused clusters in the FAT. */
    fsckErrCannotRepairReadOnly,    /* Volume cannot be repaired because we're on read-only mode. */
    fsckErrCannotRepairAfterRetry,  /* Volume couldn't be repaired after one or more retries. */
};

#endif /* _FSCK_MSDOS_ERRORS_H */

