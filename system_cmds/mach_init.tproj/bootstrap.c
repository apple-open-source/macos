/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
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
/*
 * bootstrap -- fundamental service initiator and port server
 * Mike DeMoney, NeXT, Inc.
 * Copyright, 1990.  All rights reserved.
 *
 * bootstrap.c -- implementation of bootstrap main service loop
 */

/*
 * Imports
 */
#import	<mach/mach.h>
#import	<mach/boolean.h>
#import	<mach/message.h>
#import <mach/notify.h>
#import <mach/mig_errors.h>
#include <mach/mach_traps.h>
#include <mach/mach_interface.h>
#include <mach/bootstrap.h>
#include <mach/host_info.h>
#include <mach/mach_host.h>
#include <mach/exception.h>

#import <sys/ioctl.h>
#import	<string.h>
#import	<ctype.h>
#import	<stdio.h>
#import <libc.h>

#include "bootstrap.h"

#import "bootstrap_internal.h"
#import "lists.h"
#import "error_log.h"
#import "parser.h"

/* Mig should produce a declaration for this,  but doesn't */
extern boolean_t bootstrap_server(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);

/*
 * Exports
 */
const char *program_name;	/* our name for error messages */

#ifndef CONF_FILE
#define	CONF_FILE	"/etc/bootstrap.conf"	/* default config file */
#endif CONF_FILE

const char *conf_file = CONF_FILE;

const unsigned BOOTSTRAP_REPLY_TIMEOUT = 10 * 1000; // 10 sec reply timeout
#ifdef notyet
port_all_t backup_port;
#endif /* notyet */
/*
 * Last resort configuration
 *
 * If we can't find a /etc/bootstrap.conf, we use this configuration.
 * The services defined here are compatable with the old mach port stuff,
 * and of course, the names for these services should not be modified without
 * modifying mach_init in libc.
 */
const char *default_conf =
	"init \"/sbin/init\";"
	"services NetMessage;";
	
mach_port_t inherited_bootstrap_port = MACH_PORT_NULL;
boolean_t forward_ok = FALSE;
boolean_t debugging = FALSE;
boolean_t register_self = FALSE;
int init_priority = BASEPRI_USER;
char *register_name = NULL;
mach_port_t	bootstrap_master_device_port;	/* local name */
mach_port_t	bootstrap_master_host_port;	/* local name */
mach_port_t	bootstrap_notification_port;	/* local name */
mach_port_t	security_port;
mach_port_t	root_ledger_wired;
mach_port_t	root_ledger_paged;
task_port_t	bootstrap_self;

#ifndef ASSERT
#define ASSERT(p)
#endif

/*
 * Private macros
 */
#define	NELEM(x)		(sizeof(x)/sizeof(x[0]))
#define	END_OF(x)		(&(x)[NELEM(x)])
#define	streq(a,b)		(strcmp(a,b) == 0)

/*
 * Private declarations
 */	
static void add_init_arg(const char *arg);
static void wait_for_go(mach_port_t init_notify_port);
static void init_ports(void);
static void start_server(server_t *serverp);
static void unblock_init(task_port_t task, mach_port_t newBootstrap);
static void exec_server(server_t *serverp);
static char **argvize(const char *string);
static void server_loop(void);
static void msg_destroy(mach_msg_header_t *m);

/*
 * Private ports we hold receive rights for.  We also hold receive rights
 * for all the privileged ports.  Those are maintained in the server
 * structs.
 */
mach_port_t bootstrap_port_set;
mach_port_t notify_port;

static int init_pid;
static int running_servers;
static char init_args[BOOTSTRAP_MAX_CMD_LEN];

void _myExit(int arg)
{
    exit(arg);
}

