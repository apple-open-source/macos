/*
 * $Xorg: constype.c,v 1.3 2000/08/17 19:48:29 cpqbld Exp $
 * 
 * consoletype - utility to print out string identifying Sun console type
 *
 * Copyright 1988 SRI 
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of SRI not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  SRI makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * Author:  Doug Moran, SRI
 */
/* $XFree86: xc/programs/Xserver/hw/sun/constype.c,v 3.9 2003/11/21 05:17:01 dawes Exp $ */

/*
SUN-SPOTS DIGEST         Thursday, 17 March 1988       Volume 6 : Issue 31

Date:    Wed, 2 Mar 88 14:50:26 PST
From:    Doug Moran <moran@ai.sri.com>
Subject: Program to determine console type

There have been several requests in this digest for programs to determine
the type of the console.  Below is a program that I wrote to produce an
identifying string (I start suntools in my .login file and use this pgm to
determine which arguments to use).

Caveat:  my cluster has only a few of these monitor types, so the pgm has
not been fully tested.

Note on coding style: the function wu_fbid is actually located in a local
library, accounting for what otherwise might appear to be a strange coding
style.
*/
#include <stdio.h>
#if defined(SVR4) || defined(CSRG_BASED)
#include <string.h>
#else
/*  SunOS  */
#include <strings.h>
#endif
#include <unistd.h>

int wu_fbid(char *devname, char **fbname, int *fbtype);

int
main (argc, argv)
    int argc;
    char **argv;
{
    int fbtype = -1;
    char *fbname, *dev;
    int print_num = 0;
    int error;

    if (argc > 1 && argv[1][0] == '/') {
	dev = argv[1];
	argc--; argv++;
    } else
	dev = "/dev/fb";
    error = wu_fbid(dev, &fbname, &fbtype );
    if (argc > 1 && strncmp (argv[1], "-num", strlen(argv[1])) == 0)
	print_num = 1;

    printf ("%s", fbname ? fbname : "tty");
    if (print_num) {
	printf (" %d", fbtype);
    }
    putchar ('\n');
    return error;
}
#include <sys/ioctl.h>
#include <sys/file.h>
#if defined(SVR4) || defined(__bsdi__)
#include <fcntl.h>
#include <sys/fbio.h>
#if defined(SVR4) && defined(sun)
/* VIS_GETIDENTIFIER ioctl added in Solaris 2.3 */
#include <sys/visual_io.h>
#endif
#else
#ifndef CSRG_BASED
#include <sun/fbio.h>
#else
#include <machine/fbio.h>
#endif
#endif

/* Sun doesn't see fit to update <sys/fbio.h> to reflect the addition
 * of the TCX 
 */
#define XFBTYPE_TCX		21
#define XFBTYPE_LASTPLUSONE	22

/* decoding as of Release 3.4 : fbio.h 1.3 87/01/09 SMI */
	/* the convention for entries in this table is to translate the
	 * macros for frame buffer codes (in <sun/fbio.h>) to short names
	 * thus:
	 *	FBTYPE_SUNxBW		becomes bwx
	 *	FBTYPE_SUNxCOLOR	becomes cgx
	 *	FBTYPE_SUNxGP		becomes gpx
	 *	FBTYPE_NOTSUN[1-9]	becomes ns[A-J]
	 */
static char *decode_fb[] = {
	"bw1", "cg1",
	"bw2", "cg2",
	"gp2",
	"bw3", "cg3",
	"cg8", "cg4",
	"nsA", "nsB", "nsC", 
#ifdef FBTYPE_SUNFAST_COLOR
	"gx/cg6", 
#endif
#ifdef FBTYPE_SUNROP_COLOR
	"rop", 
#endif
#ifdef FBTYPE_SUNFB_VIDEO
	"vid", 
#endif
#ifdef FBTYPE_SUNGIFB
	"gifb", 
#endif
#ifdef FBTYPE_SUNGPLAS
	"plas", 
#endif
#ifdef FBTYPE_SUNGP3
	"gp3/cg12", 
#endif
#ifdef FBTYPE_SUNGT
	"gt", 
#endif
#ifdef FBTYPE_SUNLEO
	"leo/zx", 
#endif
#ifdef FBTYPE_MDICOLOR
	"mdi/cg14",
#endif
	};

int wu_fbid(devname, fbname, fbtype)
	char* devname;
	char** fbname;
	int* fbtype;
{
	struct fbgattr fbattr;
	int fd, ioctl_ret;
#ifdef VIS_GETIDENTIFIER
	int vistype;
	char *visname = NULL;
	struct vis_identifier fbid;
#endif

	if ( (fd = open(devname, O_RDWR, 0)) == -1 ) {
	    *fbname = "unable to open fb";
	    return 2;
	}

#ifdef VIS_GETIDENTIFIER
	if ((vistype = ioctl(fd, VIS_GETIDENTIFIER, &fbid)) >= 0) {
	    visname = fbid.name;
	}
#endif

	/* FBIOGATTR fails for early frame buffer types */
	if ((ioctl_ret = ioctl(fd,FBIOGATTR,&fbattr))<0) /*success=>0(false)*/
	    ioctl_ret = ioctl(fd, FBIOGTYPE, &fbattr.fbtype);
	close(fd);
	if ( ioctl_ret == -1 ) {
#ifdef VIS_GETIDENTIFIER
	    if (visname != NULL) {
		*fbname = visname;
		*fbtype = vistype;
		return 0;
	    } 
#endif
	    *fbname = "ioctl on fb failed";
	    return 2;
	}
	*fbtype = fbattr.fbtype.fb_type;
	/* The binary is obsolete and needs to be re-compiled:
	 * the ioctl returned a value beyond what was possible
	 * when the program was compiled */
	if (fbattr.fbtype.fb_type>=FBTYPE_LASTPLUSONE) {
	    if (fbattr.fbtype.fb_type == XFBTYPE_TCX) {
		*fbname = "tcx";
		return 0;
	    } else {
#ifdef VIS_GETIDENTIFIER
		if (visname != NULL) {
		    *fbname = visname;
		    *fbtype = vistype;
		    return 0;
		} 
#endif
		*fbname = "unk";
		return 1;
	    }
	}
	/* The source is obsolete.  The table "decode_fb" does not
	 * have entries for some of the values returned by the ioctl.
	 * Compare <sun/fbio.h> to the entries in "decode_fb" */
	if ( decode_fb[fbattr.fbtype.fb_type] == NULL ) {
#ifdef VIS_GETIDENTIFIER
	    if (visname != NULL) {
		*fbname = visname;
		*fbtype = vistype;
		return 0;
	    } 
#endif
            *fbname = "unk";
	    return 1;
	}
	*fbname = decode_fb[fbattr.fbtype.fb_type];
	return 0;
}
