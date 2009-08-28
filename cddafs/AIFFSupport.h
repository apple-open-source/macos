/*
 * Copyright (c) 2000-2008 Apple Inc. All rights reserved.
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


#ifndef __AIFF_SUPPORT_H__
#define __AIFF_SUPPORT_H__

// This file only needs to be included from the kernel portions of the project.
#ifdef KERNEL

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <libkern/OSByteOrder.h>

#pragma pack(push,2)

// Number of channels in file
enum
{
	kMono	= 1,
	kStereo	= 2
};

// Number of bits/sample
enum
{
	k8BitsPerSample				= 8,
	k16BitsPerSample			= 16,
	k16BitLittleEndianFormat    = 0x736F7774 // 'sowt'
};

// AIFF-C Versions
enum
{
    kAIFCVersion1                = 0xA2805140
};

// Marker ID's
enum
{
	kAIFFID					= 0x41494646,	// 'AIFF'
	kAIFCID					= 0x41494643,	// 'AIFC'
	kFormatVersionID		= 0x46564552,	// 'FVER',
    kCommonID				= 0x434F4D4D,	// 'COMM',
    kFormID					= 0x464F524D,	// 'FORM',
    kSoundDataID			= 0x53534E44	// 'SSND'
};

// Float80 from MacTypes.h
struct Float80 {
	int16_t		exp;		// exponent
	uint16_t	man[4];		// mantissa
};
typedef struct Float80 	Float80;

// From AIFF.h
struct ChunkHeader {
    uint32_t						ckID;
    int32_t							ckSize;
};
typedef struct ChunkHeader			ChunkHeader;

struct ContainerChunk {
	uint32_t						ckID;
	int32_t							ckSize;
	uint32_t						formType;
};
typedef struct ContainerChunk		ContainerChunk;

struct FormatVersionChunk {
	uint32_t						ckID;
	int32_t							ckSize;
	uint32_t						timeStamp;
};
typedef struct FormatVersionChunk	FormatVersionChunk;

struct ExtCommonChunk {
	uint32_t						ckID;
	int32_t							ckSize;
	int16_t							numChannels;
	uint32_t						numSampleFrames;
	int16_t							sampleSize;
	Float80							sampleRate;
	uint32_t						compressionType;
	char							compressionName[1]; // variable length array, Pascal string
};
typedef struct ExtCommonChunk		ExtCommonChunk;

struct SoundDataChunk {
	uint32_t						ckID;
	int32_t							ckSize;
	uint32_t						offset;
	uint32_t						blockSize;
};
typedef struct SoundDataChunk		SoundDataChunk;

struct CDAIFFHeader {
	ContainerChunk			containerChunk;
	FormatVersionChunk		formatVersionChunk;
	ExtCommonChunk			commonChunk;
	SoundDataChunk			soundDataChunk;
};
typedef struct CDAIFFHeader			CDAIFFHeader;


// Building Routines
void
BuildCDAIFFHeader ( CDAIFFHeader * header, uint32_t fileSize );


#pragma pack(pop)


#ifdef __cplusplus
}
#endif

#endif	/* KERNEL */

#endif // __AIFF_SUPPORT_H__