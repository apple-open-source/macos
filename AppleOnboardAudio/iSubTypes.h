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

// iSub filter state structures
typedef struct _sPreviousValues {
    float	xl_1;
    float	xr_1;
    float	xl_2;
    float	xr_2;
    float	yl_1;
    float	yr_1;
    float	yl_2;
    float	yr_2;
} PreviousValues;

typedef struct _siSubCoefficients {
    float	b0;
    float	b1;
    float	b2;
    float	a1;
    float	a2;
} iSubCoefficients;

typedef struct _iSubProcessingParams_t {
    float				srcPhase;
    float				srcState;
    PreviousValues 		filterState;
    PreviousValues 		filterState2;
    PreviousValues 		phaseCompState;
    iSubCoefficients	coefficients;
    float				*lowFreqSamples;
    float				*highFreqSamples;
	SInt16 				*iSubBuffer;
	UInt32 				iSubBufferLen;
	UInt32 				iSubLoopCount;
	SInt32 				iSubBufferOffset;
	iSubAudioFormatType	iSubFormat;
	UInt32 				sampleRate;
	UInt32 				adaptiveSampleRate;
} iSubProcessingParams_t, *iSubProcessingParamsPtr_t;

#endif
