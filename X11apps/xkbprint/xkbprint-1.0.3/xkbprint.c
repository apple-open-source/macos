/* $Xorg: xkbprint.c,v 1.4 2000/08/17 19:54:50 cpqbld Exp $ */
/************************************************************
 Copyright (c) 1995 by Silicon Graphics Computer Systems, Inc.

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
/* $XFree86: xc/programs/xkbprint/xkbprint.c,v 3.10 2003/05/27 22:27:07 tsi Exp $ */

#include <stdio.h>
#include <ctype.h>
#include <X11/Xlocale.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKBgeom.h>
#include <X11/extensions/XKM.h>
#include <X11/extensions/XKBfile.h>
#include <X11/keysym.h>

#if defined(sgi)
#include <malloc.h>
#endif

#define	DEBUG_VAR_NOT_LOCAL
#define	DEBUG_VAR debugFlags
#include <stdlib.h>

#include "utils.h"
#include "xkbprint.h"

/***====================================================================***/


#define	WANT_DEFAULT	0
#define	WANT_PS_FILE	1
#define	WANT_X_SERVER	2

#define	INPUT_UNKNOWN	0
#define	INPUT_XKB	1
#define	INPUT_XKM	2

#ifdef notyet
static char *fileTypeExt[] = {
	"XXX",
	"xkm",
	"xkb"
};
#endif

static	unsigned	outputFormat= WANT_DEFAULT;
static	char *		wantLocale= "C";
static	char *		rootDir;
static	char *		inputFile;
static	char *		outputFile;
static	char *		outputFont= NULL;
static	char *		inDpyName,*outDpyName;
static	Display *	inDpy;
static	Display *	outDpy;
#ifdef NOTYET
static	Bool		computeDflts= False;
#endif
static	XKBPrintArgs	args;
static	unsigned	warningLevel= 5;
static	Bool		synch;

/***====================================================================***/

static void
Usage(int argc, char *argv[])
{
    fprintf(stderr,"Usage: %s [options] input-file [ output-file ]\n",argv[0]);
    fprintf(stderr,"Legal options:\n");
    fprintf(stderr,"-?,-help      Print this message\n");
    fprintf(stderr,"-color        Use colors from geometry\n");
#ifdef DEBUG
    fprintf(stderr,"-d [flags]    Report debugging information\n");
#endif
    fprintf(stderr,"-dflts        Compute defaults for missing components\n");
    fprintf(stderr,"-diffs        Only show explicit key definitions\n");
    fprintf(stderr,"-eps          Generate an EPS file\n");
    fprintf(stderr,"-fit          Fit keyboard image on page (default)\n");
    fprintf(stderr,"-full         Print keyboard image full sized\n");
    fprintf(stderr,"-grid <n>     Print a grid with <n> mm resolution\n");
    fprintf(stderr,"-if <name>    Specifies the name of an internal font to dump\n");
#ifdef DEBUG
    fprintf(stderr,"-I[<dir>]     Specifies a top level directory\n");
    fprintf(stderr,"              for include directives.  You can\n");
    fprintf(stderr,"              specify multiple directories.\n");
#endif
    fprintf(stderr,"-kc           Also print keycodes, if possible\n");
    fprintf(stderr,"-label <what> Specifies the label to be drawn on keys\n");
    fprintf(stderr,"              Legal values for <what> are:\n");
    fprintf(stderr,"                  none,name,code,symbols\n");
    fprintf(stderr,"-lc <locale>  Use <locale> for fonts and symbols\n");
    fprintf(stderr,"-level1       Use level 1 PostScript (default)\n");
    fprintf(stderr,"-level2       Use level 2 PostScript\n");
    fprintf(stderr,"-lg <num>     Use keyboard group <num> to print labels\n");
    fprintf(stderr,"-ll <num>     Use shift level <num> to print labels\n");
    fprintf(stderr,"-mono         Ignore colors from geometry (default)\n");
    fprintf(stderr,"-n <num>      Print <num> copies (default 1)\n");
    fprintf(stderr,"-nkg <num>    Number of groups to print on each key\n");
    fprintf(stderr,"-nokc         Don't print keycodes, even if possible\n");
    fprintf(stderr,"-npk <num>    Number of keyboards to print on each page\n");
    fprintf(stderr,"-ntg <num>    Total number of groups to print\n");
    fprintf(stderr,"-o <file>     Specifies output file name\n");
    fprintf(stderr,"-R[<DIR>]     Specifies the root directory for relative\n");
    fprintf(stderr,"              path names\n");
    fprintf(stderr,"-pict <what>  Specifies use of pictographs instead of\n");
    fprintf(stderr,"              keysym names where available, <what> can\n");
fprintf(stderr,"              be \"all\", \"none\" or \"common\" (default)\n");
    fprintf(stderr,"-synch        Force synchronization\n");
    fprintf(stderr,"-w <lvl>      Set warning level (0=none, 10=all)\n");
}

