/* Copyright 1998 by the Massachusetts Institute of Technology.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */

/* $Header$ */

#include <stdio.h>

#include <Sound.h>

#include <ErrorLib.h>

void
main (void)
{
	OSStatus theError;
	char errorString [kMaxErrorLength];
	
	for (;;) {
		printf ("Enter error number: ");
		scanf ("%d", &theError);

		GetErrorLongFormat (theError, errorString, sizeof(errorString));
		printf ("Long format: %s\n", errorString);

		GetErrorShortFormat (theError, errorString, sizeof(errorString));
		printf ("Short format: %s\n", errorString);

		GetErrorLongString (theError, errorString, sizeof(errorString));
		printf ("Long string: %s\n", errorString);

		GetErrorShortString (theError, errorString, sizeof(errorString));
		printf ("Short string: %s\n", errorString);

		GetErrorManager (theError, errorString, sizeof(errorString));
		printf ("Manager name: %s\n", errorString);
	}
}	
