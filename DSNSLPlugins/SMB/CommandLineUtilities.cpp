/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
 
/*!
 *	@header CommandLineUtilities
 *	Implement a way to execute a command and get back its result.
 */ 

#include "CommandLineUtilities.h"
#include "NSLDebugLog.h"
#include "CNSLTimingUtils.h"

#if USE_RASDEBUGLOG
#define DEBUG 0
extern void RASDebugLogV(const char* inFormat, va_list stuff);
#endif

/* Globals */
FILE 				*ec_log_file = NULL;
static pid_t		*ec_childpid = NULL;

/*-----------------------------------------------------------------------------
 *	executecommand
 *---------------------------------------------------------------------------*/
int executecommand(char *command, char **output)
{
	Boolean	canceled = false;
	char*	outPtr = NULL;
	int		returnVal;

	returnVal = myexecutecommandas( command, NULL, NULL, true, kTimeOutVal, &outPtr, &canceled, getuid(), getgid() );
	
	if ( output )
		*output = outPtr;
	else if ( outPtr )
		free( outPtr );
		
	return returnVal;
}


int myexecutecommandas(const char *command, const char* path, const char * argv[], Boolean useSHELL, size_t timeout_delay, char **output, Boolean* canceledFlag,
	uid_t uid, gid_t gid)
{
	FILE	*pipe = NULL;
#ifdef LOG_TO_FILE
	#warning "DEBUG CODE, DO NOT SUBMIT!!!!"
	FILE *				destFP = NULL;
#endif
	size_t	output_size = 0;
	size_t	output_count = 0;
	int		pipe_fd = 0;
	char	line[BUFSIZ+1] = {0};
	int		has_timedout = 0;
	time_t	starting_time = time(NULL);

	/* Open a path to execute command */
	pipe = ec_popen(command, path, argv, useSHELL, uid, gid);

	if (pipe == NULL) {
		DBGLOG( 
			"executecommand(): can't popen(): %s\n", strerror(errno));
		goto executecommand_exit;
	}
	pipe_fd = fileno(pipe);

//#define NO_BLOCK_ON_READ
#ifdef NO_BLOCK_ON_READ
	int		flags = 0;
	/* Make sure reads won't block */
	if ((flags = fcntl(pipe_fd, F_GETFL, 0)) < 0) {
		DBGLOG(  
			"executecommand(): can't fcntl(GET): %s\n", strerror(errno));
		goto executecommand_exit;
	}
	flags |= O_NONBLOCK;
	if (fcntl(pipe_fd, F_SETFL, flags) < 0) {
		DBGLOG(  
			"executecommand(): can't fcntl(SET): %s\n", strerror(errno));
		goto executecommand_exit;
	}
#endif
    fd_set	rset;
    
    FD_ZERO(&rset);
	
	/* Read as long as there is data */
	do {
		size_t	line_output_size = 0;
		
		/* Have we exceeded our delay? */
		if (((unsigned long) time(NULL) - 
			(unsigned long) starting_time) > timeout_delay) {

			DBGLOG(  "executecommand(): timed out\n");

			has_timedout = 1;
			break;
		}
		
		/* Clear last file error */
		clearerr(pipe);
		
        
        FD_SET(pipe_fd, &rset);
        struct timeval tv = {1,0};

        if ( select(pipe_fd+1, &rset, NULL, NULL, &tv) > 0 )
		{
			/* Call read in non-blocking way (may return EAGAIN) */
			if (has_timedout == 0) 
			{
				line_output_size = fread(line, 1, BUFSIZ, pipe);
	
				if (line_output_size == 0) 
				{
					continue;
				}
			}
				
			/* Feed output buffer if asked */
			if (output != NULL && line_output_size > 0) 
			{
				if (output_size == 0) 
				{
					if (*output != NULL)
					{
						free(*output);
					}
					
					output_size = line_output_size + 1;
					*output = (char*)malloc(output_size);
				}
				else 
				{
					output_size += line_output_size;
					*output = (char*)realloc(*output, output_size);
				}
				
				if (*output != NULL) 
				{
					memcpy(&(*output)[output_count], line, line_output_size);
					output_count += line_output_size;
					(*output)[output_count] = 0;
				}
			}
		}
	} while (feof(pipe) == 0 && has_timedout == 0 && !(*canceledFlag));
#ifdef LOG_TO_FILE
	#warning "DEBUG CODE, DO NOT SUBMIT!!!!"
		// let's dump this out to a file
		destFP = fopen( "/tmp/myexecutecommandas.out", "a" );
		char				headerString[1024];
		
		if ( destFP )
		{
			sprintf( headerString, "\n**** Results %d bytes %d strlen ****\n", output_count, (*output)?strlen( *output ):0);
			fputs( headerString, destFP );
			if ( (*output) )
				fputs( *output, destFP );
			fputs( "\n**** endof results *****\n\n", destFP );
			fclose( destFP );
		}
		else
			syslog( LOG_ALERT, "COULD NOT OPEN /tmp/myexecutecommandas.out!\n" );
#endif	
	/* Clear error code if no error or if EAGAIN error */
	if (ferror(pipe) == 0 || errno == EAGAIN) {
		errno = 0;
	}
	
	/* Remove all weird characters except LF and CR and tab*/
/*	if (output != NULL && *output != NULL && !(*canceledFlag)) {
		unsigned char	*p = (unsigned char*) *output;
		for (; *p != (unsigned char) 0; p++) {
			if (*p == (unsigned char) 0x0A) continue;
			if (*p == (unsigned char) 0x0D) continue;
			if (*p == (unsigned char) 0x09) continue;
			if ((*p < (unsigned char) 0x20) 
				|| (*p > (unsigned char) 0x7F)) *p = 0x20;
		}
	}
*/
	/* Exit (keep track of exit status unless a timeout occurred) */
executecommand_exit:	
	if (pipe != NULL) {
		int result = ec_pclose(pipe, has_timedout);

		if (has_timedout == 1 && errno == ESRCH) {
			errno = ENOERR;
		} else {
			errno = result;
		}
	}

	if (errno != ENOERR) {
		DBGLOG(  
			"executecommand(%s): errno=%d, %s\n", command, errno, strerror(errno));
	}

	return errno;
}


