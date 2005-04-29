/************************************************************
 Copyright (c) 1996 by Silicon Graphics Computer Systems, Inc.

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
/* $XFree86: xc/programs/setxkbmap/setxkbmap.c,v 3.8 2003/11/17 22:20:50 dawes Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <limits.h>
#include <ctype.h>
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKBfile.h>
#include <X11/extensions/XKBconfig.h>
#include <X11/extensions/XKBrules.h>

#ifndef PATH_MAX
#ifdef MAXPATHLEN
#define PATH_MAX MAXPATHLEN
#else
#define PATH_MAX 1024
#endif
#endif

#ifndef DFLT_XKB_CONFIG_ROOT
#define	DFLT_XKB_CONFIG_ROOT "/usr/X11R6/lib/X11/xkb"
#endif
#ifndef DFLT_XKB_RULES_FILE
#define	DFLT_XKB_RULES_FILE "xfree86"
#endif
#ifndef DFLT_XKB_LAYOUT
#define	DFLT_XKB_LAYOUT "us"
#endif
#ifndef DFLT_XKB_MODEL
#define	DFLT_XKB_MODEL "pc101"
#endif

#define	UNDEFINED	0
#define	FROM_SERVER	1
#define	FROM_RULES	2
#define	FROM_CONFIG	3
#define	FROM_CMD_LINE	4
#define	NUM_SOURCES	5

#define	RULES_NDX	0
#define	CONFIG_NDX	1
#define	DISPLAY_NDX	2
#define	LOCALE_NDX	3
#define	MODEL_NDX	4
#define	LAYOUT_NDX	5
#define	VARIANT_NDX	6
#define KEYCODES_NDX	7
#define	TYPES_NDX	8
#define	COMPAT_NDX	9
#define	SYMBOLS_NDX	10
#define	GEOMETRY_NDX	11
#define	KEYMAP_NDX	12
#define	NUM_STRING_VALS	13

/***====================================================================***/
Bool			print= False;
Bool			synch= False;
int			verbose= 5;

Display *		dpy;

char *	srcName[NUM_SOURCES] = {
	"undefined", "X server", "rules file", "config file", "command line"
};

char *	svName[NUM_STRING_VALS]= {
	"rules file", "config file", "X display", "locale",
	"keyboard model", "keyboard layout", "layout variant",
	"keycodes", "types", "compatibility map", "symbols", "geometry",
	"keymap"
};
int	svSrc[NUM_STRING_VALS];
char *	svValue[NUM_STRING_VALS];

XkbConfigFieldsRec	cfgDflts;
XkbConfigRtrnRec	cfgResult;

XkbRF_RulesPtr		rules= NULL;
XkbRF_VarDefsRec	rdefs;

Bool			clearOptions= False;
int			szOptions= 0;
int			numOptions= 0;
char **			options= NULL;

int			szInclPath= 0;
int			numInclPath= 0;
char **			inclPath= NULL;

XkbDescPtr		xkb= NULL;

/***====================================================================***/

#define	streq(s1,s2)	(strcmp(s1,s2)==0)
#define	strpfx(s1,s2)	(strncmp(s1,s2,strlen(s2))==0)

#define	MSG(s)		printf(s)
#define	MSG1(s,a)	printf(s,a)
#define	MSG2(s,a,b)	printf(s,a,b)
#define	MSG3(s,a,b,c)	printf(s,a,b,c)

#define	VMSG(l,s)	if (verbose>(l)) printf(s)
#define	VMSG1(l,s,a)	if (verbose>(l)) printf(s,a)
#define	VMSG2(l,s,a,b)	if (verbose>(l)) printf(s,a,b)
#define	VMSG3(l,s,a,b,c) if (verbose>(l)) printf(s,a,b,c)

