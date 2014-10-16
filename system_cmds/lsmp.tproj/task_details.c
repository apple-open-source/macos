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

#include "common.h"

static my_per_task_info_t NOT_FOUND_TASK_INFO = {
    .task = NULL,
    .task_kobject = NULL,
    .pid = -1,
    .info = {0,0,0,0,0,0},
    .table = NULL,
    .tableCount = 0,
    .tree = NULL,
    .treeCount = 0,
    .valid = FALSE,
    .processName = "Unknown",
};

char * get_task_name_by_pid(pid_t pid);

static void proc_pid_to_name(int pid, char *pname){
    if (0 == proc_name(pid, pname, PROC_NAME_LEN)) {
        strcpy(pname, "Unknown Process");
    }
}

static my_per_task_info_t *global_taskinfo = NULL;
static uint32_t global_taskcount = 0;

my_per_task_info_t * allocate_taskinfo_memory(uint32_t taskCount)
{
    my_per_task_info_t * retval = malloc(taskCount * sizeof(my_per_task_info_t));
    if (retval) {
        bzero((void *)retval, taskCount * sizeof(my_per_task_info_t));
    }
    global_taskcount = taskCount;
    global_taskinfo = retval;
    return retval;
}

void deallocate_taskinfo_memory(my_per_task_info_t *data){
    if (data) {
        free(data);
        global_taskinfo = NULL;
        global_taskcount = 0;
    }
}

kern_return_t collect_per_task_info(my_per_task_info_t *taskinfo, task_t target_task)
{
    kern_return_t ret = KERN_SUCCESS;
    unsigned int kotype = 0;
    vm_offset_t kobject = (vm_offset_t)0;
    
    taskinfo->task = target_task;
    pid_for_task(target_task, &taskinfo->pid);
    
    ret = mach_port_space_info(target_task, &taskinfo->info,
                               &taskinfo->table, &taskinfo->tableCount,
                               &taskinfo->tree, &taskinfo->treeCount);
    
    if (ret != KERN_SUCCESS) {
        fprintf(stderr, "mach_port_space_info() failed: pid:%d error: %s\n",taskinfo->pid, mach_error_string(ret));
        taskinfo->pid = 0;
        return ret;
    }
    
    proc_pid_to_name(taskinfo->pid, taskinfo->processName);
    ret = mach_port_kernel_object(mach_task_self(), taskinfo->task, &kotype, (unsigned *)&kobject);
    
    if (ret == KERN_SUCCESS && kotype == IKOT_TASK) {
        taskinfo->task_kobject = kobject;
        taskinfo->valid = TRUE;
    }
    
    return ret;
}

struct exc_port_info {
    mach_msg_type_number_t   count;
    mach_port_t      ports[EXC_TYPES_COUNT];
    exception_mask_t masks[EXC_TYPES_COUNT];
    exception_behavior_t behaviors[EXC_TYPES_COUNT];
    thread_state_flavor_t flavors[EXC_TYPES_COUNT];
};

void get_exc_behavior_string(exception_behavior_t b, char *out_string, size_t len)
{
    out_string[0]='\0';
    
    if (b & MACH_EXCEPTION_CODES)
        strncat(out_string, "MACH +", len);
    switch (b & ~MACH_EXCEPTION_CODES) {
        case EXCEPTION_DEFAULT:
            strncat(out_string, " DEFAULT", len);
            break;
        case EXCEPTION_STATE:
            strncat(out_string, " STATE", len);
            break;
        case EXCEPTION_STATE_IDENTITY:
            strncat(out_string, " IDENTITY", len);
            break;
        default:
            strncat(out_string, " UNKNOWN", len);
    }
}

void get_exc_mask_string(exception_mask_t m, char *out_string, size_t len)
{
    out_string[0]='\0';
    
    if (m & (1<<EXC_BAD_ACCESS))
        strncat(out_string, " BAD_ACCESS", len);
    if (m & (1<<EXC_BAD_INSTRUCTION))
        strncat(out_string," BAD_INSTRUCTION", len);
    if (m & (1<<EXC_ARITHMETIC))
        strncat(out_string," ARITHMETIC", len);
    if (m & (1<<EXC_EMULATION))
        strncat(out_string," EMULATION", len);
    if (m & (1<<EXC_SOFTWARE))
        strncat(out_string," SOFTWARE", len);
    if (m & (1<<EXC_BREAKPOINT))
        strncat(out_string," BREAKPOINT", len);
    if (m & (1<<EXC_SYSCALL))
        strncat(out_string," SYSCALL", len);
    if (m & (1<<EXC_MACH_SYSCALL))
        strncat(out_string," MACH_SYSCALL", len);
    if (m & (1<<EXC_RPC_ALERT))
        strncat(out_string," RPC_ALERT", len);
    if (m & (1<<EXC_CRASH))
        strncat(out_string," CRASH", len);
    if (m & (1<<EXC_RESOURCE))
        strncat(out_string," RESOURCE", len);
    if (m & (1<<EXC_GUARD))
        strncat(out_string," GUARD", len);
    
}