#pragma mark-
/*=============================================================================
 *								SUPPORT SECTION
 *===========================================================================*/
/*-----------------------------------------------------------------------------
 *	ec_popen
 *	Mimic the popen(cmdstring, "r") function.
 *---------------------------------------------------------------------------*/
FILE *ec_popen(const char *cmdstring, const char* path, const char * argv[], Boolean useSHELL, uid_t uid, gid_t gid)
{
	int		pfd[2] = {0,};
	pid_t	pid = 0;
	FILE	*fp = NULL;
	
	if (ec_childpid == NULL) {
		ec_childpid = (pid_t*) calloc(NOFILE, sizeof(pid_t));
		if (ec_childpid == NULL) return NULL;
	}
	
	/* Open a pipe (usually 'pfd[0]'=3 reads from 'pfd[1]'=4) */
	if (pipe(pfd) < 0) {
		return NULL;
	}

	/* Fork the child */
	if ((pid = vfork()) < 0) {
		(void) close(pfd[0]);
		(void) close(pfd[1]);
		return NULL;
	}
	
	/* Child */
	if (pid == 0) {
		int	i,new_fd1=-1,new_fd2=-1;
		
		/* Link stdout to 'pfd[1]': fd#1 shares fd#4 file table entry */
		(void) close(pfd[0]);
		if (pfd[1] != STDOUT_FILENO) {
			new_fd1 = dup2(pfd[1], STDOUT_FILENO);
			(void) close(pfd[1]);
		}
		
		/* Link stderr to stdout: fd#2 shares fd#1 file table entry */
		new_fd2 = dup2(STDOUT_FILENO, STDERR_FILENO);

		/* Close all descriptors in child's copy of 'ec_childpid', */
		/* including those inherited from parents and that were copied */
		/* during the fork(), such as open sockets, etc. */
		for (i = 0; i < NOFILE; i++) {
			if (new_fd1 != -1 && i == new_fd1) continue;
			if (new_fd2 != -1 && i == new_fd2) continue;
            
            if (i == STDIN_FILENO) continue;
            if (i == STDOUT_FILENO) continue;
            if (i == STDERR_FILENO) continue;
            
			(void) close(i);
		}
		
		/* Impersonate 'uid/gid' and exec the command */
		(void) setgid(gid);
		(void) setuid(uid);

		setsid ();	// divorce ourselves from our parents process space

		if (useSHELL)
			execl(SHELL, "sh", "-c", cmdstring, (char*) NULL);
		else
			execvp(path, argv);

		_exit(127);
	}
	
	if (useSHELL)
	{
		DBGLOG( "executecommand(%s): child #%d forked...\n", cmdstring, pid);
	}
	else if ( getenv("NSLDEBUG") )
	{
		const char*	curPtr = NULL;
		int		i =0;
		DBGLOG( "executecommand(" );
		
		while ( (curPtr = argv[i++]) )
			DBGLOG( "%s ", curPtr );
			
		DBGLOG( "): child #%d forked...\n", cmdstring, pid);
	}
	
	/* Parent */
	(void) close(pfd[1]);
	if ((fp = fdopen(pfd[0], "r")) == NULL) return NULL;
	ec_childpid[fileno(fp)] = pid;
	return fp;
}
	
	
/*-----------------------------------------------------------------------------
 *	ec_pclose
 *	Mimic the pclose(FILE*) function. Kill the shell process we spawned and 
 *	all its children if 'killit'=1. Return exit status of the shell process
 *	we spawned (MSB only, the LSB or signal number is stripped out) or
 *	'ECHILD' if the process was prematurely killed.
 *---------------------------------------------------------------------------*/