/* It was a bozo who made main return a value!  Civil disobedience, here. */
void
main(int argc, const char * const argv[])
{
	const char *argp;
	char c;
	server_t *init_serverp;
	server_t *serverp;
	mach_port_t init_notify_port;
	kern_return_t result;
	int pid;
	int force_fork = FALSE;
	mach_port_t prev_port;
#if defined(__APPLE__)
        extern void exit(int);
	
	/* signal (SIGUSR2, _myExit); */
#endif

	/* Initialize error handling */
 	program_name = rindex(*argv, '/');
	if (program_name)
		program_name++;
	else
		program_name = *argv;
 	argv++; argc--;

	init_pid = getpid();
	init_errlog(init_pid == 1);

	/*
	 * Get master host and device ports
	 */
#if 0
	result = bootstrap_ports(bootstrap_port,
			     &bootstrap_master_host_port,
			     &bootstrap_master_device_port,
			     &root_ledger_wired,
			     &root_ledger_paged,
			     &security_port);
	if (result != KERN_SUCCESS) {
	printf("bootstrap_ports failed \n");
		kern_error(result, "bootstrap_ports");
	}
#endif /* 0 */
	/*
	 *	This task will become the bootstrap task.
	 */
	bootstrap_self = mach_task_self();

	/* Initialize globals */
	init_lists();

	/* Parse command line args */
	while (argc > 0 && **argv == '-') {
		boolean_t init_arg = FALSE;
		argp = *argv++ + 1; argc--;
		while (*argp) {
			switch (c = *argp++) {
			case 'd':
				debugging = TRUE;
				break;
			case 'D':
				debugging = FALSE;
				break;
			case 'F':
				force_fork = TRUE;
				break;
			case 'f':
				if (argc > 0) {
					conf_file = *argv++; argc--;
				} else
					fatal("-f requires config file name");
				break;
			case '-':
				init_arg = TRUE;
				break;
			case 'r':
				register_self = forward_ok = TRUE;
				if (argc > 0) {
					register_name = *argv++; argc--;
				} else
					fatal("-r requires name");
				break;
			default:
				init_arg = TRUE;
				break;
			}
		}
		if (init_arg) {
			add_init_arg(argv[-1]);
			goto copyargs;
		}
	}
    copyargs:
	while (argc != 0) {
		argc--;
		add_init_arg(*argv++);
	}

	log("Started");

	/* Parse the config file */
	init_config();

	/* set_default_policy(bootstrap_master_host_port); */

	/*
	 * If we have to run init as pid 1, use notify port to
	 * synchronize init and bootstrap
	 */
	if ((init_serverp = find_init_server()) != NULL) {
		if (init_pid != 1 && ! debugging)
			fatal("Can't start init server if not pid 1");
            result = mach_port_allocate(bootstrap_self, MACH_PORT_RIGHT_RECEIVE,
                                    &init_notify_port);
            if (result != KERN_SUCCESS)
                    kern_fatal(result, "mach_port_allocate");
            result = mach_port_insert_right(mach_task_self(), init_notify_port, init_notify_port, MACH_MSG_TYPE_MAKE_SEND);
            if (result != KERN_SUCCESS)
                kern_fatal(result, "mach_port_insert_right");
            task_set_bootstrap_port(bootstrap_self, init_notify_port);
            if (result != KERN_SUCCESS)
                kern_fatal(result, "task_set_bootstrap_port");
	    /*
	     *  XXX restart the service if it dies?
	     */
                result = mach_port_request_notification(mach_task_self(),
						mach_task_self(),
						MACH_NOTIFY_DEAD_NAME,
						0,
						init_notify_port,
						MACH_MSG_TYPE_MAKE_SEND_ONCE,
						&prev_port);
	    if (result != KERN_SUCCESS)
			kern_fatal(result, "mach_port_request_notification");
		debug("Set port %d for parent proc notify port",
		      init_notify_port);
                } else if (init_args[0] != '\0')
		fatal("Extraneous command line arguments");
	
	/* Fork if either not debugging or running /etc/init */
	if (force_fork || !debugging || init_serverp != NULL) {
		pid = fork();
		if (pid < 0)
			unix_fatal("fork");
	} else
		pid = 0;
	
	/* Bootstrap service runs in child (if there is one) */
	if (pid == 0) {	/* CHILD */
		/*
		 * If we're initiated by pid 1, we shouldn't get ever get 
		 * killed; designate ourselves as an "init process".
		 *
		 * This should go away with new signal stuff!
		 */
            if (init_pid == 1)
			init_process();

            /* Create declared service ports, our service port, etc */	
		init_ports();

		/* Kick off all server processes */
		for (  serverp = FIRST(servers)
		     ; !IS_END(serverp, servers)
		     ; serverp = NEXT(serverp))
			start_server(serverp);
		
		/*
		 * If our priority's to be changed, set it up here.
		 */
#ifdef notyet
		if (init_priority >= 0) {
			result = task_priority(mach_task_self(), init_priority,
				TRUE);
			if (result != KERN_SUCCESS)
				kern_error(result, "task_priority %d",
					init_priority);
		}
#endif /* notyet */
		/* Process bootstrap service requests */
		server_loop();	/* Should never return */
                exit(1);
	}
	
	/* PARENT */
	if (init_serverp != NULL) {
		int i;

		strncat(init_serverp->cmd,
			init_args,
			sizeof(init_serverp->cmd));
		/*
		 * Wait, then either exec /etc/init or exit
		 * (which panics the system)
		 */
		for (i = 3; i; i--)
			close(i);
		wait_for_go(init_notify_port);
                exec_server(init_serverp);
		exit(1);
	}
	exit(0);
}

