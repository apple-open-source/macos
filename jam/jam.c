/*
 * /+\
 * +\	Copyright 1993, 1996 Christopher Seiwald.
 * \+/
 *
 * This file is part of jam.
 *
 * License is hereby granted to use this software and distribute it
 * freely, as long as this copyright notice is retained and modifications 
 * are clearly marked.
 *
 * ALL WARRANTIES ARE HEREBY DISCLAIMED.
 */

# include "jam.h"
# include "option.h"
# include "make.h"
# ifdef FATFS
# include "patchlev.h"
# else
# include "patchlevel.h"
# endif

/* These get various function declarations. */

# include "lists.h"
# include "parse.h"
# include "variable.h"
# include "compile.h"
# include "rules.h"
# include "newstr.h"
# include "scan.h"
# ifdef FATFS
# include "timestam.h"
# else
# include "timestamp.h"
# endif

#ifdef APPLE_EXTENSIONS
# include "timingdata.h"
#endif

/* Macintosh is "special" */

# ifdef macintosh
# include <QuickDraw.h>
# else
# include <stdarg.h>
# endif

/*
 * jam.c - make redux
 *
 * See jam(1) and Jamfile(5) for usage information.
 *
 * These comments document the code.
 *
 * The top half of the code is structured such:
 *
 *                       jam 
 *                      / | \ 
 *                 +---+  |  \
 *                /       |   \ 
 *         jamgram     option  \ 
 *        /  |   \              \
 *       /   |    \              \
 *      /    |     \             |
 *  scan     |     compile      make
 *   |       |    /    \       / |  \
 *   |       |   /      \     /  |   \
 *   |       |  /        \   /   |    \
 * jambase parse         rules  search make1
 *                               |	|   \
 *                               |	|    \
 *                               |	|     \
 *                           timestamp command execute
 *
 *
 * The support routines are called by all of the above, but themselves
 * are layered thus:
 *
 *                     variable|expand
 *                      /  |   |   |
 *                     /   |   |   |
 *                    /    |   |   |
 *                 lists   |   |   filesys
 *                    \    |   |
 *                     \   |   |
 *                      \  |   |
 *                     newstr  |
 *                        \    |
 *                         \   |
 *                          \  |
 *                          hash
 *
 * Roughly, the modules are:
 *
 *	command.c - maintain lists of commands
 *	compile.c - compile parsed jam statements
 *	execunix.c - execute a shell script on UNIX
 *	execvms.c - execute a shell script, ala VMS
 *	expand.c - expand a buffer, given variable values
 *	fileunix.c - manipulate file names and scan directories on UNIX
 *	filevms.c - manipulate file names and scan directories on VMS
 *	hash.c - simple in-memory hashing routines 
 *	headers.c - handle #includes in source files
 *	jambase.c - compilable copy of Jambase
 *	jamgram.y - jam grammar
 *	lists.c - maintain lists of strings
 *	make.c - bring a target up to date, once rules are in place
 *	make1.c - execute command to bring targets up to date
 *	newstr.c - string manipulation routines
 *	option.c - command line option processing
 *	parse.c - make and destroy parse trees as driven by the parser
 *	regexp.c - Henry Spencer's regexp
 *	rules.c - access to RULEs, TARGETs, and ACTIONs
 *	scan.c - the jam yacc scanner
 *	search.c - find a target along $(SEARCH) or $(LOCATE) 
 *	timestamp.c - get the timestamp of a file or archive member
 *	variable.c - handle jam multi-element variables
 *
 * 05/04/94 (seiwald) - async multiprocess (-j) support
 * 02/08/95 (seiwald) - -n implies -d2.
 * 02/22/95 (seiwald) - -v for version info.
 */

struct globs globs = {
	0,			/* noexec */
	1,			/* jobs */
	0,			/* ignore */
#ifdef APPLE_EXTENSIONS
	0,                      /* apple_jam_extensions */
	0,			/* parsable_output */
	0,			/* ascii_output_annotation */
	0,			/* debug_parsable_output */
	NULL,			/* cmdline_defines */
	0,			/* enable_timings */
        NULL,			/* timing_entry */
        0.0,			/* header_scanning_time */
        0,			/* headers_scanned */
        0,                      /* num_targets_to_update */
#endif
# ifdef macintosh
	{ 0, 0 }		/* debug - suppress tracing output */
# else
	{ 0, 1 } 		/* debug ... */
# endif
} ;


#ifdef APPLE_EXTENSIONS