#define KILL_CHILDREN_IF_TIMEOUT 1
int ec_pclose(FILE *fp, int killit)
{
	int		fd,stat;
	pid_t	pid;
	int		killed = 0;
	
	if (ec_childpid == NULL) return -1;
	
	fd = fileno(fp);
	if ((pid = ec_childpid[fd]) == 0) return -1;
	
	ec_childpid[fd] = 0;
	if (fclose(fp) == EOF) return -1;
	
	#if KILL_CHILDREN_IF_TIMEOUT
	if (killit == 1) {
		ec_terminate_process_by_id(pid);
		DBGLOG( 
			"executecommand(): pclose() killed pid #%d and its children\n", pid);
		killed = 1;
	}
	#endif

	while (waitpid(pid, &stat, 0) < 0) {
		if (errno == ECHILD) {
			errno = ENOERR;
			return killed == 1 ? ECHILD : ENOERR;
		}
		if (errno != EINTR) {
			return -1;
		}
	}

	if (killit == 1 && WTERMSIG(stat) == SIGTERM) return ECHILD;
	
	return WEXITSTATUS(stat);
}


/*-----------------------------------------------------------------------------
 *	ec_fprintf
 *	Support routine. Only used for debugging, i.e., to log debug messages.
 *---------------------------------------------------------------------------*/
