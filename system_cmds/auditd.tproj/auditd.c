/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 2004 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <mach/port.h>
#include <mach/mach_error.h>
#include <mach/mach_traps.h>
#include <mach/mach.h>
#include <mach/host_special_ports.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <notify.h>

#include <bsm/audit.h>
#include <bsm/audit_uevents.h>
#include <bsm/libbsm.h>

#include <auditd.h>
#include "auditd_control_server.h"
#include "audit_triggers_server.h"
#define NA_EVENT_STR_SIZE 25

static int ret, minval;
static char *lastfile = NULL;

static int allhardcount = 0;

mach_port_t	bp = MACH_PORT_NULL;
mach_port_t control_port = MACH_PORT_NULL;
mach_port_t signal_port = MACH_PORT_NULL;
mach_port_t port_set = MACH_PORT_NULL;

#ifndef __BSM_INTERNAL_NOTIFY_KEY
#define __BSM_INTERNAL_NOTIFY_KEY "com.apple.audit.change"
#endif  /* __BSM_INTERNAL_NOTIFY_KEY */

TAILQ_HEAD(, dir_ent) dir_q;


/* Error starting auditd */
void fail_exit()
{
	audit_warn_nostart();
	exit(1);
}

/*
 * Free our local list of directory names
 */
void free_dir_q()
{
	struct dir_ent *dirent;

	while ((dirent = TAILQ_FIRST(&dir_q))) {       
		TAILQ_REMOVE(&dir_q, dirent, dirs);
		free(dirent->dirname);
		free(dirent);
	}
}

/*
 * generate the timestamp string
 */
int getTSstr(char *buf, int len)
{
	struct timeval ts;
	struct timezone tzp;
	time_t tt;

	if(gettimeofday(&ts, &tzp) != 0) {
		return -1;
	}
	tt = (time_t)ts.tv_sec;
	if(!strftime(buf, len, "%Y%m%d%H%M%S", gmtime(&tt))) {
		return -1;
	}

	return 0;
}

/*
 * Concat the directory name to the given file name
 * XXX We should affix the hostname also
 */
char *affixdir(char *name, struct dir_ent *dirent) 
{
	char *fn;
	char *curdir;
	const char *sep = "/";

	curdir = dirent->dirname;
	syslog(LOG_INFO, "dir = %s\n", dirent->dirname);

	fn = (char *) malloc (strlen(curdir) + strlen(sep) 
				+ (2 * POSTFIX_LEN) + 1);
	if(fn == NULL) {
		return NULL;
	}
	strcpy(fn, curdir);
	strcat(fn, sep);
	strcat(fn, name);

	return fn;
}

/* Close the previous audit trail file */
int close_lastfile(char *TS)
{
	char *ptr;
	char *oldname;

	if(lastfile != NULL) {
		oldname = (char *)malloc(strlen(lastfile) + 1);
		if(oldname == NULL) {
			return -1;
		}
		strcpy(oldname, lastfile);

		/* rename the last file -- append timestamp */

		if((ptr = strstr(lastfile, NOT_TERMINATED)) != NULL) {
			*ptr = '.'; 
			strcpy(ptr+1, TS);
			if(rename(oldname, lastfile) != 0) {
				syslog(LOG_ERR, "Could not rename %s to %s \n",
						oldname, lastfile);
			}
			else {
				syslog(LOG_INFO, "renamed %s to %s \n",
						oldname, lastfile);
			}
		}

		free(lastfile); 
		free(oldname);

		lastfile = NULL;
	}

	return 0;
}

/*
 * Create the new file name, swap with existing audit file
 */
int swap_audit_file()
{
	char timestr[2 * POSTFIX_LEN];
	char *fn;
	char TS[POSTFIX_LEN];
	struct dir_ent *dirent;

	if(getTSstr(TS, POSTFIX_LEN) != 0) {
		return -1;
	}

	strcpy(timestr, TS);
	strcat(timestr, NOT_TERMINATED);

	/* try until we succeed */
	while((dirent = TAILQ_FIRST(&dir_q))) {
		if((fn = affixdir(timestr, dirent)) == NULL) {
			return -1;
		}

		syslog(LOG_INFO, "New audit file is %s\n", fn);
		if (open(fn, O_RDONLY | O_CREAT, S_IRUSR | S_IRGRP) < 0) {
			perror("File open");
		}
		else if (auditctl(fn) != 0) {
			syslog(LOG_ERR, "auditctl failed! : %s\n", 
				strerror(errno));
		}
		else {
			/* Success */ 
			close_lastfile(TS);
			lastfile = fn;
			return 0;
		}

		/* Tell the administrator about lack of permissions for dirent */ 
		audit_warn_getacdir(dirent->dirname);

		/* Try again with a different directory */
		TAILQ_REMOVE(&dir_q, dirent, dirs);
		free(dirent->dirname);
		free(dirent);
	}
	return -1;
}

