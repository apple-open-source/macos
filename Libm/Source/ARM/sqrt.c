
/*
 *	sqrt.c
 *
 *		by Ian Ollmann
 *
 *	Copyright (c) 2007, Apple Inc.  All Rights Reserved. 
 *
 *  Cheesy implementation of sqrt.
 */
 
#include <math.h>

double sqrt( double x )
{
	return __builtin_sqrt(x);
}