/***====================================================================***/

static Bool
parseArgs(int argc, char *argv[])
{
register int i;

    args.copies=	1;
    args.grid=		0;
    args.level1=	True;
    args.scaleToFit=	True;
    args.wantColor=	False;
    args.wantSymbols=	COMMON_SYMBOLS;
    args.wantKeycodes=	True;
    args.wantDiffs=	False;
    args.wantEPS=	False;
    args.label=		LABEL_AUTO;
    args.baseLabelGroup=0;
    args.nLabelGroups=	1;
    args.nTotalGroups=	0;
    args.nKBPerPage=	0;
    args.labelLevel=	0;
    for (i=1;i<argc;i++) {
	if ((argv[i][0]!='-')||(uStringEqual(argv[i],"-"))) {
	    if (inputFile==NULL) {
		inputFile= argv[i];
	    }
	    else if (outputFile==NULL) {
		outputFile= argv[i];
	    }
	    else {
		uWarning("Too many file names on command line\n");
		uAction("Compiling %s, writing to %s, ignoring %s\n",
					inputFile,outputFile,argv[i]);
	    }
	}
	else if ((strcmp(argv[i],"-?")==0)||(strcmp(argv[i],"-help")==0)) {
	    Usage(argc,argv);
	    exit(0);
	}
	else if (strcmp(argv[i],"-color")==0) {
	    args.wantColor= True;
	}
#ifdef DEBUG
	else if (strcmp(argv[i],"-d")==0) {
	    if ((i>=(argc-1))||(!isdigit(argv[i+1][0]))) {
		debugFlags= 1;
	    }
	    else {
		sscanf(argv[++i],"%i",&debugFlags);
	    }
	    uInformation("Setting debug flags to %d\n",debugFlags);
	}
#endif
	else if (strcmp(argv[i],"-dflts")==0) {
#ifdef NOTYET
	    computeDflts= True;
#endif
	    uWarning("Compute defaults not implemented yet\n");
	}
	else if (strcmp(argv[i],"-diffs")==0) {
	    args.wantDiffs= True;
	}
	else if (strcmp(argv[i],"-eps")==0) {
	    args.wantEPS= True;
	}
	else if (strcmp(argv[i],"-fit")==0) {
	    args.scaleToFit= True;
	}
	else if (strcmp(argv[i],"-full")==0) {
	    args.scaleToFit= False;
	}
	else if (strcmp(argv[i],"-grid")==0) {
	    int tmp;
	    if (++i>=argc) {
		uWarning("Grid frequency not specified\n");
		uAction("Trailing \"-grid\" option ignored\n");
	    }
	    else if ((sscanf(argv[i],"%i",&tmp)!=1)||(tmp<1)) {
		uWarning("Grid frequency must be an integer > zero\n");
		uAction("Illegal frequency %d ignored\n",tmp);
	    }	 
	    else args.grid= tmp;
	}
#ifdef NOTYET
	else if (strncmp(argv[i],"-I",2)==0) {
	    if (!XkbAddDirectoryToPath(&argv[i][2])) {
		uAction("Exiting\n");
		exit(1);
	    }
	    uInternalError("Includes not implemented yet\n");
	}
#endif
	else if (strcmp(argv[i],"-if")==0) {
	    if (++i>=argc) {
		uWarning("Internal Font name not specified\n");
		uAction("Trailing \"-if\" option ignored\n");
	    }
	    else outputFont= argv[i];
	}
	else if (strcmp(argv[i],"-kc")==0) {
	    args.wantKeycodes= True;
	}
	else if (strcmp(argv[i],"-label")==0) {
	    if (++i>=argc) {
		uWarning("Label type not specified\n");
		uAction("Trailing \"-label\" option ignored\n");
	    }
	    else if (uStrCaseEqual(argv[i],"none")) 
		args.label= LABEL_NONE;
	    else if (uStrCaseEqual(argv[i],"name")) 
		args.label= LABEL_KEYNAME;
	    else if (uStrCaseEqual(argv[i],"code")) 
		args.label= LABEL_KEYCODE;
	    else if (uStrCaseEqual(argv[i],"symbols")) 
		args.label= LABEL_SYMBOLS;
	    else {
		uWarning("Unknown label type \"%s\" specified\n",argv[i]);
		uAction("Ignored\n");
	    }
	}
	else if (strcmp(argv[i],"-lc")==0) {
	    if (++i>=argc) {
		uWarning("Locale not specified\n");
		uAction("Trailing \"-lc\" option ignored\n");
	    }
	    else wantLocale= argv[i];
	}
	else if (strcmp(argv[i],"-lg")==0) {
	    int tmp;
	    if (++i>=argc) {
		uWarning("Label group not specified\n");
		uAction("Trailing \"-lg\" option ignored\n");
	    }
	    else if ((sscanf(argv[i],"%i",&tmp)!=1)||(tmp<1)||(tmp>4)) {
		uWarning("Label group must be an integer in the range 1..4\n");
		uAction("Illegal group %d ignored\n",tmp);
	    }	 
	    else args.baseLabelGroup= tmp-1;
	}
	else if (strcmp(argv[i],"-ll")==0) {
	    int tmp;
	    if (++i>=argc) {
		uWarning("Label level not specified\n");
		uAction("Trailing \"-ll\" option ignored\n");
	    }
	    else if ((sscanf(argv[i],"%i",&tmp)!=1)||(tmp<1)||(tmp>255)) {
		uWarning("Label level must be in the range 1..255\n");
		uAction("Illegal level %d ignored\n",tmp);
	    }	 
	    else args.labelLevel= tmp-1;
	}
	else if (strcmp(argv[i],"-level1")==0)
	    args.level1= True;
	else if (strcmp(argv[i],"-level2")==0)
	    args.level1= False;
	else if (strcmp(argv[i],"-mono")==0) {
	    args.wantColor= False;
	}
	else if (strcmp(argv[i],"-n")==0) {
	    int tmp;
	    if (++i>=argc) {
		uWarning("Number of copies not specified\n");
		uAction("Trailing \"-n\" option ignored\n");
	    }
	    else if ((sscanf(argv[i],"%i",&tmp)!=1)||(tmp<1)) {
		uWarning("Number of copies must be an integer > zero\n");
		uAction("Illegal count %d ignored\n",tmp);
	    }	 
	    else args.copies= tmp;
	}
	else if (strcmp(argv[i],"-nokc")==0) {
	    args.wantKeycodes= False;
	}
	else if (strcmp(argv[i],"-nkg")==0) {
	    int tmp;
	    if (++i>=argc) {
		uWarning("Number of groups per key not specified\n");
		uAction("Trailing \"-nkg\" option ignored\n");
	    }
	    else if ((sscanf(argv[i],"%i",&tmp)!=1)||(tmp<1)||(tmp>2)) {
		uWarning("Groups per key must be in the range 1..2\n");
		uAction("Illegal number of groups %d ignored\n",tmp);
	    }	 
	    else args.nLabelGroups= tmp;
	}
	else if (strcmp(argv[i],"-npk")==0) {
	    int tmp;
	    if (++i>=argc) {
		uWarning("Number of keyboards per page not specified\n");
		uAction("Trailing \"-npk\" option ignored\n");
	    }
	    else if ((sscanf(argv[i],"%i",&tmp)!=1)||(tmp<1)||(tmp>2)) {
		uWarning("Keyboards per page must be in the range 1..2\n");
		uAction("Illegal number of keyboards %d ignored\n",tmp);
	    }	 
	    else args.nKBPerPage= tmp;
	}
	else if (strcmp(argv[i],"-ntg")==0) {
	    int tmp;
	    if (++i>=argc) {
		uWarning("Total number of groups not specified\n");
		uAction("Trailing \"-ntg\" option ignored\n");
	    }
	    else if ((sscanf(argv[i],"%i",&tmp)!=1)||(tmp<1)||(tmp>4)) {
		uWarning("Total number of groups must be in the range 1..4\n");
		uAction("Illegal number of groups %d ignored\n",tmp);
	    }	 
	    else args.nTotalGroups= tmp;
	}
	else if (strcmp(argv[i],"-o")==0) {
	    if (++i>=argc) {
		uWarning("No output file specified\n");
		uAction("Trailing \"-o\" option ignored\n");
	    }
	    else if (outputFile!=NULL) {
		uWarning("Multiple output files specified\n");
		uAction("Compiling %s, ignoring %s\n",outputFile,argv[i]);
	    }
	    else outputFile= argv[i];
	}
	else if (strncmp(argv[i],"-R",2)==0) {
	    if (argv[i][2]=='\0') {
		uWarning("No root directory specified\n");
		uAction("Ignoring -R option\n");
	    }
	    else if (rootDir!=NULL) {
		uWarning("Multiple root directories specified\n");
		uAction("Using %s, ignoring %s\n",rootDir,argv[i]);
	    }
	    else rootDir= &argv[i][2];
	}
	else if (strcmp(argv[i],"-pict")==0) {
	    if (++i>=argc) {
		uWarning("No level of pictographs specified\n");
		uAction("Trailing \"-pict\" option ignored\n");
	    }
	    else if (strcmp(argv[i],"none")==0)
		args.wantSymbols= NO_SYMBOLS;
	    else if (strcmp(argv[i],"common")==0)
		args.wantSymbols= COMMON_SYMBOLS;
	    else if (strcmp(argv[i],"all")==0)
		args.wantSymbols= ALL_SYMBOLS;
	    else if (outputFile!=NULL) {
		uWarning("Unknown pictograph level specified\n");
		uAction("Ignoring illegal value %s\n",argv[i]);
	    }
	}
	else if ((strcmp(argv[i],"-synch")==0)||(strcmp(argv[i],"-s")==0)) {
	    synch= True;
	}
	else if (strcmp(argv[i],"-w")==0) {
	    if ((i>=(argc-1))||(!isdigit(argv[i+1][0]))) {
		warningLevel= 0;
	    }
	    else {
		int itmp;
		if (sscanf(argv[++i],"%i",&itmp))
		    warningLevel = itmp;
	    }
	}
	else {
	    uError("Unknown flag \"%s\" on command line\n",argv[i]);
	    Usage(argc,argv);
	    return False;
	}
    }
    if (rootDir) {
	if (warningLevel>8) {
	    uWarning("Changing root directory to \"%s\"\n",rootDir);
	}
	if ((chdir(rootDir)<0) && (warningLevel>0)) {
	    uWarning("Couldn't change root directory to \"%s\"\n",rootDir);
	    uAction("Root directory (-R) option ignored\n");
	}
    }
    if (outputFont!=NULL) {
	Bool  ok;
	FILE *file= NULL;

	if (outputFile==NULL) {
	    outputFile= uAlloc(strlen(outputFont)+5);
	    sprintf(outputFile,"%s.pfa",outputFont);
	}
	else if (uStringEqual(outputFile,"-"))
	    file= stdout;

	if (file==NULL)
	    file= fopen(outputFile,"w");
	   	 
	if (!file) {
	    uError("Couldn't open \"%s\" to dump internal font \"%s\"\n",
						outputFile,outputFont);
	    uAction("Exiting\n");
	    exit(1);
	}
	ok= DumpInternalFont(file,outputFont);
	if (file!=stdout)
	    fclose(file);
	if (!ok) {
	    uWarning("No internal font to dump\n");
	    if (file!=stdout) {
		uAction("Removing \"%s\"\n",outputFile);
		unlink(outputFile);
	    }
	}
	exit((ok!=0));
    }
    if (inputFile==NULL) {
	uError("No input file specified\n");
	Usage(argc,argv);
	return False;
    }
    else if (uStringEqual(inputFile,"-")) {
	/* Nothing */
    }
    else if (strchr(inputFile,':')==NULL) {
	int	len= strlen(inputFile);
	if ((len>4)&&(strcmp(&inputFile[len-4],".xkm")==0)) {
	    /* Nothing */
	}
	else {
	    FILE *file;
	    file= fopen(inputFile,"r");
	    if (file) {
		fclose(file);
	    }
	    else {
		fprintf(stderr,"Cannot open \"%s\" for reading\n",inputFile);
		return False;
	    }
	}
    }
    else {
	inDpyName= inputFile;
	inputFile= NULL;
    }

    if (outputFormat==WANT_DEFAULT)
	outputFormat= WANT_PS_FILE;
    if ((outputFile==NULL)&&(inputFile!=NULL)&&uStringEqual(inputFile,"-")) {
	int len;
	len= strlen("stdin.eps")+2;
	outputFile= uTypedCalloc(len,char);
	if (outputFile==NULL) {
	    uInternalError("Cannot allocate space for output file name\n");
	    uAction("Exiting\n");
	    exit(1);
	}
	if (args.wantEPS)	sprintf(outputFile,"stdin.eps");
	else			sprintf(outputFile,"stdin.ps");
    }
    else if ((outputFile==NULL)&&(inputFile!=NULL)) {
	int len;
	char *base,*ext;

	base= strrchr(inputFile,'/');
	if (base==NULL)	base= inputFile;
	else		base++;

	len= strlen(base)+strlen("eps")+2;
	outputFile= uTypedCalloc(len,char);
	if (outputFile==NULL) {
	    uInternalError("Cannot allocate space for output file name\n");
	    uAction("Exiting\n");
	    exit(1);
	}
	ext= strrchr(base,'.');
	if (ext==NULL) {
	    if (args.wantEPS)	sprintf(outputFile,"%s.eps",base);
	    else		sprintf(outputFile,"%s.ps",base);
	}
	else {
	    strcpy(outputFile,base);
	    if (args.wantEPS)	strcpy(&outputFile[ext-base+1],"eps");
	    else		strcpy(&outputFile[ext-base+1],"ps");
	}
    }
    else if (outputFile==NULL) {
	int len;
	char *ch,*name,buf[128];
	if (inDpyName[0]==':')	
	     sprintf(name=buf,"server%s",inDpyName);
	else name= inDpyName;

	len= strlen(name)+strlen("eps")+2;
	outputFile= uTypedCalloc(len,char);
	if (outputFile==NULL) {
	    uInternalError("Cannot allocate space for output file name\n");
	    uAction("Exiting\n");
	    exit(1);
	}
	strcpy(outputFile,name);
	for (ch=outputFile;(*ch)!='\0';ch++) {
	    if 	(*ch==':')	*ch= '-';
	    else if (*ch=='.')	*ch= '_';
	}
	*ch++= '.';
	if (args.wantEPS)	strcpy(ch,"eps");
	else			strcpy(ch,"ps");
    }
    else if (strchr(outputFile,':')!=NULL) {
	outDpyName= outputFile;
	outputFile= NULL;
	outputFormat= WANT_X_SERVER;
	uInternalError("Output to an X server not implemented yet\n");
	return False;
    }
    return True;
}

