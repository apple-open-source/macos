/*
 * Copyright (c) 2002-2016 Apple Inc. All rights reserved.
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
#include <assert.h>

#include "common.h"

#pragma mark kobject to name hash table implementation

#if (K2N_TABLE_SIZE & (K2N_TABLE_SIZE - 1) != 0)
#error K2N_TABLE_SIZE must be a power of two
#endif

#define K2N_TABLE_MASK	(K2N_TABLE_SIZE - 1)

static uint32_t k2n_hash(natural_t kobject) {
    return (uint64_t)kobject * 2654435761 >> 32;
}

static struct k2n_table_node *k2n_table_lookup_next_internal(struct k2n_table_node *node, natural_t kobject) {
    while (node) {
        if (kobject == node->kobject)
            return node;

        node = node->next;
    }

    return NULL;
}

struct k2n_table_node *k2n_table_lookup_next(struct k2n_table_node *node, natural_t kobject) {
    if (!node) {
        return NULL;
    }
    return k2n_table_lookup_next_internal(node->next, kobject);
}

struct k2n_table_node *k2n_table_lookup(struct k2n_table_node **table, natural_t kobject) {
    uint32_t hv;
    struct k2n_table_node *node;

    hv = k2n_hash(kobject);

    node = table[hv & K2N_TABLE_MASK];

    return k2n_table_lookup_next_internal(node, kobject);
}

static void k2n_table_enter(struct k2n_table_node **table, natural_t kobject, ipc_info_name_t *info_name) {
    uint32_t hv;
    struct k2n_table_node *node;

    hv = k2n_hash(kobject);

    node = malloc(sizeof (struct k2n_table_node));
    assert(node);

    node->kobject = kobject;
    node->info_name = info_name;
    assert(kobject == info_name->iin_object);

    node->next = table[hv & K2N_TABLE_MASK];
    table[hv & K2N_TABLE_MASK] = node;
}

#pragma mark -

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
    .k2ntable = {0},
    .processName = "Unknown",
    .exceptionInfo = {0},
    .threadInfos = NULL,
    .threadCount = 0,
    .threadExceptionInfos = NULL
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
    int i;
    kern_return_t ret = KERN_SUCCESS;
    unsigned int kotype = 0;
    vm_offset_t kobject = (vm_offset_t)0;

    taskinfo->task = target_task;
    pid_for_task(target_task, &taskinfo->pid);

    ret = task_get_exception_ports(taskinfo->task, EXC_MASK_ALL, taskinfo->exceptionInfo.masks, &taskinfo->exceptionInfo.count, taskinfo->exceptionInfo.ports, taskinfo->exceptionInfo.behaviors, taskinfo->exceptionInfo.flavors);

    if (ret != KERN_SUCCESS) {
        fprintf(stderr, "task_get_exception_ports() failed: pid:%d error: %s\n",taskinfo->pid, mach_error_string(ret));
        taskinfo->pid = 0;
    }

    /* collect threads port as well */
    taskinfo->threadCount = 0;
    thread_act_array_t threadPorts;
    ret = task_threads(taskinfo->task, &threadPorts, &taskinfo->threadCount);

    if (ret != KERN_SUCCESS) {
        fprintf(stderr, "task_threads() failed: pid:%d error: %s\n",taskinfo->pid, mach_error_string(ret));
        taskinfo->threadCount = 0;
    } else {
        /* collect the thread information */
        taskinfo->threadInfos = (struct my_per_thread_info *)malloc(sizeof(struct my_per_thread_info) * taskinfo->threadCount);
        bzero(taskinfo->threadInfos, sizeof(struct my_per_thread_info) * taskinfo->threadCount);

        /* now collect exception ports for each of those threads as well */
        taskinfo->threadExceptionInfos = (struct exc_port_info *) malloc(sizeof(struct exc_port_info) * taskinfo->threadCount);
        boolean_t found_exception = false;
        for (int i = 0; i < taskinfo->threadCount; i++) {
            unsigned th_kobject = 0;
            unsigned th_kotype = 0;
            ipc_voucher_t th_voucher = IPC_VOUCHER_NULL;
            thread_identifier_info_data_t th_info;
            mach_msg_type_number_t th_info_count = THREAD_IDENTIFIER_INFO_COUNT;
            struct exc_port_info *excinfo = &(taskinfo->threadExceptionInfos[i]);

            ret = thread_get_exception_ports(threadPorts[i], EXC_MASK_ALL, excinfo->masks, &excinfo->count, excinfo->ports, excinfo->behaviors, excinfo->flavors);
            if (ret != KERN_SUCCESS){
                fprintf(stderr, "thread_get_exception_ports() failed: pid: %d thread: %d error %s\n", taskinfo->pid, threadPorts[i], mach_error_string(ret));
            }

            if (excinfo->count != 0) {
                found_exception = true;
            }

            taskinfo->threadInfos[i].thread = threadPorts[i];

            if (KERN_SUCCESS == mach_port_kernel_object(mach_task_self(), threadPorts[i], &th_kotype, &th_kobject)) {
                taskinfo->threadInfos[i].th_kobject = th_kobject;
                if (KERN_SUCCESS == thread_info(threadPorts[i], THREAD_IDENTIFIER_INFO, (thread_info_t)&th_info, &th_info_count)) {
                    taskinfo->threadInfos[i].th_id = th_info.thread_id;
                }
            }

            if (KERN_SUCCESS == thread_get_mach_voucher(threadPorts[i], 0, &th_voucher) && th_voucher != IPC_VOUCHER_NULL) {
                char *detail = copy_voucher_detail(mach_task_self(), th_voucher, NULL);
                taskinfo->threadInfos[i].voucher_detail = strndup(detail, VOUCHER_DETAIL_MAXLEN);
                free(detail);

                mach_port_deallocate(mach_task_self(), th_voucher);
            }

            mach_port_deallocate(mach_task_self(), threadPorts[i]);
            threadPorts[i] = MACH_PORT_NULL;
        }

        if (found_exception == false) {
            free(taskinfo->threadExceptionInfos);
            taskinfo->threadExceptionInfos = NULL;
        }

    }

    vm_deallocate(mach_task_self(), threadPorts, taskinfo->threadCount * sizeof(thread_act_t));
    threadPorts = NULL;

    ret = mach_port_space_info(target_task, &taskinfo->info, &taskinfo->table, &taskinfo->tableCount, &taskinfo->tree, &taskinfo->treeCount);

    if (ret != KERN_SUCCESS) {
        fprintf(stderr, "mach_port_space_info() failed: pid:%d error: %s\n",taskinfo->pid, mach_error_string(ret));
        taskinfo->pid = 0;
        return ret;
    }

    bzero(taskinfo->k2ntable, K2N_TABLE_SIZE * sizeof (struct k2n_table_node *));
    for (i = 0; i < taskinfo->tableCount; i++) {
        k2n_table_enter(taskinfo->k2ntable, taskinfo->table[i].iin_object, &taskinfo->table[i]);
    }

    proc_pid_to_name(taskinfo->pid, taskinfo->processName);

    ret = mach_port_kernel_object(mach_task_self(), taskinfo->task, &kotype, (unsigned *)&kobject);

    if (ret == KERN_SUCCESS && kotype == IKOT_TASK) {
        taskinfo->task_kobject = kobject;
        taskinfo->valid = TRUE;
    }

    return ret;
}

