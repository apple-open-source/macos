/*
 *  fabsf.c
 *  cLibm
 *
 *  Created by Ian Ollmann on 6/13/07.
 *  Copyright 2007 Apple Inc. All rights reserved.
 *
 */

#include <math.h>

float fabsf( float f )
{
	return __builtin_fabsf(f);
}
