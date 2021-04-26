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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <libproc.h>
#include <assert.h>
#include <mach/mach.h>
//#include <mach/mach_port.h.h>
#include <mach/mach_voucher.h>
#include "common.h"
#include "json.h"

const char * kobject_name(natural_t kotype)
{
	switch (kotype) {
        case IKOT_NONE:             return "message-queue";
        case IKOT_THREAD_CONTROL:   return "THREAD-CONTROL";
        case IKOT_TASK_CONTROL:     return "TASK-CONTROL";
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
        case IKOT_IOKIT_IDENT:      return "IOKIT-IDENT";
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
        case IKOT_WORK_INTERVAL:    return "WORK_INTERVAL";
        case IKOT_UX_HANDLER:       return "UX_HANDLER";
        case IKOT_UEXT_OBJECT:      return "UEXT_OBJECT";
        case IKOT_ARCADE_REG:       return "ARCADE_REG";
        case IKOT_EVENTLINK:        return "EVENTLINK";
        case IKOT_TASK_INSPECT:     return "TASK-INSPECT";
        case IKOT_TASK_READ:        return "TASK-READ";
        case IKOT_THREAD_INSPECT:   return "THREAD-INSPECT";
        case IKOT_THREAD_READ:      return "THREAD-READ";
        case IKOT_SUID_CRED:        return "SUID_CRED";
        case IKOT_HYPERVISOR:       return "HYPERVISOR";
        case IKOT_UNKNOWN:
        default:                    return "UNKNOWN";
	}
}

const port_status_flag_info_t port_status_flags[] = {
	[PORT_FLAG_TO_INDEX(MACH_PORT_STATUS_FLAG_TEMPOWNER)] = {
		.flag = MACH_PORT_STATUS_FLAG_TEMPOWNER,
		.compact_name = "T",
		.name = "tempowner",
	},
	[PORT_FLAG_TO_INDEX(MACH_PORT_STATUS_FLAG_GUARDED)] = {
		.flag = MACH_PORT_STATUS_FLAG_GUARDED,
		.compact_name = "G",
		.name = "guarded",
	},
	[PORT_FLAG_TO_INDEX(MACH_PORT_STATUS_FLAG_STRICT_GUARD)] = {
		.flag = MACH_PORT_STATUS_FLAG_STRICT_GUARD,
		.compact_name = "S",
		.name = "strict guard",
	},
	[PORT_FLAG_TO_INDEX(MACH_PORT_STATUS_FLAG_IMP_DONATION)] = {
		.flag = MACH_PORT_STATUS_FLAG_IMP_DONATION,
		.compact_name = "I",
		.name = "imp donation",
	},
	[PORT_FLAG_TO_INDEX(MACH_PORT_STATUS_FLAG_REVIVE)] = {
		.flag = MACH_PORT_STATUS_FLAG_REVIVE,
		.compact_name = "R",
		.name = "revive",
	},
	[PORT_FLAG_TO_INDEX(MACH_PORT_STATUS_FLAG_TASKPTR)] = {
		.flag = MACH_PORT_STATUS_FLAG_TASKPTR,
		.compact_name = "P",
		.name = "taskptr",
	},
	{0},
};

#define VOUCHER_DETAIL_PREFIX "            "

static const unsigned int voucher_contents_size = 8192;
static uint8_t voucher_contents[voucher_contents_size];

typedef struct {
    int total;
    int sendcount;
    int receivecount;
    int sendoncecount;
    int portsetcount;
    int deadcount;
    int dncount;
    int vouchercount;
} task_table_entry_counts;

typedef task_table_entry_counts * task_table_entry_counts_t;

static void show_task_table_entry(ipc_info_name_t *entry, my_per_task_info_t *taskinfo, uint32_t taskCount, my_per_task_info_t *allTaskInfos, task_table_entry_counts_t counts, JSON_t json);

static uint32_t safesize (int len){
    return (len > 0) ? len : 0;
}