void get_exc_behavior_string(exception_behavior_t b, char *out_string, size_t len)
{
    out_string[0]='\0';

    if (b & MACH_EXCEPTION_CODES)
        strlcat(out_string, "MACH +", len);
    switch (b & ~MACH_EXCEPTION_CODES) {
        case EXCEPTION_DEFAULT:
            strlcat(out_string, " DEFAULT", len);
            break;
        case EXCEPTION_STATE:
            strlcat(out_string, " STATE", len);
            break;
        case EXCEPTION_STATE_IDENTITY:
            strlcat(out_string, " IDENTITY", len);
            break;
        default:
            strlcat(out_string, " UNKNOWN", len);
    }
}

void get_exc_mask_string(exception_mask_t m, char *out_string, size_t len)
{
    out_string[0]='\0';

    if (m & (1<<EXC_BAD_ACCESS))
        strlcat(out_string, " BAD_ACCESS", len);
    if (m & (1<<EXC_BAD_INSTRUCTION))
        strlcat(out_string," BAD_INSTRUCTION", len);
    if (m & (1<<EXC_ARITHMETIC))
        strlcat(out_string," ARITHMETIC", len);
    if (m & (1<<EXC_EMULATION))
        strlcat(out_string," EMULATION", len);
    if (m & (1<<EXC_SOFTWARE))
        strlcat(out_string," SOFTWARE", len);
    if (m & (1<<EXC_BREAKPOINT))
        strlcat(out_string," BREAKPOINT", len);
    if (m & (1<<EXC_SYSCALL))
        strlcat(out_string," SYSCALL", len);
    if (m & (1<<EXC_MACH_SYSCALL))
        strlcat(out_string," MACH_SYSCALL", len);
    if (m & (1<<EXC_RPC_ALERT))
        strlcat(out_string," RPC_ALERT", len);
    if (m & (1<<EXC_CRASH))
        strlcat(out_string," CRASH", len);
    if (m & (1<<EXC_RESOURCE))
        strlcat(out_string," RESOURCE", len);
    if (m & (1<<EXC_GUARD))
        strlcat(out_string," GUARD", len);
}