static void
add_init_arg(const char *arg)
{
	strncat(init_args, " ", sizeof(init_args));
	strncat(init_args, arg, sizeof(init_args));
}

static void
wait_for_go(mach_port_t init_notify_port)
{
        struct {
            mach_msg_header_t hdr;
            mach_msg_trailer_t trailer;
        } init_go_msg;
	kern_return_t result;

        /*
	 * For now, we just blindly wait until we receive a message or
	 * timeout.  We don't expect any notifications, and if we get one,
	 * it probably means something dire has happened; so we might as
	 * well give a shot at letting init run.
	 */	
        result = mach_msg(&init_go_msg.hdr, MACH_RCV_MSG, 0, sizeof(init_go_msg), init_notify_port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
        if (result != KERN_SUCCESS) {
            kern_error(result, "mach_msg(receive) failed in wait_for_go");
        }
        result = task_set_bootstrap_port(mach_task_self(), init_go_msg.hdr.msgh_remote_port);
        if (result != KERN_SUCCESS) {
            kern_error(result, "task_get_bootstrap_port()");
        }
}

static void
init_ports(void)
{
	kern_return_t result;
	service_t *servicep;
	mach_port_name_t previous;
	mach_port_t pport;

	/*
	 *	This task will become the bootstrap task.
	 */
	bootstrap_self = mach_task_self();

	/* get inherited bootstrap port */
	result = task_get_bootstrap_port(bootstrap_self,
					 &inherited_bootstrap_port);
	if (result != KERN_SUCCESS)
		kern_fatal(result, "task_get_bootstrap_port");

	/* We set this explicitly as we start each child */
	task_set_bootstrap_port(bootstrap_self, MACH_PORT_NULL);
	if (inherited_bootstrap_port == MACH_PORT_NULL)
		forward_ok = FALSE;
	
	/* Create port set that server loop listens to */
	result = mach_port_allocate(bootstrap_self, MACH_PORT_RIGHT_PORT_SET,
				&bootstrap_port_set);
	if (result != KERN_SUCCESS)
		kern_fatal(result, "port_set_allocate");
	/* Create notify port and add to server port set */
	result = mach_port_allocate(bootstrap_self, MACH_PORT_RIGHT_RECEIVE,
				&notify_port);
	if (result != KERN_SUCCESS)
		kern_fatal(result, "mach_port_allocate");

	result = mach_port_request_notification(bootstrap_self,
						bootstrap_self,
						MACH_NOTIFY_DEAD_NAME,
						0,
						notify_port,
						MACH_MSG_TYPE_MAKE_SEND_ONCE,
						&pport);
	if (result != KERN_SUCCESS)
		kern_fatal(result, "task_set_notify_port");

	result = mach_port_move_member(bootstrap_self,
				   notify_port,
				   bootstrap_port_set);
	if (result != KERN_SUCCESS)
		kern_fatal(result, "mach_port_move_member");
	
	/* Create "self" port and add to server port set */
	result = mach_port_allocate(bootstrap_self, MACH_PORT_RIGHT_RECEIVE,
				&bootstraps.bootstrap_port);
	if (result != KERN_SUCCESS)
		kern_fatal(result, "mach_port_allocate");
        result = mach_port_insert_right(mach_task_self(), bootstraps.bootstrap_port, bootstraps.bootstrap_port, MACH_MSG_TYPE_MAKE_SEND);
        if (result != KERN_SUCCESS)
            kern_fatal(result, "mach_port_insert_right");
	result = mach_port_move_member(bootstrap_self,
				   bootstraps.bootstrap_port,
				   bootstrap_port_set);
	if (result != KERN_SUCCESS)
		kern_fatal(result, "mach_port_move_member");
#ifdef notyet 
	/* register "self" port with anscestor */		
	if (register_self && forward_ok) {
		result = bootstrap_register(inherited_bootstrap_port, register_name, bootstraps.bootstrap_port);
		if (result != KERN_SUCCESS)
			kern_fatal(result, "register self");
	}
#endif /* notyet*/
	/*
	 * Allocate service ports for declared services.
	 */
	for (  servicep = FIRST(services)
	     ; ! IS_END(servicep, services)
	     ; servicep = NEXT(servicep))
	{
	 	switch (servicep->servicetype) {
		case DECLARED:
			result = mach_port_allocate(bootstrap_self, MACH_PORT_RIGHT_RECEIVE,
				&(servicep->port));
			if (result != KERN_SUCCESS)
				kern_fatal(result, "mach_port_allocate");
			result = mach_port_insert_right(bootstrap_self, servicep->port, servicep->port, MACH_MSG_TYPE_MAKE_SEND);
			if (result != KERN_SUCCESS)
				kern_fatal(result, "mach_port_insert_right");
			debug("Declared port %d for service %s",
			      servicep->port,
			      servicep->name);
#ifdef notyet
			result = port_set_backup(task_self(),
						 servicep->port,
						 backup_port,
						 &previous);
			if (result != KERN_SUCCESS)
				kern_fatal(result, "port_set_backup");
#endif /* notyet */
			break;
		case SELF:
			servicep->port = bootstraps.bootstrap_port;
			servicep->server = new_server(MACHINIT,
				program_name, init_priority);
			info("Set port %d for self port",
			      bootstraps.bootstrap_port);
			break;
		case REGISTERED:
			fatal("Can't allocate REGISTERED port!?!");
			break;
		}
	}
}

static void
start_server(server_t *serverp)
{
	kern_return_t result;
	mach_port_t old_port;
	task_port_t init_task;
	int pid;
	
	/* get rid of any old server port (this might be a restart) */
	old_port = serverp->port;
	serverp->port = MACH_PORT_NULL;
        if (old_port != MACH_PORT_NULL) {
            msg_destroy_port(old_port);
        }
	/* Allocate privileged port for requests from service */
	result = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE ,&serverp->port);
	info("Allocating port %d for server %s", serverp->port, serverp->cmd);
	if (result != KERN_SUCCESS)	
		kern_fatal(result, "port_allocate");

        result = mach_port_insert_right(mach_task_self(), serverp->port, serverp->port, MACH_MSG_TYPE_MAKE_SEND);
        if (result != KERN_SUCCESS)
            kern_fatal(result, "mach_port_insert_right");
	/* Add privileged server port to bootstrap port set */
	result = mach_port_move_member(mach_task_self(),
				   serverp->port,
				   bootstrap_port_set);
	if (result != KERN_SUCCESS)
		kern_fatal(result, "mach_port_move_member");

	/*
	 * Do what's appropriate to get bootstrap port setup in server task
	 */
	switch (serverp->servertype) {
	case ETCINIT:
		/*
		 * This is pid 1 init program -- need to punch stuff
		 * back into pid 1 task rather than create a new task
		 */
		result = task_for_pid(mach_task_self(), init_pid, &init_task);
                if (result != KERN_SUCCESS)
                    kern_fatal(result, "task_for_pid");
		serverp->task_port = init_task;
                unblock_init(init_task, serverp->port);
		break;

	case MACHINIT:
		break;

	case SERVER:
	case RESTARTABLE:
		/* Give trusted service a unique bootstrap port */
		result = task_set_bootstrap_port(mach_task_self(), serverp->port);
		if (result != KERN_SUCCESS)
			kern_fatal(result, "task_set_bootstrap_port");

		pid = fork();
		if (pid < 0)
			unix_error("fork");
		else if (pid == 0) {	/* CHILD */
			exec_server(serverp);
			exit(1);
		} else {		/* PARENT */
			result = task_set_bootstrap_port(mach_task_self(),
							 MACH_PORT_NULL);
			if (result != KERN_SUCCESS)
				kern_fatal(result, "task_set_bootstrap_port");

			result = task_for_pid(mach_task_self(),
						  pid,
						  &serverp->task_port);
			if (result != KERN_SUCCESS)
				kern_fatal(result, "getting server task port");
			running_servers += 1;
		}
		break;
	}
}

