/* $Xorg: resource.c,v 1.4 2001/02/09 02:05:40 xorgcvs Exp $ */
/*

Copyright 1988, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/* $XFree86: xc/programs/xdm/resource.c,v 3.12 2003/09/17 05:48:32 herrb Exp $ */

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 *
 * resource.c
 */

# include "dm.h"
# include "dm_error.h"

# include <X11/Intrinsic.h>
# include <X11/Xmu/CharSet.h>

char	*config;

char	*servers;
int	request_port;
int	debugLevel;
char	*errorLogFile;
int	daemonMode;
char	*pidFile;
int	lockPidFile;
int	sourceAddress;
char	*authDir;
int	autoRescan;
int	removeDomainname;
char	*keyFile;
char	*accessFile;
char	**exportList;
#if !defined(ARC4_RANDOM)
char	*randomFile;
#endif
#ifdef DEV_RANDOM
char	*randomDevice;
#endif
#if !defined(ARC4_RANDOM)
char	*prngdSocket;
int	prngdPort;
#endif

char	*greeterLib;
char	*willing;
int	choiceTimeout;	/* chooser choice timeout */

# define DM_STRING	0
# define DM_INT		1
# define DM_BOOL	2
# define DM_ARGV	3

/*
 * the following constants are supposed to be set in the makefile from
 * parameters set util/imake.includes/site.def (or *.macros in that directory
 * if it is server-specific).  DO NOT CHANGE THESE DEFINITIONS!
 */
#ifndef __EMX__
#ifndef DEF_SERVER_LINE 
#define DEF_SERVER_LINE ":0 local /usr/bin/X11/X :0"
#endif
#ifndef XRDB_PROGRAM
#define XRDB_PROGRAM "/usr/bin/X11/xrdb"
#endif
#ifndef DEF_SESSION
#define DEF_SESSION "/usr/bin/X11/xterm -ls"
#endif
#ifndef DEF_USER_PATH
#define DEF_USER_PATH ":/bin:/usr/bin:/usr/bin/X11:/usr/ucb"
#endif
#ifndef DEF_SYSTEM_PATH
#define DEF_SYSTEM_PATH "/etc:/bin:/usr/bin:/usr/bin/X11:/usr/ucb"
#endif
#ifndef DEF_SYSTEM_SHELL
#define DEF_SYSTEM_SHELL "/bin/sh"
#endif
#ifndef DEF_FAILSAFE_CLIENT
#define DEF_FAILSAFE_CLIENT "/usr/bin/X11/xterm"
#endif
#ifndef DEF_XDM_CONFIG
#define DEF_XDM_CONFIG "/usr/lib/X11/xdm/xdm-config"
#endif
#ifndef DEF_CHOOSER
#define DEF_CHOOSER "/usr/lib/X11/xdm/chooser"
#endif
#ifndef DEF_AUTH_NAME
#ifdef HASXDMAUTH
#define DEF_AUTH_NAME	"XDM-AUTHORIZATION-1 MIT-MAGIC-COOKIE-1"
#else
#define DEF_AUTH_NAME	"MIT-MAGIC-COOKIE-1"
#endif
#endif
#ifndef DEF_AUTH_DIR
#define DEF_AUTH_DIR "/usr/lib/X11/xdm"
#endif
#ifndef DEF_USER_AUTH_DIR
#define DEF_USER_AUTH_DIR	"/tmp"
#endif
#ifndef DEF_KEY_FILE
#define DEF_KEY_FILE	""
#endif
#ifndef DEF_ACCESS_FILE
#define DEF_ACCESS_FILE	""
#endif
#ifndef DEF_RANDOM_FILE
#define DEF_RANDOM_FILE "/dev/mem"
#endif
#ifndef DEF_PRNGD_SOCKET 
#define DEF_PRNGD_SOCKET "/tmp/entropy"
#endif
#ifndef DEF_PRNGD_PORT
#define DEF_PRNGD_PORT "0"
#endif
#ifndef DEF_GREETER_LIB
#define DEF_GREETER_LIB "/usr/lib/X11/xdm/libXdmGreet.so"
#endif
#else
/* unfortunately I have to declare all of them, because there is a limit
 * in argument size in OS/2
 * but everything needs to be fixed again
 */