#define	ERR(s)		fprintf(stderr,s)
#define	ERR1(s,a)	fprintf(stderr,s,a)
#define	ERR2(s,a,b)	fprintf(stderr,s,a,b)
#define	ERR3(s,a,b,c)	fprintf(stderr,s,a,b,c)

/***====================================================================***/

Bool addToList ( int *sz, int *num, char ***listIn, char *newVal );
void usage ( int argc, char ** argv );
void dumpNames ( Bool wantRules, Bool wantCNames );
void trySetString ( int which, char * newVal, int src );
Bool setOptString ( int *arg, int argc, char **argv, int which, int src );
int parseArgs ( int argc, char ** argv );
Bool getDisplay ( int argc, char ** argv );
Bool getServerValues ( void );
FILE * findFileInPath ( char * name, char * subdir );
Bool addStringToOptions ( char * opt_str, int * sz_opts, int * num_opts, char *** opts );
char * stringFromOptions ( char * orig, int numNew, char ** newOpts );
Bool applyConfig ( char * name );
Bool applyRules ( void );
Bool applyComponentNames ( void );
void printKeymap( void );

/***====================================================================***/

Bool
addToList(int *sz,int *num,char ***listIn,char *newVal)
{
register int i;
char **list;

    if ((!newVal)||(!newVal[0])) {
	*num= 0;
	return True;
    }
    list= *listIn;
    for (i=0;i<*num;i++) {
	if (streq(list[i],newVal))
	    return True;
    }
    if ((list==NULL)||(*sz<1)) {
	*num= 0;
	*sz= 4;
	list= (char **)calloc(*sz,sizeof(char *));
	*listIn= list;
    }
    else if (*num>=*sz) {
	*sz*= 2;
	list= (char **)realloc(list,(*sz)*sizeof(char *));
	*listIn= list;
    }
    if (!list) {
	ERR("Internal Error! Allocation failure in add to list!\n");
	ERR("                Exiting.\n");
	exit(-1);
    }
    list[*num]= strdup(newVal);
    (*num)= (*num)+1;
    return True;
}

/***====================================================================***/

void
usage(int argc,char **argv)
{
    MSG1("Usage: %s [args] [<layout> [<variant> [<option> ... ]]]\n",argv[0]);
    MSG("Where legal args are:\n");
    MSG("-?,-help            Print this message\n");
    MSG("-compat <name>      Specifies compatibility map component name\n");
    MSG("-config <file>      Specifies configuration file to use\n");
    MSG("-display <dpy>      Specifies display to use\n");
    MSG("-geometry <name>    Specifies geometry component name\n");
    MSG("-I[<dir>]           Add <dir> to list of directories to be used\n");
    MSG("-keycodes <name>    Specifies keycodes component name\n");
    MSG("-keymap <name>      Specifies name of keymap to load\n");
    MSG("-layout <name>      Specifies layout used to choose component names\n");
    MSG("-model <name>       Specifies model used to choose component names\n");
    MSG("-option <name>      Adds an option used to choose component names\n");
    MSG("-print              Print a complete xkb_keymap description and exit\n");
    MSG("-rules <name>       Name of rules file to use\n");
    MSG("-symbols <name>     Specifies symbols component name\n");
    MSG("-synch              Synchronize request w/X server\n");
    MSG("-types <name>       Specifies types component name\n");
    MSG("-v[erbose] [<lvl>]  Sets verbosity (1..10).  Higher values yield\n");
    MSG("                    more messages\n");
    MSG("-variant <name>     Specifies layout variant used to choose component names\n");
}

