/*
 * Copyright (c) 2002-2007 Apple Inc. All rights reserved.
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


#define PROC_NAME_LEN 256
#define BUFSTR_LEN 30

#if TARGET_OS_EMBEDDED
#define TASK_FOR_PID_USAGE_MESG "\nPlease check your boot-args to ensure you have access to task_for_pid()."
#else
#define TASK_FOR_PID_USAGE_MESG ""
#endif
/*
 * WARNING - these types are copied from xnu/osfmk/kern/ipc_kobject.h
 * Need to stay in sync to print accurate results.
 */
#define	IKOT_NONE				0
#define IKOT_THREAD				1
#define	IKOT_TASK				2
#define	IKOT_HOST				3
#define	IKOT_HOST_PRIV			4
#define	IKOT_PROCESSOR			5
#define	IKOT_PSET				6
#define	IKOT_PSET_NAME			7
#define	IKOT_TIMER				8
#define	IKOT_PAGING_REQUEST		9
#define	IKOT_MIG				10
#define	IKOT_MEMORY_OBJECT		11
#define	IKOT_XMM_PAGER			12
#define	IKOT_XMM_KERNEL			13
#define	IKOT_XMM_REPLY			14
#define IKOT_UND_REPLY			15
#define IKOT_HOST_NOTIFY		16
#define IKOT_HOST_SECURITY		17
#define	IKOT_LEDGER				18
#define IKOT_MASTER_DEVICE		19
#define IKOT_TASK_NAME			20
#define IKOT_SUBSYSTEM			21
#define IKOT_IO_DONE_QUEUE		22
#define IKOT_SEMAPHORE			23
#define IKOT_LOCK_SET			24
#define IKOT_CLOCK				25
#define IKOT_CLOCK_CTRL			26
#define IKOT_IOKIT_SPARE		27
#define IKOT_NAMED_ENTRY		28
#define IKOT_IOKIT_CONNECT		29
#define IKOT_IOKIT_OBJECT		30
#define IKOT_UPL				31
#define IKOT_MEM_OBJ_CONTROL	32
#define IKOT_AU_SESSIONPORT		33
#define IKOT_FILEPORT			34
#define IKOT_LABELH             35
#define	IKOT_UNKNOWN			36	/* magic catchall	*/
#define	IKOT_MAX_TYPE	(IKOT_UNKNOWN+1)	/* # of IKOT_ types	*/

#define SHOW_PORT_STATUS_FLAGS(flags) \
	(flags & MACH_PORT_STATUS_FLAG_TEMPOWNER)	?"T":"-", \
	(flags & MACH_PORT_STATUS_FLAG_GUARDED)		?"G":"-", \
	(flags & MACH_PORT_STATUS_FLAG_STRICT_GUARD)	?"S":"-", \
	(flags & MACH_PORT_STATUS_FLAG_IMP_DONATION)	?"I":"-", \
	(flags & MACH_PORT_STATUS_FLAG_REVIVE)		?"R":"-", \
	(flags & MACH_PORT_STATUS_FLAG_TASKPTR)		?"P":"-"

/* private structure to wrap up per-task info */
typedef struct my_per_task_info {
    task_t task;
    pid_t pid;
    ipc_info_space_t info;
    ipc_info_name_array_t table;
    mach_msg_type_number_t tableCount;
    ipc_info_tree_name_array_t tree;
    mach_msg_type_number_t treeCount;
    char processName[PROC_NAME_LEN];
} my_per_task_info_t;

my_per_task_info_t *psettaskinfo;
mach_msg_type_number_t taskCount;

