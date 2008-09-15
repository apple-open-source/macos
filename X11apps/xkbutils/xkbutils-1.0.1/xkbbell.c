/* $Xorg: xkbbell.c,v 1.4 2000/08/17 19:54:51 cpqbld Exp $ */
/************************************************************
Copyright (c) 1993 by Silicon Graphics Computer Systems, Inc.

Permission to use, copy, modify, and distribute this
software and its documentation for any purpose and without
fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright
notice and this permission notice appear in supporting
documentation, and that the name of Silicon Graphics not be 
used in advertising or publicity pertaining to distribution 
of the software without specific prior written permission.
Silicon Graphics makes no representation about the suitability 
of this software for any purpose. It is provided "as is"
without any express or implied warranty.

SILICON GRAPHICS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS 
SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY 
AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL SILICON
GRAPHICS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, 
DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE 
OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION  WITH
THE USE OR PERFORMANCE OF THIS SOFTWARE.

********************************************************/
/* $XFree86: xc/programs/xkbutils/xkbbell.c,v 1.4 2001/01/17 23:46:13 dawes Exp $ */

#include <stdio.h>
#include <string.h>
#include <X11/Xproto.h>
#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XI.h>

static	char		*dpyName = NULL;
static	int		 volume = 0;
static	int		 devSpec = XkbUseCoreKbd;
static	int		 class= -1;
static	int		 id= -1;
static	char *		 bellName;
static	Atom		 nameAtom = None;
static	int		 synch= 0;
static	int		 win = None;
static	int		 force = 0;
static	int		 nobeep = 0;

static int
parseArgs(int argc, char *argv[])
{
int i;

    for (i=1;i<argc;i++) {
	if ( strcmp(argv[i],"-display")==0 ) {
	    if ( ++i<argc )	dpyName= argv[i];
	    else {
		fprintf(stderr,"Must specify a display with -display option\n");
		return 0;
	    }
	}
	else if ((strcmp(argv[i],"-help")==0) || (strcmp(argv[i],"-usage")==0)){
	    return 0;
	}
	else if ( strcmp(argv[i],"-synch")==0 ) {
	    synch= 1;
	}
	else if ( strcmp(argv[i],"-force")==0 ) {
	    force= 1;
	}
	else if ( strcmp(argv[i],"-nobeep")==0 ) {
	    nobeep= 1;
	}
	else if ( strcmp(argv[i],"-dev")==0 ) {
	    if ( ++i<argc ) {
		if (sscanf(argv[i]," %i ",&devSpec)!=1) {
		    fprintf(stderr,"Device ID must be an integer\n");
		    return 0;
		}
	    }
	    else {
		fprintf(stderr,"Must specify a device ID with -dev option\n");
		return 0;
	    }
	}
	else if ( strcmp(argv[i],"-kf")==0 ) {
	    if ( ++i<argc ) {
		if (sscanf(argv[i]," %i ",&id)!=1) {
		    fprintf(stderr,"Keyboard feedback ID must be an integer\n");
		    return 0;
		}
		class= KbdFeedbackClass;
	    }
	    else {
		fprintf(stderr,"Must specify a keyboard feedback ID for -kf\n");
		return 0;
	    }
	}
	else if ( strcmp(argv[i],"-bf")==0 ) {
	    if ( ++i<argc ) {
		if (sscanf(argv[i]," %i ",&id)!=1) {
		    fprintf(stderr,"Bell feedback ID must be an integer\n");
		    return 0;
		}
		class= BellFeedbackClass;
	    }
	    else {
		fprintf(stderr,"Must specify a bell feedback ID for -bf\n");
		return 0;
	    }
	}
	else if ( strcmp(argv[i],"-v")==0 ) {
	    if ( ++i<argc ) {
		if ((sscanf(argv[i]," %i ",&volume)!=1)||
				(volume<-100)||(volume>100)) {
		    fprintf(stderr,"Volume must be in the range -100..100\n");
		    return 0;
		}
	    }
	    else {
		fprintf(stderr,"Must specify volume for -v\n");
		return 0;
	    }
	}
	else if ( strcmp(argv[i],"-w")==0 ) {
	    if ( ++i<argc ) {
		if (sscanf(argv[i]," %i ",&win)!=1) {
		    fprintf(stderr,"Must specify a numeric window ID\n");
		    return 0;
		}
	    }
	    else {
		fprintf(stderr,"Must specify a window ID for -w\n");
		return 0;
	    }
	}
	else {
	    if ( i<argc-1 ) {
		fprintf(stderr,"Bell name must be the last argument\n");
		return 0;
	    }
	    bellName= argv[i];
	}
    }
    return 1;
}