void
dumpNames(Bool wantRules,Bool wantCNames)
{
    if (wantRules) {
	if (svValue[MODEL_NDX])	 MSG1("model:      %s\n",svValue[MODEL_NDX]);
	if (svValue[LAYOUT_NDX])  MSG1("layout:     %s\n",svValue[LAYOUT_NDX]);
	if (svValue[VARIANT_NDX]) MSG1("variant:    %s\n",svValue[VARIANT_NDX]);
	if (options) {
	   char *opt_str=stringFromOptions(NULL, numOptions, options);
	   MSG1("options:    %s\n", opt_str);
	   free(opt_str);
	}
    }
    if (wantCNames) {
	if (svValue[KEYMAP_NDX])
	    MSG1("keymap:     %s\n",svValue[KEYMAP_NDX]);
	if (svValue[KEYCODES_NDX])
	    MSG1("keycodes:   %s\n",svValue[KEYCODES_NDX]);
	if (svValue[TYPES_NDX])
	    MSG1("types:      %s\n",svValue[TYPES_NDX]);
	if (svValue[COMPAT_NDX])
	    MSG1("compat:     %s\n",svValue[COMPAT_NDX]);
	if (svValue[SYMBOLS_NDX])
	    MSG1("symbols:    %s\n",svValue[SYMBOLS_NDX]);
	if (svValue[GEOMETRY_NDX])
	    MSG1("geometry:   %s\n",svValue[GEOMETRY_NDX]);
    }
    return;
}

/***====================================================================***/

void
trySetString(int which,char *newVal,int src)
{
    if (svValue[which]!=NULL) {
	if (svSrc[which]==src) {
	    VMSG2(0,"Warning! More than one %s from %s\n",
						svName[which],srcName[src]);
	    VMSG2(0,"         Using \"%s\", ignoring \"%s\"\n",
						svValue[which],newVal);
	    return;
	}
	else if (svSrc[which]>src) {
	    VMSG1(5,"Warning! Multiple definitions of %s\n",svName[which]);
	    VMSG2(5,"         Using %s, ignoring %s\n",srcName[svSrc[which]],
						srcName[src]);
	    return;
	}
    }
    svSrc[which]= src;
    svValue[which]= newVal;
    return;
}

Bool
setOptString(int *arg,int argc,char **argv,int which,int src)
{
int	ndx;
char *	opt;

    ndx= *arg;
    opt= argv[ndx];
    if (ndx>=argc-1) {
	VMSG1(0,"No %s specified on the command line\n",svName[which]);
	VMSG1(0,"Trailing %s option ignored\n",opt);
	return True;
    }
    ndx++;
    *arg= ndx;
    if (svValue[which]!=NULL) {
	if (svSrc[which]==src) {
	    VMSG2(0,"More than one %s on %s\n",svName[which],srcName[src]);
	    VMSG2(0,"Using \"%s\", ignoring \"%s\"\n",svValue[which],argv[ndx]);
	    return True;
	}
	else if (svSrc[which]>src) {
	    VMSG1(5,"Multiple definitions of %s\n",svName[which]);
	    VMSG2(5,"Using %s, ignoring %s\n",srcName[svSrc[which]],
						srcName[src]);
	    return True;
	}
    }
    svSrc[which]= src;
    svValue[which]= argv[ndx];
    return True;
}

/***====================================================================***/