int ec_fprintf(FILE *file, const char *format, ...) 
{
	#define MAX_FORMAT_STR_SIZE	256
	char	my_format[MAX_FORMAT_STR_SIZE];
    int		result = 0;
    
	sprintf(my_format, "[%d] ", (int) getpid());
	strncat(my_format, format, MAX_FORMAT_STR_SIZE - strlen(my_format) -1);
	if (file == NULL) {
		#if defined(STANDALONE) || (defined(DEBUG) && !defined(USE_RASDEBUGLOG))
	    result = vfprintf(stdout, my_format, 
			(va_list)((long)&format + (long)sizeof(char*)));
		#else
		#if USE_RASDEBUGLOG
		RASDebugLogV(my_format, 
			(va_list)((long)&format + (long)sizeof(char*)));
		#endif
		#endif
	} else {
		#if defined(STANDALONE) || (defined(DEBUG) && !defined(USE_RASDEBUGLOG))
	    result = vfprintf(file, my_format, 
			(va_list)((long)&format + (long)sizeof(char*)));
		fflush(file);
		#else
		#if USE_RASDEBUGLOG
		RASDebugLogV(my_format, 
			(va_list)((long)&format + (long)sizeof(char*)));
		#endif
		#endif
	}
    return result;
}


#pragma mark-
/*-----------------------------------------------------------------------------
 *	ec_terminate_process_by_id
 *	Terminate asked process and all its children. Beware that daemons should
 *	not be terminated using this routine because it attempts first to kill
 *	the children but that might be of no avail if the parent daemon keeps
 *	reforking them!
 *---------------------------------------------------------------------------*/
void ec_terminate_process_by_id(int pid)
{
	int			mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
	size_t		buf_size;
	int			result;
	result = sysctl(mib, 4, NULL, &buf_size, NULL, 0);
	if (result >= 0) {
		struct kinfo_proc	*processes = NULL;
    	int					i,nb_entries;
    	nb_entries = buf_size / sizeof(struct kinfo_proc);
    	processes = (struct kinfo_proc*) malloc(buf_size);
    	if (processes != NULL) {
    		result = sysctl(mib, 4, processes, &buf_size, NULL, 0);
    		if (result >= 0) {
			    for (i = 0; i < nb_entries; i++) {
			    	if (processes[i].kp_eproc.e_ppid != pid) continue;
					(void) kill(processes[i].kp_proc.p_pid, SIGTERM);
					(void) kill(processes[i].kp_proc.p_pid, SIGKILL);
			    }
    		}
    		free(processes);
    	}
	}
	(void) kill(pid, SIGTERM);
	(void) kill(pid, SIGKILL);
}


/*-----------------------------------------------------------------------------
 *	ec_terminate_process_by_name
 *	Terminate asked process(es).
 *---------------------------------------------------------------------------*/
int ec_terminate_process_by_name(const char *name)
{
	int			mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
	size_t		buf_size;
	int			result;
	result = sysctl(mib, 4, NULL, &buf_size, NULL, 0);
	if (result >= 0) {
		struct kinfo_proc	*processes = NULL;
    	int					i,nb_entries;
    	nb_entries = buf_size / sizeof(struct kinfo_proc);
    	processes = (struct kinfo_proc*) malloc(buf_size);
    	if (processes != NULL) {
    		result = sysctl(mib, 4, processes, &buf_size, NULL, 0);
    		if (result >= 0) {
			    for (i = 0; i < nb_entries; i++) {
			    	if (processes[i].kp_proc.p_comm == NULL ||
			 			strcmp(processes[i].kp_proc.p_comm, name) != 0) continue;
					(void) kill(processes[i].kp_proc.p_pid, SIGTERM);
					(void) kill(processes[i].kp_proc.p_pid, SIGKILL);
			    }
    		}
    		free(processes);
    	}
    }
    return errno;
}


/*-----------------------------------------------------------------------------
 *	ec_terminate_daemon_by_name
 *	Terminate asked processes (case-sensitive search) and verify all its
 *	children are terminated as well. Output an English log message. Return 1
 *	if the termination was successful. Wait a few seconds after terminating
 *	the daemon before sending a KILL siginal if it appears it's still running.
 *---------------------------------------------------------------------------*/
