/*
 *  AppleDBDMAClipLib.h
 *  AppleOnboardAudio
 *
 * 	Private header for floating point library
 *
 *  Created by Aram Lindahl on Thu Nov 14 2002.
 *  Copyright (c) 2002 Apple Computer. All rights reserved.
 *
 */

#include "iSubTypes.h"	

#pragma mark ----------------------------- 
#pragma mark ••• Constants, Types & Tables
#pragma mark ----------------------------- 

#define kMaxLookahead			64						
#define kMaxNumFilters 			10
#define kMaxNumLimiters 		3
#define kMaxNumCrossoverBands	3

typedef enum _FilterTypes_t {
	kLowPass		= 'lpas', 
	kHighPass		= 'hpas',	
	kBandPass		= 'bpas',
	kBandReject		= 'brej',
	kParametric		= 'para',
	kLowShelf		= 'lslf',
	kHighShelf		= 'hslf',
	kAllPass		= 'apas'
} FilterType;

typedef enum _LimiterTypes_t {
	kFeedforward	= 'ffor',
	kFeedback		= 'fbck'
} LimiterType;

typedef struct _CrossoverParamStruct_t {
	UInt32  	numBands;
	UInt32		phaseReverseHigh;
 	float		frequency[kMaxNumCrossoverBands];
	float		delay[kMaxNumCrossoverBands];
} CrossoverParamStruct, *CrossoverParamStructPtr;

typedef struct _CrossoverStruct_t {
	UInt32		numBands;
	UInt32		phaseReverseHigh;
	float		delay[kMaxNumCrossoverBands];
	float *		outBufferPtr[kMaxNumCrossoverBands];
	float 		c1_1st[kMaxNumCrossoverBands];	
	float 		c1_2nd[kMaxNumCrossoverBands];	
	float 		c2_2nd[kMaxNumCrossoverBands];	

	float 		inTap_1stL[kMaxNumCrossoverBands];	
	float 		outTap_1stL[kMaxNumCrossoverBands];

	float 		inTap_1stR[kMaxNumCrossoverBands];	
	float 		outTap_1stR[kMaxNumCrossoverBands];
	
	float 		inTap1_2ndL[kMaxNumCrossoverBands];
	float 		inTap2_2ndL[kMaxNumCrossoverBands];
	float 		outTap1_2ndL[kMaxNumCrossoverBands];
	float 		outTap2_2ndL[kMaxNumCrossoverBands];	

	float 		inTap1_2ndR[kMaxNumCrossoverBands];
	float 		inTap2_2ndR[kMaxNumCrossoverBands];
	float 		outTap1_2ndR[kMaxNumCrossoverBands];
	float 		outTap2_2ndR[kMaxNumCrossoverBands];	

	Boolean		bypassAll;
} CrossoverStruct, *CrossoverStructPtr;

typedef struct _LimiterStruct_t {
	LimiterType	type[kMaxNumLimiters];	
	UInt32		numLimiters;
	float		gain[kMaxNumLimiters];
	float 		threshold[kMaxNumLimiters];	
	float 		oneMinusOneOverRatio[kMaxNumLimiters];	
	float 		attackTc[kMaxNumLimiters];	
	float 		releaseTc[kMaxNumLimiters];	
	UInt32  	bypass[kMaxNumLimiters];	
	UInt32  	lookahead[kMaxNumLimiters];	

	float 		prev_g[kMaxNumLimiters];	
	float 		prev_peakL[kMaxNumLimiters];
	float 		prev_peakR[kMaxNumLimiters];
	float 		prev_outputL[kMaxNumLimiters];
	float 		prev_outputR[kMaxNumLimiters];
	UInt32 		writeIndex[kMaxNumLimiters];	
	float 		lookaheadDelayBuffer[kMaxNumLimiters][kMaxLookahead];

	Boolean		bypassAll;
} LimiterStruct, *LimiterStructPtr;

typedef struct _LimiterParamStruct_t {
	LimiterType	type;
	float		threshold;
	float		gain;
	float		ratio;
	float		attack;
	float		release;
	UInt32		lookahead;
} LimiterParamStruct, *LimiterParamStructPtr;

typedef struct _EQStruct_t {
	FilterType	type[kMaxNumFilters];
	UInt32		numSoftwareFilters;
	float		b0[kMaxNumFilters], b1[kMaxNumFilters], b2[kMaxNumFilters], a1[kMaxNumFilters], a2[kMaxNumFilters];
    float		in_del1L[kMaxNumFilters], in_del2L[kMaxNumFilters], out_del1L[kMaxNumFilters], out_del2L[kMaxNumFilters];
	float		in_del1R[kMaxNumFilters], in_del2R[kMaxNumFilters], out_del1R[kMaxNumFilters], out_del2R[kMaxNumFilters];
	float		leftSoftVolume;
	float		rightSoftVolume;
	Boolean		bypassFilter[kMaxNumFilters];
	Boolean		runInSoftware[kMaxNumFilters];
	Boolean		bypassAll;
	Boolean		phaseReverse;
} EQStruct, *EQStructPtr;

typedef struct _EQParamStruct_t {
	FilterType	type;
	float		fc;
	float		Q;
	float		gain;
} EQParamStruct, *EQParamStructPtr;

typedef struct FourDotTwenty {
	unsigned char integerAndFraction1;
	unsigned char fraction2;
	unsigned char fraction3;
} FourDotTwenty, *FourDotTwentyPtr;

// alternate form for EQ state:
/*
typedef struct _FilterStruct_t {
	float			b0, b1, b2, a1, a2; 							
    float			in_del1L, in_del2L, out_del1L, out_del2L;
	float			in_del1R, in_del2R, out_del1R, out_del2R;
	Boolean			bypassFilter;
	Boolean			used;
} FilterStruct, *FilterStructPtr;

FilterStructPtr	filterStructPtr[kMaxNumFilters];
*/
typedef enum {							
    e_Mode_Disabled = 0,
    e_Mode_CopyLeftToRight,
    e_Mode_CopyRightToLeft
} DualMonoModeType;
