/*
 *  Apple02DBDMAAudioClip.h
 *  Apple02Audio
 *
 * 	Private header for for floating point library
 *
 *  Created by Aram Lindahl on Thu Nov 14 2002.
 *  Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
 *
 */

#include "iSubTypes.h"	

#pragma mark ----------------------------- 
#pragma mark еее Constants, Types & Tables
#pragma mark ----------------------------- 

typedef enum {							
    e_Mode_Disabled = 0,
    e_Mode_CopyLeftToRight,
    e_Mode_CopyRightToLeft
} DualMonoModeType;

//	floating point types
typedef	float	Float32;
typedef double	Float64;

const float kMixingToMonoScale = 0.5f;

const float kOneOver65535 = 1.0f/65535.0f;
const float kOneOver1000000000 = 1.0f/1000000000.0f;

// -24 dB to +24 dB in 1 dB steps
const UInt16 kZeroGaindBConvTableOffset = 24;
const UInt16 kMaxZeroGain = 24;
const UInt16 kMinZeroGain = 24;

static float zeroGaindBConvTable[] = {
    0.0631,  0.0708,  0.0794,  0.0891,  0.1000,  0.1122,  0.1259, 
    0.1413,  0.1585,  0.1778,  0.1995,  0.2239,  0.2512,  0.2818,
    0.3162,  0.3548,  0.3981,  0.4467,  0.5012,  0.5623,  0.6310, 
    0.7079,  0.7943,  0.8913,  1.0000,  1.1220,  1.2589,  1.4125, 
    1.5849,  1.7783,  1.9953,  2.2387,  2.5119,  2.8184,  3.1623, 
    3.5481,  3.9811,  4.4668,  5.0119,  5.6234,  6.3096,  7.0795, 
    7.9433,  8.9125, 10.0000, 11.2202, 12.5893, 14.1254, 15.8489
};

// -12 dB to +12 dB in 0.5 dB steps
const UInt16 kInputGaindBConvTableOffset = 24;
static float inputGaindBConvTable[] = {
	0.251189,	0.266073,	0.281838,	0.298538,
	0.316228,	0.334965,	0.354813,	0.375837,
	0.398107,	0.421697,	0.446684,	0.473151,
	0.501187,	0.530884,	0.562341,	0.595662,
	0.630957,	0.668344,	0.707946,	0.749894,
	0.794328,	0.841395,	0.891251,	0.944061,
	1.000000,	1.059254,	1.122018,	1.188502,
	1.258925,	1.333521,	1.412538,	1.496236,
	1.584893,	1.678804,	1.778279,	1.883649,
	1.995262,	2.113489,	2.238721,	2.371374,
	2.511886,	2.660725,	2.818383,	2.985383,
	3.162278,	3.349654,	3.548134,	3.758374,
	3.981072
};

