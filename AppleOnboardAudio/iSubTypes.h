//-----------------------------------------------------------
// iSubTypes.h
// AppleUSBAudio
// 
// Types used in engines, clip routines
//
// Created by Aram Lindahl on Fri Mar 01 2002.
// Copyright (c) 2002 Apple Computer. All rights reserved.
//-----------------------------------------------------------
#ifndef __ISUB_TYPES__
#define __ISUB_TYPES__

// describes the interfaces the iSub supports 
typedef enum {							
    e_iSubAltInterface_8bit_Mono = 1,
    e_iSubAltInterface_8bit_Stereo,
    e_iSubAltInterface_16bit_Mono,
    e_iSubAltInterface_16bit_Stereo,
    e_iSubAltInterface_20bit_Mono,
    e_iSubAltInterface_20bit_Stereo,
} iSubAltInterfaceType;

// describes the iSub audio format
typedef struct _iSubAudioFormat {
    iSubAltInterfaceType	altInterface;
    UInt32 			numChannels;		
    UInt32 			bytesPerSample;		
    UInt32 			outputSampleRate;		
} iSubAudioFormatType;

#endif