/*
 * Read the audit_control file contents
 */
int read_control_file()
{
	char cur_dir[MAX_DIR_SIZE];
	struct dir_ent *dirent;
	au_qctrl_t qctrl;

	/* Clear old values */
	free_dir_q();
	endac(); // force a re-read of the file the next time

        /* Post that the audit config changed */
        notify_post(__BSM_INTERNAL_NOTIFY_KEY);

	/* Read the list of directories into a local linked list */
	/* XXX We should use the reentrant interfaces once they are available */
	while(getacdir(cur_dir, MAX_DIR_SIZE) >= 0) {
		dirent = (struct dir_ent *) malloc (sizeof(struct dir_ent));
		if(dirent == NULL) {
			return -1;
		}	

		dirent->softlim = 0;
		dirent->dirname = (char *) malloc (MAX_DIR_SIZE);
		if(dirent->dirname == NULL) {
			free(dirent);
			return -1;
		}

		strcpy(dirent->dirname, cur_dir);
		TAILQ_INSERT_TAIL(&dir_q, dirent, dirs);
	}

	allhardcount = 0;

	if(swap_audit_file() == -1) {
		syslog(LOG_ERR, "Could not swap audit file\n");	
		/*
		 * XXX Faulty directory listing? - user should be given 
		 * XXX an opportunity to change the audit_control file 
		 * XXX switch to a reduced mode of auditing?
		 */
		return -1;
	}

	/*
	 * XXX There are synchronization problems here
 	 * XXX what should we do if a trigger for the earlier limit
	 * XXX is generated here? 
	 */
	if(0 == (ret = getacmin(&minval))) {

		syslog(LOG_INFO, "min free = %d\n", minval);

		if (auditon(A_GETQCTRL, &qctrl, sizeof(qctrl)) != 0) {
				syslog(LOG_ERR, 
					"could not get audit queue settings\n");
				return -1;
		}
		qctrl.aq_minfree = minval;
		if (auditon(A_SETQCTRL, &qctrl, sizeof(qctrl)) != 0) {
				syslog(LOG_ERR, 
					"could not set audit queue settings\n");
				return -1;
		}
	}

	return 0;
}

/*
 * Close all log files, control files, and tell the audit system.
 */
int close_all() 
{
	int err_ret = 0;
	char TS[POSTFIX_LEN];
	int aufd;
	token_t *tok;

	/* Generate an audit record */
	if((aufd = au_open()) == -1) {
		syslog(LOG_ERR, "Could not create audit shutdown event.\n");
	} else {

		if((tok = au_to_text("auditd::Audit shutdown")) != NULL) {
			au_write(aufd, tok);
		}

		if(au_close(aufd, 1, AUE_audit_shutdown) == -1) {
			syslog(LOG_ERR, "Could not close audit shutdown event.\n");
		}
	}

	/* flush contents */
	err_ret = auditctl(NULL);
	if (err_ret != 0) {
		syslog(LOG_ERR, "auditctl failed! : %s\n", 
			strerror(errno));
		err_ret = 1;
	}
	if(getTSstr(TS, POSTFIX_LEN) == 0) {
		close_lastfile(TS);
	}
	if(lastfile != NULL)
		free(lastfile);

	free_dir_q();
	if((remove(AUDITD_PIDFILE) == -1) || err_ret) {
		syslog(LOG_ERR, "Could not unregister\n");
		audit_warn_postsigterm();
		return (1);
	}
	endac();
	syslog(LOG_INFO, "Finished.\n");
	return (0);
}

/*
 * When we get a signal, we are often not at a clean point. 
 * So, little can be done in the signal handler itself.  Instead,
 * we send a message to the main servicing loop to do proper
 * handling from a non-signal-handler context.
 */