kern_return_t print_task_exception_info(my_per_task_info_t *taskinfo, JSON_t json)
{

    char behavior_string[30];
    char mask_string[200];

    JSON_KEY(json, exception_ports);
    JSON_ARRAY_BEGIN(json);

    boolean_t header_required = TRUE;
    for (int i = 0; i < taskinfo->exceptionInfo.count; i++) {
        if (taskinfo->exceptionInfo.ports[i] != MACH_PORT_NULL) {
            if (header_required) {

                printf("    exc_port    flavor <behaviors>           mask   \n");
                header_required = FALSE;
            }
            get_exc_behavior_string(taskinfo->exceptionInfo.behaviors[i], behavior_string, sizeof(behavior_string));
            get_exc_mask_string(taskinfo->exceptionInfo.masks[i], mask_string, sizeof(mask_string));

            JSON_OBJECT_BEGIN(json);
            JSON_OBJECT_SET(json, port, "0x%08x", taskinfo->exceptionInfo.ports[i]);
            JSON_OBJECT_SET(json, flavor, "0x%03x", taskinfo->exceptionInfo.flavors[i]);
            JSON_OBJECT_SET(json, behavior, "%s", behavior_string);
            JSON_OBJECT_SET(json, mask, "%s", mask_string);
            JSON_OBJECT_END(json); // exception port

            printf("    0x%08x  0x%03x  <%s>           %s  \n" , taskinfo->exceptionInfo.ports[i], taskinfo->exceptionInfo.flavors[i], behavior_string, mask_string);
        }

    }

    JSON_ARRAY_END(json); // exception ports

    return KERN_SUCCESS;
}