int
parseArgs(int argc,char **argv)
{
int 	i;
Bool	ok;
unsigned	present;

    ok= True;
    addToList(&szInclPath,&numInclPath,&inclPath,".");
    addToList(&szInclPath,&numInclPath,&inclPath,DFLT_XKB_CONFIG_ROOT);
    for (i=1;(i<argc)&&ok;i++) {
	if (argv[i][0]!='-') {
	    if (!svSrc[LAYOUT_NDX])
		 trySetString(LAYOUT_NDX,argv[i],FROM_CMD_LINE);
	    else if (!svSrc[VARIANT_NDX])
		 trySetString(VARIANT_NDX,argv[i],FROM_CMD_LINE);
	    else ok= addToList(&szOptions,&numOptions,&options,argv[i]);
	}
	else if (streq(argv[i],"-compat"))
	    ok= setOptString(&i,argc,argv,COMPAT_NDX,FROM_CMD_LINE);
	else if (streq(argv[i],"-config"))
	    ok= setOptString(&i,argc,argv,CONFIG_NDX,FROM_CMD_LINE);
	else if (streq(argv[i],"-display"))
	    ok= setOptString(&i,argc,argv,DISPLAY_NDX,FROM_CMD_LINE);
	else if (streq(argv[i],"-geometry"))
	    ok= setOptString(&i,argc,argv,GEOMETRY_NDX,FROM_CMD_LINE);
	else if (streq(argv[i],"-help")||streq(argv[i],"-?")) {
	    usage(argc,argv);
	    exit(0);
	}
	else if (strpfx(argv[i],"-I")) 
	    ok= addToList(&szInclPath,&numInclPath,&inclPath,&argv[i][2]);
	else if (streq(argv[i],"-keycodes"))
	    ok= setOptString(&i,argc,argv,KEYCODES_NDX,FROM_CMD_LINE);
	else if (streq(argv[i],"-keymap"))
	    ok= setOptString(&i,argc,argv,KEYMAP_NDX,FROM_CMD_LINE);
	else if (streq(argv[i],"-layout"))
	    ok= setOptString(&i,argc,argv,LAYOUT_NDX,FROM_CMD_LINE);
	else if (streq(argv[i],"-model"))
	    ok= setOptString(&i,argc,argv,MODEL_NDX,FROM_CMD_LINE);
	else if (streq(argv[i],"-option")) {
	    if ((i==argc-1)||(argv[i+1][0]=='\0')||(argv[i+1][0]=='-')) {
		clearOptions= True;
		ok= addToList(&szOptions,&numOptions,&options,"");
                if (i<argc-1 && argv[i+1][0]=='\0')
                   i++;
	    }
	    else {
		ok= addToList(&szOptions,&numOptions,&options,argv[++i]);
	    }
	}
	else if (streq(argv[i],"-print"))
	    print= True;
	else if (streq(argv[i],"-rules"))
	    ok= setOptString(&i,argc,argv,RULES_NDX,FROM_CMD_LINE);
	else if (streq(argv[i],"-symbols"))
	    ok= setOptString(&i,argc,argv,SYMBOLS_NDX,FROM_CMD_LINE);
	else if (streq(argv[i],"-synch"))
	    synch= True;
	else if (streq(argv[i],"-types"))
	    ok= setOptString(&i,argc,argv,TYPES_NDX,FROM_CMD_LINE);
	else if (streq(argv[i],"-verbose")||(streq(argv[i],"-v"))) {
	    if ((i<argc-1)&&(isdigit(argv[i+1][0])))
	 	 verbose= atoi(argv[++i]);
	    else verbose++;
	    if (verbose<0) {
		ERR1("Illegal verbose level %d.  Reset to 0\n",verbose);
		verbose=0;
	    }
	    else if (verbose>10) {
		ERR1("Illegal verbose level %d.  Reset to 10\n",verbose);
		verbose= 10;
	    }
	    VMSG1(7,"Setting verbose level to %d\n",verbose);
	}
	else if (streq(argv[i],"-variant"))
	    ok= setOptString(&i,argc,argv,VARIANT_NDX,FROM_CMD_LINE);
	else {
	    ERR1("Error!   Option \"%s\" not recognized\n",argv[i]);
	    ok= False;
	}
    }

    present= 0;
    if (svValue[TYPES_NDX])	present++;
    if (svValue[COMPAT_NDX])	present++;
    if (svValue[SYMBOLS_NDX])	present++;
    if (svValue[KEYCODES_NDX])	present++;
    if (svValue[GEOMETRY_NDX])	present++;
    if (svValue[CONFIG_NDX])	present++;
    if (svValue[MODEL_NDX])	present++;
    if (svValue[LAYOUT_NDX])	present++;
    if (svValue[VARIANT_NDX])	present++;
    if (svValue[KEYMAP_NDX] && present) {
	ERR("No other components can be specified when a keymap is present\n");
	return False;
    }
    return ok;
}