static void
relay_signal(int signal)
{
	mach_msg_empty_send_t msg;

	msg.header.msgh_id = signal;
	msg.header.msgh_remote_port = signal_port;
	msg.header.msgh_local_port = MACH_PORT_NULL;
	msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MAKE_SEND, 0);
	mach_msg(&(msg.header), MACH_SEND_MSG|MACH_SEND_TIMEOUT, sizeof(msg),
		 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
}

/* registering the daemon */
int register_daemon()
{
	FILE * pidfile;
	int fd;
	pid_t pid;

	/* Set up the signal hander */
	if (signal(SIGTERM, relay_signal) == SIG_ERR) {
		fail_exit();
	}
	if (signal(SIGCHLD, relay_signal) == SIG_ERR) {
		fail_exit();
	}

	if ((pidfile = fopen(AUDITD_PIDFILE, "a")) == NULL) {
		audit_warn_tmpfile();
		return -1;
	}

	/* attempt to lock the pid file; if a lock is present, exit */
	fd = fileno(pidfile);
	if(flock(fd, LOCK_EX | LOCK_NB) < 0) {
		syslog(LOG_ERR, "PID file is locked (is another auditd running?).\n");
		audit_warn_ebusy();
		return -1;
	}

	pid = getpid();
	ftruncate(fd, 0);
	if(fprintf(pidfile, "%u\n", pid) < 0) {
		/* should not start the daemon */
		fail_exit();
	}

	fflush(pidfile);
	return 0;
}

/*
 * React to input from the audit tool
 */
kern_return_t auditd_control(auditd_port, flags)
        mach_port_t auditd_port;
		int flags;
{
	int err_ret = 0;

	switch(flags) {

		case OPEN_NEW :
			/* create a new file and swap with the one being used in kernel */
			if(swap_audit_file() == -1) {
				syslog(LOG_ERR, "Error swapping audit file\n");				
			}
			break;

		case READ_FILE :
			if(read_control_file() == -1) {
				syslog(LOG_ERR, "Error in audit control file\n");				
			}
			break;

		case CLOSE_AND_DIE : 
			err_ret = close_all();
			exit (err_ret);
			break;

		default :
			break;
	}

	return KERN_SUCCESS;
}

/*
 * Suppress duplicate messages within a 30 second interval.
 * This should be enough to time to rotate log files without
 * thrashing from soft warnings generated before the log is
 * actually rotated.
 */
#define DUPLICATE_INTERVAL 30
/*
 * Implementation of the audit_triggers() MIG routine.
 */
kern_return_t audit_triggers(audit_port, flags)
        mach_port_t audit_port;
		int flags;
{
	static int last_flags;
	static time_t last_time;
	struct dir_ent *dirent;

	/*
	 * Suppres duplicate messages from the kernel within the specified interval
	 */
	struct timeval ts;
	struct timezone tzp;
	time_t tt;

	if(gettimeofday(&ts, &tzp) == 0) {
		tt = (time_t)ts.tv_sec;
		if ((flags == last_flags) && (tt < (last_time + DUPLICATE_INTERVAL))) {
			return KERN_SUCCESS;
		}
		last_flags = flags;
		last_time = tt;
	}

		syslog(LOG_INFO, 
		  "audit_triggers() called within auditd with flags = %d\n",
			flags);
	/* 
	 * XXX Message processing is done here 
 	 */
	dirent = TAILQ_FIRST(&dir_q); 
	if(flags == AUDIT_TRIGGER_LOW_SPACE) {
		if(dirent && (dirent->softlim != 1)) {
			TAILQ_REMOVE(&dir_q, dirent, dirs);
				/* add this node to the end of the list */
				TAILQ_INSERT_TAIL(&dir_q, dirent, dirs);
				audit_warn_soft(dirent->dirname);
				dirent->softlim = 1;
						
			if (TAILQ_NEXT(TAILQ_FIRST(&dir_q), dirs) != NULL && swap_audit_file() == -1) {
				syslog(LOG_ERR, "Error swapping audit file\n");
			}

				/* 
				 * check if the next dir has already reached its 
				 * soft limit
				 */
				dirent = TAILQ_FIRST(&dir_q);
				if(dirent->softlim == 1)  {
					/* all dirs have reached their soft limit */
					audit_warn_allsoft();
				}
			}
		else {
			/* 
			 * Continue auditing to the current file
			 * Also generate  an allsoft warning
			 * XXX do we want to do this ?
			 */
			audit_warn_allsoft();
		}
	}
	else if (flags == AUDIT_TRIGGER_FILE_FULL) {

		/* delete current dir, go on to next */
		TAILQ_REMOVE(&dir_q, dirent, dirs);
        	audit_warn_hard(dirent->dirname);
        	free(dirent->dirname);
        	free(dirent);

		if(swap_audit_file() == -1) {
			syslog(LOG_ERR, "Error swapping audit file in response to AUDIT_TRIGGER_FILE_FULL message\n");	
	
			/* Nowhere to write to */
			audit_warn_allhard(++allhardcount);
		}
	}
	return KERN_SUCCESS;
}

