/*
 * ntfs_sfm.h - Services For Macintosh (SFM) associated on-disk structures.
 *
 * Copyright (c) 2006-2008 Anton Altaparmakov.  All Rights Reserved.
 * Portions Copyright (c) 2006-2008 Apple Inc.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution. 
 * 3. Neither the name of Apple Inc. ("Apple") nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ALTERNATIVELY, provided that this notice and licensing terms are retained in
 * full, this file may be redistributed and/or modified under the terms of the
 * GNU General Public License (GPL) Version 2, in which case the provisions of
 * that version of the GPL will apply to you instead of the license terms
 * above.  You can obtain a copy of the GPL Version 2 at
 * http://developer.apple.com/opensource/licenses/gpl-2.txt.
 */

#ifndef _OSX_NTFS_SFM_H
#define _OSX_NTFS_SFM_H

#include <sys/time.h>

#include "ntfs_endian.h"
#include "ntfs_types.h"

/*
 * Services For Macintosh (SFM) uses various NTFS named streams to store Mac
 * OS X specific data that is otherwise not stored on NTFS.
 *
 * The streams are:
 *
 *	AFP_AfpInfo
 *	AFP_DeskTop
 *	AFP_IdIndex
 *	AFP_Resource
 *	Comments
 *
 * Of those I believe we can ignore AFP_DeskTop, AFP_IdIndex, and Comments.
 *
 * That leaves AFP_Resource and AFP_AfpInfo to be dealt with.
 *
 * AFP_Resource
 * ============
 *
 * This is simply the Mac OS X resource fork.  We do not need to parse its
 * contents, i.e. as far as NTFS is concerned this is just a stream of bytes,
 * just like any regular file content.
 *
 * The resource fork on Mac OS X can only exist for regular files thus we need
 * to ensure we do not allow it to be created on directories or other special
 * files.
 *
 * Finally, the resource fork is accessed via the named stream and/or extended
 * attribute API in Mac OS X but it is accessed under the name defined by the
 * constant XATTR_RESOURCEFORK_NAME.  (This is defined in <sys/xattr.h> as
 * follows:
 *
 *	#define XATTR_RESOURCEFORK_NAME	"com.apple.ResourceFork"
 *
 * So the NTFS driver needs to translate XATTR_RESOURCEFORK_NAME from/to
 * "AFP_Resource".
 *
 * AFP_AfpInfo
 * ===========
 *
 * This is more complicated to deal with than AFP_Resource as it is a structure
 * that the NTFS driver has to decode and use its various contents in different
 * ways.
 *
 * The AFP_AfpInfo structure in addition to a signature, version, and some
 * reserved fields contains three items of interest: backup time, Finder info,
 * and ProDOS info.
 *
 * Backup Time
 * ===========
 *
 * We use this to implement the Mac OS X backup time attribute which is
 * otherwise missing from NTFS.
 *
 * The backup time is in AppleDouble defined format which is: "a signed number
 * of * seconds before or after 12:00 a.m. (midnight), January 1, 2000
 * Greenwich Mean Time (GMT).  In other words, the start of the year 2000 GMT
 * corresponds to a date-time of 0.  When creating set the backup time to
 * 0x80000000, the earliest reasonable time."  (Quote from the AppleDouble
 * Specification.)
 *
 * Finder Info
 * ===========
 *
 * We use this to implement the Mac OS X Finder info.
 *
 * The Finder info is accessed via the extended attribute API in Mac OS X but
 * it is accessed under the name defined by the constant XATTR_FINDERINFO_NAME.
 * (This is defined in <sys/xattr.h> as follows:
 *
 *	#define XATTR_FINDERINFO_NAME	"com.apple.FinderInfo"
 *
 * So the NTFS driver needs to translate XATTR_FINDERINFO_NAME from/to
 * "AFP_AfpInfo".
 *
 * We pretty much treat the AFP_AfpInfo as an opaque data structure except for
 * one thing: we hide the type and creator when returning the Finder info for a
 * symbolic link (this is what HFS does so we follow suit).
 *
 * Any information not set should be zero and in particular when someone wants
 * to read the Finder info it is not present, return it as if it were present
 * but empty (i.e. zero).
 *
 * Note that the FINDER_ATTR_IS_HIDDEN flag does not get set when a file is
 * made invisible via the Finder on an AFP mounted SFM volume.  Instead the
 * FILE_ATTR_HIDDEN bit in the file attributes of the $STANDARD_INFORMATION
 * attribute of the MFT record is set.  This is what we do for NTFS already
 * thus we can ignore the FINDER_ATTR_IS_HIDDEN flag when writing the Finder
 * info.  However, when a getattrlist() call is performed to check the Finder
 * info on an AFP mounted SFM volume, the FINDER_ATTR_IS_HIDDEN bit is set in
 * the Finder info so we need to set this bit on read as well when the
 * FILE_ATTR_HIDDEN bit in the file attributes of the $STANDARD_INFORMATION
 * attribute is set in the MFT record.
 *
 * ProDOS (Apple-II) Info
 * ======================
 *
 * Any information not set should be zero.
 *
 * Note that in theory, when setting Finder info type or creator, we should
 * keep the ProDOS information in sync appropriately, but SMB does not do this
 * so we do not need to do it either.  Also Leopard does not define an API for
 * obtaining the ProDOS info any more so it would be a waste of time to do
 * anything with it as there are no clients (except a few die hard Apple-II
 * fans perhaps) who might ever request it.
 *
 * Thus we always create this as filled with zeroes and preserve it on read and
 * write.
 */

