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

typedef struct FourDotTwenty {
	unsigned char integerAndFraction1;
	unsigned char fraction2;
	unsigned char fraction3;
} FourDotTwenty, *FourDotTwentyPtr;

typedef enum {							
    e_Mode_Disabled = 0,
    e_Mode_CopyLeftToRight,
    e_Mode_CopyRightToLeft
} DualMonoModeType;