Bool
getDisplay(int argc,char **argv)
{
int	major,minor,why;

    major= XkbMajorVersion;
    minor= XkbMinorVersion;
    dpy= XkbOpenDisplay(svValue[DISPLAY_NDX],NULL,NULL,&major,&minor,&why);
    if (!dpy) {
	if (svValue[DISPLAY_NDX]==NULL)	
	    svValue[DISPLAY_NDX]= getenv("DISPLAY");
	if (svValue[DISPLAY_NDX]==NULL)
	    svValue[DISPLAY_NDX]= "default display";
	switch (why) {
	    case XkbOD_BadLibraryVersion:
		ERR3("%s was compiled with XKB version %d.%02d\n",argv[0],
					XkbMajorVersion,XkbMinorVersion);
		ERR2("Xlib supports incompatible version %d.%02d\n",
					major,minor);
		break;
	    case XkbOD_ConnectionRefused:
                ERR1("Cannot open display \"%s\"\n",svValue[DISPLAY_NDX]);
		break;
	    case XkbOD_NonXkbServer:
		ERR1("XKB extension not present on %s\n",svValue[DISPLAY_NDX]);
		break;
	    case XkbOD_BadServerVersion:
                ERR3("%s was compiled with XKB version %d.%02d\n",argv[0],
				XkbMajorVersion,XkbMinorVersion);
		ERR3("Server %s uses incompatible version %d.%02d\n",
				svValue[DISPLAY_NDX],major,minor);
		break;
	    default:
		ERR1("Unknown error %d from XkbOpenDisplay\n",why);
		break;
	}
	return False;
    }
    if (synch)
	XSynchronize(dpy,True);
    return True;
}

/***====================================================================***/

Bool
getServerValues(void)
{
XkbRF_VarDefsRec 	vd;
char *			tmp= NULL;

    if (!XkbRF_GetNamesProp(dpy,&tmp,&vd) || !tmp) {
	VMSG1(3,"Couldn't interpret %s property\n",_XKB_RF_NAMES_PROP_ATOM);
        tmp = DFLT_XKB_RULES_FILE;
        vd.model = DFLT_XKB_MODEL;
        vd.layout = DFLT_XKB_LAYOUT;
        VMSG3(3,"Use defaults: rules - '%s' model - '%s' layout - '%s'\n",
                tmp, vd.model, vd.layout);
    }
    if (tmp)
	trySetString(RULES_NDX,tmp,FROM_SERVER);
    if (vd.model)
	trySetString(MODEL_NDX,vd.model,FROM_SERVER);
    if (vd.layout)
	trySetString(LAYOUT_NDX,vd.layout,FROM_SERVER);
    if (vd.variant)
	trySetString(VARIANT_NDX,vd.variant,FROM_SERVER);
    if ((vd.options)&&(!clearOptions)) {
	addStringToOptions(vd.options,&szOptions,&numOptions,&options);
	XFree(vd.options);
    }
    return True;
}

/***====================================================================***/

FILE *
findFileInPath(char *name,char *subdir)
{
register int	i;
char		buf[PATH_MAX];
FILE *		fp;

    if (name[0]=='/') {
	fp= fopen(name,"r");
	if ((verbose>7)||((!fp)&&(verbose>0)))
	    MSG2("%s file %s\n",(fp?"Found":"Didn't find"),name);
	return fp;
    }
    for (i=0;(i<numInclPath);i++) {
	if ((strlen(inclPath[i])+strlen(subdir)+strlen(name)+2)>PATH_MAX) {
	    VMSG3(0,"Path too long (%s/%s%s). Ignored.\n",inclPath[i],subdir,
									name);
	    continue;
	}
	sprintf(buf,"%s/%s%s",inclPath[i],subdir,name);
	fp= fopen(name,"r");
	if ((verbose>7)||((!fp)&&(verbose>5)))
	    MSG2("%s file %s\n",(fp?"Found":"Didn't find"),buf);
	if (fp!=NULL)
	    return fp;
    }
    return NULL;
}