uint32_t show_recipe_detail(mach_voucher_attr_recipe_t recipe, char *voucher_outstr, uint32_t maxlen, JSON_t json) {
    JSON_OBJECT_BEGIN(json);

    uint32_t len = 0;
    len += snprintf(&voucher_outstr[len], safesize(maxlen - len), VOUCHER_DETAIL_PREFIX "Key: %u, ", recipe->key);
    len += snprintf(&voucher_outstr[len], safesize(maxlen - len), "Command: %u, ", recipe->command);
    len += snprintf(&voucher_outstr[len], safesize(maxlen - len), "Previous voucher: 0x%x, ", recipe->previous_voucher);
    len += snprintf(&voucher_outstr[len], safesize(maxlen - len), "Content size: %u\n", recipe->content_size);

    JSON_OBJECT_SET(json, key, %u, recipe->key);
    JSON_OBJECT_SET(json, command, %u, recipe->command);
    JSON_OBJECT_SET(json, previous_voucher, "0x%x", recipe->previous_voucher);
    JSON_OBJECT_SET(json, content_size, %u, recipe->content_size);

	switch (recipe->key) {
	case MACH_VOUCHER_ATTR_KEY_IMPORTANCE:
		// content may not be valid JSON, exclude
		// JSON_OBJECT_SET(json, importance_info, "%s", (char *)recipe->content);
		len += snprintf(&voucher_outstr[len], safesize(maxlen - len), VOUCHER_DETAIL_PREFIX "IMPORTANCE INFO: %s", (char *)recipe->content);
		break;
	case MACH_VOUCHER_ATTR_KEY_BANK:
		// content may not be valid JSON, exclude
		// JSON_OBJECT_SET(json, resource_accounting_info, "%s", (char *)recipe->content);
		len += snprintf(&voucher_outstr[len], safesize(maxlen - len), VOUCHER_DETAIL_PREFIX "RESOURCE ACCOUNTING INFO: %s", (char *)recipe->content);
		break;
	default:
		len += print_hex_data(&voucher_outstr[len], safesize(maxlen - len), VOUCHER_DETAIL_PREFIX, "Recipe Contents", (void *)recipe->content, MIN(recipe->content_size, lsmp_config.voucher_detail_length));
		break;
	}

    if (len + 1 < maxlen && voucher_outstr[len - 1] != '\n') {
        voucher_outstr[len++] = '\n';
        voucher_outstr[len] = '\0';
    }
    JSON_OBJECT_END(json); // recipe
	return len;
}


char * copy_voucher_detail(mach_port_t task, mach_port_name_t voucher, JSON_t json) {
    unsigned int recipe_size = voucher_contents_size;
    kern_return_t kr = KERN_SUCCESS;
    bzero((void *)&voucher_contents[0], sizeof(voucher_contents));
    unsigned v_kobject = 0;
    unsigned v_kotype = 0;
    uint32_t detail_maxlen = VOUCHER_DETAIL_MAXLEN;
    char * voucher_outstr = (char *)malloc(detail_maxlen);
    voucher_outstr[0] = '\0';
    uint32_t plen = 0;

    kr = mach_port_kernel_object( task,
                                 voucher,
                                 &v_kotype, (unsigned *)&v_kobject);
    if (kr == KERN_SUCCESS && v_kotype == IKOT_VOUCHER ) {

        kr = mach_voucher_debug_info(task, voucher,
                                     (mach_voucher_attr_raw_recipe_array_t)&voucher_contents[0],
                                     &recipe_size);
        if (kr != KERN_SUCCESS && kr != KERN_NOT_SUPPORTED) {
            plen += snprintf(&voucher_outstr[plen], safesize(detail_maxlen - plen), VOUCHER_DETAIL_PREFIX "Voucher: 0x%x Failed to get contents %s\n", v_kobject, mach_error_string(kr));
            return voucher_outstr;
        }

        if (recipe_size == 0) {
            plen += snprintf(&voucher_outstr[plen], safesize(detail_maxlen - plen), VOUCHER_DETAIL_PREFIX "Voucher: 0x%x has no contents\n", v_kobject);
            return voucher_outstr;
        }

        plen += snprintf(&voucher_outstr[plen], safesize(detail_maxlen - plen), VOUCHER_DETAIL_PREFIX "Voucher: 0x%x\n", v_kobject);
        unsigned int used_size = 0;
        mach_voucher_attr_recipe_t recipe = NULL;
        while (recipe_size > used_size) {
            recipe = (mach_voucher_attr_recipe_t)&voucher_contents[used_size];
            if (recipe->key) {
                plen += show_recipe_detail(recipe, &voucher_outstr[plen], safesize(detail_maxlen - plen), json);
            }
            used_size += sizeof(mach_voucher_attr_recipe_data_t) + recipe->content_size;
        }
    } else {
        plen += snprintf(&voucher_outstr[plen], safesize(detail_maxlen - plen), VOUCHER_DETAIL_PREFIX "Invalid voucher: 0x%x\n", voucher);
    }

    return voucher_outstr;
}

