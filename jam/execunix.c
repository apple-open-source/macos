/*
 * Copyright 1993, 1995 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

# include "jam.h"
# include "execcmd.h"
# include "lists.h"
# include "variable.h"
# include <errno.h>

# if defined( unix ) || defined( NT ) || defined( __OS2__ )

# if defined( _AIX) || \
	(defined (COHERENT) && defined (_I386)) || \
	defined(__sgi) || \
	defined(__Lynx__) || \
	defined(M_XENIX) || \
	defined(__QNX__) || \
	defined(__BEOS__) || \
	defined(__ISC)
# define vfork() fork()
# endif

# if defined( NT ) || defined( __OS2__ )

# include <process.h>

# if !defined( __BORLANDC__ ) && !defined( __OS2__ )
# define wait my_wait
static int my_wait(int *status);
# endif

# endif

# include <unistd.h>
# include <sys/wait.h>

/*
 * execunix.c - execute a shell script on UNIX/WinNT/OS2
 *
 * If $(JAMSHELL) is defined, uses that to formulate execvp()/spawnvp().
 * The default is:
 *
 *	/bin/sh -c %		[ on UNIX ]
 *	cmd.exe /c %		[ on OS2/WinNT ]
 *
 * Each word must be an individual element in a jam variable value.
 *
 * In $(JAMSHELL), % expands to the command string and ! expands to 
 * the slot number (starting at 1) for multiprocess (-j) invocations.
 * If $(JAMSHELL) doesn't include a %, it is tacked on as the last
 * argument.
 *
 * Don't just set JAMSHELL to /bin/sh or cmd.exe - it won't work!
 *
 * External routines:
 *	execcmd() - launch an async command execution
 * 	execwait() - wait and drive at most one execution completion
 *
 * Internal routines:
 *	onintr() - bump intr to note command interruption
 *
 * 04/08/94 (seiwald) - Coherent/386 support added.
 * 05/04/94 (seiwald) - async multiprocess interface
 * 01/22/95 (seiwald) - $(JAMSHELL) support
 * 06/02/97 (gsar)    - full async multiprocess support for Win32
 */

static int intr = 0;

static int cmdsrunning = 0;

# ifdef NT
static void (*istat)( int );
void onintr( int );
# else
static void (*istat)();
#endif

static struct
{
	int	pid;		/* on win32, a real process handle */
	void	(*func)();
	void 	*closure;
# if defined( NT ) || defined( __OS2__ )
	char	*tempfile;
# endif
} cmdtab[ MAXJOBS ] = {{0}};

/*
 * onintr() - bump intr to note command interruption
 */

void
onintr( disp )
int disp;
{
	intr++;
#ifdef APPLE_EXTENSIONS
	pbx_printf( "JAM ", "...interrupted\n" );
#else
	printf( "...interrupted\n" );
#endif
}

/*
 * execcmd() - launch an async command execution
 */