static const char *
kobject_name(natural_t kotype)
{
	switch (kotype) {
        case IKOT_NONE:             return "message-queue";
        case IKOT_THREAD:           return "THREAD";
        case IKOT_TASK:             return "TASK";
        case IKOT_HOST:             return "HOST";
        case IKOT_HOST_PRIV:        return "HOST-PRIV";
        case IKOT_PROCESSOR:        return "PROCESSOR";
        case IKOT_PSET:             return "PROCESSOR-SET";
        case IKOT_PSET_NAME:        return "PROCESSOR-SET-NAME";
        case IKOT_TIMER:            return "TIMER";
        case IKOT_PAGING_REQUEST:   return "PAGER-REQUEST";
        case IKOT_MIG:              return "MIG";
        case IKOT_MEMORY_OBJECT:    return "MEMORY-OBJECT";
        case IKOT_XMM_PAGER:        return "XMM-PAGER";
        case IKOT_XMM_KERNEL:       return "XMM-KERNEL";
        case IKOT_XMM_REPLY:        return "XMM-REPLY";
        case IKOT_UND_REPLY:        return "UND-REPLY";
        case IKOT_HOST_NOTIFY:      return "message-queue";
        case IKOT_HOST_SECURITY:    return "HOST-SECURITY";
        case IKOT_LEDGER:           return "LEDGER";
        case IKOT_MASTER_DEVICE:    return "MASTER-DEVICE";
        case IKOT_TASK_NAME:        return "TASK-NAME";
        case IKOT_SUBSYSTEM:        return "SUBSYSTEM";
        case IKOT_IO_DONE_QUEUE:    return "IO-QUEUE-DONE";
        case IKOT_SEMAPHORE:        return "SEMAPHORE";
        case IKOT_LOCK_SET:         return "LOCK-SET";
        case IKOT_CLOCK:            return "CLOCK";
        case IKOT_CLOCK_CTRL:       return "CLOCK-CONTROL";
        case IKOT_IOKIT_SPARE:      return "IOKIT-SPARE";
        case IKOT_NAMED_ENTRY:      return "NAMED-MEMORY";
        case IKOT_IOKIT_CONNECT:    return "IOKIT-CONNECT";
        case IKOT_IOKIT_OBJECT:     return "IOKIT-OBJECT";
        case IKOT_UPL:              return "UPL";
        case IKOT_MEM_OBJ_CONTROL:  return "XMM-CONTROL";
        case IKOT_AU_SESSIONPORT:   return "SESSIONPORT";
        case IKOT_FILEPORT:         return "FILEPORT";
        case IKOT_LABELH:           return "MACF-LABEL";
        case IKOT_UNKNOWN:
        default:                    return "UNKNOWN";
	}
}

static void proc_pid_to_name(int pid, char *pname){
    if (0 == proc_name(pid, pname, PROC_NAME_LEN)) {
        strcpy(pname, "Unknown Process");
    }
}

static int get_recieve_port_status(task_t taskp, mach_port_name_t portname, mach_port_info_ext_t *info){
    if (info == NULL) {
        return -1;
    }
    mach_msg_type_number_t statusCnt;
    kern_return_t ret;
    statusCnt = MACH_PORT_INFO_EXT_COUNT;
    ret = mach_port_get_attributes(taskp,
                                   portname,
                                   MACH_PORT_INFO_EXT,
                                   (mach_port_info_t)info,
                                   &statusCnt);
    if (ret != KERN_SUCCESS) {
        fprintf(stderr, "mach_port_get_attributes(0x%08x) failed: %s\n",
                portname,
                mach_error_string(ret));
        return -1;
    }

    return 0;
}

static void get_receive_port_context(task_t taskp, mach_port_name_t portname, mach_port_context_t *context) {
	if (context == NULL) {
		return;
	}

	kern_return_t ret;
	ret = mach_port_get_context(taskp, portname, context);
	if (ret != KERN_SUCCESS) {
		fprintf(stderr, "mach_port_get_context(0x%08x) failed: %s\n",
			portname,
			mach_error_string(ret));
		*context = (mach_port_context_t)0;
	}
	return;
}

