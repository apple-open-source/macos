/*
 * Copyright (c) 2002-20014 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */
#include <unistd.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach_debug/ipc_info.h>
#include <stdio.h>
#include <stdlib.h>
#include <libproc.h>
#include <TargetConditionals.h>
#include "common.h"


#if TARGET_OS_EMBEDDED
#define TASK_FOR_PID_USAGE_MESG "\nPlease check your boot-args to ensure you have access to task_for_pid()."
#else
#define TASK_FOR_PID_USAGE_MESG ""
#endif


struct prog_configs lsmp_config = {
    .show_all_tasks         = FALSE,
    .show_voucher_details   = FALSE,
    .verbose                = FALSE,
    .pid                    = 0,
};

my_per_task_info_t *psettaskinfo;
mach_msg_type_number_t taskCount;

static void print_usage(char *progname) {
    fprintf(stderr, "Usage: %s -p <pid> [-a|-v|-h] \n", "lsmp");
    fprintf(stderr, "Lists information about mach ports. Please see man page for description of each column.\n");
    fprintf(stderr, "\t-p <pid> :  print all mach ports for process id <pid>. \n");
    fprintf(stderr, "\t-a :  print all mach ports for all processeses. \n");
    fprintf(stderr, "\t-v :  print verbose details for kernel objects.\n");
    fprintf(stderr, "\t-h :  print this help.\n");
    exit(1);
}


int main(int argc, char *argv[]) {
    kern_return_t ret;
    task_t aTask;
    my_per_task_info_t *taskinfo = NULL;
    task_array_t tasks;
    char *progname = "lsmp";
    int i, option = 0;
    lsmp_config.voucher_detail_length = 128; /* default values for config */
    
    while((option = getopt(argc, argv, "hvalp:")) != -1) {
		switch(option) {
            case 'a':
                /* user asked for info on all processes */
                lsmp_config.pid = 0;
                lsmp_config.show_all_tasks = 1;
                break;
                
            case 'l':
                /* for compatibility with sysdiagnose's usage of -all */
                lsmp_config.voucher_detail_length = 1024;
                /* Fall through to 'v' */
                
            case 'v':
                lsmp_config.show_voucher_details = TRUE;
                lsmp_config.verbose = TRUE;
                break;
                
            case 'p':
                lsmp_config.pid = atoi(optarg);
                if (lsmp_config.pid == 0) {
                    fprintf(stderr, "Unknown pid: %s\n", optarg);
                    exit(1);
                }
                break;
                
            default:
                fprintf(stderr, "Unknown argument. \n");
                /* Fall through to 'h' */
                
            case 'h':
                print_usage(progname);
                break;
                
        }
    }
    argc -= optind;
    argv += optind;
    
    
	/* if privileged, get the info for all tasks so we can match ports up */
	if (geteuid() == 0) {
		processor_set_name_array_t psets;
		mach_msg_type_number_t psetCount;
		mach_port_t pset_priv;
        
		
		ret = host_processor_sets(mach_host_self(), &psets, &psetCount);
		if (ret != KERN_SUCCESS) {
			fprintf(stderr, "host_processor_sets() failed: %s\n", mach_error_string(ret));
			exit(1);
		}
		if (psetCount != 1) {
			fprintf(stderr, "Assertion Failure: pset count greater than one (%d)\n", psetCount);
			exit(1);
		}
        
		/* convert the processor-set-name port to a privileged port */
		ret = host_processor_set_priv(mach_host_self(), psets[0], &pset_priv);
		if (ret != KERN_SUCCESS) {
			fprintf(stderr, "host_processor_set_priv() failed: %s\n", mach_error_string(ret));
			exit(1);
		}
		mach_port_deallocate(mach_task_self(), psets[0]);
		vm_deallocate(mach_task_self(), (vm_address_t)psets, (vm_size_t)psetCount * sizeof(mach_port_t));
        
		/* convert the processor-set-priv to a list of tasks for the processor set */
		ret = processor_set_tasks(pset_priv, &tasks, &taskCount);
		if (ret != KERN_SUCCESS) {
			fprintf(stderr, "processor_set_tasks() failed: %s\n", mach_error_string(ret));
			exit(1);
		}
		mach_port_deallocate(mach_task_self(), pset_priv);
        
	}
	else
	{
		fprintf(stderr, "warning: should run as root for best output (cross-ref to other tasks' ports).\n");
		/* just the one process */
		ret = task_for_pid(mach_task_self(), lsmp_config.pid, &aTask);
		if (ret != KERN_SUCCESS) {
			fprintf(stderr, "task_for_pid() failed: %s %s\n", mach_error_string(ret), TASK_FOR_PID_USAGE_MESG);
			exit(1);
		}
        taskCount = 1;
        tasks = &aTask;
	}
    
    /* convert each task to structure of pointer for the task info */
    psettaskinfo = allocate_taskinfo_memory(taskCount);
    
    for (i = 0; i < taskCount; i++) {
        ret = collect_per_task_info(&psettaskinfo[i], tasks[i]);
        if (ret != KERN_SUCCESS) {
            printf("Ignoring failure of mach_port_space_info() for task %d for '-all'\n", tasks[i]);
            continue;
        }
        
        if (psettaskinfo[i].pid == lsmp_config.pid)
            taskinfo = &psettaskinfo[i];
        
        ret = KERN_SUCCESS;
    }
    
    
    if (lsmp_config.show_all_tasks == FALSE) {
        if (taskinfo == NULL) {
            fprintf(stderr, "Failed to find task ipc information for pid %d\n", lsmp_config.pid);
            exit(1);
        }
        printf("Process (%d) : %s\n", taskinfo->pid, taskinfo->processName);
        show_task_mach_ports(taskinfo, taskCount, psettaskinfo);
        print_task_exception_info(taskinfo);
        printf("\n");
        print_task_threads_special_ports(taskinfo);

        
    } else {
        for (i=0; i < taskCount; i++) {
            if (psettaskinfo[i].valid != TRUE)
                continue;
            printf("Process (%d) : %s\n", psettaskinfo[i].pid, psettaskinfo[i].processName);
            show_task_mach_ports(&psettaskinfo[i], taskCount, psettaskinfo);
            print_task_exception_info(&psettaskinfo[i]);

            if (lsmp_config.verbose) {
                printf("\n");
                print_task_threads_special_ports(&psettaskinfo[i]);
            }

            printf("\n\n");
        }
    }
    
	if (taskCount > 1) {
        vm_deallocate(mach_task_self(), (vm_address_t)tasks, (vm_size_t)taskCount * sizeof(mach_port_t));
    }
    
    deallocate_taskinfo_memory(psettaskinfo);
    
	return(0);
}