kern_return_t print_task_threads_special_ports(my_per_task_info_t *taskinfo, JSON_t json)
{
    kern_return_t kret = KERN_SUCCESS;
    mach_msg_type_number_t threadcount = taskinfo->threadCount;
    boolean_t header_required = TRUE;
    boolean_t newline_required = TRUE;
    struct my_per_thread_info * info = NULL;

    JSON_KEY(json, threads);
    JSON_ARRAY_BEGIN(json);

    for (int i = 0; i < threadcount; i++) {
        JSON_OBJECT_BEGIN(json);

        info = &taskinfo->threadInfos[i];
        if (header_required) {
            printf("Thread_KObject  Thread-ID     Port Description.");
            header_required = FALSE;
        }

        if (newline_required) {
            printf("\n");
        }
        newline_required = TRUE;

        if (info->th_kobject != 0) {
            /* TODO: Should print tid and stuff */
            printf("0x%08x       ", info->th_kobject);
            printf("0x%llx  ", info->th_id);

            JSON_OBJECT_SET(json, kobject, "0x%08x", info->th_kobject);
            JSON_OBJECT_SET(json, tid, "0x%llx", info->th_id);
        }

        if (info->voucher_detail != NULL) {
            /* TODO: include voucher detail in JSON */
            printf("%s\n", info->voucher_detail);
        }

        JSON_KEY(json, exception_ports);
        JSON_ARRAY_BEGIN(json);

        /* print the thread exception ports also */
        if (taskinfo->threadExceptionInfos != NULL)
        {

            struct exc_port_info *excinfo = &taskinfo->threadExceptionInfos[i];
            char behavior_string[30];
            char mask_string[200];

            if (excinfo->count > 0) {
                boolean_t header_required = TRUE;
                for (int i = 0; i < excinfo->count; i++) {
                    JSON_OBJECT_BEGIN(json);

                    if (excinfo->ports[i] != MACH_PORT_NULL) {
                        if (header_required) {
                            printf("\n    exc_port    flavor <behaviors>           mask   -> name    owner\n");
                            header_required = FALSE;
                        }
                        get_exc_behavior_string(excinfo->behaviors[i], behavior_string, sizeof(behavior_string));
                        get_exc_mask_string(excinfo->masks[i], mask_string, sizeof(mask_string));

                        JSON_OBJECT_SET(json, port, "0x%08x", excinfo->ports[i]);
                        JSON_OBJECT_SET(json, flavor, "0x%03x", excinfo->flavors[i]);
                        JSON_OBJECT_SET(json, behavior, "%s", behavior_string);
                        JSON_OBJECT_SET(json, mask, "%s", mask_string);

                        printf("    0x%08x  0x%03x  <%s>           %s  " , excinfo->ports[i], excinfo->flavors[i], behavior_string, mask_string);

                        ipc_info_name_t actual_sendinfo;
                        if (KERN_SUCCESS == get_ipc_info_from_lsmp_spaceinfo(excinfo->ports[i], &actual_sendinfo)) {
                            my_per_task_info_t *recv_holder_taskinfo;
                            mach_port_name_t recv_name = MACH_PORT_NULL;
                            if (KERN_SUCCESS == get_taskinfo_of_receiver_by_send_right(&actual_sendinfo, &recv_holder_taskinfo, &recv_name)) {

                                JSON_OBJECT_SET(json, name, "0x%08x", recv_name);
                                JSON_OBJECT_SET(json, ipc-object, "0x%08x", actual_sendinfo.iin_object);
                                JSON_OBJECT_SET(json, pid, %d, recv_holder_taskinfo->pid);
                                JSON_OBJECT_SET(json, process, "%s", recv_holder_taskinfo->processName);

                                printf("   -> 0x%08x  0x%08x  (%d) %s\n",
                                       recv_name,
                                       actual_sendinfo.iin_object,
                                       recv_holder_taskinfo->pid,
                                       recv_holder_taskinfo->processName);
                            }

                        } else {
                            fprintf(stderr, "failed to find");
                        }

                        printf("\n");

                    }
                    JSON_OBJECT_END(json); // exception port
                }
            }
        }
        JSON_ARRAY_END(json); // exception ports
        JSON_OBJECT_END(json); // thread
    }

    JSON_ARRAY_END(json); // threads
    printf("\n");
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
    *out_taskinfo = &NOT_FOUND_TASK_INFO;
    struct k2n_table_node *k2nnode;

    for (int j = 0; j < global_taskcount; j++) {
        if ((k2nnode = k2n_table_lookup(global_taskinfo[j].k2ntable, sendright->iin_object))) {
            assert(k2nnode->info_name->iin_object == sendright->iin_object);

            if (k2nnode->info_name->iin_type & MACH_PORT_TYPE_RECEIVE) {
                *out_taskinfo = &global_taskinfo[j];
                *out_recv_info = k2nnode->info_name->iin_name;
                return KERN_SUCCESS;
            }
        }
    }

    return KERN_FAILURE;
}

kern_return_t get_ipc_info_from_lsmp_spaceinfo(mach_port_t port_name, ipc_info_name_t *out_sendright){
    kern_return_t retval = KERN_FAILURE;
    bzero(out_sendright, sizeof(ipc_info_name_t));
    my_per_task_info_t *mytaskinfo = NULL;
    for (int i = global_taskcount - 1; i >= 0; i--){
        if (global_taskinfo[i].task == mach_task_self()){
            mytaskinfo = &global_taskinfo[i];
            break;
        }
    }
    if (mytaskinfo) {
        for (int k = 0; k < mytaskinfo->tableCount; k++) {
            if (port_name == mytaskinfo->table[k].iin_name){
                bcopy(&mytaskinfo->table[k], out_sendright, sizeof(ipc_info_name_t));
                retval = KERN_SUCCESS;
                break;
            }
        }
    }
    return retval;

}
