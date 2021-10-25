/*
 * dir.h - Exports for directory handling. 
 *
 * Copyright (c) 2002      Anton Altaparmakov
 * Copyright (c) 2005-2006 Yura Pakhuchiy
 * Copyright (c) 2004-2005 Richard Russon
 * Copyright (c) 2005-2008 Szabolcs Szakacsits
 * Copyright (c) 2010-2011 Adnan Shaheen
 * Copyright (c) 2008-2012 Tuxera Inc.
 *
 * See LICENSE file for licensing information.
 */

#ifndef _NTFS_DIR_H
#define _NTFS_DIR_H

/*
 * The little endian Unicode strings $I30, $SII, $SDH, $O, $Q, $R
 * as a global constant.
 */
extern ntfschar NTFS_INDEX_I30[5];
extern ntfschar NTFS_INDEX_SII[5];
extern ntfschar NTFS_INDEX_SDH[5];
extern ntfschar NTFS_INDEX_O[3];
extern ntfschar NTFS_INDEX_Q[3];
extern ntfschar NTFS_INDEX_R[3];

#endif /* defined _NTFS_DIR_H */