void
execcmd( string, func, closure, shell, exportvars )
char *string;
void (*func)();
void *closure;
LIST *shell;
int exportvars;
{
	int pid;
	int slot;
	char *argv[ MAXARGC + 1 ];	/* +1 for NULL */
#ifdef APPLE_EXTENSIONS
	// :mferris:20001213 We use a pipe to send commands to sh through stdin instead of passing them as an argument with "-c" since there are problems with the command line arguments somehow getting mangled when they contain weird UTF-8 encoded file names...
	int pipeForStdInToShell[2];
#endif

# if defined( NT ) || defined( __OS2__ )
	static char *comspec;
	char *p;

	/* XXX this is questionable practice, since COMSPEC has
	 * a high degree of variability, resulting in Jamfiles
	 * that frequently won't work.  COMSPEC also denotes a shell
	 * fit for interative use, not necessarily/merely a shell
	 * capable of launching commands.  Besides, people can
	 * just set JAMSHELL instead.
	 */
	if( !comspec && !( comspec = getenv( "COMSPEC" ) ) )
	    comspec = "cmd.exe";
# endif

	/* Find a slot in the running commands table for this one. */

	for( slot = 0; slot < MAXJOBS; slot++ )
	    if( !cmdtab[ slot ].pid )
		break;

	if( slot == MAXJOBS )
	{
	    printf( "no slots for child!\n" );
	    exit( EXITBAD );
	}

# if defined( NT ) || defined( __OS2__ )
	if( !cmdtab[ slot ].tempfile )
	{
	    char *tempdir;

	    if( !( tempdir = getenv( "TEMP" ) ) &&
		!( tempdir = getenv( "TMP" ) ) )
		    tempdir = "\\temp";

	    cmdtab[ slot ].tempfile = malloc( strlen( tempdir ) + 14 );

	    sprintf( cmdtab[ slot ].tempfile, "%s\\jamtmp%02d.bat", 
				tempdir, slot );
	}

	/* Trim leading, ending white space */

	while( isspace( *string ) )
		++string;

	p = strchr( string, '\n' );

	while( p && isspace( *p ) )
		++p;

	/* If multi line or too long, write to bat file. */
	/* Otherwise, exec directly. */
	/* Frankly, if it is a single long line I don't think the */
	/* command interpreter will do any better -- it will fail. */

	if( p && *p || strlen( string ) > MAXLINE )
	{
	    FILE *f;

	    /* Write command to bat file. */

	    f = fopen( cmdtab[ slot ].tempfile, "w" );
	    fputs( string, f );
	    fclose( f );

	    string = cmdtab[ slot ].tempfile;
	}
# endif
	
	/* Forumulate argv */
	/* If shell was defined, be prepared for % and ! subs. */
	/* Otherwise, use stock /bin/sh (on unix) or comspec (on NT). */

	if( shell )
	{
	    int i;
	    char jobno[4];
	    int gotpercent = 0;

	    sprintf( jobno, "%d", slot + 1 );

	    for( i = 0; shell && i < MAXARGC; i++, shell = list_next( shell ) )
	    {
		switch( shell->string[0] )
		{
		case '%':	argv[i] = string; gotpercent++; break;
		case '!':	argv[i] = jobno; break;
		default:	argv[i] = shell->string;
		}
		if( DEBUG_EXECCMD )
		    printf( "argv[%d] = '%s'\n", i, argv[i] );
	    }

	    if( !gotpercent )
		argv[i++] = string;

	    argv[i] = 0;
	}
	else
	{
# if defined( NT ) || defined( __OS2__ )
	    argv[0] = comspec;
	    argv[1] = "/Q/C";		/* anything more is non-portable */
	    argv[2] = string;
	    argv[3] = 0;
# else /* ! ( defined( NT ) || defined( __OS2__ ) ) */
	    argv[0] = "/bin/sh";
#ifdef APPLE_EXTENSIONS
	    argv[1] = "-s";
	    argv[2] = NULL;
#else /* ! ( APPLE_EXTENSIONS ) */
	    argv[1] = "-c";
	    argv[2] = string;
	    argv[3] = 0;
#endif /* ! ( APPLE_EXTENSIONS ) */

# endif /* ! ( defined( NT ) || defined( __OS2__ ) ) */
	}

	/* Catch interrupts whenever commands are running. */

	if( !cmdsrunning++ )
	    istat = signal( SIGINT, onintr );

#ifdef APPLE_EXTENSIONS
	/* Set up pipes, etc so that we can correctly multiplex output from multiple simultaneous commands */
    #if 0 // not yet implemented -- we assume one command at a time for now
	{
	    int   stdout_pipe[2], stderr_pipe[2];

	    pipe(stdout_pipe);
	    pipe(stderr_pipe);
	    printf("stdout_pipe[0]=%i, stdout_pipe[1]=%i\n", stdout_pipe[0], stdout_pipe[1]);
	    printf("stderr_pipe[0]=%i, stderr_pipe[1]=%i\n", stderr_pipe[0], stderr_pipe[1]);
	}
    #endif
#endif

	/* Start the command */

# if defined( NT ) || defined( __OS2__ )
	if( ( pid = spawnvp( P_NOWAIT, argv[0], argv ) ) < 0 )
	{
	    perror( "spawn" );
	    exit( EXITBAD );
	}
# else
#ifdef APPLE_EXTENSIONS
	// Create the pipe we will use to send stdin to child
	pipe(pipeForStdInToShell);
#endif

	if ((pid = vfork()) == 0) 
   	{
#ifdef APPLE_EXTENSIONS
	    // CHILD: Close write end of pipe in child and make stdin be the read end.
	    close(pipeForStdInToShell[1]);
	    dup2(pipeForStdInToShell[0], 0);
#endif
	    if (exportvars)
		var_setenv_all_exported_variables();
	    //else
		//printf("NOT ");
	    //printf("doing setenv for exec of %s\n", string);
	    execvp( argv[0], argv );
	    _exit(127);
	}

	if( pid == -1 )
	{
	    perror( "vfork" );
	    exit( EXITBAD );
	}
# endif
	/* Save the operation for execwait() to find. */

	cmdtab[ slot ].pid = pid;
	cmdtab[ slot ].func = func;
	cmdtab[ slot ].closure = closure;

#ifdef APPLE_EXTENSIONS
	// PARENT: 
	// Close read end of pipe in parent.
	close(pipeForStdInToShell[0]);
	// Write the script string to the write end of the pipe, then close that one too.
	{
	    int toWrite = strlen(string);
	    int written = write(pipeForStdInToShell[1], string, toWrite);
	    
	    if (toWrite != written) {
		printf("Error: only wrote %d bytes of %d bytes of command to sub-shell.", written, toWrite);
	    }
	}
	close(pipeForStdInToShell[1]);
#endif

	/* Wait until we're under the limit of concurrent commands. */
	/* Don't trust globs.jobs alone. */

	while( cmdsrunning >= MAXJOBS || cmdsrunning >= globs.jobs )
	    if( !execwait() )
		break;
}

