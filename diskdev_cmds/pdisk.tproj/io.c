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
// io.c - simple io and input parsing routines
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

#import <stdio.h>
#import <unistd.h>
#import <sys/fcntl.h>
#import <stdlib.h>
#import <string.h>
#import <stdarg.h>
#import <errno.h>

#include "pdisk.h"
#include "io.h"
#include "errors.h"


//
// Defines
//
#define BAD_DIGIT 17	/* must be greater than any base */
#define	STRING_CHUNK	16
#define UNGET_MAX_COUNT 10
#ifndef __linux__
#ifdef __APPLE__
#define loff_t off_t
#define llseek lseek
#else
#define loff_t long
#define llseek lseek
#endif
#endif


//
// Types
//


//
// Global Constants
//
const long kDefault = -1;


//
// Global Variables
//
short unget_buf[UNGET_MAX_COUNT+1];
int unget_count;
char io_buffer[MAXIOSIZE];


//
// Forward declarations
//
int compute_block_size(int fd);
long get_number(int first_char);
char* get_string(int eos);


//
// Routines
//
int
getch()
{
    if (unget_count > 0) {
	return (unget_buf[--unget_count]);
    } else {
	return (getc(stdin));
    }
}


void
ungetch(int c)
{
    // In practice there is never more than one character in
    // the unget_buf, but what's a little overkill among friends?

    if (unget_count < UNGET_MAX_COUNT) {
	unget_buf[unget_count++] = c;
    } else {
	fatal(-1, "Programmer error in ungetch().");
    }
}

	
void
flush_to_newline(int keep_newline)
{
    int		c;

    for (;;) {
	c = getch();

	if (c <= 0) {
	    break;
	} else if (c == '\n') {
	    if (keep_newline) {
		ungetch(c);
	    }
	    break;
	} else {
	    // skip
	}
    }
    return;
}


int
get_okay(char *prompt, int default_value)
{
    int		c;

    flush_to_newline(0);
    printf(prompt);

    for (;;) {
	c = getch();

	if (c <= 0) {
	    break;
	} else if (c == ' ' || c == '\t') {
	    // skip blanks and tabs
	} else if (c == '\n') {
	    ungetch(c);
	    return default_value;
	} else if (c == 'y' || c == 'Y') {
	    return 1;
	} else if (c == 'n' || c == 'N') {
	    return 0;
	} else {
	    flush_to_newline(0);
	    printf(prompt);
	}
    }
    return -1;
}

	
int
get_command(char *prompt, int promptBeforeGet, int *command)
{
    int		c;

    if (promptBeforeGet) {
	printf(prompt);
    }	
    for (;;) {
	c = getch();

	if (c <= 0) {
	    break;
	} else if (c == ' ' || c == '\t') {
	    // skip blanks and tabs
	} else if (c == '\n') {
	    printf(prompt);
	} else {
	    *command = c;
	    return 1;
	}
    }
    return 0;
}

	
int
get_number_argument(char *prompt, long *number, long default_value)
{
    int c;
    int result = 0;

    for (;;) {
	c = getch();

	if (c <= 0) {
	    break;
	} else if (c == ' ' || c == '\t') {
	    // skip blanks and tabs
	} else if (c == '\n') {
	    if (default_value < 0) {
		printf(prompt);
	    } else {
		ungetch(c);
		*number = default_value;
		result = 1;
		break;
	    }
	} else if ('0' <= c && c <= '9') {
	    *number = get_number(c);
	    result = 1;
	    break;
	} else {
	    ungetch(c);
	    *number = 0;
	    break;
	}
    }
    return result;
}


long
get_number(int first_char)
{
    register int c;
    int base;
    int digit;
    int ret_value;

    if (first_char != '0') {
	c = first_char;
	base = 10;
	digit = BAD_DIGIT;
    } else if ((c=getch()) == 'x' || c == 'X') {
	c = getch();
	base = 16;
	digit = BAD_DIGIT;
    } else {
	c = first_char;
	base = 8;
	digit = 0;
    }
    ret_value = 0;
    for (ret_value = 0; ; c = getch()) {
	if (c >= '0' && c <= '9') {
	    digit = c - '0';
	} else if (c >='A' && c <= 'F') {
	    digit = 10 + (c - 'A');
	} else if (c >='a' && c <= 'f') {
	    digit = 10 + (c - 'a');
	} else {
	    digit = BAD_DIGIT;
	}
	if (digit >= base) {
	    break;
	}
	ret_value = ret_value * base + digit;
    }
    ungetch(c);
    return(ret_value);
}

	
int
get_string_argument(char *prompt, char **string, int reprompt)
{
    int c;
    int result = 0;

    for (;;) {
	c = getch();

	if (c <= 0) {
	    break;
	} else if (c == ' ' || c == '\t') {
	    // skip blanks and tabs
	} else if (c == '\n') {
	    if (reprompt) {
		printf(prompt);
	    } else {
		ungetch(c);
		*string = NULL;
		break;
	    }
	} else if (c == '"' || c == '\'') {
	    *string = get_string(c);
	    result = 1;
	    break;
	} else if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z')
		|| (c == '-' || c == '/')) {
	    ungetch(c);
	    *string = get_string(' ');
	    result = 1;
	    break;
	} else {
	    ungetch(c);
	    *string = NULL;
	    break;
	}
    }
    return result;
}