static void
unblock_init(task_port_t task, mach_port_t newBootstrap)
{
	mach_msg_header_t init_go_msg;
	kern_return_t result;

	/*
	 * Proc 1 is blocked in a msg_receive on its notify port, this lets
	 * it continue, and we hand off its new bootstrap port
	 */
	init_go_msg.msgh_remote_port = inherited_bootstrap_port;
	init_go_msg.msgh_local_port = newBootstrap;
        init_go_msg.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND);
        result = mach_msg(&init_go_msg, MACH_SEND_MSG, sizeof(init_go_msg), 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	if (result != KERN_SUCCESS)
		kern_fatal(result, "unblock_init mach_msg(send) failed");
	debug("sent go message");
}

static void
exec_server(server_t *serverp)
{
	int nfds, fd;
        char **argv;
	
	/*
	 * Setup environment for server, someday this should be Mach stuff
	 * rather than Unix crud
	 */
	log("Initiating server %s [pid %d]", serverp->cmd, getpid());
	argv = argvize(serverp->cmd);
	nfds = getdtablesize();

#ifdef notyet 
	/*
	 * If our priority's to be changed, set it up here.
	 */
	if (serverp->priority >= 0) {
		kern_return_t result;
		result = task_priority(mach_task_self(), serverp->priority,
					 TRUE);
		if (result != KERN_SUCCESS)
			kern_error(result, "task_priority %d",
				serverp->priority);
	}
#endif /* notyet */
	close_errlog();

	/*
	 * Mark this process as an "init process" so that it won't be
	 * blown away by init when it starts up (or changes states).
	 */
	if (init_pid == 1) {
		debug("marking process %s as init_process\n", argv[0]);
		init_process();
	}

        for (fd =  debugging ? 3 : 0; fd < nfds; fd++)
		close(fd);
	fd = open("/dev/tty", O_RDONLY);
	if (fd >= 0) {
		ioctl(fd, TIOCNOTTY, 0);
		close(fd);
	}

        execv(argv[0], argv);
	unix_error("Can't exec %s", argv[0]);
}	

