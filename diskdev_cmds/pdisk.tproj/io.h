/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
//
// io.h - simple io and input parsing routines
//
// Written by Eryk Vershen (eryk@apple.com)
//

/*
 * Copyright 1996,1997 by Apple Computer, Inc.
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * APPLE COMPUTER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL APPLE COMPUTER BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 */


//
// Defines
//
#define	PBLOCK_SIZE	512
#define MAXIOSIZE	2048


//
// Types
//
struct media {
    int fd;		/* what to read/write on */
    int block_size;	/* units to read/write (multiple of PBLOCK_SIZE) */
    int factor;		/* block_size/PBLOCK_SIZE */
};
typedef struct media media;


//
// Global Constants
//
extern const long kDefault;


//
// Global Variables
//


//
// Forward declarations
//
void bad_input(char *fmt, ...);
int close_media(media *dev);
void flush_to_newline(int keep_newline);
int get_command(char *prompt, int promptBeforeGet, int *command);
long get_multiplier(long divisor);
int get_number_argument(char *prompt, long *number, long default_value);
int get_okay(char *prompt, int default_value);
int get_string_argument(char *prompt, char **string, int reprompt);
int getch();
int number_of_digits(unsigned long value);
media *open_media(const char *path, int oflag);
int read_media(media *dev, unsigned long num, char *buf, int quiet);
void ungetch(int c);
int write_media(media *dev, unsigned long num, char *buf);
