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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_voucher.h>
#include "common.h"

const char * kobject_name(natural_t kotype)
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
        case IKOT_TASK_RESUME:      return "TASK_RESUME";
        case IKOT_VOUCHER:          return "VOUCHER";
        case IKOT_VOUCHER_ATTR_CONTROL: return "VOUCHER_ATTR_CONTROL";
        case IKOT_UNKNOWN:
        default:                    return "UNKNOWN";
	}
}

#define VOUCHER_DETAIL_PREFIX "            "

static const unsigned int voucher_contents_size = 8192;
static uint8_t voucher_contents[voucher_contents_size];


void show_recipe_detail(mach_voucher_attr_recipe_t recipe) {
    printf(VOUCHER_DETAIL_PREFIX "Key: %u, ", recipe->key);
    printf("Command: %u, ", recipe->command);
    printf("Previous voucher: 0x%x, ", recipe->previous_voucher);
    printf("Content size: %u\n", recipe->content_size);
    switch (recipe->key) {
        case MACH_VOUCHER_ATTR_KEY_ATM:
            printf(VOUCHER_DETAIL_PREFIX "ATM ID: %llu\n", *(uint64_t *)(uintptr_t)recipe->content);
            break;
        case MACH_VOUCHER_ATTR_KEY_IMPORTANCE:
            printf(VOUCHER_DETAIL_PREFIX "IMPORTANCE INFO: %s\n", (char *)recipe->content);
            break;
        case MACH_VOUCHER_ATTR_KEY_BANK:
            printf(VOUCHER_DETAIL_PREFIX "RESOURCE ACCOUNTING INFO: %s\n", (char *)recipe->content);
            break;
        default:
            print_hex_data(VOUCHER_DETAIL_PREFIX, "Recipe Contents", (void *)recipe->content, MIN(recipe->content_size, lsmp_config.voucher_detail_length));
            break;
    }
    
}

void show_voucher_detail(mach_port_t task, mach_port_name_t voucher) {
    unsigned int recipe_size = voucher_contents_size;
    kern_return_t kr = KERN_SUCCESS;
    bzero((void *)&voucher_contents[0], sizeof(voucher_contents));
    unsigned v_kobject = 0;
    unsigned v_kotype = 0;
    kr = mach_port_kernel_object( task,
                                 voucher,
                                 &v_kotype, (unsigned *)&v_kobject);
    if (kr == KERN_SUCCESS && v_kotype == IKOT_VOUCHER ) {
 
        kr = mach_voucher_debug_info(task, voucher,
                                     (mach_voucher_attr_raw_recipe_array_t)&voucher_contents[0],
                                     &recipe_size);
        if (kr != KERN_SUCCESS && kr != KERN_NOT_SUPPORTED) {
            printf(VOUCHER_DETAIL_PREFIX "Voucher: 0x%x Failed to get contents %s\n", v_kobject, mach_error_string(kr));
            return;
        }
 
        if (recipe_size == 0) {
            printf(VOUCHER_DETAIL_PREFIX "Voucher: 0x%x has no contents\n", v_kobject);
            return;
        }
        printf(VOUCHER_DETAIL_PREFIX "Voucher: 0x%x\n", v_kobject);
        unsigned int used_size = 0;
        mach_voucher_attr_recipe_t recipe = NULL;
        while (recipe_size > used_size) {
            recipe = (mach_voucher_attr_recipe_t)&voucher_contents[used_size];
            if (recipe->key) {
                show_recipe_detail(recipe);
            }
            used_size += sizeof(mach_voucher_attr_recipe_data_t) + recipe->content_size;
        }
    } else {
        printf(VOUCHER_DETAIL_PREFIX "Invalid voucher: 0x%x\n", voucher);
    }
}