static void show_task_mach_ports(my_per_task_info_t *taskinfo){
    int i, emptycount = 0, portsetcount = 0, sendcount = 0, receivecount = 0, sendoncecount = 0, deadcount = 0, dncount = 0, pid;
    kern_return_t ret;
    pid_for_task(taskinfo->task, &pid);
    
    printf("  name      ipc-object    rights     flags   boost  reqs  recv  send sonce oref  qlimit  msgcount  context            identifier  type\n");
    printf("---------   ----------  ----------  -------- -----  ---- ----- ----- ----- ----  ------  --------  ------------------ ----------- ------------\n");
	for (i = 0; i < taskinfo->tableCount; i++) {
		int j, k;
		boolean_t found = FALSE;
		boolean_t send = FALSE;
		boolean_t sendonce = FALSE;
		boolean_t dnreq = FALSE;
		int sendrights = 0;
		unsigned int kotype = 0;
		vm_offset_t kobject = (vm_offset_t)0;
        
        /* skip empty slots in the table */
        if ((taskinfo->table[i].iin_type & MACH_PORT_TYPE_ALL_RIGHTS) == 0) {
            emptycount++;
            continue;
        }
        
        
		if (taskinfo->table[i].iin_type == MACH_PORT_TYPE_PORT_SET) {
			mach_port_name_array_t members;
			mach_msg_type_number_t membersCnt;
			
			ret = mach_port_get_set_status(taskinfo->task,
										   taskinfo->table[i].iin_name,
										   &members, &membersCnt);
			if (ret != KERN_SUCCESS) {
				fprintf(stderr, "mach_port_get_set_status(0x%08x) failed: %s\n",
						taskinfo->table[i].iin_name,
						mach_error_string(ret));
				continue;
			}
			printf("0x%08x  0x%08x  port-set    -------- -----  ---      1                                                        %d  members\n",
				   taskinfo->table[i].iin_name,
				   taskinfo->table[i].iin_object,
				   membersCnt);
			/* get some info for each portset member */
			for (j = 0; j < membersCnt; j++) {
				for (k = 0; k < taskinfo->tableCount; k++) {
					if (taskinfo->table[k].iin_name == members[j]) {
                        mach_port_info_ext_t info;
			mach_port_status_t port_status;
			mach_port_context_t port_context = (mach_port_context_t)0;
                        if (0 != get_recieve_port_status(taskinfo->task, taskinfo->table[k].iin_name, &info)) {
                            bzero((void *)&info, sizeof(info));
                        }
			port_status = info.mpie_status;
			get_receive_port_context(taskinfo->task, taskinfo->table[k].iin_name, &port_context);
			printf(" -          0x%08x  %s  --%s%s%s%s%s%s %5d  %s%s%s  %5d %5.0d %5.0d   %s   %6d  %8d  0x%016llx 0x%08x  (%d) %s\n",
							   taskinfo->table[k].iin_object,
							   (taskinfo->table[k].iin_type & MACH_PORT_TYPE_SEND) ? "recv,send ":"recv      ",
				SHOW_PORT_STATUS_FLAGS(port_status.mps_flags),
				info.mpie_boost_cnt,
                               (taskinfo->table[k].iin_type & MACH_PORT_TYPE_DNREQUEST) ? "D" : "-",
                               (port_status.mps_nsrequest) ? "N" : "-",
                               (port_status.mps_pdrequest) ? "P" : "-",
                               1,
                               taskinfo->table[k].iin_urefs,
                               port_status.mps_sorights,
                               (port_status.mps_srights) ? "Y" : "N",
                               port_status.mps_qlimit,
                               port_status.mps_msgcount,
			       (uint64_t)port_context,
							   taskinfo->table[k].iin_name,
							   pid,
                               taskinfo->processName);
						break;
					}
				}
			}
            
			ret = vm_deallocate(mach_task_self(), (vm_address_t)members,
								membersCnt * sizeof(mach_port_name_t));
			if (ret != KERN_SUCCESS) {
				fprintf(stderr, "vm_deallocate() failed: %s\n",
						mach_error_string(ret));
				exit(1);
			}
			portsetcount++;
			continue;
		}
        
		if (taskinfo->table[i].iin_type & MACH_PORT_TYPE_SEND) {
			send = TRUE;
			sendrights = taskinfo->table[i].iin_urefs;
			sendcount++;
		}
		
		if (taskinfo->table[i].iin_type & MACH_PORT_TYPE_DNREQUEST) {
			dnreq = TRUE;
			dncount++;
		}
        
		if (taskinfo->table[i].iin_type & MACH_PORT_TYPE_RECEIVE) {
			mach_port_status_t status;
			mach_port_info_ext_t info;
			mach_port_context_t context = (mach_port_context_t)0;
            ret = get_recieve_port_status(taskinfo->task, taskinfo->table[i].iin_name, &info);
	    get_receive_port_context(taskinfo->task, taskinfo->table[i].iin_name, &context);
            /* its ok to fail in fetching attributes */
            if (ret < 0) {
                continue;
            }
	    status = info.mpie_status;
			printf("0x%08x  0x%08x  %s  --%s%s%s%s%s%s %5d  %s%s%s  %5d %5.0d %5.0d   %s   %6d  %8d  0x%016llx \n",
				   taskinfo->table[i].iin_name,
				   taskinfo->table[i].iin_object,
				   (send) ? "recv,send ":"recv      ",
				   SHOW_PORT_STATUS_FLAGS(status.mps_flags),
				   info.mpie_boost_cnt,
				   (dnreq) ? "D":"-",
				   (status.mps_nsrequest) ? "N":"-",
				   (status.mps_pdrequest) ? "P":"-",
                   1,
				   sendrights,
                   status.mps_sorights,
				   (status.mps_srights) ? "Y":"N",
				   status.mps_qlimit,
				   status.mps_msgcount,
				   (uint64_t)context);
			receivecount++;
            
			/* show other rights (in this and other tasks) for the port */
			for (j = 0; j < taskCount; j++) {
				for (k = 0; k < psettaskinfo->tableCount; k++) {
					if (&psettaskinfo[j].table[k] == &taskinfo->table[i] ||
						psettaskinfo[j].table[k].iin_object != taskinfo->table[i].iin_object)
						continue;
                    
                    printf("                  +     %s  -------- -----  %s%s%s        %5d         <-                                       0x%08x  (%d) %s\n",
						   (psettaskinfo[j].table[k].iin_type & MACH_PORT_TYPE_SEND_ONCE) ?
					       "send-once " : "send      ",
						   (psettaskinfo[j].table[k].iin_type & MACH_PORT_TYPE_DNREQUEST) ? "D" : "-",
                           "-",
                           "-",
                           psettaskinfo[j].table[k].iin_urefs,
						   psettaskinfo[j].table[k].iin_name,
						   psettaskinfo[j].pid,
                           psettaskinfo[j].processName);
				}
			}
			continue;
		}
		else if (taskinfo->table[i].iin_type & MACH_PORT_TYPE_DEAD_NAME)
		{
			printf("0x%08x  0x%08x  dead-name  -------- -----  ---        %5d      \n",
				   taskinfo->table[i].iin_name,
				   taskinfo->table[i].iin_object,
				   taskinfo->table[i].iin_urefs);
			deadcount++;
			continue;
		}
        
		if (taskinfo->table[i].iin_type & MACH_PORT_TYPE_SEND_ONCE) {
			sendonce = TRUE;
			sendoncecount++;
		}
		
		printf("0x%08x  0x%08x  %s  -------- -----  %s%s%s        %5.0d     ",
			   taskinfo->table[i].iin_name,
			   taskinfo->table[i].iin_object,
			   (send) ? "send      ":"send-once ",
			   (dnreq) ? "D":"-",
			   "-",
			   "-",
			   (send) ? sendrights : 0);
		
		/* converting to kobjects is not always supported */
		ret = mach_port_kernel_object(taskinfo->task,
									  taskinfo->table[i].iin_name,
									  &kotype, (unsigned *)&kobject);
		if (ret == KERN_SUCCESS && kotype != 0) {
			printf("                                             0x%08x  %s\n", (natural_t)kobject, kobject_name(kotype));
			continue;
		}
        
		/* not kobject - find the receive right holder */
		for (j = 0; j < taskCount && !found; j++) {
			for (k = 0; k < psettaskinfo[j].tableCount && !found; k++) {
				if ((psettaskinfo[j].table[k].iin_type & MACH_PORT_TYPE_RECEIVE) &&
					psettaskinfo[j].table[k].iin_object == taskinfo->table[i].iin_object ) {
                    mach_port_status_t port_status;
		    mach_port_info_ext_t info;
		    mach_port_context_t port_context = (mach_port_context_t)0;
                    if (0 != get_recieve_port_status(psettaskinfo[j].task, psettaskinfo[j].table[k].iin_name, &info)) {
                        bzero((void *)&port_status, sizeof(port_status));
                    }
		    port_status = info.mpie_status;
		    get_receive_port_context(psettaskinfo[j].task, psettaskinfo[j].table[k].iin_name, &port_context);
                    printf("   ->   %6d  %8d  0x%016llx 0x%08x  (%d) %s\n",
                           port_status.mps_qlimit,
                           port_status.mps_msgcount,
			   (uint64_t)port_context,
						   psettaskinfo[j].table[k].iin_name,
						   psettaskinfo[j].pid,
                           psettaskinfo[j].processName);
					found = TRUE;
				}
			}
		}
		if (!found)
			printf("                                             0x00000000  (-) Unknown Process\n");
	}
	printf("total     = %d\n", taskinfo->tableCount + taskinfo->treeCount - emptycount);
	printf("SEND      = %d\n", sendcount);
	printf("RECEIVE   = %d\n", receivecount);
	printf("SEND_ONCE = %d\n", sendoncecount);
	printf("PORT_SET  = %d\n", portsetcount);
	printf("DEAD_NAME = %d\n", deadcount);
	printf("DNREQUEST = %d\n", dncount);

}