static char **
argvize(const char *string)
{
	static char *argv[100], args[1000];
	const char *cp;
	char *argp, term;
	int nargs;
	
	/*
	 * Convert a command line into an argv for execv
	 */
	nargs = 0;
	argp = args;
	
	for (cp = string; *cp;) {
		while (isspace(*cp))
			cp++;
		term = (*cp == '"') ? *cp++ : '\0';
		if (nargs < NELEM(argv))
			argv[nargs++] = argp;
		while (*cp && (term ? *cp != term : !isspace(*cp))
		 && argp < END_OF(args)) {
			if (*cp == '\\')
				cp++;
			*argp++ = *cp;
			if (*cp)
				cp++;
		}
		*argp++ = '\0';
	}
	argv[nargs] = NULL;
	return argv;
}

/*
 * server_loop -- pick requests off our service port and process them
 * Also handles notifications
 */
#define	bootstrapMaxRequestSize	1024
#define	bootstrapMaxReplySize	1024

static void
server_loop(void)
{
    bootstrap_info_t *bootstrap;
    service_t *servicep;
    server_t *serverp;
    kern_return_t result;
    mach_port_name_t previous;
    mach_port_array_t array;
    mach_msg_type_number_t count;
    int i;
        
    union {
    	mach_msg_header_t hdr;
	char body[bootstrapMaxRequestSize];
    } msg;
    union {
    	mach_msg_header_t hdr;
#ifdef notyet
	death_pill_t death;
#endif /* notyet */
	char body[bootstrapMaxReplySize];
    } reply;
	    
    for (;;) {
	memset(&msg, 0, sizeof(msg));
        result = mach_msg_overwrite_trap(&msg.hdr, MACH_RCV_MSG|MACH_RCV_INTERRUPT|MACH_RCV_TIMEOUT, 0, sizeof(msg), bootstrap_port_set, 500, MACH_PORT_NULL, MACH_MSG_NULL, 0);
	if (result != KERN_SUCCESS) {
            if (result != MACH_RCV_TIMED_OUT) {
                kern_error(result, "server_loop: msg_receive()");
            }
	    continue;
	}

#if	DEBUG
	debug("received message on port %d\n", msg.hdr.msgh_local_port);
#endif	DEBUG

	/*
	 * Pick off notification messages
	 */
	if (msg.hdr.msgh_local_port == notify_port) {
	    mach_port_name_t np;

            switch (msg.hdr.msgh_id) {
	    case MACH_NOTIFY_DEAD_NAME:
                np = ((mach_dead_name_notification_t *)&msg)->not_port;
#if	DEBUG
		info("Notified dead name %d", np);
#endif	DEBUG
		if (np == inherited_bootstrap_port)
		{
		    inherited_bootstrap_port = MACH_PORT_NULL;
		    forward_ok = FALSE;
		    break;
		}
		
		/*
		 * This may be notification that the task_port associated
		 * with a task we've launched has been deleted.  This
		 * happens only when the server dies.
		 */
		serverp = lookup_server_by_task_port(np);
		if (serverp != NULL) {
		    info("Notified that server %s died\n", serverp->cmd);
		    if (running_servers <= 0) {
			    /* This "can't" happen */
			    running_servers = 0;
			    error("task count error");
		    } else {
			    running_servers -= 1;
			    log("server %s died",
				serverp != NULL ? serverp->cmd : "Unknown");
			    if (serverp != NULL) {
				    /*
				     * FIXME: need to control execs
				     * when server fails immediately
				     */
				    if (   serverp->servertype == RESTARTABLE
					/*
					&& haven't started this recently */)
					    start_server(serverp);
			    }
		    }
		    break;
		}

		/*
		 * Check to see if a subset requestor port was deleted.
		 */
		while (bootstrap = lookup_bootstrap_req_by_port(np)) {
			info("notified that requestor of subset %d died",
				bootstrap->bootstrap_port);
			delete_bootstrap(bootstrap);
		}

		/*
		 * Check to see if a defined service has gone
		 * away.
		 */
		while (servicep = lookup_service_by_port(np)) {
		    /*
		     * Port gone, server died.
		     */
		    debug("Received destroyed notification for service %s "
			  "on bootstrap port %d\n",
			  servicep->name, servicep->bootstrap);
		    if (servicep->servicetype == REGISTERED) {
			debug("Service %s failed - deallocate", servicep->name);
			mach_port_deallocate(mach_task_self(),np);
			mach_port_deallocate(mach_task_self(),servicep->port);
			delete_service(servicep);
		    } else {
			/*
			 * Allocate a new, backed-up port for this service.
			 */
			log("Service %s failed - re-initialize",
			    servicep->name);
			msg_destroy_port(servicep->port);
			result = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &servicep->port);
			if (result != KERN_SUCCESS)
				kern_fatal(result, "port_allocate");
                        result = mach_port_insert_right(bootstrap_self, servicep->port, servicep->port, MACH_MSG_TYPE_MAKE_SEND);
                        if (result != KERN_SUCCESS)
                                kern_fatal(result, "mach_port_insert_right");
#if 0
                        result = port_set_backup(mach_task_self(),
						    servicep->port,
						    backup_port,
						    &previous);
			if (result != KERN_SUCCESS)
				kern_fatal(result, "port_set_backup");
#endif
                        servicep->isActive = FALSE;
		    }
		}
		break;
	    default:
		error("Unexpected notification: %d", msg.hdr.msgh_id);
		break;
	    }
	}
#if 0
        else if (msg.hdr.msg_local_port == backup_port) {
	    notification_t *not = (notification_t *) &msg.hdr;
	    mach_port_name_t np = not->notify_port;

	    /*
	     * Port sent back to us, server died.
	     */
	    info("port %d returned via backup", np);
	    servicep = lookup_service_by_port(np);
	    if (servicep != NULL) {
	        debug("Received %s notification for service %s",
		      not->notify_header.msg_id
		      == NOTIFY_PORT_DESTROYED
		      ? "destroyed" : "receive rights",
		      servicep->name);
		log("Service %s failed - port backed-up", servicep->name);
		ASSERT(canReceive(servicep->port));
		servicep->isActive = FALSE;
		result = port_set_backup(mach_task_self(),
					 servicep->port,
					 backup_port,
					 &previous);
		if (result != KERN_SUCCESS)
			kern_fatal(result, "port_set_backup");
	    } else
	    	msg_destroy_port(np);
	}
#endif
        else
            {	/* must be a service request */
            bootstrap_server(&msg.hdr, &reply.hdr);
#ifdef	DEBUG
	    debug("Handled request.");
#endif	DEBUG
		reply.hdr.msgh_local_port = MACH_PORT_NULL;
                result = mach_msg(&reply.hdr, MACH_SEND_MSG|MACH_SEND_TIMEOUT, reply.hdr.msgh_size, 0, MACH_PORT_NULL,
				  BOOTSTRAP_REPLY_TIMEOUT, MACH_PORT_NULL);
#ifdef	DEBUG
		debug("Reply sent.");
#endif	DEBUG
                if (result != MACH_MSG_SUCCESS) {
		    kern_error(result, "msg_send");
                }
            }
	/* deallocate uninteresting ports received in message */
	msg_destroy(&msg.hdr);
    }
}