int ec_terminate_daemon_by_name(const char *name, char **log)
{
    int					mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
	pid_t				ppid = 0;
	struct kinfo_proc	*processes;
    size_t				buf_size;
   	int					i,j,found,nb_entries,result;
   	int					try_again,nb_tries = 10;
   	int					success = 0;
	
	/* First look for pid of parent  */
    result = sysctl(mib, 4, NULL, &buf_size, NULL, 0);
	if (result < 0) return success;	
	nb_entries = buf_size / sizeof(struct kinfo_proc);
	if (nb_entries > 0) {
		processes = (struct kinfo_proc*) malloc(buf_size);
		if (processes == NULL) return success;
		result = sysctl(mib, 4, processes, &buf_size, NULL, 0);
 		if (result < 0) {
 			free(processes);
 			return success;	
 		}
	    for (found = i = 0; i < nb_entries && found == 0; i++) {
	    	if (processes[i].kp_proc.p_comm == NULL ||
	 			strcmp(processes[i].kp_proc.p_comm, name) != 0) continue;
	 		ppid = processes[i].kp_eproc.e_ppid;
	 		for (j = 0 ; j < nb_entries; j++) {
	 			if (i == j || processes[j].kp_proc.p_pid != ppid) continue;
		    	if (processes[j].kp_proc.p_comm == NULL ||
		 			strcmp(processes[j].kp_proc.p_comm, name) != 0) continue;
		 		found = 1;
	 			break;
	 		}
	    }
		free(processes);
	}
	
	/* Terminate parent and set up our log */
	if (ppid > 0) {
		result = kill(ppid, SIGTERM);
		DBGLOG( 
			"terminate_daemon(%s): terminated pid #%d (%d)\n", 
			name, (int) ppid, result);
		if (result == ENOERR) {
			success = 1;
		    if (log != NULL) {
		    	*log = (char*)malloc(256);
		    	if (*log != NULL) {
			    	sprintf(*log, "%s: stopped (pid #%d)\n", 
			    		name, (int) ppid);
				}
			}
		} else {
		    if (log != NULL) {
		    	*log = (char*)malloc(256);
		    	if (*log != NULL) {
			    	sprintf(*log, "%s: could not stop\n", name);
				}
			}
		}
	} else {
		success = 1;
	    if (log != NULL) {
	    	*log = (char*)malloc(256);
	    	if (*log != NULL) {
		    	sprintf(*log, "%s: already stopped\n", name);
			}
		}
	}
	
	/* No matter the outcome of the above SIGTERM make sure everybody's dead */
	/* but wait a bit before (else, for instance, apachectl start may fail) */
	do {
		SmartSleep(2*USEC_PER_SEC);
		try_again = 0;
	    result = sysctl(mib, 4, NULL, &buf_size, NULL, 0);
		if (result < 0) return success;	
		nb_entries = buf_size / sizeof(struct kinfo_proc);
		if (nb_entries > 0) {
			processes = (struct kinfo_proc*) malloc(buf_size);
			if (processes == NULL) return success;
			result = sysctl(mib, 4, processes, &buf_size, NULL, 0);
	 		if (result < 0) {
	 			free(processes);
	 			return success;	
	 		}
		    for (i = 0; i < nb_entries; i++) {
		    	if (processes[i].kp_proc.p_comm == NULL ||
		 		strcmp(processes[i].kp_proc.p_comm, name) != 0) continue;
		 		result = kill(processes[i].kp_proc.p_pid, SIGTERM);
		 		if (result != 0) 
				result = kill(processes[i].kp_proc.p_pid, SIGKILL);
				try_again = 1;

				DBGLOG( 
					"terminate_daemon(%s): force-killed pid #%d (%d)\n", 
					name, (int) processes[i].kp_proc.p_pid, result);
		    }
		    free(processes);
	    }
	} while (try_again == 1 && nb_tries-- > 0);
	
   	return success;
}