kern_return_t print_task_exception_info(my_per_task_info_t *taskinfo)
{
    kern_return_t kr = KERN_SUCCESS;
    exception_mask_t mask = EXC_MASK_ALL;
    struct exc_port_info excinfo;
    char behavior_string[30];
    char mask_string[200];
    
    kr = task_get_exception_ports(taskinfo->task, mask, excinfo.masks, &excinfo.count, excinfo.ports, excinfo.behaviors, excinfo.flavors);
    
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "Failed task_get_exception_ports task: %d pid: %d\n", taskinfo->task, taskinfo->pid);
        return kr;
    }
    boolean_t header_required = TRUE;
    for (int i = 0; i < excinfo.count; i++) {
        if (excinfo.ports[i] != MACH_PORT_NULL) {
            if (header_required) {
                printf("port        flavor <behaviors>           mask   \n");
                header_required = FALSE;
            }
            get_exc_behavior_string(excinfo.behaviors[i], behavior_string, sizeof(behavior_string));
            get_exc_mask_string(excinfo.masks[i], mask_string, 200);
            printf("0x%08x  0x%03x  <%s> %s  \n" , excinfo.ports[i], excinfo.flavors[i], behavior_string, mask_string);
            mach_port_deallocate(mach_task_self(), excinfo.ports[i]);
        }
        
    }
    
    return kr;
}


kern_return_t print_task_threads_special_ports(my_per_task_info_t *taskinfo)
{
    kern_return_t kret = KERN_SUCCESS;
    thread_act_array_t threadlist;
    mach_msg_type_number_t threadcount=0;
    kret = task_threads(taskinfo->task, &threadlist, &threadcount);
    boolean_t header_required = TRUE;
    boolean_t newline_required = TRUE;
    unsigned th_kobject = 0;
    unsigned th_kotype = 0;
    
    for (int i = 0; i < threadcount; i++) {
        if (header_required) {
            printf("Threads             Thread-ID     DispatchQ     Port Description.");
            header_required = FALSE;
        }
        
        if (newline_required) {
            printf("\n");
        }
        newline_required = TRUE;
        if (KERN_SUCCESS == mach_port_kernel_object(mach_task_self(), threadlist[i], &th_kotype, &th_kobject)) {
            /* TODO: Should print tid and stuff */
            thread_identifier_info_data_t th_info;
            mach_msg_type_number_t th_info_count = THREAD_IDENTIFIER_INFO_COUNT;
            printf("0x%08x ", th_kobject);
            if (KERN_SUCCESS == thread_info(threadlist[i], THREAD_IDENTIFIER_INFO, (thread_info_t)&th_info, &th_info_count)) {
                printf("%16llu  0x%016llx  ", th_info.thread_id, th_info.dispatch_qaddr);
            }
            else
                printf("                                     ");
            
        }

        ipc_voucher_t th_voucher = IPC_VOUCHER_NULL;
        if (KERN_SUCCESS == thread_get_mach_voucher(threadlist[i], 0, &th_voucher) && th_voucher != IPC_VOUCHER_NULL) {
            show_voucher_detail(mach_task_self(), th_voucher);
            mach_port_deallocate(mach_task_self(), th_voucher);
            newline_required = FALSE;
        }
        mach_port_deallocate(mach_task_self(), threadlist[i]);
    }
    return kret;
}

char * get_task_name_by_pid(pid_t pid) {
    char * retval = "Unknown";
    for (int i = 0; i < global_taskcount; i++) {
        if (pid == global_taskinfo[i].pid) {
            return global_taskinfo[i].processName;
        }
    }
    return retval;
}

my_per_task_info_t * get_taskinfo_by_kobject(natural_t kobj) {
    my_per_task_info_t *retval = &NOT_FOUND_TASK_INFO;
    for (int j = 0; j < global_taskcount; j++) {
        if (global_taskinfo[j].task_kobject == kobj) {
            retval = &global_taskinfo[j];
            break;
        }
    }
    return retval;
}

kern_return_t get_taskinfo_of_receiver_by_send_right(ipc_info_name_t *sendright, my_per_task_info_t **out_taskinfo, mach_port_name_t *out_recv_info)
{
    kern_return_t retval = KERN_FAILURE;
    boolean_t found = FALSE;
    *out_taskinfo = &NOT_FOUND_TASK_INFO;
 
    for (int j = 0; j < global_taskcount && !found; j++) {
        for (int k = 0; k < global_taskinfo[j].tableCount && !found; k++) {
            if ((global_taskinfo[j].table[k].iin_type & MACH_PORT_TYPE_RECEIVE) &&
                global_taskinfo[j].table[k].iin_object == sendright->iin_object ) {
                *out_taskinfo = &global_taskinfo[j];
                *out_recv_info = global_taskinfo[j].table[k].iin_name;
                found = TRUE;
                retval = KERN_SUCCESS;
            }
        }
    }
    return retval;
}