#define DEF_SERVER_LINE ":0 local /XFree86/bin/X :0"
#ifndef XRDB_PROGRAM
#define XRDB_PROGRAM "/XFree86/bin/xrdb"
#endif
#ifndef DEF_SESSION
#define DEF_SESSION "/XFree86/bin/xterm -ls"
#endif
#ifndef DEF_USER_PATH
#define DEF_USER_PATH "c:\\os2;c:\\os2\apps;\\XFree86\\bin"
#endif
#ifndef DEF_SYSTEM_PATH
#define DEF_SYSTEM_PATH "c:\\os2;c:\\os2\apps;\\XFree86\\bin"
#endif
#ifndef DEF_SYSTEM_SHELL
#define DEF_SYSTEM_SHELL "sh"
#endif
#ifndef DEF_FAILSAFE_CLIENT
#define DEF_FAILSAFE_CLIENT "/XFree86/bin/xterm"
#endif
#ifndef DEF_XDM_CONFIG
#define DEF_XDM_CONFIG "/XFree86/lib/X11/xdm/xdm-config"
#endif
#ifndef DEF_CHOOSER
#define DEF_CHOOSER "/XFree86/lib/X11/xdm/chooser"
#endif
#ifndef DEF_AUTH_NAME
#ifdef HASXDMAUTH
#define DEF_AUTH_NAME	"XDM-AUTHORIZATION-1 MIT-MAGIC-COOKIE-1"
#else
#define DEF_AUTH_NAME	"MIT-MAGIC-COOKIE-1"
#endif
#endif
#ifndef DEF_AUTH_DIR
#define DEF_AUTH_DIR "/XFree86/lib/X11/xdm"
#endif
#ifndef DEF_USER_AUTH_DIR
#define DEF_USER_AUTH_DIR	"/tmp"
#endif
#ifndef DEF_KEY_FILE
#define DEF_KEY_FILE	""
#endif
#ifndef DEF_ACCESS_FILE
#define DEF_ACCESS_FILE	""
#endif
#ifndef DEF_RANDOM_FILE
#define DEF_RANDOM_FILE ""
#endif
#ifndef DEF_GREETER_LIB
#define DEF_GREETER_LIB "/XFree86/lib/X11/xdm/libXdmGreet.so"
#endif

#endif /* __EMX__ */

#define DEF_UDP_PORT	"177"	    /* registered XDMCP port, dont change */

struct dmResources {
	char	*name, *class;
	int	type;
	char	**dm_value;
	char	*default_value;
} DmResources[] = {
{ "servers",	"Servers", 	DM_STRING,	&servers,
				DEF_SERVER_LINE} ,
{ "requestPort","RequestPort",	DM_INT,		(char **) &request_port,
				DEF_UDP_PORT} ,
{ "debugLevel",	"DebugLevel",	DM_INT,		(char **) &debugLevel,
				"0"} ,
{ "errorLogFile","ErrorLogFile",	DM_STRING,	&errorLogFile,
				""} ,
{ "daemonMode",	"DaemonMode",	DM_BOOL,	(char **) &daemonMode,
				"true"} ,
{ "pidFile",	"PidFile",	DM_STRING,	&pidFile,
				""} ,
{ "lockPidFile","LockPidFile",	DM_BOOL,	(char **) &lockPidFile,
				"true"} ,
{ "authDir",	"authDir",	DM_STRING,	&authDir,
				DEF_AUTH_DIR} ,
{ "autoRescan",	"AutoRescan",	DM_BOOL,	(char **) &autoRescan,
				"true"} ,
{ "removeDomainname","RemoveDomainname",DM_BOOL,(char **) &removeDomainname,
				"true"} ,
{ "keyFile",	"KeyFile",	DM_STRING,	&keyFile,
				DEF_KEY_FILE} ,
{ "accessFile",	"AccessFile",	DM_STRING,	&accessFile,
				DEF_ACCESS_FILE} ,
{ "exportList",	"ExportList",	DM_ARGV,	(char **) &exportList,
				""} ,
#if !defined(ARC4_RANDOM)
{ "randomFile",	"RandomFile",	DM_STRING,	&randomFile,
				DEF_RANDOM_FILE} ,
{ "prgndSocket", "PrngdSocket", DM_STRING,	&prngdSocket, 
				DEF_PRNGD_SOCKET},
{ "prngdPort", "PrngdPort",	DM_INT,		(char **) &prngdPort,
				DEF_PRNGD_PORT},
#endif
#ifdef DEV_RANDOM
{ "randomDevice", "RandomDevice", DM_STRING,	&randomDevice,
				DEV_RANDOM} ,
#endif
{ "greeterLib",	"GreeterLib",	DM_STRING,	&greeterLib,
				DEF_GREETER_LIB} ,
{ "choiceTimeout","ChoiceTimeout",DM_INT,	(char **) &choiceTimeout,
				"15"} ,
{ "sourceAddress","SourceAddress",DM_BOOL,	(char **) &sourceAddress,
				"false"} ,
{ "willing",	"Willing",	DM_STRING,	&willing,
				""} ,
};

