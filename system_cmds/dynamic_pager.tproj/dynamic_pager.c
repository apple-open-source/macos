/* File created by Chris Youngworth, Apple Computer 2/11/99 */


#define mig_external

#include <mach/port.h>
#include <mach/mach_error.h>
#include <mach/mach_traps.h>
#include <mach/mach.h>
#ifndef MACH_BSD
#define MACH_BSD
#endif
#include <mach/bootstrap.h>
#include <mach/mach_syscalls.h>
#include <mach/mig_errors.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/gmon.h>
#include <errno.h>
#include <kvm.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nlist.h>
#include <ctype.h>
#include <unistd.h> 
#include <paths.h>

#include <default_pager/default_pager_types.h>
#include <default_pager_alerts_server.h>
#include <backing_store_alerts.h>
#include <backing_store_triggers_server.h>

int	debug = 0;
int	file_count = 0;
int 	low_water = 0;
int	hi_water = 0;
int	local_hi_water = 0;
int	size  = 20000000;
int	priority = 0;
int	options = 0;
char	fileroot[512];


/* global parameters for application notification option */
mach_port_t	trigger_port = NULL;
mach_port_t	notify_port = NULL;
int		notify_high = 0;
int		bs_recovery;

/*
void	setprof __P((struct kvmvars *kvp, int state));
void	dumpstate __P((struct kvmvars *kvp));
void	reset __P((struct kvmvars *kvp));
*/



mach_msg_return_t
server_alert_loop(
    mach_msg_size_t max_size,
    mach_port_t rcv_name,
    mach_msg_options_t options)
{
    mig_reply_error_t *bufRequest = 0, *bufReply = 0;
    register mach_msg_return_t mr;
    register kern_return_t kr;

    if ((kr = vm_allocate(mach_task_self(),
                     (vm_address_t *)&bufRequest,
                     max_size + MAX_TRAILER_SIZE,
                     TRUE)) != KERN_SUCCESS)
      return kr;
    mlock(bufRequest, max_size + MAX_TRAILER_SIZE);
    if ((kr = vm_allocate(mach_task_self(),
                     (vm_address_t *)&bufReply,
                     max_size + MAX_TRAILER_SIZE,
                     TRUE)) != KERN_SUCCESS)
      return kr;
    mlock(bufReply, max_size + MAX_TRAILER_SIZE);
    while(TRUE) {
       mr = mach_msg_overwrite_trap(&bufRequest->Head, MACH_RCV_MSG|options,
                                 0, max_size, rcv_name,
                                 MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL,
                                 (mach_msg_header_t *) 0, 0);
        if (mr == MACH_MSG_SUCCESS) {
           /* we have a request message */

           if(!(default_pager_alerts_server(
				&bufRequest->Head, &bufReply->Head)))
		backing_store_triggers_server(
				&bufRequest->Head, &bufReply->Head);

           if (!(bufReply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) &&
               bufReply->RetCode != KERN_SUCCESS) {
               if (bufReply->RetCode == MIG_NO_REPLY)
                /*
                 * This return code is a little tricky--
                 * it appears that the demux routine found an
                 * error of some sort, but since that error
                 * would not normally get returned either to
                 * the local user or the remote one, we pretend it's
                 * ok.
                 */

               bufRequest->Head.msgh_remote_port = MACH_PORT_NULL;
               mach_msg_destroy(&bufRequest->Head);
	       continue;
           }

           if (bufReply->Head.msgh_remote_port == MACH_PORT_NULL) {
              /* no reply port, so destroy the reply */
              if (bufReply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX)
                  mach_msg_destroy(&bufReply->Head);
	   } else {
		break;
	   }
	} else {
	   break;
	}   
    } 

    (void)vm_deallocate(mach_task_self(),
                    (vm_address_t) bufRequest,
                    max_size + MAX_TRAILER_SIZE);
    (void)vm_deallocate(mach_task_self(),
                    (vm_address_t) bufReply,
                    max_size + MAX_TRAILER_SIZE);
    return KERN_FAILURE;

}       


kern_return_t
backing_store_triggers(dynamic_pager, hi_wat, flags, port)
 		mach_port_t	dynamic_pager;
		int		hi_wat;
		int		flags;
		mach_port_t 	port;
{
	if((hi_wat + size) > low_water) 
		return KERN_FAILURE; /* let ipc system clean up port */
	notify_port = port;
	notify_high = hi_wat;
	if(hi_water < notify_high) {
		local_hi_water = notify_high;
	} else {
		local_hi_water = hi_water;
	}
	if(notify_high > hi_water) {
		default_pager_space_alert(trigger_port, HI_WAT_ALERT);
	}
	return KERN_SUCCESS;
}