/*
 * execwait() - wait and drive at most one execution completion
 */

int
execwait()
{
	int i;
	int status, w;
	int rstat;

	/* Handle naive make1() which doesn't know if cmds are running. */

	if( !cmdsrunning )
	    return 0;

	/* Pick up process pid and status */
    
	while( ( w = wait( &status ) ) == -1 && errno == EINTR )
		;

	if( w == -1 )
	{
	    printf( "child process(es) lost!\n" );
	    perror("wait");
	    exit( EXITBAD );
	}

	/* Find the process in the cmdtab. */

	for( i = 0; i < MAXJOBS; i++ )
	    if( w == cmdtab[ i ].pid )
		break;

	if( i == MAXJOBS )
	{
	    printf( "waif child found!\n" );
	    exit( EXITBAD );
	}

	/* Drive the completion */

	if( !--cmdsrunning )
	    signal( SIGINT, istat );

	if( intr )
	    rstat = EXEC_CMD_INTR;
	else if( w == -1 || status != 0 )
	    rstat = EXEC_CMD_FAIL;
	else
	    rstat = EXEC_CMD_OK;

	cmdtab[ i ].pid = 0;
	
	(*cmdtab[ i ].func)( cmdtab[ i ].closure, rstat );

	return 1;
}

# if defined( NT ) && !defined( __BORLANDC__ )

# define WIN32_LEAN_AND_MEAN

# include <windows.h>		/* do the ugly deed */

static int
my_wait( status )
int *status;
{
	int i, num_active = 0;
	DWORD exitcode, waitcode;
	static HANDLE *active_handles = 0;

	if (!active_handles)
	    active_handles = (HANDLE *)malloc(globs.jobs * sizeof(HANDLE) );

	/* first see if any non-waited-for processes are dead,
	 * and return if so.
	 */
	for ( i = 0; i < globs.jobs; i++ ) {
	    if ( cmdtab[i].pid ) {
		if ( GetExitCodeProcess((HANDLE)cmdtab[i].pid, &exitcode) ) {
		    if ( exitcode == STILL_ACTIVE )
			active_handles[num_active++] = (HANDLE)cmdtab[i].pid;
		    else {
			CloseHandle((HANDLE)cmdtab[i].pid);
			*status = (int)((exitcode & 0xff) << 8);
			return cmdtab[i].pid;
		    }
		}
		else
		    goto FAILED;
	    }
	}

	/* if a child exists, wait for it to die */
	if ( !num_active ) {
	    errno = ECHILD;
	    return -1;
	}
	waitcode = WaitForMultipleObjects( num_active,
					   active_handles,
					   FALSE,
					   INFINITE );
	if ( waitcode != WAIT_FAILED ) {
	    if ( waitcode >= WAIT_ABANDONED_0
		&& waitcode < WAIT_ABANDONED_0 + num_active )
		i = waitcode - WAIT_ABANDONED_0;
	    else
		i = waitcode - WAIT_OBJECT_0;
	    if ( GetExitCodeProcess(active_handles[i], &exitcode) ) {
		CloseHandle(active_handles[i]);
		*status = (int)((exitcode & 0xff) << 8);
		return (int)active_handles[i];
	    }
	}

FAILED:
	errno = GetLastError();
	return -1;
    
}

# endif /* NT && !__BORLANDC__ */

# endif /* unix || NT || __OS2__ */