# define NUM_DM_RESOURCES	(sizeof DmResources / sizeof DmResources[0])

# define boffset(f)	XtOffsetOf(struct display, f)

struct displayResource {
	char	*name, *class;
	int	type;
	int	offset;
	char	*default_value;
};

/* resources for managing the server */

struct displayResource serverResources[] = {
{ "serverAttempts","ServerAttempts",DM_INT,	boffset(serverAttempts),
				"1" },
{ "openDelay",	"OpenDelay",	DM_INT,		boffset(openDelay),
				"15" },
{ "openRepeat",	"OpenRepeat",	DM_INT,		boffset(openRepeat),
				"5" },
{ "openTimeout","OpenTimeout",	DM_INT,		boffset(openTimeout),
				"120" },
{ "startAttempts","StartAttempts",DM_INT,	boffset(startAttempts),
				"4" },
{ "pingInterval","PingInterval",DM_INT,		boffset(pingInterval),
				"5" },
{ "pingTimeout","PingTimeout",	DM_INT,		boffset(pingTimeout),
				"5" },
{ "terminateServer","TerminateServer",DM_BOOL,	boffset(terminateServer),
				"false" },
{ "grabServer",	"GrabServer",	DM_BOOL,	boffset(grabServer),
				"false" },
{ "grabTimeout","GrabTimeout",	DM_INT,		boffset(grabTimeout),
				"3" },
{ "resetSignal","Signal",	DM_INT,		boffset(resetSignal),
				"1" },	/* SIGHUP */
{ "termSignal",	"Signal",	DM_INT,		boffset(termSignal),
				"15" },	/* SIGTERM */
{ "resetForAuth","ResetForAuth",DM_BOOL,	boffset(resetForAuth),
				"false" },
{ "authorize",	"Authorize",	DM_BOOL,	boffset(authorize),
				"true" },
{ "authComplain","AuthComplain",DM_BOOL,	boffset(authComplain),
				"true" },
{ "authName",	"AuthName",	DM_ARGV,	boffset(authNames),
				DEF_AUTH_NAME },
{ "authFile",	"AuthFile",	DM_STRING,	boffset(clientAuthFile),
				"" },
};

# define NUM_SERVER_RESOURCES	(sizeof serverResources/\
				 sizeof serverResources[0])

/* resources which control the session behaviour */

struct displayResource sessionResources[] = {
{ "resources",	"Resources",	DM_STRING,	boffset(resources),
				"" },
{ "xrdb",	"Xrdb",		DM_STRING,	boffset(xrdb),
				XRDB_PROGRAM },
{ "setup",	"Setup",	DM_STRING,	boffset(setup),
				"" },
{ "startup",	"Startup",	DM_STRING,	boffset(startup),
				"" },
{ "reset",	"Reset",	DM_STRING,	boffset(reset),
				"" },
{ "session",	"Session",	DM_STRING,	boffset(session),
				DEF_SESSION },
{ "userPath",	"Path",		DM_STRING,	boffset(userPath),
				DEF_USER_PATH },
{ "systemPath",	"Path",		DM_STRING,	boffset(systemPath),
				DEF_SYSTEM_PATH },
{ "systemShell","Shell",	DM_STRING,	boffset(systemShell),
				DEF_SYSTEM_SHELL },
{ "failsafeClient","FailsafeClient",	DM_STRING,	boffset(failsafeClient),
				DEF_FAILSAFE_CLIENT },
{ "userAuthDir","UserAuthDir",	DM_STRING,	boffset(userAuthDir),
				DEF_USER_AUTH_DIR },
{ "chooser",	"Chooser",	DM_STRING,	boffset(chooser),
				DEF_CHOOSER },
};

