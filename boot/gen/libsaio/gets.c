/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright 1993, NeXT Computer Inc.
 * All rights reserved.
 */

#import "libsaio.h"

int gets(char *buf);

/*
 * return a string in buf iff typing is begun within timeout units
 */
int 
Gets(
    char *buf,
    int timeout,
    char *prompt,
    char *message
)
{
	int	ch = 0;
	int	next_second;

	printf("%s",prompt);
	if (message)
	    printf("%s",message);
	if (timeout) {
#if 0
	    printf("[");
	    for (i = 0; i < timeout; i++)
		putchar('-');
	    printf("]\b\b");
#endif
		
	    for(next_second = time18() + 18; timeout; ) {
		if (ch = readKeyboardStatus()) {
		    break;
		}
		if (time18() >= next_second) {
		    next_second += 18;
		    timeout--;
#if 0
		    printf(" \b\b");
#endif
		}
	    }
#if 0
	    putchar('\r');
	    printf("%s",prompt);
	    for (i = 0; i < strlen(message) + orig_timeout + 4; i++)
		putchar(' ');
	    putchar('\r');
	    printf("%s",prompt);
#endif
    
	    if (ch == 0) {
		printf("\n");
		return 0;
	    }
	}
	return(gets(buf));
}

int
gets(
    char *buf
)
{
	char *lp;
	int c;

	lp = buf;
	for (;;) {
		c = getchar() & 0x7f;
		if (c < ' ' && c != '\n' && c != '\b') c = 0;
		if (c == 0x7f) c = '\b';

		switch(c) {
		case '\0':
			continue;
		case '\n':
			*lp++ = '\0';
			putchar('\n');
			return 1;
		case '\b':
			if (lp > buf) {
				lp--;
				putchar('\b');
				putchar(' ');
				putchar('\b');
			}
			continue;
		default:
			*lp++ = c;
		}
	}
}
