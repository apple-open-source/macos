/* $XFree86: xc/programs/Xserver/hw/xfree86/etc/mmapr.c,v 1.7 2004/01/05 16:42:10 tsi Exp $ */
/*
 * Copyright 2002 through 2004 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
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
        "mmapr [-{bwlqL}] <file> <offset> <length>\n\n"
        "endianness flags:\n\n"
        " -b   output one byte at a time\n"
        " -w   output up to two aligned bytes at a time\n"
        " -l   output up to four aligned bytes at a time (default)\n"
        " -q   output up to eight aligned bytes at a time\n");
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
    if (errno || (BadString && *BadString))
        usage();

    BadString = (char *)0;
    Length = strtoul(argv[3], &BadString, 0);
    if (errno || (BadString && *BadString))
        usage();

    if (Length <= 0)
        return 0;

    if ((fd = open(argv[1], O_RDONLY)) < 0)
    {
        fprintf(stderr, "mmapr:  Unable to open \"%s\":  %s.\n",
            argv[1], strerror(errno));
        exit(1);
    }

    pagesize = getpagesize();
    offset = Offset & (off_t)(-pagesize);
    length = ((Offset + Length + pagesize - 1) & (off_t)(-pagesize)) - offset;
    buffer = mmap((caddr_t)0, length, PROT_READ, MAP_SHARED, fd, offset);
    close(fd);
    if (buffer == MAP_FAILED)
    {
        fprintf(stderr, "mmapr:  Unable to mmap \"%s\":  %s.\n",
            argv[1], strerror(errno));
        exit(1);
    }

    Offset -= offset;
    while (Length > 0)
    {
        if ((Offset & sizeof(datab)) ||
            (Length < sizeof(dataw)) ||
            (size < sizeof(dataw)))
        {
            datab = *(volatile unsigned char *)((char *)buffer + Offset);
            fwrite((void *)&datab, sizeof(datab), 1, stdout);
            Offset += sizeof(datab);
            Length -= sizeof(datab);
        }
        else
        if ((Offset & sizeof(dataw)) ||
            (Length < sizeof(datal)) ||
            (size < sizeof(datal)))
        {
            dataw = *(volatile unsigned short *)((char *)buffer + Offset);
            fwrite((void *)&dataw, sizeof(dataw), 1, stdout);
            Offset += sizeof(dataw);
            Length -= sizeof(dataw);
        }
        else
        if ((Offset & sizeof(datal)) ||
            (Length < sizeof(dataL)) ||
            (size < sizeof(dataL)))
        {
            datal = *(volatile unsigned int *)((char *)buffer + Offset);
            fwrite((void *)&datal, sizeof(datal), 1, stdout);
            Offset += sizeof(datal);
            Length -= sizeof(datal);
        }
        else
        if ((Offset & sizeof(dataL)) ||
            (Length < sizeof(dataq)) ||
            (size < sizeof(dataq)))
        {
            dataL = *(volatile unsigned long *)((char *)buffer + Offset);
            fwrite((void *)&dataL, sizeof(dataL), 1, stdout);
            Offset += sizeof(dataL);
            Length -= sizeof(dataL);
        }
        else
        {
            dataq = *(volatile unsigned long long *)((char *)buffer + Offset);
            fwrite((void *)&dataq, sizeof(dataq), 1, stdout);
            Offset += sizeof(dataq);
            Length -= sizeof(dataq);
        }
    }

    munmap(buffer, length);

    return 0;
}