/***====================================================================***/

Bool
addStringToOptions(char *opt_str,int *sz_opts,int *num_opts,char ***opts)
{
char 	*tmp,*str,*next;
Bool	ok= True;

    if ((str= malloc(strlen(opt_str)+1))!=NULL)
	 strcpy(str,opt_str);
    else return False;
    for (tmp= str,next=NULL;(tmp && *tmp!='\0')&&ok;tmp=next) {
	next= strchr(str,',');
	if (next) {
	    *next= '\0';
	    next++;
	}
	ok= addToList(sz_opts,num_opts,opts,tmp)&&ok;
    }
    free(str);
    return ok;
}

/***====================================================================***/

char *
stringFromOptions(char *orig,int numNew,char **newOpts)
{
int	len,i,nOut;

    if (orig)	len= strlen(orig)+1;
    else	len= 0;
    for (i=0;i<numNew;i++) {
	if (newOpts[i])
	    len+= strlen(newOpts[i])+1;
    }
    if (len<1)
	return NULL;
    if (orig) {
	orig= (char *)realloc(orig,len);
	nOut= 1;
    }
    else {
	orig= (char *)calloc(len,1);
	nOut= 0;
    }
    for (i=0;i<numNew;i++) {
	if (!newOpts[i])
	    continue;
	if (nOut>0) {
	     strcat(orig,",");
	     strcat(orig,newOpts[i]);
	}
	else strcpy(orig,newOpts[i]);
	nOut++;
    }
    return orig;
}

/***====================================================================***/

Bool
applyConfig(char *name)
{
FILE *	fp;
Bool	ok;
    
    if ((fp=findFileInPath(name,""))==NULL)
	return False;
    ok= XkbCFParse(fp,XkbCFDflts,NULL,&cfgResult);
    fclose(fp);
    if (!ok) {
	ERR1("Couldn't find configuration file \"%s\"\n", name);
	return False;
    }
    if (cfgResult.rules_file) {
	trySetString(RULES_NDX,cfgResult.rules_file,FROM_CONFIG);
	cfgResult.rules_file= NULL;
    }
    if (cfgResult.model) {
	trySetString(MODEL_NDX,cfgResult.model,FROM_CONFIG);
	cfgResult.model= NULL;
    }
    if (cfgResult.layout) {
	trySetString(LAYOUT_NDX,cfgResult.layout,FROM_CONFIG);
	cfgResult.layout= NULL;
    }
    if (cfgResult.variant) {
	trySetString(VARIANT_NDX,cfgResult.variant,FROM_CONFIG);
	cfgResult.variant= NULL;
    }
    if (cfgResult.options) {
	addStringToOptions(cfgResult.options,&szOptions,&numOptions,&options);
	cfgResult.options= NULL;
    }
    if (cfgResult.keymap) {
	trySetString(KEYMAP_NDX,cfgResult.keymap,FROM_CONFIG);
	cfgResult.keymap= NULL;
    }
    if (cfgResult.keycodes) {
	trySetString(KEYCODES_NDX,cfgResult.keycodes,FROM_CONFIG);
	cfgResult.keycodes= NULL;
    }
    if (cfgResult.geometry) {
	trySetString(GEOMETRY_NDX,cfgResult.geometry,FROM_CONFIG);
	cfgResult.geometry= NULL;
    }
    if (cfgResult.symbols) {
	trySetString(SYMBOLS_NDX,cfgResult.symbols,FROM_CONFIG);
	cfgResult.symbols= NULL;
    }
    if (cfgResult.types) {
	trySetString(TYPES_NDX,cfgResult.types,FROM_CONFIG);
	cfgResult.types= NULL;
    }
    if (cfgResult.compat) {
	trySetString(COMPAT_NDX,cfgResult.compat,FROM_CONFIG);
	cfgResult.compat= NULL;
    }
    if (verbose>5) {
	MSG("After config file:\n");
	dumpNames(True,True);
    }
    return True;
}