/*
 * msg_destroy -- walk through a received message and deallocate any
 * useless ports or out-of-line memory
 */
static void
msg_destroy(mach_msg_header_t *m)
{
#ifdef notyet /* [ */
	msg_type_t *mtp;
	msg_type_long_t *mtlp;
	void *dp;
	unsigned type, size, num;
	boolean_t inlne;
	mach_port_t *pp;
	kern_return_t result;

	msg_destroy_port(m->msg_remote_port);
	for (  mtp = (msg_type_t *)(m + 1)
	     ; (unsigned)mtp - (unsigned)m < m->msg_size
	     ;)
	{
		inlne = mtp->msg_type_inline;
		if (mtp->msg_type_longform) {
			mtlp = (msg_type_long_t *)mtp;
			type = mtlp->msg_type_long_name;
			size = mtlp->msg_type_long_size;
			num = mtlp->msg_type_long_number;
			dp = (void *)(mtlp + 1);
		} else {
			type = mtp->msg_type_name;
			size = mtp->msg_type_size;
			num = mtp->msg_type_number;
			dp = (void *)(mtp + 1);
		}
		if (inlne)
			mtp = (msg_type_t *)(dp + num * (size/BITS_PER_BYTE));
		else {
			mtp = (msg_type_t *)(dp + sizeof(void *));
			dp = *(char **)dp;
		}
		if (MSG_TYPE_PORT_ANY(type)) {
			for (pp = (mach_port_t *)dp; num-- > 0; pp++)
				msg_destroy_port(*pp);
		}
		if ( ! inlne ) {
			result = vm_deallocate(mach_task_self(), (vm_address_t)dp,
			 num * (size/BITS_PER_BYTE));
			if (result != KERN_SUCCESS)
				kern_error(result,
					   "vm_deallocate: msg_destroy");
		}
	}
#endif /* notyet ] */
}

