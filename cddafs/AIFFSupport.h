/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

// AIFFSupport.h created by CJS on Wed 10-May-2000
// Portions gleaned from AIFF.h, MacTypes.h, Endian.h, ConditionalMacros.h
// in the Macintosh Universal Headers

#ifndef __AIFF_SUPPORT_H__
#define __AIFF_SUPPORT_H__

// This file only needs to be included from the kernel portions of the project.
#ifdef KERNEL

// From ConditionalMacros.h

#if defined(__GNUC__) && (defined(__APPLE_CPP__) || defined(__APPLE_CC__) || defined(__NEXT_CPP__))
    /*
        gcc based compilers used on Mac OS X
    */
    #if defined(__ppc__) || defined(powerpc) || defined(ppc)
        #define TARGET_CPU_PPC          1
        #define TARGET_CPU_68K          0
        #define TARGET_CPU_X86          0
        #define TARGET_CPU_MIPS         0
        #define TARGET_CPU_SPARC        0   
        #define TARGET_CPU_ALPHA        0
        #define TARGET_RT_MAC_CFM       0
        #define TARGET_RT_MAC_MACHO     1
        #define TARGET_RT_MAC_68881     0
        #define TARGET_RT_LITTLE_ENDIAN 0
        #define TARGET_RT_BIG_ENDIAN    1
    #elif defined(m68k)
        #define TARGET_CPU_PPC          0
        #define TARGET_CPU_68K          1
        #define TARGET_CPU_X86          0
        #define TARGET_CPU_MIPS         0
        #define TARGET_CPU_SPARC        0   
        #define TARGET_CPU_ALPHA        0
        #define TARGET_RT_MAC_CFM       0
        #define TARGET_RT_MAC_MACHO     1
        #define TARGET_RT_MAC_68881     0
        #define TARGET_RT_LITTLE_ENDIAN 0
        #define TARGET_RT_BIG_ENDIAN    1
    #elif defined(sparc)
        #define TARGET_CPU_PPC          0
        #define TARGET_CPU_68K          0
        #define TARGET_CPU_X86          0
        #define TARGET_CPU_MIPS         0
        #define TARGET_CPU_SPARC        1
        #define TARGET_CPU_ALPHA        0
        #define TARGET_RT_MAC_CFM       0
        #define TARGET_RT_MAC_MACHO     1
        #define TARGET_RT_MAC_68881     0
        #define TARGET_RT_LITTLE_ENDIAN 0
        #define TARGET_RT_BIG_ENDIAN    1
    #elif defined(__i386__) || defined(i386) || defined(intel)
        #define TARGET_CPU_PPC          0
        #define TARGET_CPU_68K          0
        #define TARGET_CPU_X86          1
        #define TARGET_CPU_MIPS         0
        #define TARGET_CPU_SPARC        0
        #define TARGET_CPU_ALPHA        0
        #define TARGET_RT_MAC_CFM       0
        #define TARGET_RT_MAC_MACHO     1
        #define TARGET_RT_MAC_68881     0
        #define TARGET_RT_LITTLE_ENDIAN 1
        #define TARGET_RT_BIG_ENDIAN    0
    #else
        #error unrecognized GNU C compiler
    #endif
    
    #define PRAGMA_IMPORT               0
    #define PRAGMA_STRUCT_ALIGN         1
    #define PRAGMA_ONCE                 0
    #define PRAGMA_STRUCT_PACK          0
    #define PRAGMA_STRUCT_PACKPUSH      0
    #define PRAGMA_ENUM_PACK            0
    #define PRAGMA_ENUM_ALWAYSINT       0
    #define PRAGMA_ENUM_OPTIONS         0
    #define FOUR_CHAR_CODE(x)           (x)

    #define TYPE_EXTENDED               0
    #if __GNUC__ >= 2
        #define TYPE_LONGLONG           1
    #else
        #define TYPE_LONGLONG           0
    #endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

#if PRAGMA_STRUCT_ALIGN
	#pragma options align=mac68k
#elif PRAGMA_STRUCT_PACKPUSH
	#pragma pack(push, 2)
#elif PRAGMA_STRUCT_PACK
	#pragma pack(2)
#endif

#include <libkern/OSTypes.h>

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
	k16BitLittleEndianFormat    = FOUR_CHAR_CODE('sowt')
};

// AIFF-C Versions
enum
{
    kAIFCVersion1                = ( SInt32 ) 0xA2805140
};

// Marker ID's
enum
{
	kAIFFID					= FOUR_CHAR_CODE('AIFF'),
	kAIFCID					= FOUR_CHAR_CODE('AIFC'),
	kFormatVersionID		= FOUR_CHAR_CODE('FVER'),
    kCommonID				= FOUR_CHAR_CODE('COMM'),
    kFormID					= FOUR_CHAR_CODE('FORM'),
    kSoundDataID			= FOUR_CHAR_CODE('SSND')
};

// Float80 from MacTypes.h
struct Float80 {
	SInt16  exp;		// exponent
	UInt16  man[4];		// mantissa
};
typedef struct Float80 	Float80;

// From AIFF.h
struct ChunkHeader {
    UInt32							ckID;
    SInt32							ckSize;
};
typedef struct ChunkHeader			ChunkHeader;