/* These definitions need to be before any of the other ntfs_*.h includes. */

 /*
  * File type and creator for symbolic links as used by HFS+ and SFM.
  *
  * FINDER_TYPE_SYMBOLIC_LINK is 'slnk' and is called kSymLinkFileType in HFS+.
  * FINDER_CREATOR_SYMBOLIC_LINK is 'rhap' and is called kSymLinkCreator
  * HFS+.
  */
enum {
	FINDER_TYPE_SYMBOLIC_LINK = const_cpu_to_le32(0x6b6e6c73),
	FINDER_CREATOR_SYMBOLIC_LINK = const_cpu_to_le32(0x70616872)
};

enum {
	FINDER_ATTR_HAS_BEEN_INITED		= const_cpu_to_le16(0x0001),
	FINDER_ATTR_RESERVED			= const_cpu_to_le16(0x0002),
	FINDER_ATTR_HAS_CUSTOM_ICON		= const_cpu_to_le16(0x0004),
	FINDER_ATTR_IS_STATIONARY		= const_cpu_to_le16(0x0008),
	FINDER_ATTR_NAME_LOCKED			= const_cpu_to_le16(0x0010),
	FINDER_ATTR_HAS_BUNDLE			= const_cpu_to_le16(0x0020),
	FINDER_ATTR_IS_HIDDEN			= const_cpu_to_le16(0x0040),
	FINDER_ATTR_IS_ALIAS			= const_cpu_to_le16(0x0080),
	FINDER_ATTR_IS_ON_DESK			= const_cpu_to_le16(0x0100),
	FINDER_ATTR_COLOR_BIT0			= const_cpu_to_le16(0x0200),
	FINDER_ATTR_COLOR_BIT1			= const_cpu_to_le16(0x0400),
	FINDER_ATTR_COLOR_BIT2			= const_cpu_to_le16(0x0800),
	FINDER_ATTR_COLOR_RESERVED		= const_cpu_to_le16(0x1000),
	FINDER_ATTR_REQUIRES_SWITCH_LAUNCH	= const_cpu_to_le16(0x2000),
	FINDER_ATTR_IS_SHARED			= const_cpu_to_le16(0x4000),
	FINDER_ATTR_HAS_NO_INITS		= const_cpu_to_le16(0x8000)
} __attribute__((__packed__));

typedef le16 FINDER_ATTR_FLAGS;

typedef struct {
/*Ofs*/
/*  0*/	le32 type;
/*  4*/	le32 creator;
/*  8*/	FINDER_ATTR_FLAGS attrs;
/* 10*/	le32 location;
/* 14*/	le16 window;
/* 16*/	u8 reserved[16];
/* sizeof() = 32 (0x20) bytes */
} __attribute__((__packed__, __aligned__(8))) FINDER_INFO;

typedef struct {
	le16 file_type;
	le32 aux_type;
} __attribute__((__packed__, __aligned__(8))) PRODOS_INFO;

#define AfpInfo_Signature	const_cpu_to_le32(0x00504641)
#define AfpInfo_Version		const_cpu_to_le32(0x00010000)

typedef struct {
/*Ofs*/
/*  0*/	le32 signature;		/* Must be "AFP\0". */
/*  4*/	le32 version;		/* Must be 0x00010000. */
/*  8*/	le32 fileid;		/* This is the inode number as returned by the
				   AFP connection.  For some reason this is not
				   the same as the underlying NTFS inode
				   number.  Need to set this to zero on create,
				   preserve it on write, and ignore it on
				   read. */
/* 12*/	le32 backup_time;	/* Backup time for the file/dir in AppleDouble
				   time format (see above). */
/* 16*/	FINDER_INFO finder_info;/* Finder Info (32 bytes, see above). */
/* 48*/	PRODOS_INFO prodos_info;/* ProDOS Info (6 bytes, see above). */
/* 54*/ u8 reserved[6];		/* Reserved. */
/* sizeof() = 60 (0x3c) bytes */
} __attribute__((__packed__, __aligned__(8))) AFPINFO;

#include "ntfs_volume.h"