Bool
applyRules(void)
{
int	i;
char *	rfName;

    if (svSrc[MODEL_NDX]||svSrc[LAYOUT_NDX]||svSrc[VARIANT_NDX]||options) {
	char 			buf[PATH_MAX];
	XkbComponentNamesRec	rnames;

        if(svSrc[VARIANT_NDX] < svSrc[LAYOUT_NDX])
            svValue[VARIANT_NDX] = NULL;

	rdefs.model= svValue[MODEL_NDX];
	rdefs.layout= svValue[LAYOUT_NDX];
	rdefs.variant= svValue[VARIANT_NDX];
	if (options)
	    rdefs.options=stringFromOptions(rdefs.options,numOptions,options);

	if (svSrc[RULES_NDX])
	     rfName= svValue[RULES_NDX];
	else rfName= DFLT_XKB_RULES_FILE;

	if (rfName[0]=='/') {
	    rules= XkbRF_Load(rfName,svValue[LOCALE_NDX],True,True);
	}
	else {
	    for (i=0;(i<numInclPath)&&(!rules);i++) {
		if ((strlen(inclPath[i])+strlen(rfName)+8)>PATH_MAX) {
		    VMSG2(0,"Path too long (%s/rules/%s). Ignored.\n",
						inclPath[i],rfName);
		    continue;
		}
		sprintf(buf,"%s/rules/%s",inclPath[i],svValue[RULES_NDX]);
		rules= XkbRF_Load(buf,svValue[LOCALE_NDX],True,True);
	    }
	}
	if (!rules) {
	    ERR1("Couldn't find rules file (%s) \n",svValue[RULES_NDX]);
	    return False;
	}
	XkbRF_GetComponents(rules,&rdefs,&rnames);
	if (rnames.keycodes) {
	    trySetString(KEYCODES_NDX,rnames.keycodes,FROM_RULES);
	    rnames.keycodes= NULL;
	}
	if (rnames.symbols) {
	    trySetString(SYMBOLS_NDX,rnames.symbols,FROM_RULES);
	    rnames.symbols= NULL;
	}
	if (rnames.types) {
	    trySetString(TYPES_NDX,rnames.types,FROM_RULES);
	    rnames.types= NULL;
	}
	if (rnames.compat) {
	    trySetString(COMPAT_NDX,rnames.compat,FROM_RULES);
	    rnames.compat= NULL;
	}
	if (rnames.geometry) {
	    trySetString(GEOMETRY_NDX,rnames.geometry,FROM_RULES);
	    rnames.geometry= NULL;
	}
	if (rnames.keymap) {
	    trySetString(KEYMAP_NDX,rnames.keymap,FROM_RULES);
	    rnames.keymap= NULL;
	}
	if (verbose>6) {
	    MSG1("Applied rules from %s:\n",svValue[RULES_NDX]);
	    dumpNames(True,False);
	}
    }
    else if (verbose>6) {
	MSG("No rules variables specified.  Rules file ignored\n");
    }
    return True;
}

/* Primitive sanity check - filter out 'map names' (inside parenthesis) */
/* that can confuse xkbcomp parser */
Bool
checkName(char *name, char* string)
{
   char *i = name, *opar = NULL;
   Bool ret = True;

   if(!name)
      return True;

   while (*i){
      if (opar == NULL) {
         if (*i == '(')
         opar = i;
      } else {
         if ((*i == '(') || (*i == '|') || (*i == '+')) {
             ret = False;
             break;
         }
         if (*i == ')')
             opar = NULL;
      }
      i++;
   }
   if (opar)
      ret = False;
   if (!ret) {
      char c;
      int n = 1;
      for(i = opar+1; *i && n; i++) {
         if (*i == '(') n++;
         if (*i == ')') n--;
      }
      if (*i) i++;
      c = *i;
      *i = '\0';
      ERR1("Illegal map name '%s' ", opar);
      *i = c;
      ERR2("in %s name '%s'\n", string, name);
   }
   return ret;
}