static Display *
GetDisplay(char *program, char *dpyName)
{
int	mjr,mnr,error;
Display	*dpy;

    mjr= XkbMajorVersion;
    mnr= XkbMinorVersion;
    dpy= XkbOpenDisplay(dpyName,NULL,NULL,&mjr,&mnr,&error);
    if (dpy==NULL) {
	switch (error) {
	    case XkbOD_BadLibraryVersion:
		uInformation("%s was compiled with XKB version %d.%02d\n",
				program,XkbMajorVersion,XkbMinorVersion);
		uError("X library supports incompatible version %d.%02d\n",
				mjr,mnr);
		break;
	    case XkbOD_ConnectionRefused:
		uError("Cannot open display \"%s\"\n",dpyName);
		break;
	    case XkbOD_NonXkbServer:
		uError("XKB extension not present on %s\n",dpyName);
		break;
	    case XkbOD_BadServerVersion:
		uInformation("%s was compiled with XKB version %d.%02d\n",
				program,XkbMajorVersion,XkbMinorVersion);
		uError("Server %s uses incompatible version %d.%02d\n",
				dpyName,mjr,mnr);
		break;
	    default:
		uInternalError("Unknown error %d from XkbOpenDisplay\n",error);
	}
    }
    else if (synch)
	XSynchronize(dpy,True);
    return dpy;
}