/*
 * msg_destroy_port -- deallocate port if it's not important to bootstrap
 * Bad name, this is used for things other than msg_destroy.
 */
void
msg_destroy_port(mach_port_t p)
{
	if (   p == MACH_PORT_NULL
	    || p == mach_task_self()
	    || p == mig_get_reply_port()
	    || p == bootstrap_port_set
	    || p == inherited_bootstrap_port
	    || lookup_service_by_port(p)
	    || lookup_server_by_port(p)
	    || lookup_bootstrap_by_port(p) != &bootstraps
	    || lookup_bootstrap_req_by_port(p) != &bootstraps
	    || p == bootstraps.bootstrap_port
	    || p == bootstraps.requestor_port)
		return;

#if	DEBUG
	debug("Deallocating port %d", p);
#endif	DEBUG
	(void) mach_port_deallocate(mach_task_self(), p);
}

boolean_t
canReceive(mach_port_t port)
{
	mach_port_type_t p_type;
	kern_return_t result;
	
	result = mach_port_type(mach_task_self(), port, &p_type);
	if (result != KERN_SUCCESS) {
		kern_error(result, "port_type");
		return FALSE;
	}
	return ((p_type & MACH_PORT_TYPE_RECEIVE) != 0);
}


boolean_t
canSend(mach_port_t port)
{
	mach_port_type_t p_type;
	kern_return_t result;
	
	result = mach_port_type(mach_task_self(), port, &p_type);
	if (result != KERN_SUCCESS) {
		kern_error(result, "port_type");
		return FALSE;
	}
	return ((p_type & MACH_PORT_TYPE_PORT_RIGHTS) != 0);
}
void
set_default_policy(
	mach_port_t master_host_port)
{
#if 0
    host_priority_info_data_t	host_pri_info;
	mach_port_t		default_processor_set_name;
	mach_port_t		default_processor_set;
	policy_rr_base_data_t	rr_base;
	policy_rr_limit_data_t	rr_limit;
	kern_return_t		result;
	mach_msg_type_number_t	count;
	static char here[] = "default_pager_set_policy";

	count = HOST_PRIORITY_INFO_COUNT;
	result = host_info(mach_host_self(),
		      HOST_PRIORITY_INFO,
		      (host_info_t) &host_pri_info,
		      &count);
	if (result != KERN_SUCCESS)
		kern_fatal(result, "Could not get host priority info");

	rr_base.quantum = 0;
	rr_base.base_priority = host_pri_info.system_priority;
	rr_limit.max_priority = host_pri_info.system_priority;

	(void)processor_set_default(mach_host_self(),
				    &default_processor_set_name);
	(void)host_processor_set_priv(master_host_port,
				      default_processor_set_name,
				      &default_processor_set);

	result = task_set_policy(mach_host_self(), default_processor_set,
			    POLICY_RR,
			    (policy_base_t) & rr_base, POLICY_RR_BASE_COUNT,
			    (policy_limit_t) & rr_limit, POLICY_RR_LIMIT_COUNT,
			    TRUE);
	if (result != KERN_SUCCESS)
		kern_fatal(result, "Could not set task policy ");
#endif
}
