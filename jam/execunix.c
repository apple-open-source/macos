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
#ifdef APPLE_EXTENSIONS
# include "timingdata.h"
# include <sys/resource.h>
#endif

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
#ifdef APPLE_EXTENSIONS
	int	output_fd;	  /* file descriptor on which we're receiving stderr output from the command */
	char *  output_buffer;    /* buffer of characters we haven't emitted yet */
	int     output_capacity;  /* capacity of 'output_buffer' */
	int     output_length;    /* number of characters we haven't emitted yet */
#endif
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
	pbx_printf( "GMSG", "...interrupted\n" );
#else
	printf( "...interrupted\n" );
#endif
}

int next_available_cmd_slot ()
{
    int slot;
    for( slot = 0; slot < MAXJOBS; slot++ )
	if( !cmdtab[ slot ].pid )
	    return slot;
    return -1;
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
	static char usePipeForShellCommands = -1;
#endif
#ifdef APPLE_EXTENSIONS
	// We also use a pipe to capture data from subprocesses, so that we can demux that output during parallel builds.  At least for now, we combine stdout and stderr output into the same pipe.
	int output_pipe[2];
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

#ifdef APPLE_EXTENSIONS
	if( usePipeForShellCommands == -1 ) {
	    if( getenv( "USE_CMDLINE_FOR_SHELL_COMMANDS" ) != NULL ) {
		/*printf("NOT USING PIPES\n");*/
		usePipeForShellCommands = 0;
	    } else {
		/*printf("USING PIPES\n");*/
		usePipeForShellCommands = 1;
	    }
	}
#endif

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
	    if (usePipeForShellCommands) {
		argv[1] = "-s";
		argv[2] = NULL;
	    } else {
		argv[1] = "-c";
		argv[2] = string;
		argv[3] = 0;
	    }
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

	/* Start the command */

# if defined( NT ) || defined( __OS2__ )
	if( ( pid = spawnvp( P_NOWAIT, argv[0], argv ) ) < 0 )
	{
	    perror( "spawn" );
	    exit( EXITBAD );
	}
# else
#ifdef APPLE_EXTENSIONS
	if (usePipeForShellCommands) {
	    // Create the pipe we will use to send stdin to child
	    pipe(pipeForStdInToShell);
	}
#endif

#ifdef APPLE_EXTENSIONS
	/* Create a pipe so that we can correctly multiplex output from multiple simultaneous commands */
	if (PARSABLE_OUTPUT) {
	    pipe(output_pipe);
	}
#endif

	if ((pid = vfork()) == 0) 
   	{
#ifdef APPLE_EXTENSIONS
	    if (usePipeForShellCommands) {
		// CHILD: Close write end of pipe in child and make stdin be the read end.
		close(pipeForStdInToShell[1]);
		dup2(pipeForStdInToShell[0], 0);
	    }
#endif

#ifdef APPLE_EXTENSIONS
	    if (PARSABLE_OUTPUT) {
		// CHILD: Close the read end of output_pipe in the child, and dup the write end to both stdout and stderr.
		close(output_pipe[0]);
		dup2(output_pipe[1], 1);
		dup2(output_pipe[1], 2);
	    }
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
	if (usePipeForShellCommands) {
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
	}
#endif

#ifdef APPLE_EXTENSIONS
	if (PARSABLE_OUTPUT) {
	    // PARENT: Close the write ends of output_pipe.  Keep track of the file descriptor for the read end, so that we can select() on it.
	    cmdtab[slot].output_fd = output_pipe[0];
	    close(output_pipe[1]);
	    cmdtab[slot].output_capacity = 16 * 1024;
	    cmdtab[slot].output_buffer = malloc(cmdtab[slot].output_capacity);
	    cmdtab[slot].output_length = 0;
	    if (DEBUG_PARSABLE_OUTPUT) {
		printf("cmdtab[%i] = {output_fd=%i, output_capacity=%i, output_buffer=%p, output_length=%u}\n", slot, cmdtab[slot].output_fd, cmdtab[slot].output_capacity, cmdtab[slot].output_buffer, cmdtab[slot].output_length);
	    }
	}
	else {
	    cmdtab[slot].output_fd = -1;
	    cmdtab[slot].output_capacity = 0;
	    cmdtab[slot].output_buffer = NULL;
	    cmdtab[slot].output_length = 0;
	}
#endif
	/* Wait until we're under the limit of concurrent commands. */
	/* Don't trust globs.jobs alone. */

	while( cmdsrunning >= MAXJOBS || cmdsrunning >= globs.jobs )
	    if( !execwait() )
		break;
}


#ifdef APPLE_EXTENSIONS

int find_cmd_slot_for_fd (int fd)
{
    int i;
    for (i = 0; i < MAXJOBS; i++) {
	if (cmdtab[i].pid != 0  &&  cmdtab[i].output_fd == fd) {
	    return i;
	}
    }
    return -1;
}

void emit_annotated_output_line (const char * line_prefix_string, const unsigned char * string, unsigned length, int add_newline_if_needed)
{
    if (DEBUG_PARSABLE_OUTPUT) {
	//printf("emit_annotated_output_line('%s', '%*.*s', %i)\n", line_prefix_string, length, length, string, add_newline_if_needed);
    }
    pbx_printf(line_prefix_string, "");
    fwrite(string, length, 1, stdout);
    if (add_newline_if_needed  &&  (length == 0 || string[length-1] != '\n')) {
	printf("\n");
    }
}

void emit_annotated_output_lines_for_cmd_slot (int slot, int empty_the_buffer)
{
    // Construct the line prefix string, of the form "ROxx" (where xx is the slot number).
    unsigned char line_prefix_string[5] = "RO00";
    line_prefix_string[2] = '0' + slot / 10;
    line_prefix_string[3] = '0' + slot % 10;

    // Emit all the complete lines.
    const char * buf_ptr = cmdtab[slot].output_buffer;
    const char * first_unemitted_char = buf_ptr;
    const char * buf_limit = buf_ptr + cmdtab[slot].output_length;
    while (buf_ptr < buf_limit) {

	// Spool to the first character after the next newline.
	while (buf_ptr < buf_limit  &&  *buf_ptr++ != '\n');

	// Unless we're at the end, we emit this line.
	if (buf_ptr < buf_limit  ||  (buf_ptr == buf_limit  &&  buf_ptr > first_unemitted_char  &&  buf_ptr[-1] == '\n')) {
	    emit_annotated_output_line(line_prefix_string, first_unemitted_char, buf_ptr - first_unemitted_char, 0 /*add_newline_if_needed*/);
	    first_unemitted_char = buf_ptr;
	}
    }

    // If we're supposed to completely empty the buffer, we do so now (and append a trailing '\n', if needed).   Otherwise, we keep the last chars.
    if (empty_the_buffer) {
	if (buf_limit - first_unemitted_char > 0) {
	    emit_annotated_output_line(line_prefix_string, first_unemitted_char, buf_limit - first_unemitted_char, 1 /*add_newline_if_needed*/);
	}
	cmdtab[slot].output_length = 0;
    }
    else {
	unsigned num_remaining_chars = buf_limit - first_unemitted_char;
	memcpy(cmdtab[slot].output_buffer, first_unemitted_char, num_remaining_chars);
	cmdtab[slot].output_length = num_remaining_chars;
    }
}

void append_to_cmd_slot_buffer (int slot, const char * bytes, unsigned length)
{
    if (DEBUG_PARSABLE_OUTPUT) {
	printf("append_to_cmd_slot_buffer(%i, %p, %u)\n", slot, bytes, length);
    }
    // Grow the buffer, if needed.
    int capacity = cmdtab[slot].output_capacity;
    while (cmdtab[slot].output_length + length > (unsigned)capacity) {
	capacity += (capacity < 65536) ? capacity : 16384;
    }
    if (capacity != cmdtab[slot].output_capacity) {
	cmdtab[slot].output_buffer = realloc(cmdtab[slot].output_buffer, capacity);
	cmdtab[slot].output_capacity = capacity;
    }
    // Append the bytes.
    memcpy(cmdtab[slot].output_buffer + cmdtab[slot].output_length, bytes, length);
    cmdtab[slot].output_length += length;
}

int fetch_data_from_cmd_in_slot (int slot)
{
    if (DEBUG_PARSABLE_OUTPUT) {
	printf("fetch_data_from_cmd_in_slot(%i)\n", slot);
    }
    unsigned char buffer[4096];
    int result = read(cmdtab[slot].output_fd, buffer, sizeof(buffer));
    if (result == 0) {
	// Writer has closed its end; close our read end and emit any buffered incomplete lines.
	if (DEBUG_PARSABLE_OUTPUT) {
	    printf("closing %i\n", cmdtab[slot].output_fd);
	}
	close(cmdtab[slot].output_fd);
	cmdtab[slot].output_fd = -1;
	emit_annotated_output_lines_for_cmd_slot(slot, 1 /*empty_the_buffer*/);
	return 0;
    }
    else if (result >= 0) {
	// Buffer the data.
	append_to_cmd_slot_buffer(slot, buffer, result);
	// Emit any complete lines.
	emit_annotated_output_lines_for_cmd_slot(slot, 0 /*empty_the_buffer*/);
    }
    return result;
}

void print_fd_set (const fd_set * fd_set_ptr, unsigned highest_fd)
{
    unsigned   i, is_first = 1;

    for (i = 0; i <= highest_fd; i++) {
	if (FD_ISSET(i, fd_set_ptr)) {
	    printf("%s%i", is_first ? "" : ",", i);
	    is_first = 0;
	}
    }
}

#endif

/*
 * execwait() - wait and drive at most one execution completion
 */

int
execwait()
{
	int i;
	int status, w;
	int rstat;
#ifdef APPLE_EXTENSIONS
	struct rusage ru;
#endif

	/* Handle naive make1() which doesn't know if cmds are running. */

	if( !cmdsrunning )
	    return 0;

	/* First gather input for all active children */
#ifdef APPLE_EXTENSIONS
	int slot_to_wait_for = -1;
	if (PARSABLE_OUTPUT) {
	    do {
		fd_set    readable_fd_set;
		fd_set    exception_fd_set;
		int       i, lowest_descriptor = 1024, highest_descriptor = -1;

		FD_ZERO(&readable_fd_set);
		FD_ZERO(&exception_fd_set);
		for (i = 0; i < MAXJOBS; i++) {
		    if (cmdtab[i].pid != 0) {
			if (cmdtab[i].output_fd >= 0) {
			    FD_SET(cmdtab[i].output_fd, &readable_fd_set);
			    FD_SET(cmdtab[i].output_fd, &exception_fd_set);
			    if (cmdtab[i].output_fd > highest_descriptor) {
				highest_descriptor = cmdtab[i].output_fd;
			    }
			    if (cmdtab[i].output_fd < lowest_descriptor) {
				lowest_descriptor = cmdtab[i].output_fd;
			    }
			}
		    }
		}
		if (DEBUG_PARSABLE_OUTPUT) {
		    //printf("highest_descriptor=%i\n", highest_descriptor);
		}
		if (highest_descriptor < 0) {
		    break;
		}

		if (DEBUG_PARSABLE_OUTPUT) {
		    printf("select(num_bits=%i, readable_fd_set={", highest_descriptor+1);
		    print_fd_set(&readable_fd_set, highest_descriptor);
		    printf("}, writable_fd_set=NULL, exception_fd_set={");
		    print_fd_set(&exception_fd_set, highest_descriptor);
		    printf("}, timeout=NULL) -> ");
		}
		fflush(stdout);
		int num_ready = select(highest_descriptor+1, &readable_fd_set, NULL, &exception_fd_set, NULL);
		if (DEBUG_PARSABLE_OUTPUT) {
		    printf("num_ready=%i, readable_fd_set={", num_ready);
		    print_fd_set(&readable_fd_set, highest_descriptor);
		    printf("}, exception_fd_set={");
		    print_fd_set(&exception_fd_set, highest_descriptor);
		    printf("}");
		    if (num_ready < 0) {
			printf(", errno=%i(%s)\n", errno, strerror(errno));
		    }
		    printf("\n");
		}
		if (num_ready < 0) {
		    break;
		}
		for (i = lowest_descriptor; i <= highest_descriptor; i++) {
		    if (FD_ISSET(i, &readable_fd_set)) {
			int slot = find_cmd_slot_for_fd(i);
			if (DEBUG_PARSABLE_OUTPUT) {
			    printf("find_cmd_slot_for_fd(%i) -> %i\n", i, slot);
			}
			if (slot >= 0) {
			    fetch_data_from_cmd_in_slot(slot);
			    if (cmdtab[slot].output_fd == -1) {
				slot_to_wait_for = slot;
			    }
			    break;
			}
		    }
		}
		for (i = lowest_descriptor; i <= highest_descriptor; i++) {
		    if (FD_ISSET(i, &exception_fd_set)) {
			break;
		    }
		}
	    }
	    while (slot_to_wait_for == -1);
	}
#endif

	/* Pick up process pid and status */
	if (DEBUG_PARSABLE_OUTPUT) {
	    printf("waitpid(cmdtab[%i].pid)\n", slot_to_wait_for);
	}
#ifdef APPLE_EXTENSIONS
	while( ( w = wait4( slot_to_wait_for >= 0 ? cmdtab[slot_to_wait_for].pid : -1, &status, 0, &ru ) ) == -1 && errno == EINTR )
		;
#else
	while( ( w = waitpid( slot_to_wait_for >= 0 ? cmdtab[slot_to_wait_for].pid : -1, &status, 0 ) ) == -1 && errno == EINTR )
	    ;
#endif

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

#ifdef APPLE_EXTENSIONS
	if( globs.enable_timings ) {
	    record_last_resource_usage( &ru );
	}

	if (cmdtab[i].output_fd != -1) {
	    close(cmdtab[i].output_fd);
	    cmdtab[i].output_fd = -1;
	}
	if (DEBUG_PARSABLE_OUTPUT) {
	    printf("getting rid of buffer for slot %i\n", i);
	}
	free(cmdtab[i].output_buffer), cmdtab[i].output_buffer = NULL;
	cmdtab[i].output_capacity = 0;
	cmdtab[i].output_length = 0;
#endif
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