# define NUM_SESSION_RESOURCES	(sizeof sessionResources/\
				 sizeof sessionResources[0])

XrmDatabase	DmResourceDB;

static void
GetResource (
    char    *name,
    char    *class,
    int	    valueType,
    char    **valuep,
    char    *default_value)
{
    char	*type;
    XrmValue	value;
    char	*string, *new_string;
    char	str_buf[50];
    int	len;

    if (DmResourceDB && XrmGetResource (DmResourceDB,
	name, class,
	&type, &value))
    {
	string = value.addr;
	len = value.size;
    }
    else
    {
	string = default_value;
	len = strlen (string);
    }

    Debug ("%s/%s value %*.*s\n", name, class, len, len, string);

    if (valueType == DM_STRING && *valuep)
    {
	if (strlen (*valuep) == len && !strncmp (*valuep, string, len))
	    return;
	else
	    free (*valuep);
    }

    switch (valueType) {
    case DM_STRING:
	new_string = malloc ((unsigned) (len+1));
	if (!new_string) {
		LogOutOfMem ("GetResource");
		return;
	}
	strncpy (new_string, string, len);
	new_string[len] = '\0';
	*(valuep) = new_string;
	break;
    case DM_INT:
	strncpy (str_buf, string, sizeof (str_buf));
	str_buf[sizeof (str_buf)-1] = '\0';
	*((int *) valuep) = atoi (str_buf);
	break;
    case DM_BOOL:
	strncpy (str_buf, string, sizeof (str_buf));
	str_buf[sizeof (str_buf)-1] = '\0';
	XmuCopyISOLatin1Lowered (str_buf, str_buf);
	if (!strcmp (str_buf, "true") ||
	    !strcmp (str_buf, "on") ||
	    !strcmp (str_buf, "yes"))
		*((int *) valuep) = 1;
	else if (!strcmp (str_buf, "false") ||
		 !strcmp (str_buf, "off") ||
		 !strcmp (str_buf, "no"))
		*((int *) valuep) = 0;
	break;
    case DM_ARGV:
	freeArgs (*(char ***) valuep);
	*((char ***) valuep) = parseArgs ((char **) 0, string);
	break;
    }
}

XrmOptionDescRec configTable [] = {
{"-server",	NULL,			XrmoptionSkipArg,	(caddr_t) NULL },
{"-udpPort",	NULL,			XrmoptionSkipArg,	(caddr_t) NULL },
{"-error",	NULL,			XrmoptionSkipArg,	(caddr_t) NULL },
{"-resources",	NULL,			XrmoptionSkipArg,	(caddr_t) NULL },
{"-session",	NULL,			XrmoptionSkipArg,	(caddr_t) NULL },
{"-debug",	NULL,			XrmoptionSkipArg,	(caddr_t) NULL },
{"-xrm",	NULL,			XrmoptionSkipArg,	(caddr_t) NULL },
{"-config",	".configFile",		XrmoptionSepArg,	(caddr_t) NULL }
};

XrmOptionDescRec optionTable [] = {
{"-server",	".servers",		XrmoptionSepArg,	(caddr_t) NULL },
{"-udpPort",	".requestPort",		XrmoptionSepArg,	(caddr_t) NULL },
{"-error",	".errorLogFile",	XrmoptionSepArg,	(caddr_t) NULL },
{"-resources",	"*resources",		XrmoptionSepArg,	(caddr_t) NULL },
{"-session",	"*session",		XrmoptionSepArg,	(caddr_t) NULL },
{"-debug",	"*debugLevel",		XrmoptionSepArg,	(caddr_t) NULL },
{"-xrm",	NULL,			XrmoptionResArg,	(caddr_t) NULL },
{"-daemon",	".daemonMode",		XrmoptionNoArg,		"true"         },
{"-nodaemon",	".daemonMode",		XrmoptionNoArg,		"false"        }
};

