/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <sys/types.h>
#include <unistd.h>
#include <libc.h>
#include <errno.h>
#include <mach/mach.h>
#include "stuff/bool.h"
#include <mach-o/dyld.h>
#include <mach-o/dyld_debug.h>
#include "ofi.h"
#include "debug.h"

#undef USE_TASK_CREATE

static task_port_t create_core_task(void);

static enum bool remove_core_tasks_memory(
    task_port_t core_task);

static enum bool load_task_from_core_ofile(
    struct ofile *ofile,
    task_port_t core_task);

/*
 * _dyld_debug_task_from_core() takes an NSObjectFileImage of a core file and
 * creates a task from that that a dyld debug thread can be created in. The
 * task port is returned indirectly through the core_task pointer.
 */
enum dyld_debug_return
_dyld_debug_task_from_core(
NSObjectFileImage objectFileImage,
task_port_t *core_task)
{
    struct ofi *ofi;
    struct dyld_data_section data;

	*core_task = TASK_NULL;
	ofi = (struct ofi *)objectFileImage;
	
	/* the object file image must be a core file */
	if(ofi->ofile.mh->filetype != MH_CORE){
	    SET_LOCAL_DYLD_DEBUG_ERROR(CORE_ERROR + 1);
	    return(DYLD_INVALID_ARGUMENTS);
	}

	/* create a task to load the contents of the core file into */
	if((*core_task = create_core_task()) == TASK_NULL)
	    return(DYLD_FAILURE);

	/* load the core file into the task */
	if(load_task_from_core_ofile(&(ofi->ofile), *core_task) == FALSE){
	    (void)task_terminate(*core_task);
	    *core_task = TASK_NULL;
	    return(DYLD_FAILURE);
	}

	/*
	 * Now set the debug_port and debug_thread in the task to the special
	 * CORE_DEBUG_PORT and CORE_DEBUG_THREAD values so that the other
	 * _dyld_debug*() api will know this is a task created from a core
	 * file.
	 */
	if(get_dyld_data(*core_task, &data) != DYLD_SUCCESS){
	    (void)task_terminate(*core_task);
	    *core_task = TASK_NULL;
	    return(DYLD_FAILURE);
	}
/*
printf("data.stub_binding_helper_interface 0x%x\n",
data.stub_binding_helper_interface);
*/
	data.debug_port = CORE_DEBUG_PORT;
	data.debug_thread = CORE_DEBUG_THREAD;
	if(set_dyld_data(*core_task, &data) != DYLD_SUCCESS){
	    (void)task_terminate(*core_task);
	    *core_task = TASK_NULL;
	    return(DYLD_FAILURE);
	}

	return(DYLD_SUCCESS);
}

/*
 * create_core_task() creates a task that the core file's can be loaded into.
 * It returns the task port or TASK_NULL if it can't create the task.
 */
static
task_port_t
create_core_task(void)
{
    task_port_t core_task;
    kern_return_t r;

#ifdef USE_TASK_CREATE
#ifdef __MACH30__
    /*
     * Trying to used task_create will cause a MacOS X DP4 Gonzo1I6 kernel to
     * panic when the task port is used later.
     */
    ledger_array_t	ledger_array;
    ledger_t		ledgers;
    mach_msg_type_number_t ledgersCnt;

	ledgersCnt = 1;
	ledgers = NULL;
	ledger_array = &ledgers;
	if((r = task_create(mach_task_self(), ledger_array, ledgersCnt, FALSE,
			    &core_task)) != KERN_SUCCESS)
#else
	if((r = task_create(mach_task_self(), FALSE, &core_task)) !=
			   KERN_SUCCESS){
#endif
	    SET_MACH_DYLD_DEBUG_ERROR(r, CORE_ERROR + 2);
	    return(TASK_NULL);
	}
#else /* !defined(USE_TASK_CREATE) */
    long child_pid;
    thread_array_t thread_list;
    unsigned int count, i;

	if((child_pid = fork()) == 0){
	    while(1)
		; /* spin waiting for parent to use the task */

	    /* This child never resumes here */
	    exit(1);
	}
	if(child_pid == -1){
	    SET_ERRNO_DYLD_DEBUG_ERROR(errno, CORE_ERROR + 3);
	    return(TASK_NULL);
	}
#ifdef __MACH30__
	r = task_for_pid(mach_task_self(), child_pid, &core_task);
#else
	r = task_by_unix_pid(mach_task_self(), child_pid, &core_task);
#endif
	if(r != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(r, CORE_ERROR + 4);
	    return(TASK_NULL);
	}

	count = 0;
	if((r = task_threads(core_task, &thread_list, &count)) != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(r, CORE_ERROR + 5);
	    return(TASK_NULL);
	}
#ifdef CORE_DEBUG
	printf("task_threads count = %d\n", count);
#endif
	for(i = 0; i < count; i++){
	    /*
	     * We really want to do a thread_terminate() but doing this will
	     * cause a MacOS X DP4 Gonzo1I6 kernel to panic when the task
	     * port is used later.
	     */
	    if((r = thread_suspend(thread_list[i])) != KERN_SUCCESS){
		SET_MACH_DYLD_DEBUG_ERROR(r, CORE_ERROR + 6);
		return(TASK_NULL);
	    }
	}
	if((r = task_suspend(core_task)) != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(r, CORE_ERROR + 7);
	    return(TASK_NULL);
	}

#ifdef CORE_DEBUG
	printf("task_suspend() == KERN_SUCCESS\n");
#endif
	if(remove_core_tasks_memory(core_task) != TRUE){
	    if((r = task_terminate(core_task) != KERN_SUCCESS))
		SET_MACH_DYLD_DEBUG_ERROR(r, CORE_ERROR + 8);
	    return(TASK_NULL);
	}
#endif /* !defined(USE_TASK_CREATE) */
	return(core_task);
}