int pbx_printf( const char * category_tag, const char * format, ... )
{
    va_list   args;
    int       result = 0;

    va_start(args, format);
    if (category_tag != NULL) {
	static int new_style = -1;
	if (new_style == -1) {
	    new_style = getenv("GCC_EMIT_LINE_PREFIXES") ? 1 : 0;
	}
	if (new_style) {
	    /* Interlinear Annotation Anchor (\uFFF9), followed immediately by Interlinear Annotation Separator (\uFFFA), data, and Interlinear Annotation Terminator (\uFFFB). */
	    printf("\357\277\271" "\357\277\272" "%s" "\357\277\273", category_tag);
	}
	else {
	    printf("[%s]", category_tag);
	}
    }
    result = vprintf(format, args);
    va_end(args);
    return result;
}

#endif


/* Symbols to be defined as true for use in Jambase */

static char *othersyms[] = { OSSYMS OSPLATSYM, JAMVERSYM, 0 } ;

# ifndef __WATCOM__
# ifndef __OS2__
# ifndef NT
extern char **environ;
# endif
# endif
# endif

# ifdef macintosh
char** demakeifyArguments( int argc, char **argv )
# else
char** demakeifyArguments( argc, argv )
char    **argv;
# endif
{
	char**	newArgvBuffer = (char**)calloc(sizeof(char*), argc);
	int	i;
	int	lastOption = 0;
	int	j;

	for (i=0; i<argc; i++)
	{
#ifdef DEBUG_JAM_INPUT_ARGUMENTS
		printf("argv[%2u]='%s'\n", i, argv[i]);
#endif
		newArgvBuffer[i] = NULL;
	}

	for (i=0; i<argc; i++) {
		if (argv[i][0] == '-')
			lastOption = i+1;
		else if (strchr (argv[i], '=')) {
			char* old = argv[i];
			char* new = malloc(strlen(old)+3);
			sprintf(new, "-s%s", old);
			for (j=i; j>lastOption; j--)
				argv[j] = argv[j-1];
			newArgvBuffer[lastOption] = old;
			argv[lastOption++] = new;
		}
	}
	return newArgvBuffer;
}

# ifdef macintosh
void remakeifyArguments( int argc, char **argv , char** newArgvBuffer )
# else
void remakeifyArguments( argc, argv, newArgvBuffer  )
char    **argv;
char    **newArgvBuffer;
# endif
{
	int	i;

	for (i=0; i<argc; i++) if (newArgvBuffer[i] != NULL) {
		free (argv[i]);
		argv[i] = newArgvBuffer[i];
	}
	free (newArgvBuffer);
}