/*
 * Reap our children.
 */
static void
reap_children(void)
{
	pid_t child;
	int wstatus;

	while ((child = waitpid(-1, &wstatus, WNOHANG)) > 0) {
		if (wstatus) {
			syslog(LOG_INFO, "warn process [pid=%d] %s %d.\n", child,
				   ((WIFEXITED(wstatus)) ? 
					"exited with non-zero status" :
					"exited as a result of signal"),
				   ((WIFEXITED(wstatus)) ? 
					WEXITSTATUS(wstatus) : 
					WTERMSIG(wstatus)));
		}
	}
}

/*
 * Handle an RPC call
 */
boolean_t auditd_combined_server(
	mach_msg_header_t *InHeadP,
	mach_msg_header_t *OutHeadP)
{
	mach_port_t local_port = InHeadP->msgh_local_port;

	if (local_port == signal_port) {
		int signo = InHeadP->msgh_id;
		int ret;

		if (SIGTERM == signo) {
			ret = close_all();
			exit (ret);
		} else if (SIGCHLD == signo) {
			reap_children();
			return TRUE;
		} else {
			syslog(LOG_INFO, "Recevied signal %d.\n", signo);
			return TRUE;
		}
	} else if (local_port == control_port) {
		boolean_t result;

		result = audit_triggers_server(InHeadP, OutHeadP);
		if (!result)
			result = auditd_control_server(InHeadP, OutHeadP);
		return result;
	}
	syslog(LOG_INFO, "Recevied msg on bad port 0x%x.\n", local_port);
	return FALSE;
}

void wait_on_audit_trigger(port_set)
        mach_port_t     port_set;
{
	kern_return_t   result;
	result = mach_msg_server(auditd_combined_server, 4096, port_set, MACH_MSG_OPTION_NONE);
	syslog(LOG_ERR, "abnormal exit\n");
}

/*
 * Configure the audit controls in the kernel: the event to class mapping,
 * kernel preselection mask, etc.
 */
int config_audit_controls(long flags)
{
	au_event_ent_t *ev;
	au_evclass_map_t evc_map;
	au_mask_t aumask;
	int ctr = 0;
	char naeventstr[NA_EVENT_STR_SIZE];

	/* Process the audit event file, obtaining a class mapping for each
	 * event, and send that mapping into the kernel.
	 * XXX There's a risk here that the BSM library will return NULL
	 * for an event when it can't properly map it to a class. In that
	 * case, we will not process any events beyond the one that failed,
	 * but should. We need a way to get a count of the events.
	*/

	setauevent();
	while((ev = getauevent()) != NULL) {
		evc_map.ec_number = ev->ae_number;
		evc_map.ec_class = ev->ae_class;
		if (auditon(A_SETCLASS, &evc_map, sizeof(au_evclass_map_t)) != 0) {
			syslog(LOG_ERR, 
				"Failed to register class mapping for event %s",
				 ev->ae_name);
		} else {
			ctr++;
		}
		free(ev->ae_name);
		free(ev->ae_desc);
		free(ev);
	}
	endauevent();
	if (ctr == 0)
		syslog(LOG_ERR, "No events to class mappings registered.");
	else
		syslog(LOG_INFO, "Registered %d event to class mappings.", ctr);

	/* Get the non-attributable event string and set the kernel mask
	 * from that.
	 */
	if ((getacna(naeventstr, NA_EVENT_STR_SIZE) == 0)	
                && ( getauditflagsbin(naeventstr, &aumask) == 0)) {

		if (auditon(A_SETKMASK, &aumask, sizeof(au_mask_t))){
			syslog(LOG_ERR,
				"Failed to register non-attributable event mask.");
		} else {
			syslog(LOG_INFO, "Registered non-attributable event mask.");
		}
			
	} else {
		syslog(LOG_ERR,"Failed to obtain non-attributable event mask.");
	}

	/*
	 * Set the audit policy flags based on passed in parameter values.
	 */
	if (auditon(A_SETPOLICY, &flags, sizeof(flags))) {
		syslog(LOG_ERR,
		       "Failed to set audit policy.");
	}

	return 0;
}

