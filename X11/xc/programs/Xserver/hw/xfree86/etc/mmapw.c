/* $XFree86: xc/programs/Xserver/hw/xfree86/etc/mmapw.c,v 1.3 2003/01/01 19:16:42 tsi Exp $ */
/*
 * Copyright 2002 through 2003 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of Marc Aurele La France not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Marc Aurele La France makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as-is" without express or implied warranty.
 *
 * MARC AURELE LA FRANCE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO
 * EVENT SHALL MARC AURELE LA FRANCE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#define _LARGEFILE_SOURCE 1
#define _FILE_OFFSET_BITS 64
#undef  __STRICT_ANSI__
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef MAP_FAILED
# define MAP_FAILED ((void *)(-1))
#endif

#if defined(_SCO_DS) && !defined(_SCO_DS_LL)
#define strtoull (unsigned long long)strtoul
#endif

#if !defined(strtoull) && \
    (defined(CSRG_BASED) || \
     (defined(__GNU_LIBRARY__) && \
       (__GNU_LIBRARY__ < 6)))
# define strtoull strtouq
#endif

static unsigned char datab;
static unsigned short dataw;
static unsigned int datal;
static unsigned long dataL;
static unsigned long long dataq;

static void
usage(void)
{
    fprintf(stderr, "\n"
        "mmapw [-{bwlqL}] <file> <offset> <value>\n\n"
        "endianness flags:\n\n"
        " -b   write one byte\n"
        " -w   write two aligned bytes\n"
        " -l   write four aligned bytes (default)\n"
        " -q   write eight aligned bytes\n");
    switch (sizeof(dataL))
    {
        case sizeof(datab):
            fprintf(stderr, " -L   same as -b\n\n");
            break;

        case sizeof(dataw):
            fprintf(stderr, " -L   same as -w\n\n");
            break;

        case sizeof(datal):
            fprintf(stderr, " -L   same as -l\n\n");
            break;

        case sizeof(dataq):
            fprintf(stderr, " -L   same as -q\n\n");
            break;

        default:
            fprintf(stderr, "\n");
            break;
    }

    exit(1);
}

int
main(int argc, char **argv)
{
    unsigned long long data;
    off_t Offset = 0, offset;
    size_t Length = 0, length;
    char *BadString;
    void *buffer;
    int fd, pagesize;
    char size = sizeof(datal);

    switch (argc)
    {
        case 4:
            break;

        case 5:
            if (argv[1][0] != '-')
                usage();

            switch (argv[1][1])
            {
                case 'b':
                    size = sizeof(datab);
                    break;

                case 'w':
                    size = sizeof(dataw);
                    break;

                case 'l':
                    size = sizeof(datal);
                    break;

                case 'L':
                    size = sizeof(dataL);
                    break;

                case 'q':
                    size = sizeof(dataq);
                    break;

                default:
                    usage();
            }

            if (argv[1][2])
                usage();

            argc--;
            argv++;
            break;

        default:
            usage();
    }

    BadString = (char *)0;
    Offset = strtoull(argv[2], &BadString, 0);
    if (errno || (BadString && *BadString) || (Offset & (size - 1)))
        usage();

    BadString = (char *)0;
    data = strtoull(argv[3], &BadString, 0);
    if (errno || (BadString && *BadString))
        usage();

    if ((fd = open(argv[1], O_RDWR)) < 0)
    {
        fprintf(stderr, "mmapr:  Unable to open \"%s\":  %s.\n",
            argv[1], strerror(errno));
        exit(1);
    }

    pagesize = getpagesize();
    offset = Offset & (off_t)(-pagesize);
    length = ((Offset + Length + pagesize - 1) & (off_t)(-pagesize)) - offset;
    buffer = mmap((caddr_t)0, length, PROT_WRITE, MAP_SHARED, fd, offset);
    close(fd);
    if (buffer == MAP_FAILED)
    {
        fprintf(stderr, "mmapr:  Unable to mmap \"%s\":  %s.\n",
            argv[1], strerror(errno));
        exit(1);
    }

    Offset -= offset;
    if (size == sizeof(datab))
	*(volatile unsigned char *)((char *)buffer + Offset) =
	    (unsigned char)data;
    else if (size == sizeof(dataw))
	*(volatile unsigned short *)((char *)buffer + Offset) =
	    (unsigned short)data;
    else if (size == sizeof(datal))
	*(volatile unsigned int *)((char *)buffer + Offset) =
	    (unsigned int)data;
    else if (size == sizeof(dataL))
	*(volatile unsigned long *)((char *)buffer + Offset) =
	    (unsigned long)data;
    else if (size == sizeof(dataq))
	*(volatile unsigned long long *)((char *)buffer + Offset) =
	    (unsigned long long)data;

    munmap(buffer, length);

    return 0;
}