# ifdef macintosh
int main( int argc, char **argv, char **environ )
# else
int main( argc, argv )
char	**argv;
# endif
{
	const unsigned  realArgc = argc;
	char		**newArgvBuffer;
	int		n;
	char		*s;
	struct option	optv[N_OPTS];
	char		*all = "all";
	int		anyhow = 0;
	int		status;

# ifdef macintosh
	InitGraf(&qd.thePort);
# endif

	argc--, argv++;

	newArgvBuffer = demakeifyArguments(argc, argv);

	if( ( n = getoptions( argc, argv, "d:j:f:s:t:aknv", optv ) ) < 0 )
	{
	    printf( "\nusage: jam [ options ] targets...\n\n" );

            printf( "-a      Build all targets, even if they are current.\n" );
            printf( "-dx     Set the debug level to x (0-9).\n" );
            printf( "-fx     Read x instead of Jambase.\n" );
            printf( "-jx     Run up to x shell commands concurrently.\n" );
            printf( "-k      Ignore build errors.\n" );
            printf( "-n      Don't actually execute the updating actions.\n" );
	    printf( "-sx=y   Set variable x=y, overriding environment.\n" );
            printf( "-tx     Rebuild x, even if it is up-to-date.\n" );
            printf( "-v      Print the version of jam and exit.\n\n" );

	    exit( EXITBAD );
	}

	argc -= n, argv += n;

	/* Version info. */

	if( ( s = getoptval( optv, 'v', 0 ) ) )
	{
	    printf( "Jam/MR  " );
	    printf( "Version %s.%s.  ", VERSION, PATCHLEVEL );
	    printf( "Copyright 1993, 1997 Christopher Seiwald.\n" );
#ifdef APPLE_EXTENSIONS
	    printf( "with Apple ProjectBuilder Extensions\n" );
#endif
	    return EXITOK;
	}

	/* Pick up interesting options */

    if( ( s = getoptval( optv, 'n', 0 ) ) )
        globs.noexec++, globs.debug[2] = 1;

    if( ( s = getoptval( optv, 'k', 0 ) ) )
        globs.ignore++;

	if( ( s = getoptval( optv, 'a', 0 ) ) )
	    anyhow++;

	if( ( s = getoptval( optv, 'j', 0 ) ) )
	    globs.jobs = atoi( s );

#ifdef APPLE_EXTENSIONS
	if( getenv( "NATIVE_ARCH" ) != NULL ) {
	    globs.apple_jam_extensions = 1;
	}
	if( getenv( "ENABLE_APPLE_JAM_OUTPUT_ANNOTATION" ) != NULL ) {
	    globs.parsable_output = 1;
	    if( getenv( "DEBUG_APPLE_JAM_OUTPUT_ANNOTATION" ) != NULL ) {
		globs.debug_parsable_output = 1;
	    }
	    if( getenv( "ASCII_OUTPUT_ANNOTATION" ) != NULL ) {
		globs.ascii_output_annotation = 1;
	    }
	}

	// To enable timings of each command run by jam, either set the 
	// ENABLE_JAM_TIMINGS environment variable, or have a file named 
	// ~/.jam-timings readable in your home dir.  The latter approach
	// allows easier dynamic enabling of this while running Project Builder.
	{
	    char buf[1024];
	    sprintf( buf, "%s/.jam-timings", getenv( "HOME" ) );
	    if( getenv( "ENABLE_JAM_TIMINGS" ) != NULL || access( buf, R_OK ) != -1 ) {
		globs.enable_timings = 1;
		init_timing_data();
		globs.timing_entry = create_timing_entry();
		printf( "\njam timings enabled.  See table at end of each target's log.\n\n" );
	    }
	}
#endif

	/* Turn on/off debugging */

	for( n = 0; (s = getoptval( optv, 'd', n )) != NULL; n++ )
	{
	    int i;

	    /* First -d, turn off defaults. */

	    if( !n )
		for( i = 0; i < DEBUG_MAX; i++ )
		    globs.debug[i] = 0;

	    i = atoi( s );

	    if( i < 0 || i >= DEBUG_MAX )
	    {
		printf( "Invalid debug level '%s'.\n", s );
		continue;
	    }

	    /* n turns on levels 1-n */
	    /* +n turns on level n */

	    if( *s == '+' )
		globs.debug[i] = 1;
	    else while( i )
		globs.debug[i--] = 1;
	}

	/* Set JAMDATE first */

	{
	    char *date;
	    time_t clock;
	    time( &clock );
	    date = newstr( ctime( &clock ) );

	    /* Trim newline from date */

	    if( strlen( date ) == 25 )
		date[ 24 ] = 0;

	    var_set( "JAMDATE", list_new( L0, newstr( date ) ), VAR_SET, 1 /* export-in-environment */ );
	}

	/* load up environment variables */

	var_defines( environ, 0 /* don't export-in-environment */ );
	var_defines( othersyms, 1 /* export-in-environment */ );

	/* Load up variables set on command line. */

	globs.cmdline_defines = (const char **)malloc(sizeof(char *) * (realArgc + 1));
	for( n = 0; (s = getoptval( optv, 's', n )) != NULL; n++ )
	{
	    char *symv[2];
	    symv[0] = s;
	    symv[1] = 0;
	    var_defines( symv, 1 /* export-in-environment */ );
	  #ifdef APPLE_EXTENSIONS
	    globs.cmdline_defines[n] = s;
	  #endif
	}
	globs.cmdline_defines[n] = NULL;

	/* Initialize builtins */

	compile_builtins();

#ifdef APPLE_EXTENSIONS
	if( globs.enable_timings ) {
	    append_timing_entry( globs.timing_entry, 0, "jam internals: compile_builtins()", NULL, NULL );
	    globs.timing_entry = create_timing_entry();
	}
#endif

	/* Parse ruleset */

	for( n = 0; (s = getoptval( optv, 'f', n )) != NULL; n++ )
	    parse_file( s );

	if( !n )
	    parse_file( "+" );
	
	status = yyanyerrors();

	/* Manually touch -t targets */

	for( n = 0; (s = getoptval( optv, 't', n )) != NULL; n++ )
	    touchtarget( s );

#ifdef APPLE_EXTENSIONS
	if ( globs.enable_timings ) {
	    // The target name variable was defined in the jamfile, hopefully.
	    LIST *var_list = var_get( "TARGET_NAME" );
	    if ( var_list ) {
		set_timing_target_name( var_list->string );
	    }
	}
#endif

	/* Now make target */

	if( !argc )
	    status |= make( 1, &all, anyhow );
	else
	    status |= make( argc, argv, anyhow );

	/* Widely scattered cleanup */

	var_done();
	donerules();
	donestamps();
	donestr();

#ifndef APPLE_EXTENSIONS
	remakeifyArguments(argc, argv, newArgvBuffer);
#endif
	
#ifdef APPLE_EXTENSIONS
	if ( globs.enable_timings ) {
	    print_timing_data();
	}
#endif

	return status ? EXITBAD : EXITOK;
}