struct ContainerChunk {
	UInt32							ckID;
	SInt32							ckSize;
	UInt32							formType;
};
typedef struct ContainerChunk		ContainerChunk;
typedef ContainerChunk *			ContainerChunkPtr;

struct FormatVersionChunk {
	UInt32							ckID;
	SInt32							ckSize;
	UInt32							timeStamp;
};
typedef struct FormatVersionChunk	FormatVersionChunk;
typedef FormatVersionChunk *		FormatVersionChunkPtr;

struct ExtCommonChunk {
	UInt32							ckID;
	SInt32							ckSize;
	SInt16							numChannels;
	UInt32							numSampleFrames;
	SInt16							sampleSize;
	Float80							sampleRate;
	UInt32							compressionType;
	char							compressionName[1]; // variable length array, Pascal string
};
typedef struct ExtCommonChunk		ExtCommonChunk;
typedef ExtCommonChunk *			ExtCommonChunkPtr;

struct SoundDataChunk {
	UInt32							ckID;
	SInt32							ckSize;
	UInt32							offset;
	UInt32							blockSize;
};
typedef struct SoundDataChunk		SoundDataChunk;
typedef SoundDataChunk *			SoundDataChunkPtr;

struct CDAIFFHeader {
	union {
		struct {
			ContainerChunk					containerChunk;
			FormatVersionChunk				formatVersionChunk;
			ExtCommonChunk					commonChunk;
			SoundDataChunk					soundDataChunk;
		} AIFFHeader;
		struct {
			UInt8	filler[2352];				// force CD block alignment
		} alignedHeader;
	} u;
};
typedef struct CDAIFFHeader			CDAIFFHeader;
typedef CDAIFFHeader *				CDAIFFHeaderPtr;


// Conversion Macros
#if TARGET_RT_BIG_ENDIAN
	#define EndianContainerChunk_NtoB( data )		( data )
	#define EndianFormatVersionChunk_NtoB( data )	( data )
	#define EndianExtCommonChunk_NtoB( data )		( data )
	#define EndianSoundDataChunk_NtoB( data )		( data )
	#define EndianCDAIFFHeader_NtoB( data )			( data )
	#define EndianFloat80_NtoB( data )				( data )
#else
	#define EndianContainerChunk_NtoB( data )		(EndianContainerChunk_LtoB(data))
	#define EndianFormatVersionChunk_NtoB( data )	(EndianFormatVersionChunk_LtoB(data))
	#define EndianExtCommonChunk_NtoB( data )		(EndianExtCommonChunk_LtoB(data))
	#define EndianSoundDataChunk_NtoB( data )		(EndianSoundDataChunk_LtoB(data))
	#define EndianCDAIFFHeader_NtoB( data )			(EndianCDAIFFHeader_LtoB(data))
	#define EndianFloat80_NtoB( data )				(EndianFloat80_LtoB(data))
#endif


// From Endian.h

// Implement ÅLtoB
#define EndianS16_LtoB(value)               ((SInt16)Endian16_Swap(value))
#define EndianU16_LtoB(value)               ((UInt16)Endian16_Swap(value))
#define EndianS32_LtoB(value)               ((SInt32)Endian32_Swap(value))
#define EndianU32_LtoB(value)               ((UInt32)Endian32_Swap(value))

/*
    Implement low level Å_Swap functions.
    
        extern UInt16 Endian16_Swap(UInt16 value);
        extern UInt32 Endian32_Swap(UInt32 value);
        
    Note: Depending on the processor, you might want to implement
          these as function calls instead of macros.
    
*/


UInt16
Endian16_Swap	( UInt16 value );

UInt32
Endian32_Swap	( UInt32 value );


// override with macros
#define Endian16_Swap(value)                 \
        (((((UInt16)value)<<8) & 0xFF00)   | \
         ((((UInt16)value)>>8) & 0x00FF))

#define Endian32_Swap(value)                     \
        (((((UInt32)value)<<24) & 0xFF000000)  | \
         ((((UInt32)value)<< 8) & 0x00FF0000)  | \
         ((((UInt32)value)>> 8) & 0x0000FF00)  | \
         ((((UInt32)value)>>24) & 0x000000FF))


// Conversion Routines
ContainerChunk
EndianContainerChunk_LtoB 		( ContainerChunk chunk );

FormatVersionChunk
EndianFormatVersionChunk_LtoB 	( FormatVersionChunk chunk );

ExtCommonChunk
EndianExtCommonChunk_LtoB 		( ExtCommonChunk chunk );

SoundDataChunk
EndianSoundDataChunk_LtoB 		( SoundDataChunk chunk );

CDAIFFHeader
EndianCDAIFFHeader_LtoB			( CDAIFFHeader header );

Float80
EndianFloat80_LtoB				( Float80 value );


// Building Routines
CDAIFFHeader
BuildCDAIFFHeader 				( UInt32 fileSize );


#if PRAGMA_STRUCT_ALIGN
	#pragma options align=reset
#elif PRAGMA_STRUCT_PACKPUSH
	#pragma pack(pop)
#elif PRAGMA_STRUCT_PACK
	#pragma pack()
#endif


#ifdef __cplusplus
}
#endif

#endif	/* KERNEL */

#endif // __AIFF_SUPPORT_H__