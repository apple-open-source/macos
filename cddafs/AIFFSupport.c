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

// AIFFSupport.c created by CJS on Wed 10-May-2000

#ifndef __AIFF_SUPPORT_H__
#include "AIFFSupport.h"
#endif

#ifndef __APPLE_CDDA_FS_DEBUG_H__
#include "AppleCDDAFileSystemDebug.h"
#endif


// System Includes
#include <sys/systm.h>


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	EndianContainerChunk_LtoB	-	This converts a little endian representation
//									of a ContainerChunk into a big endian
//									representation
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

ContainerChunk
EndianContainerChunk_LtoB ( ContainerChunk chunk )
{
	
	chunk.ckID 		= EndianU32_LtoB ( chunk.ckID );
	chunk.ckSize	= EndianS32_LtoB ( chunk.ckSize );
	chunk.formType	= EndianU32_LtoB ( chunk.formType );
	
	return chunk;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	EndianFormatVersionChunk_LtoB	-	This converts a little endian representation
//										of a FormatVersionChunk into a big endian
//										representation
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

FormatVersionChunk
EndianFormatVersionChunk_LtoB ( FormatVersionChunk chunk )
{
	
	chunk.ckID 		= EndianU32_LtoB ( chunk.ckID );
	chunk.ckSize	= EndianS32_LtoB ( chunk.ckSize );
	chunk.timeStamp	= EndianU32_LtoB ( chunk.timeStamp );
	
	return chunk;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	EndianExtCommonChunk_LtoB	-	This converts a little endian representation
//									of an ExtCommonChunk into a big endian
//									representation
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

ExtCommonChunk
EndianExtCommonChunk_LtoB ( ExtCommonChunk chunk )
{
	
	chunk.ckID				= EndianU32_LtoB ( chunk.ckID );
	chunk.ckSize			= EndianS32_LtoB ( chunk.ckSize );
	chunk.numChannels		= EndianS16_LtoB ( chunk.numChannels );
	chunk.numSampleFrames	= EndianU32_LtoB ( chunk.numSampleFrames );
	chunk.sampleSize		= EndianS16_LtoB ( chunk.sampleSize );
	chunk.compressionType	= EndianU32_LtoB ( chunk.compressionType );
	chunk.sampleRate		= EndianFloat80_LtoB ( chunk.sampleRate );
	
	return chunk;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	EndianSoundDataChunk_LtoB	-	This converts a little endian representation
//									of a SoundDataChunk into a big endian
//									representation
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SoundDataChunk
EndianSoundDataChunk_LtoB ( SoundDataChunk chunk )
{
	
	chunk.ckID 		= EndianU32_LtoB ( chunk.ckID );
	chunk.ckSize 	= EndianS32_LtoB ( chunk.ckSize );
	chunk.offset	= EndianU32_LtoB ( chunk.offset );
	chunk.blockSize	= EndianU32_LtoB ( chunk.blockSize );
	
	return chunk;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	EndianCDAIFFHeader_LtoB	-	This converts a little endian representation
//								of a CDAIFFHeader into a big endian
//								representation
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CDAIFFHeader
EndianCDAIFFHeader_LtoB	( CDAIFFHeader header )
{
	
	header.u.AIFFHeader.containerChunk 		= EndianContainerChunk_LtoB 	( header.u.AIFFHeader.containerChunk );
	header.u.AIFFHeader.formatVersionChunk	= EndianFormatVersionChunk_LtoB	( header.u.AIFFHeader.formatVersionChunk );
	header.u.AIFFHeader.commonChunk			= EndianExtCommonChunk_LtoB 	( header.u.AIFFHeader.commonChunk );
	header.u.AIFFHeader.soundDataChunk		= EndianSoundDataChunk_LtoB 	( header.u.AIFFHeader.soundDataChunk );
	
	return header;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	EndianFloat80_LtoB	-	This converts a little endian representation
//							of a Float80 into a big endian representation
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Float80
EndianFloat80_LtoB	( Float80 data )
{
	
	data.exp 	= EndianS16_LtoB ( data.exp );
	data.man[0]	= EndianU16_LtoB ( data.man[0] );
	data.man[1]	= EndianU16_LtoB ( data.man[1] );
	data.man[2]	= EndianU16_LtoB ( data.man[2] );
	data.man[3]	= EndianU16_LtoB ( data.man[3] );
	
	return data;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	BuildCDAIFFHeader	-	This routine builds a CDAIFFHeader and explicitly
//							makes it Big Endian (as defined by AIFF standard)
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CDAIFFHeader
BuildCDAIFFHeader ( UInt32 fileSize )
{
	
	CDAIFFHeader			header;
	ExtCommonChunkPtr		commonChunkPtr 			= NULL;
	SoundDataChunkPtr		soundDataChunkPtr		= NULL;
	ContainerChunkPtr		containerChunkPtr		= NULL;
	FormatVersionChunkPtr	formatVersionChunkPtr	= NULL;
	UInt32					dataSize				= fileSize - sizeof ( CDAIFFHeader );
	
	// Zero out the header structure
	bzero ( &header, sizeof ( header ) );
		
	// Get the address of each sub-structure to make this easier to read
	commonChunkPtr			= &header.u.AIFFHeader.commonChunk;
	soundDataChunkPtr		= &header.u.AIFFHeader.soundDataChunk;
	containerChunkPtr		= &header.u.AIFFHeader.containerChunk;
	formatVersionChunkPtr	= &header.u.AIFFHeader.formatVersionChunk;
	
	// Setup the version chunk
	formatVersionChunkPtr->ckID			= kFormatVersionID;
	formatVersionChunkPtr->ckSize		= ( sizeof ( FormatVersionChunk ) - sizeof ( ChunkHeader ) );
	formatVersionChunkPtr->timeStamp	= kAIFCVersion1;
	
	// Setup the common chunk
	commonChunkPtr->ckID 				= kCommonID;
	commonChunkPtr->ckSize 				= ( sizeof ( ExtCommonChunk ) - sizeof ( ChunkHeader ) );
	commonChunkPtr->numChannels 		= kStereo;			// 2 channels
	commonChunkPtr->numSampleFrames 	= dataSize / 4; 	// 4 = ( k16BitsPerSample / 8 ) * kStereo
	commonChunkPtr->sampleSize 			= k16BitsPerSample;	// Set the sample size to 16 bits
	commonChunkPtr->sampleRate.exp 		= 0x400E;			// Set the sample rate to 44.1 KHz
	commonChunkPtr->sampleRate.man[0] 	= 0xAC44;			// Set the sample rate to 44.1 KHz
	commonChunkPtr->sampleRate.man[1]	= 0x0000;			// Set the sample rate to 44.1 KHz	
	commonChunkPtr->sampleRate.man[2]	= 0x0000;			// Set the sample rate to 44.1 KHz
	commonChunkPtr->sampleRate.man[3]	= 0x0000;			// Set the sample rate to 44.1 KHz
	
	// Data streamed off disc is in little endian format
	commonChunkPtr->compressionType	= k16BitLittleEndianFormat;
	
	// Setup the soundData chunk
	soundDataChunkPtr->ckID 		= kSoundDataID;
	soundDataChunkPtr->offset 		= sizeof ( header.u.alignedHeader ) - sizeof ( header.u.AIFFHeader );
	soundDataChunkPtr->ckSize 		= ( sizeof ( SoundDataChunk ) - sizeof ( ChunkHeader ) ) +
									  dataSize + soundDataChunkPtr->offset;
	soundDataChunkPtr->blockSize 	= 0;
	
	// Setup the container chunk
	containerChunkPtr->ckID 	= kFormID;
	containerChunkPtr->ckSize 	= ( sizeof ( ContainerChunk ) - sizeof ( ChunkHeader ) ) +		// size of container chunk variables
					   			  ( formatVersionChunkPtr->ckSize + sizeof ( ChunkHeader ) ) + 	// size of common chunk
					   			  ( commonChunkPtr->ckSize + sizeof ( ChunkHeader ) ) + 		// size of common chunk
					   			  ( soundDataChunkPtr->ckSize + sizeof ( ChunkHeader ) );		// size of sound data chunk

	
	// save as uncompressed AIFF-C
	containerChunkPtr->formType = kAIFCID;
	
	// Convert from natural byte order to big endian byte order
	// because AIFF Header data MUST be big endian (the audio data
	// doesn't necessarily have to)
	header = EndianCDAIFFHeader_NtoB ( header );
	
	return header;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//				End				Of			File
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