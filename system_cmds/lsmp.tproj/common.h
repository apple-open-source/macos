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


#ifndef system_cmds_common_h
#define system_cmds_common_h

#include <mach/mach.h>
#include "json.h"

#define PROC_NAME_LEN 100
#define BUFSTR_LEN 30
#define VOUCHER_DETAIL_MAXLEN 1024

/* common struct to hold all configurations, static and args based */
struct prog_configs {
    boolean_t show_all_tasks;
    boolean_t show_voucher_details;
    boolean_t verbose;
    int       voucher_detail_length;
    pid_t     pid; /* if user focusing only one pid */
    JSON_t    json_output;
};

extern struct prog_configs lsmp_config;

/* exception port information */
struct exc_port_info {
    mach_msg_type_number_t   count;
    mach_port_t      ports[EXC_TYPES_COUNT];
    exception_mask_t masks[EXC_TYPES_COUNT];
    exception_behavior_t behaviors[EXC_TYPES_COUNT];
    thread_state_flavor_t flavors[EXC_TYPES_COUNT];
};

/* private structure to hold thread specific information */
struct my_per_thread_info {
    mach_port_t thread;
    uint32_t th_kobject;
    uint64_t th_id;
    char * voucher_detail;
};

/* kobject to name hash table declarations */
#define K2N_TABLE_SIZE	256

struct k2n_table_node {
    natural_t kobject; /* kobject referred to by the name -- the key into the table */
    ipc_info_name_t *info_name; /* info about the name that refers to the key kobject -- value of the table */
    struct k2n_table_node *next;
};

struct k2n_table_node *k2n_table_lookup_next(struct k2n_table_node *node, natural_t kobject);
struct k2n_table_node *k2n_table_lookup(struct k2n_table_node **table, natural_t kobject);

/* private structure to wrap up per-task info */
typedef struct my_per_task_info {
    task_t task;
    pid_t pid;
    vm_address_t task_kobject;
    ipc_info_space_t info;
    ipc_info_name_array_t table;
    mach_msg_type_number_t tableCount;
    ipc_info_tree_name_array_t tree;
    mach_msg_type_number_t treeCount;
    boolean_t valid; /* TRUE if all data is accurately collected */
    struct k2n_table_node *k2ntable[K2N_TABLE_SIZE];
    char processName[PROC_NAME_LEN];
    struct exc_port_info exceptionInfo;
    struct my_per_thread_info * threadInfos; /* dynamically allocated in collect_per_task_info */
    unsigned int threadCount;
    struct exc_port_info *threadExceptionInfos;  /* this is 2 dimensional array with threadCount X struct exc_port_info  of ports */
} my_per_task_info_t;


/*
 * WARNING - these types are copied from xnu/osfmk/kern/ipc_kobject.h
 * Need to stay in sync to print accurate results.
 */
#define IKOT_NONE                  0
#define IKOT_THREAD                1
#define IKOT_TASK                  2
#define IKOT_HOST                  3
#define IKOT_HOST_PRIV             4
#define IKOT_PROCESSOR             5
#define IKOT_PSET                  6
#define IKOT_PSET_NAME             7
#define IKOT_TIMER                 8
#define IKOT_PAGING_REQUEST        9
#define IKOT_MIG                  10
#define IKOT_MEMORY_OBJECT        11
#define IKOT_XMM_PAGER            12
#define IKOT_XMM_KERNEL           13
#define IKOT_XMM_REPLY            14
#define IKOT_UND_REPLY            15
#define IKOT_HOST_NOTIFY          16
#define IKOT_HOST_SECURITY        17
#define IKOT_LEDGER               18
#define IKOT_MASTER_DEVICE        19
#define IKOT_TASK_NAME            20
#define IKOT_SUBSYSTEM            21
#define IKOT_IO_DONE_QUEUE        22
#define IKOT_SEMAPHORE            23
#define IKOT_LOCK_SET             24
#define IKOT_CLOCK                25
#define IKOT_CLOCK_CTRL           26
#define IKOT_IOKIT_IDENT          27
#define IKOT_NAMED_ENTRY          28
#define IKOT_IOKIT_CONNECT        29
#define IKOT_IOKIT_OBJECT         30
#define IKOT_UPL                  31
#define IKOT_MEM_OBJ_CONTROL      32
#define IKOT_AU_SESSIONPORT       33
#define IKOT_FILEPORT             34
#define IKOT_LABELH               35
#define IKOT_TASK_RESUME          36
#define IKOT_VOUCHER              37
#define IKOT_VOUCHER_ATTR_CONTROL 38
#define IKOT_WORK_INTERVAL              39
#define IKOT_UX_HANDLER                 40
#define IKOT_UEXT_OBJECT                41
#define IKOT_ARCADE_REG                 42