#define NTFS_AD2UTC_TIME_OFFSET ((s32)(30 * 365 + 7) * 24 * 3600)

/**
 * ntfs_utc2ad - convert OS X time to little endian, AppleDouble time
 * @ts:		OS X UTC time to convert to little endian, AppleDouble time
 *
 * Convert the OS X UTC time @ts to its corresponding AppleDouble time and
 * return that in little endian format.
 *
 * OS X stores time in a struct timespec consisting of a time_t (long at
 * present) tv_sec and a long tv_nsec where tv_sec is the number of 1-second
 * intervals since 1st January 1970, 00:00:00 UTC and tv_nsec is the number of
 * 1-nano-second intervals since the value of tv_sec.
 *
 * AppleDouble stores time as "a signed number of seconds before or after 12:00
 * a.m. (midnight), January 1, 2000 Greenwich Mean Time (GMT).  In other words,
 * the start of the year 2000 GMT corresponds to a date-time of 0."  (Quote
 * from the AppleDouble Specification.)
 *
 * To convert between OS X kernel time and AppleDouble format need to use a
 * fixed constant delta which is defined as NTFS_AD2UTC_TIME_OFFSET.
 *
 * So if you have a struct timespec.tv_sec = X then the AppleDouble time = X -
 * NTFS_AD2UTC_TIME_OFFSET (note .tv_nsec is ignored as the AppleDouble time
 * only has second resolution).
 */
static inline sle32 ntfs_utc2ad(const struct timespec ts)
{
	s64 ad;
	
	ad = (s64)ts.tv_sec - NTFS_AD2UTC_TIME_OFFSET;
	/* The underflow check is always neeeded. */
	if (ad < INT32_MIN)
		ad = INT32_MIN;
	/*
	 * The compiler will optimize away the overflow check if the tv_sec is
	 * stored as a 64-bit quantity.
	 */
	if (sizeof(ts.tv_sec) < 8) {
		if (ad > INT32_MAX)
			ad = INT32_MAX;
	}
	return cpu_to_sle32(ad);
}

/**
 * ntfs_ad2utc - convert little endian, AppleDouble time to OS X time
 * @time:	AppleDouble time (little endian) to convert to OS X UTC
 *
 * Convert the little endian, AppleDouble time @time to its corresponding OS X
 * UTC time and return that in cpu format.
 *
 * OS X stores time in a struct timespec consisting of a time_t (long at
 * present) tv_sec and a long tv_nsec where tv_sec is the number of 1-second
 * intervals since 1st January 1970, 00:00:00 UTC without including leap
 * seconds and tv_nsec is the number of 1-nano-second intervals since the value
 * of tv_sec.
 *
 * AppleDouble stores time as "a signed number of seconds before or after 12:00
 * a.m. (midnight), January 1, 2000 Greenwich Mean Time (GMT).  In other words,
 * the start of the year 2000 GMT corresponds to a date-time of 0."  (Quote
 * from the AppleDouble Specification.)
 *
 * To convert between OS X kernel time and AppleDouble format need to use a
 * fixed constant delta which is defined as NTFS_AD2UTC_TIME_OFFSET.
 *
 * So if you have an AppleDouble time = A then struct timespec.tv_sec = A + 
 * NTFS_AD2UTC_TIME_OFFSET (and .tv_nsec = 0).
 */
static inline struct timespec ntfs_ad2utc(const sle32 time)
{
	struct timespec ts;

	/*
	 * The compiler will optimize away the overflow check if the tv_sec is
	 * stored as a 64-bit quantity.
	 */
	if (sizeof(ts.tv_sec) < 8) {
		s64 utc = (s64)sle32_to_cpu(time) + NTFS_AD2UTC_TIME_OFFSET;
		if (utc > INT32_MAX)
			utc = INT32_MAX;
		ts.tv_sec = utc;
	} else
		ts.tv_sec = sle32_to_cpu(time) + NTFS_AD2UTC_TIME_OFFSET;
	ts.tv_nsec = 0;
	return ts;
}

__attribute__((visibility("hidden"))) extern const FINDER_INFO ntfs_empty_finder_info;

__attribute__((visibility("hidden"))) extern ntfschar NTFS_SFM_AFPINFO_NAME[12];
__attribute__((visibility("hidden"))) extern ntfschar NTFS_SFM_DESKTOP_NAME[12];
__attribute__((visibility("hidden"))) extern ntfschar NTFS_SFM_IDINDEX_NAME[12];
__attribute__((visibility("hidden"))) extern ntfschar NTFS_SFM_RESOURCEFORK_NAME[13];
__attribute__((visibility("hidden"))) extern ntfschar NTFS_SFM_COMMENTS_NAME[9];

__private_extern__ BOOL ntfs_is_sfm_name(ntfs_volume *vol,
		const ntfschar *name, const unsigned len);

#endif /* !_OSX_NTFS_SFM_H */
