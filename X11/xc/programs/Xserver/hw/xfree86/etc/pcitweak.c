/* $XFree86: xc/programs/Xserver/hw/xfree86/etc/pcitweak.c,v 1.18 2004/02/13 23:58:44 dawes Exp $ */
/*
 * Copyright (c) 1999-2002 by The XFree86 Project, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 *   1.  Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer
 *       in the documentation and/or other materials provided with the
 *       distribution, and in the same place and form as other copyright,
 *       license and disclaimer information.
 *
 *   3.  The end-user documentation included with the redistribution,
 *       if any, must include the following acknowledgment: "This product
 *       includes software developed by The XFree86 Project, Inc
 *       (http://www.xfree86.org/) and its contributors", in the same
 *       place and form as other third-party acknowledgments.  Alternately,
 *       this acknowledgment may appear in the software itself, in the
 *       same form and location as other such third-party acknowledgments.
 *
 *   4.  Except as contained in this notice, the name of The XFree86
 *       Project, Inc shall not be used in advertising or otherwise to
 *       promote the sale, use or other dealings in this Software without
 *       prior written authorization from The XFree86 Project, Inc.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE XFREE86 PROJECT, INC OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * pcitweak.c
 *
 * Author: David Dawes <dawes@xfree86.org>
 */

#include "X.h"
#include "os.h"
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSproc.h"
#include "xf86Pci.h"

#ifdef __CYGWIN__
#include <getopt.h>
#endif

#include <stdarg.h>
#include <stdlib.h>
#ifdef __linux__
/* to get getopt on Linux */
#ifndef __USE_POSIX2
#define __USE_POSIX2
#endif
#endif
#include <unistd.h>
#if defined(ISC) || defined(Lynx)
extern char *optarg;
extern int optind, opterr;
#endif

pciVideoPtr *xf86PciVideoInfo = NULL;

static void usage(void);
static Bool parsePciBusString(const char *id, int *bus, int *device, int *func);
static char *myname = NULL;

int
main(int argc, char *argv[])
{
    int c;
    PCITAG tag;
    int bus, device, func;
    Bool list = FALSE, rd = FALSE, wr = FALSE;
    Bool byte = FALSE, halfword = FALSE;
    int offset = 0;
    CARD32 value = 0;
    char *id = NULL, *end;

    myname = argv[0];
    while ((c = getopt(argc, argv, "bhlr:w:")) != -1) {
	switch (c) {
	case 'b':
	    byte = TRUE;
	    break;
	case 'h':
	    halfword = TRUE;
	    break;
	case 'l':
	    list = TRUE;
	    break;
	case 'r':
	    rd = TRUE;
	    id = optarg;
	    break;
	case 'w':
	    wr = TRUE;
	    id = optarg;
	    break;
	case '?':
	default:
	    usage();
	}
    }
    argc -= optind;
    argv += optind;

    if (list) {
	xf86Verbose = 2;
	xf86EnableIO();
	xf86scanpci(0);
	xf86DisableIO();
	exit(0);
    }

    if (rd && wr)
	usage();
    if (wr && argc != 2)
	usage();
    if (rd && argc != 1)
	usage();
    if (byte && halfword)
	usage();

    if (rd || wr) {
	if (!parsePciBusString(id, &bus, &device, &func)) {
	    fprintf(stderr, "%s: Bad PCI ID string\n", myname);
	    usage();
	}
	offset = strtoul(argv[0], &end, 0);
	if (*end != '\0') {
	    fprintf(stderr, "%s: Bad offset\n", myname);
	    usage();
	}
	if (halfword) {
	    if (offset % 2) {
		fprintf(stderr, "%s: offset must be a multiple of two\n",
			myname);
		exit(1);
	    }
	} else if (!byte) {
	    if (offset % 4) {
		fprintf(stderr, "%s: offset must be a multiple of four\n",
			myname);
		exit(1);
	    }
	}
    } else {
	usage();
    }
    
    if (wr) {
	value = strtoul(argv[1], &end, 0);
	if (*end != '\0') {
	    fprintf(stderr, "%s: Bad value\n", myname);
	    usage();
	}
    }

    xf86EnableIO();

    /*
     * This is needed to setup all the buses.  Otherwise secondary buses
     * can't be accessed.
     */
    xf86scanpci(0);

    tag = pciTag(bus, device, func);
    if (rd) {
	if (byte) {
	    printf("0x%02x\n", (unsigned int)pciReadByte(tag, offset) & 0xFF);
	} else if (halfword) {
	    printf("0x%04x\n", (unsigned int)pciReadWord(tag, offset) & 0xFFFF);
	} else {
	    printf("0x%08lx\n", (unsigned long)pciReadLong(tag, offset));
	}
    } else if (wr) {
	if (byte) {
	    pciWriteByte(tag, offset, value & 0xFF);
	} else if (halfword) {
	    pciWriteWord(tag, offset, value & 0xFFFF);
	} else {
	    pciWriteLong(tag, offset, value);
	}
    }

    xf86DisableIO();
    exit(0);
}

static void
usage()
{
    fprintf(stderr, "usage:\tpcitweak -l\n"
		    "\tpcitweak -r ID [-b | -h] offset\n"
		    "\tpcitweak -w ID [-b | -h] offset value\n"
		    "\n"
		    "\t\t-l  -- list\n"
		    "\t\t-r  -- read\n"
		    "\t\t-w  -- write\n"
		    "\t\t-b  -- read/write a single byte\n"
		    "\t\t-h  -- read/write a single halfword (16 bit)\n"
		    "\t\tID  -- PCI ID string in form bus:dev:func "
					"(all in hex)\n");
	
    exit(1);
}

Bool
parsePciBusString(const char *busID, int *bus, int *device, int *func)
{
    /*
     * The format is assumed to be "bus:device:func", where bus, device
     * and func are hexadecimal integers.  func may be omitted and assumed to
     * be zero, although it doing this isn't encouraged.
     */

    char *p, *s, *end;

    s = strdup(busID);
    p = strtok(s, ":");
    if (p == NULL || *p == 0)
	return FALSE;
    *bus = strtoul(p, &end, 16);
    if (*end != '\0')
	return FALSE;
    p = strtok(NULL, ":");
    if (p == NULL || *p == 0)
	return FALSE;
    *device = strtoul(p, &end, 16);
    if (*end != '\0')
	return FALSE;
    *func = 0;
    p = strtok(NULL, ":");
    if (p == NULL || *p == 0)
	return TRUE;
    *func = strtoul(p, &end, 16);
    if (*end != '\0')
	return FALSE;
    return TRUE;
}

#include "xf86getpagesize.c"