/*
 * remove_core_tasks_memory() takes a task port and deallocates it vm space.
 * If it is successfull it returns TRUE else it returns FALSE.
 */
static
enum bool
remove_core_tasks_memory(
task_port_t core_task)
{
    kern_return_t r;
    vm_address_t address;
    vm_size_t size;
#ifdef __MACH30__
    vm_region_info_data_t info;
    mach_msg_type_number_t infoCnt;
#else
    vm_prot_t protection, max_protection;
    vm_inherit_t inheritance;
    boolean_t shared;
    vm_offset_t offset;
#endif
    mach_port_t object_name;

	address = 0;
	do{
#ifdef __MACH30__
	    infoCnt = VM_REGION_BASIC_INFO_COUNT;
	    r = vm_region(core_task, &address, &size, VM_REGION_BASIC_INFO, 
		          info, &infoCnt, &object_name);
#else
	    r = vm_region(core_task, &address, &size, &protection,
		          &max_protection, &inheritance, &shared, &object_name,
		          &offset);
#endif
	    if(r == KERN_SUCCESS){
#ifdef CORE_DEBUG
		printf("vm_deallocate(address = 0x%x size = 0x%x)\n",
		       (unsigned int)address, (unsigned int)size);
#endif
		r = vm_deallocate(core_task, address, size);
		if(r != KERN_SUCCESS){
		    SET_MACH_DYLD_DEBUG_ERROR(r, CORE_ERROR + 9);
		    return(FALSE);
		}
		address += size;
	    }
	    else if(r != KERN_NO_SPACE && r != KERN_INVALID_ADDRESS){
#ifdef CORE_DEBUG
		printf("vm_region() failed r = %d (%s)\n",
		       r, mach_error_string(r));
#endif
		SET_MACH_DYLD_DEBUG_ERROR(r, CORE_ERROR + 10);
		return(FALSE);
	    }
	}while(r != KERN_NO_SPACE && r != KERN_INVALID_ADDRESS);
	return(TRUE);
}

/*
 * load_task_from_core_ofile() loads the segments of the core file in ofile
 * into the core task.  If it is successfull it returns TRUE else it returns
 * FALSE.
 */
static
enum bool
load_task_from_core_ofile(
struct ofile *ofile,
task_port_t core_task)
{
    unsigned long i;
    struct load_command *lc;
    struct segment_command *sg;
    kern_return_t r;

	lc = ofile->load_commands;
	for(i = 0; i < ofile->mh->ncmds; i++){
	    if(lc->cmd == LC_SEGMENT){
		sg = (struct segment_command *)lc;
		if((r = vm_allocate(core_task, (vm_address_t *)&(sg->vmaddr),
			            sg->vmsize, FALSE)) != KERN_SUCCESS){
		    SET_MACH_DYLD_DEBUG_ERROR(r, CORE_ERROR + 11);
		    return(FALSE);
		}
/*
if(sg->vmaddr == 0x2000){
printf("loading at 0x2000 the value 0x%x\n",
*((unsigned int *)(ofile->object_addr + sg->fileoff)));
}
*/
		if((r = vm_write(core_task, sg->vmaddr,
			 (pointer_t)(ofile->object_addr + sg->fileoff),
			 sg->filesize)) != KERN_SUCCESS){
		    SET_MACH_DYLD_DEBUG_ERROR(r, CORE_ERROR + 12);
		    return(FALSE);
		}
                if((r = vm_protect(core_task, sg->vmaddr,
		         (vm_size_t)sg->vmsize, FALSE, sg->initprot)) !=
			 KERN_SUCCESS){
		    SET_MACH_DYLD_DEBUG_ERROR(r, CORE_ERROR + 13);
		    return(FALSE);
		}
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	return(TRUE);
}