static int	originalArgc;
static char	**originalArgv;

void
InitResources (int argc, char **argv)
{
	XrmInitialize ();
	originalArgc = argc;
	originalArgv = argv;
	ReinitResources ();
}

void
ReinitResources (void)
{
    int	argc;
    char	**a;
    char	**argv;
    XrmDatabase newDB;

    argv = (char **) malloc ((originalArgc + 1) * sizeof (char *));
    if (!argv)
	LogPanic ("no space for argument realloc\n");
    for (argc = 0; argc < originalArgc; argc++)
	argv[argc] = originalArgv[argc];
    argv[argc] = 0;
    if (DmResourceDB)
	XrmDestroyDatabase (DmResourceDB);
    DmResourceDB = XrmGetStringDatabase ("");
    /* pre-parse the command line to get the -config option, if any */
    XrmParseCommand (&DmResourceDB, configTable,
		     sizeof (configTable) / sizeof (configTable[0]),
		     "DisplayManager", &argc, argv);
    GetResource ("DisplayManager.configFile", "DisplayManager.ConfigFile",
		 DM_STRING, &config, DEF_XDM_CONFIG);
    newDB = XrmGetFileDatabase ( config );
    if (newDB)
    {
	if (DmResourceDB)
	    XrmDestroyDatabase (DmResourceDB);
	DmResourceDB = newDB;
    }
    else if (argc != originalArgc)
	LogError ("Can't open configuration file %s\n", config );
    XrmParseCommand (&DmResourceDB, optionTable,
		     sizeof (optionTable) / sizeof (optionTable[0]),
		     "DisplayManager", &argc, argv);
    if (argc > 1)
    {
	LogError ("extra arguments on command line:");
	for (a = argv + 1; *a; a++)
		LogError (" \"%s\"", *a);
	LogError ("\n");
    }
    free (argv);
}

void
LoadDMResources (void)
{
	int	i;
	char	name[1024], class[1024];

	for (i = 0; i < NUM_DM_RESOURCES; i++) {
		snprintf (name, sizeof(name), "DisplayManager.%s", DmResources[i].name);
		snprintf (class, sizeof(class), "DisplayManager.%s", DmResources[i].class);
		GetResource (name, class, DmResources[i].type,
			      (char **) DmResources[i].dm_value,
			      DmResources[i].default_value);
	}
}

static void
CleanUpName (char *src, char *dst, int len)
{
    while (*src) {
	if (--len <= 0)
		break;
	switch (*src)
	{
	case ':':
	case '.':
	    *dst++ = '_';
	    break;
	default:
	    *dst++ = *src;
	}
	++src;
    }
    *dst = '\0';
}

static void
LoadDisplayResources (
    struct display	    *d,
    struct displayResource  *resources,
    int			    numResources)
{
    int	i;
    char	name[1024], class[1024];
    char	dpyName[512], dpyClass[512];

    CleanUpName (d->name, dpyName, sizeof (dpyName));
    CleanUpName (d->class ? d->class : d->name, dpyClass, sizeof (dpyClass));
    for (i = 0; i < numResources; i++) {
	    snprintf (name, sizeof(name), "DisplayManager.%s.%s", 
		    dpyName, resources[i].name);
	    snprintf (class, sizeof(class), "DisplayManager.%s.%s",
		    dpyClass, resources[i].class);
	    GetResource (name, class, resources[i].type,
			  (char **) (((char *) d) + resources[i].offset),
			  resources[i].default_value);
    }
}

void
LoadServerResources (struct display *d)
{
    LoadDisplayResources (d, serverResources, NUM_SERVER_RESOURCES);
}

void
LoadSessionResources (struct display *d)
{
    LoadDisplayResources (d, sessionResources, NUM_SESSION_RESOURCES);
}