void
printKeymap(void)
{
    MSG("xkb_keymap {\n");
    if (svValue[KEYCODES_NDX])
	MSG1("\txkb_keycodes  { include \"%s\"\t};\n",svValue[KEYCODES_NDX]);
    if (svValue[TYPES_NDX])
	MSG1("\txkb_types     { include \"%s\"\t};\n",svValue[TYPES_NDX]);
    if (svValue[COMPAT_NDX])
	MSG1("\txkb_compat    { include \"%s\"\t};\n",svValue[COMPAT_NDX]);
    if (svValue[SYMBOLS_NDX])
	MSG1("\txkb_symbols   { include \"%s\"\t};\n",svValue[SYMBOLS_NDX]);
    if (svValue[GEOMETRY_NDX])
	MSG1("\txkb_geometry  { include \"%s\"\t};\n",svValue[GEOMETRY_NDX]);
    MSG("};\n");
}

Bool
applyComponentNames(void)
{
    if(!checkName(svValue[TYPES_NDX],    "types"))
	return False;
    if(!checkName(svValue[COMPAT_NDX],   "compat"))
	return False;
    if(!checkName(svValue[SYMBOLS_NDX],  "symbols"))
	return False;
    if(!checkName(svValue[KEYCODES_NDX], "keycodes"))
	return False;
    if(!checkName(svValue[GEOMETRY_NDX], "geometry"))
	return False;
    if(!checkName(svValue[KEYMAP_NDX],   "keymap"))
	return False;

    if (verbose>5) {
	MSG("Trying to build keymap using the following components:\n");
	dumpNames(False,True);
    }
    if (dpy && !print) {
	XkbComponentNamesRec	cmdNames;
	cmdNames.types= svValue[TYPES_NDX];
	cmdNames.compat= svValue[COMPAT_NDX];
	cmdNames.symbols= svValue[SYMBOLS_NDX];
	cmdNames.keycodes= svValue[KEYCODES_NDX];
	cmdNames.geometry= svValue[GEOMETRY_NDX];
	cmdNames.keymap= svValue[KEYMAP_NDX];
	xkb= XkbGetKeyboardByName(dpy,XkbUseCoreKbd,&cmdNames,
			XkbGBN_AllComponentsMask, 
			XkbGBN_AllComponentsMask&(~XkbGBN_GeometryMask),
			True);
	if (!xkb) {
	    ERR("Error loading new keyboard description\n");
	    return False;
	}
	if (svValue[RULES_NDX] && (rdefs.model || rdefs.layout)) {
	    if (!XkbRF_SetNamesProp(dpy,svValue[RULES_NDX],&rdefs)) {
		VMSG(0,"Error updating the XKB names property\n");
	    }
	}
    }
    if (print) {
        printKeymap();
    }
    return True;
}


int
main(int argc,char **argv)
{
    if ((!parseArgs(argc,argv))||(!getDisplay(argc,argv)))
	exit(-1);
    svValue[LOCALE_NDX]= setlocale(LC_ALL,svValue[LOCALE_NDX]);
    svSrc[LOCALE_NDX]= FROM_SERVER;
    VMSG1(7,"locale is %s\n",svValue[LOCALE_NDX]);
    if (dpy)
        getServerValues();
    if (svValue[CONFIG_NDX] && (!applyConfig(svValue[CONFIG_NDX])))
	exit(-3);
    if (!applyRules())
	exit(-4);
    if (!applyComponentNames())
	exit(-5);
    if (dpy)
	XCloseDisplay(dpy);
    exit(0);
}