/***====================================================================***/

#ifdef notyet
#define MAX_INCLUDE_OPTS	10
static char	*includeOpt[MAX_INCLUDE_OPTS];
static int	numIncludeOpts = 0;
#endif

int
main(int argc, char *argv[])
{
FILE 	*	file;
int		ok;
XkbFileInfo 	result;

    uSetEntryFile(NullString);
    uSetDebugFile(NullString);
    uSetErrorFile(NullString);
    if (!parseArgs(argc,argv))
	exit(1);
#ifdef DEBUG
#ifdef sgi
    if (debugFlags&0x4)
	mallopt(M_DEBUG,1);
#endif
#endif
    file= NULL;
    XkbInitAtoms(NULL);
/*     XkbInitIncludePath(); */
    if (inputFile!=NULL) {
	if (uStringEqual(inputFile,"-")) {
	    static char *in= "stdin";
	    file= stdin;
	    inputFile= in;
	}
	else {
	    file= fopen(inputFile,"r");
	}
    }
    else if (inDpyName!=NULL) {
	inDpy= GetDisplay(argv[0],inDpyName);
	if (!inDpy) {
	    uAction("Exiting\n");
	    exit(1);
	}
    }
    if (outDpyName!=NULL) {
	uInternalError("Output to an X server not implemented yet\n");
	outDpy= GetDisplay(argv[0],outDpyName);
	if (!outDpy) {
	    uAction("Exiting\n");
	    exit(1);
	}
    }
    if ((inDpy==NULL) && (outDpy==NULL)) {
	int	mjr,mnr;
	mjr= XkbMajorVersion;
	mnr= XkbMinorVersion;
	if (!XkbLibraryVersion(&mjr,&mnr)) {
	    uInformation("%s was compiled with XKB version %d.%02d\n",
				argv[0],XkbMajorVersion,XkbMinorVersion);
	    uError("X library supports incompatible version %d.%02d\n",
				mjr,mnr);
	    uAction("Exiting\n");
	    exit(1);
	}
    }
    ok= True;
    if (file) {
	unsigned tmp;
	bzero((char *)&result,sizeof(result));
	if ((result.xkb= XkbAllocKeyboard())==NULL) {
	    uFatalError("Cannot allocate keyboard description\n");
	    /* NOTREACHED */
	}
	tmp= XkmReadFile(file,XkmGeometryMask,XkmKeymapLegal,&result);
	if ((tmp&XkmGeometryMask)!=0) {
	    uError("Couldn't read geometry from XKM file \"%s\"\n",inputFile);
	    uAction("Exiting\n");
	    ok= False;
	}
	if ((tmp&XkmKeyNamesMask)!=0)
	    args.wantKeycodes= False;
	if (args.label==LABEL_AUTO) {
	    if (result.defined&XkmSymbolsMask)
		 args.label= LABEL_SYMBOLS;
	    else if (result.defined&XkmKeyNamesMask)	
		 args.label= LABEL_KEYCODE;
	    else args.label= LABEL_KEYNAME;
	}
	else if ((args.label==LABEL_KEYCODE)&&((tmp&XkmKeyNamesMask)!=0)) {
	    uError("XKM file \"%s\" doesn't have keycodes\n",inputFile);
	    uAction("Cannot label keys as requested. Exiting\n");
	    ok= False;
	}
	else if ((args.label==LABEL_SYMBOLS)&&((tmp&XkmSymbolsMask)!=0)) {
	    uError("XKM file \"%s\" doesn't have symbols\n",inputFile);
	    uAction("Cannot label keys as requested. Exiting\n");
	    ok= False;
	}
    }
    else if (inDpy!=NULL) {
	bzero((char *)&result,sizeof(result));
	result.type= XkmKeymapFile;
	result.xkb= XkbGetMap(inDpy,XkbAllMapComponentsMask,XkbUseCoreKbd);
	if (result.xkb==NULL)
	    uWarning("Cannot load keyboard description\n");
	if (XkbGetNames(inDpy,XkbAllNamesMask,result.xkb)!=Success)
	    uWarning("Cannot load names\n");
	if (XkbGetGeometry(inDpy,result.xkb)!=Success) {
	    uFatalError("Cannot load geometry for %s\n",inDpyName);
	}
#ifdef NOTYET
	if (computeDflts)
	     ok= (ComputeKbdDefaults(result.xkb)==Success);
	else ok= True;
#endif
	if (args.label==LABEL_AUTO) 
	    args.label= LABEL_SYMBOLS;
    }
    else {
	fprintf(stderr,"Cannot open \"%s\" to read geometry\n",inputFile);
	ok= 0;
    }
    if (ok) {
	FILE *out = NULL;
	if (setlocale(LC_ALL,(wantLocale))==NULL) {
	    if (wantLocale!=NULL) {
		uWarning("Couldn't change to locale %s\n",wantLocale);
		uAction("Using \"C\" locale, instead\n");
	    }
	}
	/* need C numerics so decimal point doesn't get screwed up */
	setlocale(LC_NUMERIC,"C");
	if ((inDpy!=outDpy)&&
	    (XkbChangeKbdDisplay(outDpy,&result)!=Success)) {
	    uInternalError("Error converting keyboard display from %s to %s\n",
	    						inDpyName,outDpyName);
	    exit(1);
	}
	if (outputFile!=NULL) {
	    if (uStringEqual(outputFile,"-")) {
		static char *of= "stdout";
		out= stdout;
		outputFile= of;
	    }
	    else {
		out= fopen(outputFile,"w");
		if (out==NULL) {
		    uError("Cannot open \"%s\" to write keyboard description\n",
								outputFile);
		     uAction("Exiting\n");
		     exit(1);
		}
	    }
	}
	switch (outputFormat) {
	    case WANT_PS_FILE:
		ok= GeometryToPostScript(out,&result,&args);
		break;
	    case WANT_X_SERVER:
		uInternalError("Output to X server not implemented yet\n");
		break;
	    default:
		uInternalError("Unknown output format %d\n",outputFormat);
		uAction("No output file created\n");
		ok= False;
		break;
	}
	if (!ok) {
	    uError("Error creating output file\n");
	}
    }
    if (inDpy) 
	XCloseDisplay(inDpy);
    inDpy= NULL;
    if (outDpy)
	XCloseDisplay(outDpy);
    return (ok==0);
}