void setup(long flags)
{
	mach_msg_type_name_t    poly;
	int aufd;
	token_t *tok;

	/* Allocate a port set */
	if (mach_port_allocate(mach_task_self(),
				MACH_PORT_RIGHT_PORT_SET,
				&port_set) != KERN_SUCCESS)  {
		syslog(LOG_ERR, "allocation of port set failed\n");
		fail_exit();
	}

	/* Allocate a signal reflection port */
	if (mach_port_allocate(mach_task_self(),
				MACH_PORT_RIGHT_RECEIVE,
				&signal_port) != KERN_SUCCESS ||
		mach_port_move_member(mach_task_self(),
				signal_port,
				 port_set) != KERN_SUCCESS)  {
		syslog(LOG_ERR, "allocation of signal port failed\n");
		fail_exit();
	}

	/* Allocate a trigger port */
	if (mach_port_allocate(mach_task_self(),
				MACH_PORT_RIGHT_RECEIVE,
				&control_port) != KERN_SUCCESS ||
		mach_port_move_member(mach_task_self(),
				control_port,
				port_set) != KERN_SUCCESS)  {
		syslog(LOG_ERR, "allocation of trigger port failed\n");
		fail_exit();
	}

	/* create a send right on our trigger port */
	mach_port_extract_right(mach_task_self(), control_port,
		MACH_MSG_TYPE_MAKE_SEND, &control_port, &poly);

	TAILQ_INIT(&dir_q);

	/* register the trigger port with the kernel */
	if(host_set_audit_control_port(mach_host_self(), control_port) != KERN_SUCCESS) {
		syslog(LOG_ERR, "Cannot set Mach control port\n");
		fail_exit();
	}
	else {
		syslog(LOG_ERR, "Mach control port registered\n");
	}

	if(read_control_file() == -1) {
		syslog(LOG_ERR, "Error reading control file\n");
		fail_exit();
	}

	/* Generate an audit record */
	if((aufd = au_open()) == -1) {
		syslog(LOG_ERR, "Could not create audit startup event.\n");
	} else {

		if((tok = au_to_text("auditd::Audit startup")) != NULL) {
			au_write(aufd, tok);
		}

		if(au_close(aufd, 1, AUE_audit_startup) == -1) {
			syslog(LOG_ERR, "Could not close audit startup event.\n");
		}
	}

	if (config_audit_controls(flags) == 0)
		syslog(LOG_INFO, "Initialization successful\n");
	else
		syslog(LOG_INFO, "Initialization failed\n");
}


int main(int argc, char **argv)
{
	char ch;
	long flags = AUDIT_CNT;
	int debug = 0;

	while ((ch = getopt(argc, argv, "dhs")) != -1) {
		switch(ch) {

			/* debug option */
		case 'd':
			debug = 1;
			break;

			/* fail-stop option */
		case 's':
			flags &= ~(AUDIT_CNT);
			break;

			/* halt-stop option */
		case 'h':
			flags |= AUDIT_AHLT;
			break;

		case '?':
		default:
			(void)fprintf(stderr,
			"usage: auditd [-h | -s]\n");
			exit(1);
		}
	}

	openlog("auditd", LOG_CONS | LOG_PID, LOG_DAEMON);
	syslog(LOG_INFO, "starting...\n");

        if (debug == 0 && daemon(0, 0) == -1) {
		syslog(LOG_ERR, "Failed to daemonize\n");
		exit(1);
	}

	if(register_daemon() == -1) {
		syslog(LOG_ERR, "Could not register as daemon\n");
		exit(1);
	}

	setup(flags);
	wait_on_audit_trigger(port_set);
	syslog(LOG_INFO, "exiting.\n");
	
	exit(1);
}