void get_receive_port_context(task_read_t taskp, mach_port_name_t portname, mach_port_context_t *context) {
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

int get_recieve_port_status(task_read_t taskp, mach_port_name_t portname, mach_port_info_ext_t *info){
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

void show_task_mach_ports(my_per_task_info_t *taskinfo, uint32_t taskCount, my_per_task_info_t *allTaskInfos, JSON_t json)
{
    int i;
    task_table_entry_counts counts = {0};

    counts.total = taskinfo->tableCount + taskinfo->treeCount;

    JSON_KEY(json, ports)
    JSON_ARRAY_BEGIN(json);

    printf("  name      ipc-object    rights     flags   boost  reqs  recv  send sonce oref  qlimit  msgcount  context            identifier  type\n");
    printf("---------   ----------  ----------  -------- -----  ---- ----- ----- ----- ----  ------  --------  ------------------ ----------- ------------\n");
    for (i = 0; i < taskinfo->tableCount; i++) {
        show_task_table_entry(&taskinfo->table[i], taskinfo, taskCount, allTaskInfos, &counts, json);
    }

    JSON_ARRAY_END(json); // ports
    JSON_OBJECT_SET(json, total, %d, counts.total);
    JSON_OBJECT_SET(json, send_rights, %d, counts.sendcount);
    JSON_OBJECT_SET(json, receive_rights, %d, counts.receivecount);
    JSON_OBJECT_SET(json, send_once_rights, %d, counts.sendoncecount);
    JSON_OBJECT_SET(json, port_sets, %d, counts.portsetcount);
    JSON_OBJECT_SET(json, dead_names, %d, counts.deadcount);
    JSON_OBJECT_SET(json, dead_name requests, %d, counts.dncount);
    JSON_OBJECT_SET(json, vouchers, %d, counts.vouchercount);

	printf("\n");
    printf("total     = %d\n", counts.total);
    printf("SEND      = %d\n", counts.sendcount);
    printf("RECEIVE   = %d\n", counts.receivecount);
    printf("SEND_ONCE = %d\n", counts.sendoncecount);
    printf("PORT_SET  = %d\n", counts.portsetcount);
    printf("DEAD_NAME = %d\n", counts.deadcount);
    printf("DNREQUEST = %d\n", counts.dncount);
    printf("VOUCHERS  = %d\n", counts.vouchercount);
}

static void show_task_table_entry(ipc_info_name_t *entry, my_per_task_info_t *taskinfo, uint32_t taskCount, my_per_task_info_t *allTaskInfos, task_table_entry_counts_t counts, JSON_t json) {
    int j, k, port_status_flag_idx;
    kern_return_t ret;
    boolean_t send = FALSE;
    boolean_t dnreq = FALSE;
    int sendrights = 0;
    unsigned int kotype = 0;
    vm_offset_t kobject = (vm_offset_t)0;
    kobject_description_t desc;
    mach_vm_address_t kaddr;

    /* skip empty slots in the table */
    if ((entry->iin_type & MACH_PORT_TYPE_ALL_RIGHTS) == 0) {
        counts->total--;
        return;
    }

    if (entry->iin_type == MACH_PORT_TYPE_PORT_SET) {
        mach_port_name_array_t members;
        mach_msg_type_number_t membersCnt;

        ret = mach_port_get_set_status(taskinfo->task,
                                       entry->iin_name,
                                       &members, &membersCnt);
        if (ret != KERN_SUCCESS) {
            fprintf(stderr, "mach_port_get_set_status(0x%08x) failed: %s\n",
                    entry->iin_name,
                    mach_error_string(ret));
            return;
        }

        JSON_OBJECT_BEGIN(json);
        JSON_OBJECT_SET(json, type, "port set");
        JSON_OBJECT_SET(json, name, "0x%08x", entry->iin_name);
        JSON_OBJECT_SET(json, ipc-object, "0x%08x", entry->iin_object);

        JSON_KEY(json, members);
        JSON_ARRAY_BEGIN(json);

        printf("0x%08x  0x%08x  port-set    --------        ---      1                                                        %d  members\n",
               entry->iin_name,
               entry->iin_object,
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

                    JSON_OBJECT_BEGIN(json);
                    JSON_OBJECT_SET(json, ipc-object, "0x%08x", taskinfo->table[k].iin_object);

                    JSON_KEY(json, rights);
                    JSON_ARRAY_BEGIN(json);
                    JSON_ARRAY_APPEND(json, "recv");
                    if (taskinfo->table[k].iin_type & MACH_PORT_TYPE_SEND) {
                        JSON_ARRAY_APPEND(json, "send");
                    }
                    JSON_ARRAY_END(json); // rights

                    JSON_KEY(json, port_status_flags);
                    JSON_ARRAY_BEGIN(json);
                    port_status_flag_idx = 0;
                    while (0 != port_status_flags[port_status_flag_idx++].flag) {
                        if (port_status.mps_flags & INDEX_TO_PORT_FLAG(port_status_flag_idx)) {
                            JSON_ARRAY_APPEND(json, "%s", port_status_flags[port_status_flag_idx].name);
                        }
                    }
                    JSON_ARRAY_END(json); // port status flags
                    JSON_OBJECT_SET(json, boosts, %d, info.mpie_boost_cnt);

                    JSON_KEY(json, notifications);
                    JSON_ARRAY_BEGIN(json);
                    if (taskinfo->table[k].iin_type & MACH_PORT_TYPE_DNREQUEST) {
                        JSON_ARRAY_APPEND(json, "dead name");
                    }
                    if (port_status.mps_nsrequest) {
                        JSON_ARRAY_APPEND(json, "no sender");
                    }
                    if (port_status.mps_nsrequest) {
                        JSON_ARRAY_APPEND(json, "port destroy request");
                    }
                    JSON_ARRAY_END(json); // notifications

                    JSON_OBJECT_SET(json, recv_rights, %d, 1);
                    JSON_OBJECT_SET(json, send_rights, %d, taskinfo->table[k].iin_urefs);
                    JSON_OBJECT_SET(json, send_once_rights, %d, port_status.mps_sorights);
                    JSON_OBJECT_SET_BOOL(json, oref, port_status.mps_srights);
                    JSON_OBJECT_SET(json, queue_limit, %d, port_status.mps_qlimit);
                    JSON_OBJECT_SET(json, msg_count, %d, port_status.mps_msgcount);
                    JSON_OBJECT_SET(json, context, "0x%016llx", (uint64_t)port_context);
                    JSON_OBJECT_SET(json, identifier, "0x%08x", taskinfo->table[k].iin_name);
                    JSON_OBJECT_SET(json, pid, %d, taskinfo->pid);
                    JSON_OBJECT_SET(json, process, "%s", taskinfo->processName);
                    JSON_OBJECT_END(json); // member

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
                           taskinfo->pid,
                           taskinfo->processName);
                    break;
                }
            }
        }

        JSON_ARRAY_END(json); // members
        JSON_OBJECT_END(json); // port-set

        ret = vm_deallocate(mach_task_self(), (vm_address_t)members,
                            membersCnt * sizeof(mach_port_name_t));
        if (ret != KERN_SUCCESS) {
            fprintf(stderr, "vm_deallocate() failed: %s\n",
                    mach_error_string(ret));
            exit(1);
        }
        counts->portsetcount++;
        return;
    }

    if (entry->iin_type & MACH_PORT_TYPE_SEND) {
        send = TRUE;
        sendrights = entry->iin_urefs;
        counts->sendcount++;
    }

    if (entry->iin_type & MACH_PORT_TYPE_DNREQUEST) {
        dnreq = TRUE;
        counts->dncount++;
    }

    if (entry->iin_type & MACH_PORT_TYPE_RECEIVE) {
        mach_port_status_t status;
        mach_port_info_ext_t info;
        mach_port_context_t context = (mach_port_context_t)0;
        struct k2n_table_node *k2nnode;
        ret = get_recieve_port_status(taskinfo->task, entry->iin_name, &info);
        get_receive_port_context(taskinfo->task, entry->iin_name, &context);
        /* its ok to fail in fetching attributes */
        if (ret < 0) {
            return;
        }
        status = info.mpie_status;

        JSON_OBJECT_BEGIN(json);
        JSON_OBJECT_SET(json, type, "port");
        JSON_OBJECT_SET(json, name, "0x%08x", entry->iin_name);
        JSON_OBJECT_SET(json, ipc-object, "0x%08x", entry->iin_object);

        JSON_KEY(json, rights);
        JSON_ARRAY_BEGIN(json);
        JSON_ARRAY_APPEND(json, "recv");
        if (send) JSON_ARRAY_APPEND(json, "send");
        JSON_ARRAY_END(json); // rights

        JSON_KEY(json, port_status_flags);
        JSON_ARRAY_BEGIN(json);
        port_status_flag_idx = 0;
        while (0 != port_status_flags[port_status_flag_idx++].flag) {
            if (status.mps_flags & INDEX_TO_PORT_FLAG(port_status_flag_idx)) {
                JSON_ARRAY_APPEND(json, "%s", port_status_flags[port_status_flag_idx].name);
            }
        }
        JSON_ARRAY_END(json); // port status flags
        JSON_OBJECT_SET(json, boosts, %d, info.mpie_boost_cnt);

        JSON_KEY(json, notifications);
        JSON_ARRAY_BEGIN(json);
        if (dnreq) {
            JSON_ARRAY_APPEND(json, "dead name");
        }
        if (status.mps_nsrequest) {
            JSON_ARRAY_APPEND(json, "no sender");
        }
        if (status.mps_nsrequest) {
            JSON_ARRAY_APPEND(json, "port destroy request");
        }
        JSON_ARRAY_END(json); // notifications

        JSON_OBJECT_SET(json, recv_rights, %d, 1);
        JSON_OBJECT_SET(json, send_rights, %d, sendrights);
        JSON_OBJECT_SET(json, send_once_rights, %d, status.mps_sorights);
        JSON_OBJECT_SET_BOOL(json, oref, status.mps_srights);
        JSON_OBJECT_SET(json, queue_limit, %d, status.mps_qlimit);
        JSON_OBJECT_SET(json, msg_count, %d, status.mps_msgcount);
        JSON_OBJECT_SET(json, context, "0x%016llx", (uint64_t)context);
        JSON_OBJECT_END(json); // port

        printf("0x%08x  0x%08x  %s  --%s%s%s%s%s%s %5d  %s%s%s  %5d %5.0d %5.0d   %s   %6d  %8d  0x%016llx \n",
               entry->iin_name,
               entry->iin_object,
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
        counts->receivecount++;

        /* show other rights (in this and other tasks) for the port */
        for (j = 0; j < taskCount; j++) {
            if (allTaskInfos[j].valid == FALSE)
                continue;

            k2nnode = k2n_table_lookup(allTaskInfos[j].k2ntable, entry->iin_object);

            while (k2nnode) {
                if (k2nnode->info_name != entry) {
                    assert(k2nnode->info_name->iin_object == entry->iin_object);

                    printf("                  +     %s  --------        %s%s%s        %5d         <-                                       0x%08x  (%d) %s\n",
                           (k2nnode->info_name->iin_type & MACH_PORT_TYPE_SEND_ONCE) ?
                           "send-once " : "send      ",
                           (k2nnode->info_name->iin_type & MACH_PORT_TYPE_DNREQUEST) ? "D" : "-",
                           "-",
                           "-",
                           k2nnode->info_name->iin_urefs,
                           k2nnode->info_name->iin_name,
                           allTaskInfos[j].pid,
                           allTaskInfos[j].processName);
                }

                k2nnode = k2n_table_lookup_next(k2nnode, entry->iin_object);
            }
        }
        return;
    }
    else if (entry->iin_type & MACH_PORT_TYPE_DEAD_NAME)
    {
        JSON_OBJECT_BEGIN(json);
        JSON_OBJECT_SET(json, type, "dead name");
        JSON_OBJECT_SET(json, name, "0x%08x", entry->iin_name);
        JSON_OBJECT_SET(json, ipc-object, "0x%08x", entry->iin_object);
        JSON_OBJECT_SET(json, send_rights, %d, entry->iin_urefs);
        JSON_OBJECT_END(json); // dead name

        printf("0x%08x  0x%08x  dead-name   --------        ---        %5d      \n",
               entry->iin_name,
               entry->iin_object,
               entry->iin_urefs);
        counts->deadcount++;
        return;
    }

    if (entry->iin_type & MACH_PORT_TYPE_SEND_ONCE) {
        counts->sendoncecount++;
    }

    JSON_OBJECT_BEGIN(json);
    JSON_OBJECT_SET(json, name, "0x%08x", entry->iin_name);
    JSON_OBJECT_SET(json, ipc-object, "0x%08x", entry->iin_object);

    JSON_KEY(json, rights);
    JSON_ARRAY_BEGIN(json);
    JSON_ARRAY_APPEND(json, "%s", (send) ? "send":"send once");
    JSON_ARRAY_END(json); //rights

    JSON_KEY(json, notifications);
    JSON_ARRAY_BEGIN(json);
    if (dnreq) JSON_ARRAY_APPEND(json, "dead name");
    JSON_ARRAY_END(json); // notifications

    JSON_OBJECT_SET(json, send_rights, %d, (send) ? sendrights : 0);

    printf("0x%08x  0x%08x  %s  --------        %s%s%s        %5.0d     ",
           entry->iin_name,
           entry->iin_object,
           (send) ? "send      ":"send-once ",
           (dnreq) ? "D":"-",
           "-",
           "-",
           (send) ? sendrights : 0);

    /* converting to kobjects is not always supported */

    desc[0] = '\0';
    ret = mach_port_kobject_description(taskinfo->task,
                                  entry->iin_name,
                                  &kotype, &kaddr,
                                  desc);
    if (KERN_SUCCESS == ret) {
	    kobject = (unsigned) kaddr;
    } else {
	    ret = mach_port_kernel_object(taskinfo->task,
                                  entry->iin_name,
                                  &kotype, (unsigned *)&kobject);
    }

    if (ret == KERN_SUCCESS && kotype != 0) {
        JSON_OBJECT_SET(json, identifier, "0x%08x", (natural_t)kobject);
        JSON_OBJECT_SET(json, type, "%s", kobject_name(kotype));
        
        if (desc[0]) {
            JSON_OBJECT_SET(json, description, "%s", desc);
            printf("                                             0x%08x  %s %s", (natural_t)kobject, kobject_name(kotype), desc);
        } else {
            printf("                                             0x%08x  %s", (natural_t)kobject, kobject_name(kotype));
        }
        
        if ((kotype == IKOT_TASK_RESUME) ||
            (kotype == IKOT_TASK_CONTROL) ||
            (kotype == IKOT_TASK_READ) ||
            (kotype == IKOT_TASK_INSPECT) ||
            (kotype == IKOT_TASK_NAME)) {
            if (taskinfo->task_kobject == kobject) {
                /* neat little optimization since in most cases tasks have themselves in their ipc space */
                JSON_OBJECT_SET(json, pid, %d, taskinfo->pid);
                JSON_OBJECT_SET(json, process, "%s", taskinfo->processName);
                printf(" SELF (%d) %s", taskinfo->pid, taskinfo->processName);
            } else {
                my_per_task_info_t * _found_task = get_taskinfo_by_kobject((natural_t)kobject);
                JSON_OBJECT_SET(json, pid, %d, _found_task->pid);
                JSON_OBJECT_SET(json, process, "%s", _found_task->processName);
                printf(" (%d) %s", _found_task->pid, _found_task->processName);
            }
        }

			if ((kotype == IKOT_THREAD_CONTROL) ||
                (kotype == IKOT_THREAD_READ) ||
                (kotype == IKOT_THREAD_INSPECT)) {
				for (int i = 0; i < taskinfo->threadCount; i++) {
					if (taskinfo->threadInfos[i].th_kobject == kobject) {
						printf(" (%#llx)", taskinfo->threadInfos[i].th_id);
						break;
					}
				}
			}

        printf("\n");
        if (kotype == IKOT_VOUCHER) {
            counts->vouchercount++;
            if (lsmp_config.show_voucher_details) {
                JSON_KEY(json, recipes);
                JSON_ARRAY_BEGIN(json);
                char * detail = copy_voucher_detail(taskinfo->task, entry->iin_name, json);
                JSON_ARRAY_END(json); // recipes
                printf("%s\n", detail);
                free(detail);
            }
        }
        JSON_OBJECT_END(json); // kobject
        return;
    }

    /* not kobject - find the receive right holder */
    my_per_task_info_t *recv_holder_taskinfo;
    mach_port_name_t recv_name = MACH_PORT_NULL;
    if (KERN_SUCCESS == get_taskinfo_of_receiver_by_send_right(*entry, &recv_holder_taskinfo, &recv_name)) {
        mach_port_status_t port_status;
        mach_port_info_ext_t info;
        mach_port_context_t port_context = (mach_port_context_t)0;
        if (0 != get_recieve_port_status(recv_holder_taskinfo->task, recv_name, &info)) {
            bzero((void *)&port_status, sizeof(port_status));
        }
        port_status = info.mpie_status;
        get_receive_port_context(recv_holder_taskinfo->task, recv_name, &port_context);

        JSON_OBJECT_SET(json, queue_limit, %d, port_status.mps_qlimit);
        JSON_OBJECT_SET(json, msg_count, %d, port_status.mps_msgcount);
        JSON_OBJECT_SET(json, context, "0x%016llx", (uint64_t)port_context);
        JSON_OBJECT_SET(json, identifier, "0x%08x", recv_name);
        JSON_OBJECT_SET(json, pid, %d, recv_holder_taskinfo->pid);
        JSON_OBJECT_SET(json, process, "%s", recv_holder_taskinfo->processName);

        printf("   ->   %6d  %8d  0x%016llx 0x%08x  (%d) %s\n",
               port_status.mps_qlimit,
               port_status.mps_msgcount,
               (uint64_t)port_context,
               recv_name,
               recv_holder_taskinfo->pid,
               recv_holder_taskinfo->processName);

    } else {
        JSON_OBJECT_SET(json, identifier, "0x%08x", 0);
        JSON_OBJECT_SET(json, pid, %d, -1);
        JSON_OBJECT_SET(json, process, "unknown");
        printf("                                             0x00000000  (-) Unknown Process\n");
    }

    JSON_OBJECT_END(json); // non-kobject
}

uint32_t print_hex_data(char *outstr, uint32_t maxlen, char *prefix, char *desc, void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = addr;
    uint32_t plen = 0;

    if (desc != NULL)
        plen += snprintf(&outstr[plen], safesize(maxlen - plen), "%s%s:\n", prefix, desc);

    for (i = 0; i < len; i++) {

        if ((i % 16) == 0) {
            if (i != 0)
                plen += snprintf(&outstr[plen], safesize(maxlen - plen), "  %s\n", buff);

            plen += snprintf(&outstr[plen], safesize(maxlen - plen), "%s  %04x ", prefix, i);
        }

        plen += snprintf(&outstr[plen], safesize(maxlen - plen), " %02x", pc[i]);

        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    while ((i % 16) != 0) {
        plen += snprintf(&outstr[plen], safesize(maxlen - plen), "   ");
        i++;
    }

    plen += snprintf(&outstr[plen], safesize(maxlen - plen), "  %s\n", buff);

    return plen;
}