char *
get_string(int eos)
{
    int c;
    char *s;
    char *ret_value;
    char *limit;
    int length;

    ret_value = (char *) malloc(STRING_CHUNK);
    if (ret_value == NULL) {
	error(errno, "can't allocate memory for string buffer");
	return NULL;
    }
    length = STRING_CHUNK;
    limit = ret_value + length;

    c = getch();
    for (s = ret_value; ; c = getch()) {
	if (s >= limit) {
	    // expand string
	    limit = (char *) malloc(length+STRING_CHUNK);
	    if (limit == NULL) {
		error(errno, "can't allocate memory for string buffer");
		ret_value[length-1] = 0;
		break;
	    }
	    strncpy(limit, ret_value, length);
	    free(ret_value);
	    s = limit + (s - ret_value);
	    ret_value = limit;
	    length += STRING_CHUNK;
	    limit = ret_value + length;
	}
	if (c <= 0 || c == eos || (eos == ' ' && c == '\t')) {
	    *s++ = 0;
	    break;
	} else if (c == '\n') {
	    *s++ = 0;
	    ungetch(c);
	    break;
	} else {
	    *s++ = c;
	}
    }
    return(ret_value);
}


long
get_multiplier(long divisor)
{
    int c;
    int result;

    c = getch();

    if (c <= 0 || divisor <= 0) {
	result = 0;
    } else if (c == 'g' || c == 'G') {
	result = 1024*1024*1024;
    } else if (c == 'm' || c == 'M') {
	result = 1024*1024;
    } else if (c == 'k' || c == 'K') {
	result = 1024;
    } else {
	ungetch(c);
	result = 1;
    }
    if (result > 1) {
	if (result >= divisor) {
	    result /= divisor;
	} else {
	    result = 1;
	}
    }
    return result;
}


int
number_of_digits(unsigned long value)
{
    int j;

    j = 1;
    while (value > 9) {
	j++;
	value = value / 10;
    }
    return j;
}


//
// Print a message on standard error & flush the input.
//
void
bad_input(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    flush_to_newline(1);
}


int
compute_block_size(int fd)
{
    int size;
    loff_t x;
    long t;

    for (size = 512; size <= MAXIOSIZE; size *= 2) {
	if ((x = llseek(fd, (loff_t)0, 0)) < 0) {
	    error(errno, "Can't seek on file");
	    break;
	}
	if ((t = read(fd, io_buffer, size)) == size) {
	    return size;
	}
    }
    return 0;

}


int
read_media(media *dev, unsigned long num, char *buf, int quiet)
{
    int fd;
    loff_t x;
    int offset;
    long t;

    fd = dev->fd;
    {
	offset = (num % dev->factor) * PBLOCK_SIZE;
    x = ((loff_t)(num / dev->factor)) * dev->block_size;

	if ((x = llseek(fd, x, 0)) < 0) {
	    if (quiet == 0) {
		error(errno, "Can't seek on file");
	    }
	    return 0;
	}
	if (dev->block_size == PBLOCK_SIZE) {
	    if ((t = read(fd, buf, PBLOCK_SIZE)) != PBLOCK_SIZE) {
		if (quiet == 0) {
		    error((t<0?errno:0), "Can't read block %u from file (%d)",
			    num, t);
		}
		return 0;
	    }
	} else {
	    if ((t = read(fd, io_buffer, dev->block_size))
		    != dev->block_size) {
		if (quiet == 0) {
		    error((t<0?errno:0), "Can't read block %u from file (%d)",
			    num, t);
		}
		return 0;
	    }
	    memcpy(buf, &io_buffer[offset], PBLOCK_SIZE);
	}
	return 1;
    }
}


int
write_media(media *dev, unsigned long num, char *buf)
{
    int fd;
    loff_t x;
    long t;
    int offset;

    fd = dev->fd;
    if (rflag) {
	printf("Can't write block %lu to file", num);
	return 0;
    }
    {
	offset = (num % dev->factor) * PBLOCK_SIZE;
    x = ((loff_t)(num / dev->factor)) * dev->block_size;

	if ((x = lseek(fd, x, 0)) < 0) {
	    error(errno, "Can't seek on file");
	    return 0;
	}
	if (dev->block_size == PBLOCK_SIZE) {
	    if ((t = write(fd, buf, PBLOCK_SIZE)) != PBLOCK_SIZE) {
		error((t<0?errno:0), "Can't write block %u to file (%d)", num, t);
		return 0;
	    }
	} else {
	    if ((t = read(fd, io_buffer, dev->block_size))
		    != dev->block_size) {
		error((t<0?errno:0), "Can't read/write block %u from file (%d)",
			    num, t);
		return 0;
	    }
	    memcpy(&io_buffer[offset], buf, PBLOCK_SIZE);
	    if ((t = write(fd, io_buffer, dev->block_size))
		    != dev->block_size) {
		error((t<0?errno:0), "Can't write block %u to file (%d)", num, t);
		return 0;
	    }
	}
	return 1;
    }
}


int
close_media(media *dev)
{
    int result;

    {
	result = close(dev->fd);
    }
    free(dev);
    return result;
}


media *
open_media(const char *path, int oflag)
{
    media *dev;
    int fd;
    int size;

    dev = (media *) malloc(sizeof(struct media));
    if (dev == NULL) {
	error(errno, "can't allocate memory for open file");
	return NULL;
    }

    {
	fd = open(path, oflag);
	if (fd < 0) {
	    free(dev);
	    dev = NULL;
	} else {
	    dev->fd = fd;
	}
    }
    if (dev == NULL) {
    	/* do nothing */
    } else if ((size = compute_block_size(dev->fd)) == 0) {
	close_media(dev);
	dev = NULL;
    } else {
	dev->block_size = size;
	dev->factor = dev->block_size / PBLOCK_SIZE;
    }
    return dev;
}