int main(int argc, char *argv[]) {
	int pid;
    const char * dash_all_str = "-all";
    const char * dash_h = "-h";
    int show_all_tasks = 0;
	kern_return_t ret;
	my_per_task_info_t aTask;
	my_per_task_info_t *taskinfo;
    task_array_t tasks;
    
	int i ;
    
	if (argc != 2 || !strncmp(dash_h, argv[1], strlen(dash_h))) {
		fprintf(stderr, "Usage: %s [ -all | <pid> ]\n", argv[0]);
		fprintf(stderr, "Lists information about mach ports. Please see man page for description of each column.\n");
		exit(1);
	}

    if (strncmp(dash_all_str, argv[1], strlen(dash_all_str)) == 0) {
        pid = 0;
        show_all_tasks = 1;
    } else
        pid = atoi(argv[1]);
    
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
        
		/* convert each task to structure of pointer for the task info */
		psettaskinfo = (my_per_task_info_t *)malloc(taskCount * sizeof(my_per_task_info_t));
		for (i = 0; i < taskCount; i++) {
			psettaskinfo[i].task = tasks[i];
			pid_for_task(tasks[i], &psettaskinfo[i].pid);
			ret = mach_port_space_info(tasks[i], &psettaskinfo[i].info,
									   &psettaskinfo[i].table, &psettaskinfo[i].tableCount,
									   &psettaskinfo[i].tree, &psettaskinfo[i].treeCount);
			if (ret != KERN_SUCCESS) {
				fprintf(stderr, "mach_port_space_info() failed: pid:%d error: %s\n",psettaskinfo[i].pid, mach_error_string(ret));
                if (show_all_tasks == 1) {
                    printf("Ignoring failure of mach_port_space_info() for task %d for '-all'\n", tasks[i]);
                    psettaskinfo[i].pid = 0;
                    continue;
                } else {
                    exit(1);
                }
			}
            proc_pid_to_name(psettaskinfo[i].pid, psettaskinfo[i].processName);
			if (psettaskinfo[i].pid == pid)
				taskinfo = &psettaskinfo[i];
		}
	}
	else
	{
		fprintf(stderr, "warning: should run as root for best output (cross-ref to other tasks' ports).\n");
		/* just the one process */
		ret = task_for_pid(mach_task_self(), pid, &aTask.task);
		if (ret != KERN_SUCCESS) {
			fprintf(stderr, "task_for_pid() failed: %s %s\n", mach_error_string(ret), TASK_FOR_PID_USAGE_MESG);
			exit(1);
		}
		ret = mach_port_space_info(aTask.task, &aTask.info,
								   &aTask.table, &aTask.tableCount,
								   &aTask.tree, &aTask.treeCount);
		if (ret != KERN_SUCCESS) {
			fprintf(stderr, "mach_port_space_info() failed: %s\n", mach_error_string(ret));
			exit(1);
		}
		taskinfo = &aTask;
		psettaskinfo = taskinfo;
        proc_pid_to_name(psettaskinfo->pid, psettaskinfo->processName);
		taskCount = 1;
	}
    
    if( show_all_tasks == 0){
        printf("Process (%d) : %s\n", taskinfo->pid, taskinfo->processName);
        show_task_mach_ports(taskinfo);
    }else {
        for (i=0; i < taskCount; i++){
            if (psettaskinfo[i].pid == 0)
                continue;
            printf("Process (%d) : %s\n", psettaskinfo[i].pid, psettaskinfo[i].processName);
            show_task_mach_ports(&psettaskinfo[i]);
            printf("\n\n");
        }
    }
    
	if (taskCount > 1){
        vm_deallocate(mach_task_self(), (vm_address_t)tasks, (vm_size_t)taskCount * sizeof(mach_port_t));
		free(psettaskinfo);
    }
    
	return(0);
}