#define IKOT_UNKNOWN              43	/* magic catchall	*/
#define IKOT_MAX_TYPE             (IKOT_UNKNOWN+1)	/* # of IKOT_ types	*/


#define PORT_FLAG_TO_INDEX(flag) ( __builtin_ctz(flag) ) /* count trailing zeros */
#define INDEX_TO_PORT_FLAG(idx) ( 1 << idx )
typedef struct port_status_flag_info {
    natural_t flag; /* MACH_PORT_STATUS_FLAG_* */
    const char *compact_name; /* Single character name for compact representation */
    const char *name; /* human readable long name */
} port_status_flag_info_t;

/*
 * list of names for possible MACH_PORT_STATUS_FLAG_*
 * indexed by PORT_FLAG_TO_INDEX(MACH_PORT_STATUS_FLAG_*)
 */
extern const port_status_flag_info_t port_status_flags[];

#define _SHOW_PORT_STATUS_FLAG(flags, flag) \
  (flags & flag) ? port_status_flags[PORT_FLAG_TO_INDEX(flag)].compact_name : "-"
#define SHOW_PORT_STATUS_FLAGS(flags) \
  _SHOW_PORT_STATUS_FLAG(flags, MACH_PORT_STATUS_FLAG_TEMPOWNER), \
  _SHOW_PORT_STATUS_FLAG(flags, MACH_PORT_STATUS_FLAG_GUARDED), \
  _SHOW_PORT_STATUS_FLAG(flags, MACH_PORT_STATUS_FLAG_STRICT_GUARD), \
  _SHOW_PORT_STATUS_FLAG(flags, MACH_PORT_STATUS_FLAG_IMP_DONATION), \
  _SHOW_PORT_STATUS_FLAG(flags, MACH_PORT_STATUS_FLAG_REVIVE), \
  _SHOW_PORT_STATUS_FLAG(flags, MACH_PORT_STATUS_FLAG_TASKPTR)


uint32_t show_recipe_detail(mach_voucher_attr_recipe_t recipe, char * voucher_outstr, uint32_t maxlen, JSON_t json);
char *copy_voucher_detail(mach_port_t task, mach_port_name_t voucher, JSON_t json);

/* mach port related functions */
const char * kobject_name(natural_t kotype);
void get_receive_port_context(task_t taskp, mach_port_name_t portname, mach_port_context_t *context);
int get_recieve_port_status(task_t taskp, mach_port_name_t portname, mach_port_info_ext_t *info);
void show_task_mach_ports(my_per_task_info_t *taskinfo, uint32_t taskCount, my_per_task_info_t *allTaskInfos, JSON_t json);

/* task and thread related helper functions */
kern_return_t collect_per_task_info(my_per_task_info_t *taskinfo, task_t target_task);
my_per_task_info_t * allocate_taskinfo_memory(uint32_t taskCount);
void deallocate_taskinfo_memory(my_per_task_info_t *data);
kern_return_t print_task_exception_info(my_per_task_info_t *taskinfo, JSON_t json);
kern_return_t print_task_threads_special_ports(my_per_task_info_t *taskinfo, JSON_t json);
my_per_task_info_t * get_taskinfo_by_kobject(natural_t kobj);

void get_exc_behavior_string(exception_behavior_t b, char *out_string, size_t len);
void get_exc_mask_string(exception_mask_t m, char *out_string, size_t len);
kern_return_t get_taskinfo_of_receiver_by_send_right(ipc_info_name_t *sendright, my_per_task_info_t **out_taskinfo, mach_port_name_t *out_recv_info);
kern_return_t get_ipc_info_from_lsmp_spaceinfo(mach_port_t port_name, ipc_info_name_t *out_sendright);

/* basic util functions */
uint32_t print_hex_data(char *outstr, uint32_t maxlen, char *prefix, char *desc, void *addr, int len);

#endif