void get_receive_port_context(task_t taskp, mach_port_name_t portname, mach_port_context_t *context) {
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

int get_recieve_port_status(task_t taskp, mach_port_name_t portname, mach_port_info_ext_t *info){
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

void show_task_mach_ports(my_per_task_info_t *taskinfo, uint32_t taskCount, my_per_task_info_t *allTaskInfos)
{
    int i, emptycount = 0, portsetcount = 0, sendcount = 0, receivecount = 0, sendoncecount = 0, deadcount = 0, dncount = 0, vouchercount = 0, pid;
    kern_return_t ret;
    pid_for_task(taskinfo->task, &pid);
    
    printf("  name      ipc-object    rights     flags   boost  reqs  recv  send sonce oref  qlimit  msgcount  context            identifier  type\n");
    printf("---------   ----------  ----------  -------- -----  ---- ----- ----- ----- ----  ------  --------  ------------------ ----------- ------------\n");
	for (i = 0; i < taskinfo->tableCount; i++) {
		int j, k;
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
			printf("0x%08x  0x%08x  port-set    --------        ---      1                                                        %d  members\n",
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
				for (k = 0; k < allTaskInfos->tableCount; k++) {
					if (allTaskInfos[j].valid == FALSE ||
						&allTaskInfos[j].table[k] == &taskinfo->table[i] ||
						allTaskInfos[j].table[k].iin_object != taskinfo->table[i].iin_object)
						continue;
                    
                    printf("                  +     %s  --------        %s%s%s        %5d         <-                                       0x%08x  (%d) %s\n",
						   (allTaskInfos[j].table[k].iin_type & MACH_PORT_TYPE_SEND_ONCE) ?
					       "send-once " : "send      ",
						   (allTaskInfos[j].table[k].iin_type & MACH_PORT_TYPE_DNREQUEST) ? "D" : "-",
                           "-",
                           "-",
                           allTaskInfos[j].table[k].iin_urefs,
						   allTaskInfos[j].table[k].iin_name,
						   allTaskInfos[j].pid,
                           allTaskInfos[j].processName);
				}
			}
			continue;
		}
		else if (taskinfo->table[i].iin_type & MACH_PORT_TYPE_DEAD_NAME)
		{
			printf("0x%08x  0x%08x  dead-name   --------        ---        %5d      \n",
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
		
		printf("0x%08x  0x%08x  %s  --------        %s%s%s        %5.0d     ",
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
			printf("                                             0x%08x  %s", (natural_t)kobject, kobject_name(kotype));
            if ((kotype == IKOT_TASK_RESUME) || (kotype == IKOT_TASK) || (kotype == IKOT_TASK_NAME)) {
                if (taskinfo->task_kobject == kobject) {
                    /* neat little optimization since in most cases tasks have themselves in their ipc space */
                    printf(" SELF (%d) %s", taskinfo->pid, taskinfo->processName);
                } else {
                    my_per_task_info_t * _found_task = get_taskinfo_by_kobject((natural_t)kobject);
                    printf(" (%d) %s", _found_task->pid, _found_task->processName);
                }
            }
            
            printf("\n");
            if (kotype == IKOT_VOUCHER) {
                vouchercount++;
                if (lsmp_config.show_voucher_details) {
                    show_voucher_detail(taskinfo->task, taskinfo->table[i].iin_name);
                }
            }
			continue;
		}
        
        /* not kobject - find the receive right holder */
        my_per_task_info_t *recv_holder_taskinfo;
        mach_port_name_t recv_name = MACH_PORT_NULL;
        if (KERN_SUCCESS == get_taskinfo_of_receiver_by_send_right(&taskinfo->table[i], &recv_holder_taskinfo, &recv_name)) {
            mach_port_status_t port_status;
            mach_port_info_ext_t info;
            mach_port_context_t port_context = (mach_port_context_t)0;
            if (0 != get_recieve_port_status(recv_holder_taskinfo->task, recv_name, &info)) {
                bzero((void *)&port_status, sizeof(port_status));
            }
            port_status = info.mpie_status;
            get_receive_port_context(recv_holder_taskinfo->task, recv_name, &port_context);
            printf("   ->   %6d  %8d  0x%016llx 0x%08x  (%d) %s\n",
                   port_status.mps_qlimit,
                   port_status.mps_msgcount,
                   (uint64_t)port_context,
                   recv_name,
                   recv_holder_taskinfo->pid,
                   recv_holder_taskinfo->processName);

        } else
			printf("                                             0x00000000  (-) Unknown Process\n");
 
	}
	printf("total     = %d\n", taskinfo->tableCount + taskinfo->treeCount - emptycount);
	printf("SEND      = %d\n", sendcount);
	printf("RECEIVE   = %d\n", receivecount);
	printf("SEND_ONCE = %d\n", sendoncecount);
	printf("PORT_SET  = %d\n", portsetcount);
	printf("DEAD_NAME = %d\n", deadcount);
	printf("DNREQUEST = %d\n", dncount);
    printf("VOUCHERS  = %d\n", vouchercount);
    
}

void print_hex_data(char *prefix, char *desc, void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = addr;
    
    if (desc != NULL)
        printf ("%s%s:\n", prefix, desc);
    
    for (i = 0; i < len; i++) {
        
        if ((i % 16) == 0) {
            if (i != 0)
                printf ("  %s\n", buff);
            
            printf ("%s  %04x ", prefix, i);
        }
        
        printf (" %02x", pc[i]);
        
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }
    
    while ((i % 16) != 0) {
        printf ("   ");
        i++;
    }
    
    printf ("  %s\n", buff);
}
