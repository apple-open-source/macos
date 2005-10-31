/*
 *  dstools_version.c
 *  DSTools
 *
 *  Created by Steven Simon on 7/20/04.
 *  Copyright 2004 __MyCompanyName__. All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dstools_version.h"

//-----------------------------------------------------------------------------
//	dsToolAppleVersionExit
//
//	prints the build version to stdout.
//-----------------------------------------------------------------------------

void dsToolAppleVersionExit( const char *toolName )
{
	printf( "%s, Apple Computer, Inc., Version %s\n", toolName, BUILD_VERSION );
	exit( 0 );
}