kern_return_t
default_pager_space_alert(alert_port, flags)
	mach_port_t	alert_port;
	int	flags;
{
	char subfile[512];
	FILE *file_ptr;
	off_t	filesize = size;
	int	error;

	if(flags & HI_WAT_ALERT) {
/* printf("HI WAT ALERT!!\n"); */
		file_count++;
		sprintf(subfile, "%s%d", fileroot, file_count);
		file_ptr = fopen(subfile, "w+");
		fchmod(fileno(file_ptr), (mode_t)01600);
		error = fcntl(fileno(file_ptr), F_SETSIZE, &filesize);
		fclose(file_ptr);
		if(error == -1) {
			unlink(subfile);
			file_count--;
			local_hi_water = local_hi_water>>2;
			if(notify_high >= (local_hi_water)) {
				if(notify_port) {
					/* notify monitoring app of */
					/* backing store shortage  */
					backing_store_alert(notify_port,
								HI_WAT_ALERT);
					mach_port_deallocate(mach_task_self(), 
								notify_port);
					notify_high = 0;
					notify_port = 0;
				}
			}
			macx_triggers(local_hi_water, 
				low_water, HI_WAT_ALERT, alert_port);
		} else {
			if(hi_water < notify_high) {
				if(local_hi_water < notify_high) {
				   bs_recovery = notify_high - local_hi_water;
				}
				local_hi_water = notify_high;
			} else {
				if(local_hi_water < hi_water) {
				   bs_recovery = hi_water - local_hi_water;
				}
				local_hi_water = hi_water;
			}
			macx_swapon(subfile, flags, size, priority);
			if(bs_recovery <= size) {
				if((bs_recovery != 0) && (notify_port)) {
					backing_store_alert(notify_port,
								LO_WAT_ALERT);
					mach_port_deallocate(mach_task_self(), 
								notify_port);
					notify_high = 0;
					notify_port = NULL;
					
					bs_recovery = 0;
				}
			} else 
				bs_recovery = bs_recovery-size;
		}
	
		macx_triggers(local_hi_water, 
			low_water, HI_WAT_ALERT, alert_port);
	}
	if(flags & LO_WAT_ALERT) {
/* Turn into a logged message printf("LO WAT ALERT!!\n"); */
		sprintf(subfile, "%s%d", fileroot, file_count);
		if(hi_water < notify_high) {
			local_hi_water = notify_high;
		} else {
			local_hi_water = hi_water;
		}
		if((bs_recovery != 0) && (notify_port)) {
			backing_store_alert(notify_port, LO_WAT_ALERT);
			mach_port_deallocate(mach_task_self(), notify_port);
			notify_high = 0;
			notify_port = NULL;
					
			bs_recovery = 0;
		}
		if(macx_swapoff(subfile, flags) == 0) {
			unlink(subfile);
			file_count--;
		}
		macx_triggers(local_hi_water, low_water, LO_WAT_ALERT, alert_port);
	}
	return KERN_SUCCESS;
}

void
wait_on_paging_trigger(trigger_port)
	mach_port_t	trigger_port;
{
	kern_return_t	result;
	result = server_alert_loop(4096, trigger_port, MACH_MSG_OPTION_NONE);
	if (result != KERN_SUCCESS) {
	     fprintf(stderr, "dynamic_pager: default pager alert failed\n");
	     exit(1);
	}
	exit(0);
}

void
paging_setup(flags, size, priority, low, high)
	int	flags;
	int	size;
	int	priority;
	int	low;
	int	high;
{
	off_t		filesize = size;
	char 		subfile[512];
	FILE 		*file_ptr;
        kern_return_t   error;

	file_count = 0;
	sprintf(subfile, "%s%d", fileroot, file_count);
	file_ptr = fopen(subfile, "w+");
	fchmod(fileno(file_ptr), (mode_t)01600);
	fcntl(fileno(file_ptr), F_SETSIZE, &filesize);
	fclose(file_ptr);
        
	macx_swapon(subfile, flags, size, priority);
	if(hi_water) {
		daemon(0,0);

		if (mach_port_allocate(mach_task_self(), 
				MACH_PORT_RIGHT_RECEIVE, 
				&trigger_port) != KERN_SUCCESS)  {
			fprintf(stderr,"allocation of trigger port failed\n");
			exit(1);
		}
		macx_triggers(high, low, HI_WAT_ALERT, trigger_port);
		if(low) {
			macx_triggers(high, 
				low, LO_WAT_ALERT, trigger_port);
		}
		/* register control port for applications wishing to */
		/* get backing store notifications or change dynamic */
		/* pager settings. */
		set_dp_control_port(mach_host_self(), trigger_port);
		wait_on_paging_trigger(trigger_port); 
	}
	 exit(0);
}
int
main(int argc, char **argv)
{
	extern char *optarg;
	extern int optind;
	char default_filename[] = "/private/var/vm/swapfile";
	int ch;

	seteuid(getuid());
	strcpy(fileroot, default_filename);

	while ((ch = getopt(argc, argv, "F:L:H:S:P:O:")) != EOF) {
		switch((char)ch) {

		case 'F':
			strncpy(fileroot, optarg, 500);
			break;

		case 'L':
			low_water = atoi(optarg);
			break;
		case 'H':
			hi_water = atoi(optarg);
			break;
		case 'S':
			size = atoi(optarg);
			break;
		case 'P':
			priority = atoi(optarg);
			break;

		default:
			(void)fprintf(stderr,
			    "usage: dynamic_pager [-F filename] [-L low water alert trigger] [-H high water alert trigger] [-S file size] [-P priority]\n");
			exit(1);
		}
	}
	local_hi_water = hi_water;
	if((low_water != 0) && (low_water <= (size + hi_water))) {
		(void)fprintf(stderr,  "usage: low water trigger must be larger than size + hi_water\n"); 
		exit(1);
	}
	argc -= optind;
	argv += optind;
	paging_setup(0, size, priority, low_water, hi_water);
	return (0);
}
