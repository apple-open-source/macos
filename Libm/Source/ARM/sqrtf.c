
/*
 *	sqrtf.c
 *
 *		by Ian Ollmann
 *
 *	Copyright (c) 2007, Apple Inc.  All Rights Reserved. 
 *
 *  Cheesy implementation of sqrtf.
 */
 
#include <math.h>

float sqrtf( float x )
{
	return __builtin_sqrtf(x);
}