int
main(int argc, char *argv[])
{
Display	*dpy;
int	i1,i2,i3,i4,i5;

  
    if (!parseArgs(argc,argv)) {
	fprintf(stderr,"Usage: %s [ <options> ] <name>\n",argv[0]);
	fprintf(stderr,"Where legal options are:\n");
	fprintf(stderr,"-help              print this message\n");
	fprintf(stderr,"-usage             print this message\n");
	fprintf(stderr,"-display <dpy>     specifies display to use\n");
	fprintf(stderr,"-synch             turn on synchronization\n");
	fprintf(stderr,"-dev <id>          specifies device to use\n");
	fprintf(stderr,"-force             force audible bell\n");
	fprintf(stderr,"-nobeep            suppress server bell, event only\n");
	fprintf(stderr,"-bf <id>           specifies bell feedback to use\n");
	fprintf(stderr,"-kf <id>           specifies keyboard feedback to use\n");
	fprintf(stderr,"-v <volume>        specifies volume to use\n");
	fprintf(stderr,"-w <id>            specifies window to use\n");
	fprintf(stderr,"If neither device nor feedback are specified, %s uses the\n",argv[0]);
	fprintf(stderr,"default values for the core keyboard device.\n");
	return 1;
    }
    dpy = XOpenDisplay(dpyName);
    if ( !dpy ) {
	fprintf(stderr,"Couldn't open display \"%s\"\n",XDisplayName(dpyName));
	return 1;
    }
    if (synch)
	XSynchronize(dpy,1);
    i1= XkbMajorVersion;
    i2= XkbMinorVersion;
    if ( !XkbLibraryVersion(&i1,&i2) ) {
	fprintf(stderr,"Warning! X library built with XKB version %d.%02d\n",
									i1,i2);
	fprintf(stderr,"         but %s was built with %d.%02d\n",argv[0],
					XkbMajorVersion,XkbMinorVersion);
	fprintf(stderr,"         Trying anyway\n");
    }
    if ( !XkbQueryExtension(dpy,&i1,&i2,&i3,&i4,&i5)>0 ) {
	if ((i4!=0)||(i5!=0))
	    fprintf(stderr,"server supports incompatible XKB version %d.%02d\n",
									i4,i5);
	else fprintf(stderr,"XkbQueryExtension failed\n");
	fprintf(stderr,"Trying anyway\n");
    }
    if (force && (nameAtom!=None))
	fprintf(stderr,"Warning! Name ignored for forced bell requests\n");
    if (bellName!='\0')
	nameAtom = XInternAtom(dpy,bellName,0);
    if ((devSpec==XkbUseCoreKbd)&&(class<0)) {
	Bool ok;
	if (force)		ok= XkbForceBell(dpy,volume);
	else if (nobeep)	ok= XkbBellEvent(dpy,win,volume,nameAtom);
	else			ok= XkbBell(dpy,win,volume,nameAtom);
	if (!ok)
	    fprintf(stderr,"XkbBell request failed\n");
    }
    else {
	Bool ok;
	if (class<0)	class= KbdFeedbackClass;
	if (id<0)		id= 0;
	if (force)
	     ok= XkbForceDeviceBell(dpy,devSpec,class,id,volume);
	else if (nobeep)
	     ok= XkbDeviceBellEvent(dpy,win,devSpec,class,id,volume,nameAtom);
	else ok= XkbDeviceBell(dpy,win,devSpec,class,id,volume,nameAtom);
	if (!ok)
	    fprintf(stderr,"XkbDeviceBell request failed\n");
    }
/* BAIL: */
    XCloseDisplay(dpy);
    return 0;
}
