/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#import <stdlib.h>
#import <stdio.h>
#import <string.h>
#import <unistd.h>
#import <mach/mach.h>
extern void fork_mach_init(void);
#import "stuff/openstep_mach.h"
#import <mach/mig_errors.h>
#import <mach/message.h>
#import <mach-o/loader.h>
#import <mach-o/dyld_debug.h>
#import <mach-o/dyld_gdb.h>

#import "inline_strcmp.h"
#import "dyld_debug_defs.h"
#import "lock.h"
#import "debug.h"
#import "images.h"
#import "symbols.h"
#import "reloc.h"
#import "errors.h"
#import "allocate.h"
#import "register_funcs.h"
#import "mod_init_funcs.h"
#import "dyld_init.h"

/* this header file is created by mig */
#import "_dyld_event.h"

/* 4 minuite or 240 second or 240000 milliseconds timeouts */
#ifndef __MACH30__
static const unsigned long send_timeout = 240000;
#endif
static const unsigned long rcv_timeout = 240000;

/*
 * This used to trace what is going on the the dyld debug interface and is set
 * to TRUE in pickup_environment_variables() in dyld_init.c if the environment
 * variable DYLD_DEBUG_TRACE is set.
 */
enum bool dyld_debug_trace = FALSE;

/*
 * This stuff should be produced by mig(1) but it isn't so it is copied from
 * from the mig generated files to create these structures.  This is only needed
 * so the correct size of the request and reply messages can be allocated by
 * the server loop.
 */
struct inbound_message_request {
#ifdef __MACH30__
    mach_msg_header_t head;
    union{
	struct{
	    mach_msg_body_t msgh_body;
	    mach_msg_port_descriptor_t subscriber;
	    NDR_record_t NDR;
	    boolean_t inconsistent_data_ok;
	} subscribe_to_events_request;
	struct{
	    mach_msg_body_t msgh_body;
	    mach_msg_ool_descriptor_t name;
	    NDR_record_t NDR;
	    boolean_t inconsistent_data_ok;
	    mach_msg_type_number_t nameCnt;
	} defining_module_request;
	struct{
	    NDR_record_t NDR;
	    boolean_t inconsistent_data_ok;
	    module_t module;
	} is_module_bound_request;
	struct{
	    NDR_record_t NDR;
	    boolean_t inconsistent_data_ok;
	    module_t module;
	} module_name_request;
	struct{
	    NDR_record_t NDR;
	    boolean_t inconsistent_data_ok;
	    module_t module;
	} bind_module_request;
    }u;
    mach_msg_trailer_t trailer;
#else /* !defined(__MACH30__) */
    msg_header_t head;
    msg_type_t inconsistent_data_okType;
    boolean_t inconsistent_data_ok;
    union{
	struct{
	    msg_type_t subscriberType;
	    mach_port_t subscriber;
	} subscribe_to_events_request;
	struct{
	    msg_type_long_t nameType;
	    string_t name;
	} defining_module_request;
	struct{
	    msg_type_t moduleType;
	    module_t module;
	} is_module_bound_request;
	struct{
	    msg_type_t moduleType;
	    module_t module;
	} module_name_request;
	struct{
	    msg_type_t moduleType;
	    module_t module;
	} bind_module_request;
    }u;
#endif /* __MACH30__ */
};

struct inbound_message_reply {
#ifdef __MACH30__
    mach_msg_header_t head;
    union{
	struct{
	    NDR_record_t NDR;
	    kern_return_t RetCode;
	} subscribe_to_events_reply;
	struct{
	    NDR_record_t NDR;
	    kern_return_t RetCode;
	    module_t module;
	    dyld_debug_return_t return_value;
	} defining_module_reply;
	struct{
	    NDR_record_t NDR;
	    kern_return_t RetCode;
	    boolean_t bound;
	    dyld_debug_return_t return_value;
	} is_module_bound_reply;
	struct{
	    mach_msg_body_t msgh_body;
	    mach_msg_ool_descriptor_t image_name;
	    mach_msg_ool_descriptor_t module_name;
	    NDR_record_t NDR;
	    mach_msg_type_number_t image_nameCnt;
	    mach_msg_type_number_t module_nameCnt;
	    dyld_debug_return_t return_value;
	} module_name_reply;
	struct{
	    NDR_record_t NDR;
	    kern_return_t RetCode;
	    dyld_debug_return_t return_value;
	} bind_module_reply;
    }u;
#else /* !defined(__MACH30__) */
    msg_header_t head;
    msg_type_t RetCodeType;
    kern_return_t RetCode;
    msg_type_t _dyld_debug_RetCodeType;
    enum dyld_debug_return _dyld_debug_RetCode;
    union{
#ifndef __MWERKS__
	struct{
	    /* just dyld_debug return value */
	} subscribe_to_events_reply;
#endif
	struct{
	    msg_type_t moduleType;
	    module_t module;
	} defining_module_reply;
	struct{
	    msg_type_t boundType;
	    boolean_t bound;
	} is_module_bound_reply;
	struct{
	    msg_type_long_t image_nameType;
	    string_t image_name;
	    msg_type_long_t module_nameType;
	    string_t module_name;
	} module_name_reply;
#ifndef __MWERKS__
	struct{
	    /* just dyld_debug return value */
	} bind_module_reply;
#endif
    }u;
#endif /* __MACH30__ */
};

boolean_t _dyld_debug_server(
    struct inbound_message_request *request,
    struct inbound_message_reply *reply);

/*
 * These are uses for inbound messages.  After these values are set they are
 * copied into pointers in the (__DATA,__dyld) section in the images.
 */
mach_port_t debug_port = MACH_PORT_NULL;
mach_port_t debug_thread = MACH_PORT_NULL;

/*
 * This is used for outbound messages.
 */
struct event_port {
    mach_port_t port;
    struct event_port *next;
};
static struct event_port event_port_list = { MACH_PORT_NULL, NULL };
/*
 * These are used by send_past_events() to send a DYLD_IMAGE_ADDED event for
 * the dynamic linker itself if it is a separate image.  Know if it is a
 * separate image dyld_image is declared as common symbol here (with a value
 * of 0 or FALSE) and as a data symbol in dyld_reloc.c as TRUE and will be
 * over ridden from there if the dynamic linker is linked as a separate image.
 */
boolean_t dyld_image; /* these must be common, uninitialized globals */
struct mach_header *dyld_image_header;
unsigned long dyld_image_vmaddr_slide;
char *dyld_image_name;

/*
 * This is used by dyld_event_MsgError() and send_event() to know when a
 * message to an event_port failed and it should be removed from the event_port
 * list.
 */
static boolean_t event_MsgError = FALSE;

/*
 * core_image is set by start_debug_thread() from it's parameter start_from_core
 * so the server routines for inbound messages know if they are running in a
 * core file.
 */
static enum bool core_image = FALSE;

static enum dyld_debug_return set_dyld_data(
    unsigned long offset,
    unsigned long data);

static void server_loop(
    void);

static enum dyld_debug_return send_past_events(
    mach_port_t subscriber);

/*
 * These variables are used for the dyld/gdb interface as described in the
 * header file <mach-o/dyld_gdb.h>.
 */
unsigned int gdb_dyld_version = 1;
unsigned int gdb_nobject_images = NOBJECT_IMAGES;
unsigned int gdb_nlibrary_images = NLIBRARY_IMAGES;
unsigned int gdb_object_image_size = sizeof(struct object_image);
unsigned int gdb_library_image_size = sizeof(struct library_image);

/*
 * gdb_dyld_state_changed() is a dummy routine called by dyld after images get
 * added or removed/ Gdb is expected to set a break point at
 * gdb_dyld_state_changed() then re-read dyld internal data as specified in
 * the header file <mach-o/dyld_gdb.h>.
 */
void
gdb_dyld_state_changed(
void)
{
	/* do nothing */
}

/*
 * These booleans are used to allow the debugging thread to get the the state
 * of the dyld data structures to a known state.
 */
volatile enum bool debug_thread_waiting_for_lock = FALSE;
volatile enum bool lock_in_multiply_defined_handler = FALSE;
static volatile enum bool lock_in_linkedit_error_handler = FALSE;

void
multiply_defined_enter(void)
{
	lock_in_multiply_defined_handler = TRUE;
	/*
	 * If the debug thread gets this error let it run the user's multiply
	 * defined error handler if any or let the error get printed and exit.
	 */
	if(debug_thread != mach_thread_self())
	    return;

	if(debug_thread_waiting_for_lock == TRUE)
	    thread_suspend(mach_thread_self());
}

void
multiply_defined_exit(void)
{
	lock_in_multiply_defined_handler = FALSE;
}

void
linkedit_error_enter(void)
{
	/*
	 * If the debug thread gets this error let it run the user's linkedit
	 * error hander if any or let the error get printed and exit.
	 */
	if(debug_thread != mach_thread_self())
	    return;

	lock_in_linkedit_error_handler = TRUE;
	if(debug_thread_waiting_for_lock == TRUE)
	    thread_suspend(mach_thread_self());
}

/*
 * start_debug_thread() is called either from an external process that has
 * created a new thread in the process and jumps here or from a process that
 * created a task from a core file and then created a thread that jumps here.
 * They get the address of this routine from the (__DATA,__dyld) section of the
 * images which gets set in images.c when the image gets loaded.
 *
 * This routine allocates the debug_port for inbound messages and allocates a
 * port set for outbound messages.  Then this thread loops and responds to
 * inbound messges.  This must use only mach calls when core_image is TRUE as
 * it is being run from a core image.
 */
void
start_debug_thread(
enum bool start_from_core)
{
    kern_return_t r;

	/*
	 * The CORE_DEBUG is used to debug recreating a task from a core file.
	 * By writing different values as we proceed through the logic the
	 * task that created us can see where we are by looking at our memory
	 * with vm_read()'s.  This is basicly a poor man's printf.
	 */
#ifdef CORE_DEBUG
	set_dyld_data(24, 1);
#endif
	if(dyld_debug_trace)
	    print("start_debug_thread called\n");

	/*
	 * This is needed when we are started from a core file to get the
	 * valid mach task self port so we can do the mach calls needed for
	 * this debug thread.
	 */
	if(start_from_core == TRUE)
	    fork_mach_init();
#ifdef CORE_DEBUG
	set_dyld_data(24, 2);
#endif

	/*
	 * debug_thread is the thread id for this thread which responds to
	 * inbound messages.  It's value is set into the (__DATA,__dyld)
	 * section so external processes know which thread to not suppend
	 * in the task when sending it a message.
	 */
	debug_thread = mach_thread_self();
	if(dyld_debug_trace)
	    print("debug_thread %d\n", debug_thread);
#ifdef CORE_DEBUG
	set_dyld_data(24, 3);
#endif

	/*
	 * Put the value of debug_thread into the executable images'
	 * (__DATA,__dyld) section.  This uses a lock to try to protect
	 * setting debug_thread in the (__DATA,__dyld) section so that if more
	 * than one task try to start a thread only one should succeed.  But
	 * there is a critical region.  Where if the lock is taken and we get
	 * reschuled before debug_thread gets set in the (__DATA,__dyld)
	 * section.  If in this region another task trys to start the debugging 
	 * thread after seeing debug_thread is not set it will suppended all
	 * the threads in this task not knowing that one of them was the
	 * debugging thread starting up holding the lock.  Then the lock will
	 * never be released and debug_thread in the (__DATA,__dyld) section
	 * will never be set as the thread holding the lock is suppended.
	 */
	if(lock_is_set(debug_thread_lock)){
	    thread_terminate(mach_thread_self());
	}
	if(set_dyld_data(16, debug_thread) != DYLD_SUCCESS){
	    clear_lock(debug_thread_lock);
	    thread_terminate(mach_thread_self());
	}
#ifdef CORE_DEBUG
	set_dyld_data(24, 4);
#endif

	core_image = start_from_core;

	/*
	 * debug_port is the port for inbound messages.  It's value is set into
	 * the (__DATA,__dyld) section so external processes know the port to
	 * send messages to.
	 */
#ifdef __MACH30__
	if((r = mach_port_allocate(mach_task_self(),
		 MACH_PORT_RIGHT_RECEIVE, &debug_port)) != KERN_SUCCESS){
#else
	if((r = port_allocate(mach_task_self(),
				   (int *)&debug_port)) != KERN_SUCCESS){
#endif
	    clear_lock(debug_thread_lock);
	    (void)set_dyld_data(16, MACH_PORT_NULL);
	    thread_terminate(mach_thread_self());
	}
	if(dyld_debug_trace)
	    print("debug_port %d\n", debug_port);
#ifdef CORE_DEBUG
	set_dyld_data(24, 5);
#endif

	/*
	 * Put the value of debug_port into the executable images'
	 * (__DATA,__dyld) section.  Since the debug_thread was set at an
	 * offset (16) greater than where we put the debug port (12) an error
	 * can occur.
	 */
	(void)set_dyld_data(12, debug_port);
#ifdef CORE_DEBUG
	set_dyld_data(24, 6);
#endif

	/*
	 * This thread now becomes the debug thread that responds to inbound
	 * messages.
	 */
	server_loop();
}

/*
 * set_dyld_data() sets the unsigned long data into the (__DATA,__dyld)
 * section of the executable at the specified offset.  If the executable has
 * a (__DATA,__dyld) section and it is large enough such that at the offset
 * an unsigned long can be placed there this routine returns DYLD_SUCCESS.
 * else it return DYLD_FAILURE.
 */
static
enum dyld_debug_return
set_dyld_data(
unsigned long offset,
unsigned long data)
{
    unsigned long i, j;
    struct load_command *lc, *load_commands;
    struct segment_command *sg;
    struct section *s;

	/* the first image in the object_images is the executable */
	load_commands = (struct load_command *)
			((char *)object_images.images[0].image.mh +
			sizeof(struct mach_header));
	lc = load_commands;
	for(i = 0; i < object_images.images[0].image.mh->ncmds; i++){
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		s = (struct section *)
		    ((char *)sg + sizeof(struct segment_command));
		for(j = 0; j < sg->nsects; j++){
		    if(strcmp(s->segname, "__DATA") == 0 &&
		       strcmp(s->sectname, "__dyld") == 0){
			if(s->size >= offset + sizeof(unsigned long)){
			    *((long *)(s->addr + offset)) = data;
			    return(DYLD_SUCCESS);
			}
		    }
		    s++;
		}
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	return(DYLD_FAILURE);
}

/*
 * server_loop() is the debug thread's loop that responds to inbound messages.
 */
static
void
server_loop(void)
{
    struct inbound_message_request request;
    struct inbound_message_reply reply;
    kern_return_t r;
    boolean_t b;

    struct thread_basic_info info;
    unsigned int info_count, suspend_count, resume_count, i;
    mach_port_t resumed_thread;

	/* enter server loop */
	while(TRUE){

	    /* receive an inbound message */
#ifdef __MACH30__
	    r = mach_msg(
		    &request.head,		/* msg */
		    MACH_RCV_MSG,		/* option */
		    0,				/* send_size */
		    sizeof(struct inbound_message_request), /* receive_limit */
		    debug_port,			/* receive_name */
		    MACH_MSG_TIMEOUT_NONE,	/* timeout */
		    MACH_PORT_NULL);		/* notify */
#else
	    /*
	     * Set the port the message will be received on to the debug_port,
	     * and the maximum size of the message that will be received to the
	     * size of an inbound message.
	     */
	    request.head.msg_local_port = debug_port;
	    request.head.msg_size = sizeof(struct inbound_message_request);

	    r = msg_receive(&request.head, MACH_MSG_OPTION_NONE, 0);
#endif
#ifdef CORE_DEBUG
	    set_dyld_data(24, 7);
#endif

	    /*
	     * For any message receive error it is just ignored.  This happens
	     * in the normal course for example:
	     *
	     * The interface that gets the debug_port in this task from another
	     * task must also translate the debug_port it read out of this
	     * task's (__DATA,__dyld) section we left for it.  It does this by
	     * doing a port_extract/port_insert pair.  When this is done when
	     * in the msg_receive() above it will terminate between the calls
	     * with RCV_INVALID_PORT.  So if this is the case continue back
	     * around the server loop into msg_receive() again.
	     *
	     * Since there maybe more than one debugger sending messages a
	     * message sent by one that has an error is just ignored so that
	     * others can still be processed.  That is we don't stop the debug
	     * thread once it has been started.
	     */
#ifdef __MACH30__
	    if(r != MACH_MSG_SUCCESS)
#else
	    if(r != RCV_SUCCESS)
#endif
		continue;

	    /*
	     * Now before we feed the request to the server routines we need
	     * to get the dyld data structures into a known state.  All other
	     * threads are suspended at this point.  To get things into a known
	     * state we set debug_thread_waiting_for_lock.  Then if a thread has
	     * the lock we resume it until the lock is released knowing that
	     * the thread will suppend itself if debug_thread_waiting_for_lock
	     * is set when it releases the lock.  This resuming of the thread
	     * that has the lock needs to loop because if another task wants to
	     * send an inbound message it will suspend all threads again and
	     * the thread that has the lock may not have released the lock or
	     * gotten to an error handler.
	     */

            /* davidp 1/7/99
             * release_lock() sets thread_that_has_dyld_lock to MACH_PORT_NULL
             * and then resuspends the thread that had the lock. This is fine a
	     * long as that thread's initial suspend count was 1 - the resuspend
	     * logic just before cant_unlock: accounts for the suspend done in
	     * release_lock(). However, if the thread holding the lock has a
	     * suspend count greater than 1 we're hosed since the resuspend
	     * logic operates on thread_that_has_dyld_lock which was set to
             * MACH_PORT_NULL in release_lock(). To get around this I added a
	     * local that captures thread_that_has_dyld_lock and the rest of
	     * this function only operates on that local.
             */
	    if(core_image == TRUE)
		goto cant_unlock;
	    resume_count = 0;
            resumed_thread = MACH_PORT_NULL;
	    debug_thread_waiting_for_lock = TRUE;
	    if(dyld_lock == TRUE &&
		  thread_that_has_dyld_lock != MACH_PORT_NULL &&
		  lock_in_multiply_defined_handler == FALSE &&
		  lock_in_linkedit_error_handler == FALSE){

		info_count = THREAD_BASIC_INFO_COUNT;
		r = thread_info(thread_that_has_dyld_lock, THREAD_BASIC_INFO,
				(thread_info_t)&info, &info_count);
		/*
		 * If can't get the thread info for the thread that had the
		 * lock it must have been aborted. So the lock won't be released
		 * and the if the core flag is not set no information will be
		 * returned.
		 */
		if(r != KERN_SUCCESS)
		    goto cant_unlock;

                /* davidp 1/7/99
                 * Only capture this once per lock switch session. If the lock
                 * is held thread_that_has_dyld_lock will be set to
		 * MACH_PORT_NULL when the lock is released and we will lose
		 * resumed_thread and not restore the thread's suspend count
		 * properly
                 */
                if(resumed_thread == MACH_PORT_NULL)
		    resumed_thread = thread_that_has_dyld_lock;
                
		for(suspend_count = info.suspend_count;
		    suspend_count > 0;
		    suspend_count--){
		    r = thread_resume(resumed_thread);
		    resume_count++;
		    /*
		     * If the suspend count was already 0, then stop the
		     * thread_resumes as the thread is runable and this is what
		     * we wanted.
		     */
		    if(r == KERN_FAILURE)
			break;
		    /*
		     * If the thread is not a thread, the thread must have been
		     * aborted so again the lock won't be released.
		     */
		    if(r != KERN_SUCCESS)
			goto cant_unlock;
		}
		/*
		 * The thread has the lock is now runable so switch to it so it
		 * can release the lock.  The loop will continue to switch to
		 * this thread until the lock is released or a handler is called
		 * by the thread that has the lock.
		 */
		r = thread_switch(resumed_thread,
				  SWITCH_OPTION_NONE, 0);
		/*
		 * Again if the thread is not a thread, the thread must have
		 * been aborted so again the lock won't be released.
		 */
		if(r != KERN_SUCCESS)
		    goto cant_unlock;
	    }
	    while((dyld_lock == TRUE ||
		   thread_that_has_dyld_lock != MACH_PORT_NULL) &&
		  lock_in_multiply_defined_handler == FALSE &&
		  lock_in_linkedit_error_handler == FALSE);
	    /*
	     * If we did any thread_resume()'s on the thread_that_has_dyld_lock
	     * we need to get it's suspend count back to the correct value.  One
	     * thread_suppend() was done by the thread itself when if it got
	     * to the point to released the lock (that is it did not end up in
	     * an error handler).
	     */
	    if(resume_count != 0){
		if(dyld_lock == FALSE)
		    resume_count--;
#if !defined(__GONZO_BUNSEN_BEAKER__) && !defined(__HERA__) && !defined(__OPENSTEP__)
		usleep(20000);
#endif
		for(i = 0; i < resume_count; i++){
		    r = thread_suspend(resumed_thread);
		    /*
		     * If the thread is not a thread, the thread must have
		     * been aborted the lock is released so we are done.
		     */
		    if(r != KERN_SUCCESS)
			break;
		}
                
		resumed_thread = MACH_PORT_NULL;
	    }
cant_unlock:
	    if(core_image == FALSE)
		debug_thread_waiting_for_lock = FALSE;
	    /*
	     * Feed into server (_dyld_debug_server() is created by mig).
	     */
	    b = _dyld_debug_server(&request, &reply);

	    if(b == FALSE){
		/*
		 * The wrong message ID is this case, this means that it might
		 * be a new message type in the libdyld interface but the
		 * dynamic linker being talked to here is old. Ignore this
		 * message and assume the sender has some timeout waiting for
		 * the reply to know the message will not be replied to.
		 */
		continue;
	    }
	    /* send the reply */
#ifdef __MACH30__
	    r = mach_msg(
		    &reply.head,		/* msg */
		    MACH_SEND_MSG,		/* option */
		    reply.head.msgh_size,	/* send_size */
		    0,				/* receive_limit */
		    MACH_PORT_NULL,		/* receive_name */
		    MACH_MSG_TIMEOUT_NONE,	/* timeout */
		    MACH_PORT_NULL);		/* notify */
#else
	    reply.head.msg_local_port = debug_port;
	    msg_send(&reply.head, MACH_MSG_OPTION_NONE, 0);
#endif
	}
}

/*
 * server_dyld_debug_defining_module() is the server size of
 * _dyld_debug_defining_module() which is passed a symbol name and returns
 * which module the symbol name is being or would be used from.
 */
#ifdef __MACH30__
kern_return_t
#else
enum dyld_debug_return
#endif
server_dyld_debug_defining_module(
mach_port_t debug_port,
boolean_t inconsistent_data_ok,
char *name,
unsigned int nameCnt,
struct dyld_debug_module *module
#ifdef __MACH30__
, dyld_debug_return_t *retval
#endif
)
{
    struct nlist *defined_symbol;
    module_state *defined_module;
    struct image *defined_image;
    struct library_image *defined_library_image;
    enum dyld_debug_return return_value;

	/* first set all the fields of the dyld debug module to zero */
	memset(module, '\0', sizeof(struct dyld_debug_module));

	/*
	 * If the lock is set and the caller does not want inconsistent data
	 * then just return DYLD_INCONSISTENT_DATA and do nothing else.
	 */
	if(dyld_lock == TRUE && inconsistent_data_ok == FALSE){
	    return_value = DYLD_INCONSISTENT_DATA;
	    goto done;
	}

	/* look up this symbol name */
	lookup_symbol(name, NULL, NULL, FALSE, &defined_symbol, &defined_module,
		      &defined_image, &defined_library_image, NULL);

	/*
	 * If there is no symbol by this name in the images so return
	 * DYLD_INVALID_ARGUMENTS to indicate this.
	 */
	if(defined_symbol == NULL){
	    return_value = DYLD_INVALID_ARGUMENTS;
	    goto done;
	}

	/* set the fields of the dyld debug module */
	module->header = defined_image->mh;
	module->vmaddr_slide = defined_image->vmaddr_slide;
	if(defined_library_image != NULL)
	    module->module_index =
		defined_module - defined_library_image->modules;

	return_value = DYLD_SUCCESS;
done:
	(void)vm_deallocate(mach_task_self(), (vm_address_t)name,
			    (vm_size_t)nameCnt);
#ifdef __MACH30__
	*retval = return_value;
	return(KERN_SUCCESS);
#else
	return(return_value);
#endif
}

/*
 * server_dyld_debug_is_module_bound() the server size of
 * _dyld_debug_is_module_bound() which is passed a dyld_debug_module struct and
 * sets bound to TRUE if the module is bound and FALSE otherwise. 
 */
#ifdef __MACH30__
kern_return_t
#else
enum dyld_debug_return
#endif
server_dyld_debug_is_module_bound(
mach_port_t debug_port,
boolean_t inconsistent_data_ok,
struct dyld_debug_module module,
boolean_t *bound
#ifdef __MACH30__
, dyld_debug_return_t *retval
#endif
)
{
    unsigned long i, j;
    struct object_images *p;
    struct library_images *q;
    enum link_state link_state;
    enum dyld_debug_return return_value;

	/* Assume the module is not linked */
	*bound = FALSE;

	/*
	 * If the lock is set and the caller does not want inconsistent data
	 * then just return DYLD_INCONSISTENT_DATA and do nothing else.
	 */
	if(dyld_lock == TRUE && inconsistent_data_ok == FALSE){
	    return_value = DYLD_INCONSISTENT_DATA;
	    goto done;
	}

	/*
	 * Look through the object images and then the library images trying
	 * to find this module.  Then if found see if it is linked.
	 */
	for(p = &object_images ; ; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state == UNUSED)
		    continue;
		if(module.header == p->images[i].image.mh){
		    if(module.module_index != 0)
			return(DYLD_INVALID_ARGUMENTS);
		    if(link_state == LINKED || link_state == FULLY_LINKED)
			*bound = TRUE;
		    return_value = DYLD_SUCCESS;
		    goto done;
		}
	    }
	    if(p->next_images == NULL)
		break;
	}
	for(q = &library_images ; ; q = q->next_images){
	    for(i = 0; i < q->nimages; i++){
		if(module.header == q->images[i].image.mh){
		    if(module.module_index >= q->images[i].image.dyst->nmodtab)
			return(DYLD_INVALID_ARGUMENTS);
		    j = module.module_index;
		    link_state = GET_LINK_STATE(q->images[i].modules[j]);
		    if(link_state == LINKED || link_state == FULLY_LINKED)
			*bound = TRUE;
		    return_value = DYLD_SUCCESS;
		    goto done;
		}
	    }
	    if(q->next_images == NULL)
		break;
	}

	return_value = DYLD_FAILURE;
done:
#ifdef __MACH30__
	*retval = return_value;
	return(KERN_SUCCESS);
#else
	return(return_value);
#endif
}

/*
 * server_dyld_debug_bind_module() is the server side of
 * _dyld_debug_bind_module() which binds the modules specified by
 * dyld_debug_module into the program.
 */
#ifdef __MACH30__
kern_return_t
#else
enum dyld_debug_return
#endif
server_dyld_debug_bind_module(
mach_port_t debug_port,
boolean_t inconsistent_data_ok,
struct dyld_debug_module module
#ifdef __MACH30__
, dyld_debug_return_t *retval
#endif
)
{
    unsigned long i, j;
    struct object_images *p;
    struct library_images *q;
    enum link_state link_state;
    enum dyld_debug_return return_value;

	/*
	 * If the lock is set and the caller does not want inconsistent data
	 * then just return DYLD_INCONSISTENT_DATA and do nothing else.
	 */
	if(dyld_lock == TRUE && inconsistent_data_ok == FALSE){
	    return_value = DYLD_INCONSISTENT_DATA;
	    goto done;
	}

	/*
	 * Look through the object images and then the library images trying
	 * to find this module.  Then if found see if it is linked and if not
	 * force it to be linked.
	 */
	for(p = &object_images ; ; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state == UNUSED)
		    continue;
		if(module.header == p->images[i].image.mh){
		    if(module.module_index != 0){
			return_value = DYLD_INVALID_ARGUMENTS;
			goto done;
		    }
		    /*
		     * Object images are never in a linkable state, that is they
		     * have been linked or can't be linked (they were removed o
		     * replaced).
		     */
		    if(link_state == LINKED || link_state == FULLY_LINKED){
			return_value = DYLD_SUCCESS;
			goto done;
		    }
		    else{
			return_value = DYLD_INVALID_ARGUMENTS;
			goto done;
		    }
		}
	    }
	    if(p->next_images == NULL)
		break;
	}
	for(q = &library_images ; ; q = q->next_images){
	    for(i = 0; i < q->nimages; i++){
		if(module.header == q->images[i].image.mh){
		    if(module.module_index >= q->images[i].image.dyst->nmodtab){
			return_value = DYLD_INVALID_ARGUMENTS;
			goto done;
		    }
		    j = module.module_index;
		    link_state = GET_LINK_STATE(q->images[i].modules[j]);

		    /*
		     * We must beable to get the lock to bind this module,
		     * if the lock is set or we are in an error handler we
		     * don't try to bind this module.
		     */
		    if(dyld_lock == TRUE ||
		       lock_in_multiply_defined_handler == TRUE ||
		       lock_in_linkedit_error_handler == TRUE){
			return_value = DYLD_INCONSISTENT_DATA;
			goto done;
		    }

		    /*
		     * Library modules in a linked state simply return success
		     * as nothing needs to be done.  Otherwise if the module is 
		     * in any other state except unlinked (it was removed or
		     * replaced) it is an error as it can't be linked again.
		     */
		    if(link_state == LINKED || link_state == FULLY_LINKED){
			return_value = DYLD_SUCCESS;
			goto done;
		    }
		    if(link_state != UNLINKED &&
		       link_state != PREBOUND_UNLINKED){
			return_value = DYLD_INVALID_ARGUMENTS;
			goto done;
		    }

		    /*
		     * Since this is the debug thread responding to a
		     * message all other threads should have been suspened by
		     * task sending the message and with the tests above on the
		     * lock state this we should always beable to get the lock.
		     */
		    set_lock();

		    /*
		     * If this module is in the prebound unlinked state first
		     * undo the prebinding and then set it to the unlinked
		     * state.
		     */
		    if(link_state == PREBOUND_UNLINKED){
			undo_prebinding_for_library_module(
			    q->images[i].modules + j,
			    &(q->images[i].image),
			    q->images + i);
			SET_LINK_STATE(q->images[i].modules[j], UNLINKED);
		    }

		    /*
		     * This module is not linked.  So force to be linked,
		     * resolve undefineds, relocate modules that got
		     * linked in and check and report undefined symbols.
		     */
		    link_library_module(q->images + i,
					&(q->images[i].image),
					q->images[i].modules + j,
				        FALSE, FALSE, FALSE);
		    resolve_undefineds(FALSE, FALSE);
		    relocate_modules_being_linked(FALSE);
		    check_and_report_undefineds();
		    call_registered_funcs_for_add_images();
		    call_registered_funcs_for_linked_modules();
		    call_image_init_routines(FALSE);
		    call_module_initializers(FALSE, FALSE);

		    /* release lock for dyld data structures */
		    release_lock();
		    return_value = DYLD_SUCCESS;
		    goto done;
		}
	    }
	    if(q->next_images == NULL)
		break;
	}

	return_value = DYLD_FAILURE;
done:
#ifdef __MACH30__
	*retval = return_value;
	return(KERN_SUCCESS);
#else
	return(return_value);
#endif
}

/*
 * server_dyld_debug_module_name() the server size of _dyld_debug_module_name() 
 * which is passed a dyld_debug_module struct and sets image_name and
 * module_name to the names for the module and the nameCnts to the sizes.
 */
#ifdef __MACH30__
kern_return_t
#else
enum dyld_debug_return
#endif
server_dyld_debug_module_name(
mach_port_t debug_port,
boolean_t inconsistent_data_ok,
struct dyld_debug_module module,
char **image_name,
unsigned int *image_nameCnt,
char **module_name,
unsigned int *module_nameCnt
#ifdef __MACH30__
, dyld_debug_return_t *retval
#endif
)
{
    unsigned long i, j;
    struct object_images *p;
    struct library_images *q;
    struct segment_command *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    char *strings;
    struct dylib_module *dylib_modules;
    struct load_command *lc;
    struct dylinker_command *dyld;
    enum dyld_debug_return return_value;
    enum link_state link_state;

	/* Assume the module is not valid */
	*image_name = NULL;
	*image_nameCnt = 0;
	*module_name = NULL;
	*module_nameCnt = 0;

	/*
	 * If the lock is set and the caller does not want inconsistent data
	 * then just return DYLD_INCONSISTENT_DATA and do nothing else.
	 */
	if(dyld_lock == TRUE && inconsistent_data_ok == FALSE){
	    return_value = DYLD_INCONSISTENT_DATA;
	    goto done;
	}

	/*
	 * Look through the object images and then the library images trying
	 * to find this module.  Then if found see if it is linked.
	 */
	for(p = &object_images ; ; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state == UNUSED)
		    continue;
		if(module.header == p->images[i].image.mh){
		    if(module.module_index != 0){
			return_value = DYLD_INVALID_ARGUMENTS;
			goto done;
		    }
		    *image_name = p->images[i].image.name;
		    *image_nameCnt = strlen(*image_name);
		    return_value = DYLD_SUCCESS;
		    goto done;
		}
	    }
	    if(p->next_images == NULL)
		break;
	}
	for(q = &library_images ; ; q = q->next_images){
	    for(i = 0; i < q->nimages; i++){
		if(module.header == q->images[i].image.mh){
		    if(module.module_index >= q->images[i].image.dyst->nmodtab){
			return_value = DYLD_INVALID_ARGUMENTS;
			goto done;
		    }
		    j = module.module_index;
		    *image_name = q->images[i].image.name;
		    *image_nameCnt = strlen(*image_name);

		    linkedit_segment = q->images[i].image.linkedit_segment;
		    st = q->images[i].image.st;
		    dyst = q->images[i].image.dyst;

		    strings = (char *)
			(q->images[i].image.vmaddr_slide +
			 linkedit_segment->vmaddr +
			 st->stroff -
			 linkedit_segment->fileoff);
		    dylib_modules = (struct dylib_module *)
			(q->images[i].image.vmaddr_slide +
			 linkedit_segment->vmaddr +
			 dyst->modtaboff -
			 linkedit_segment->fileoff);
		    *module_name = strings + dylib_modules[j].module_name,
		    *module_nameCnt = strlen(*module_name);
		    return_value = DYLD_SUCCESS;
		    goto done;
		}
	    }
	    if(q->next_images == NULL)
		break;
	}
	/*
	 * This could be the dyld image.  In that case try to return the name
	 * in the LC_ID_DYLINKER command.
	 */
	if(module.header == dyld_image_header){
	    if(dyld_image_name == NULL){
		lc = (struct load_command *)
		    ((char *)dyld_image_header + sizeof(struct mach_header));
		dyld = NULL;
		for(i = 0; i < dyld_image_header->ncmds; i++){
		    switch(lc->cmd){
		    case LC_ID_DYLINKER:
			if(dyld == NULL){
			    dyld = (struct dylinker_command *)lc;
			    dyld_image_name = (char *)dyld + dyld->name.offset;
			}
			break;
		    }
		    lc = (struct load_command *)((char *)lc + lc->cmdsize);
		}
	    }
	    if(dyld_image_name != NULL){
		*image_name = dyld_image_name;
		*image_nameCnt = strlen(*image_name);
		return_value = DYLD_SUCCESS;
		goto done;
	    }
	}
	return_value = DYLD_FAILURE;
done:
#ifdef __MACH30__
	*retval = return_value;
	return(KERN_SUCCESS);
#else
	return(return_value);
#endif
}

#ifdef DYLD_PROFILING
/*
 * server_dyld_debug_monoutput() the server size of _dyld_debug_monoutput() 
 * which calls dyld_monoutput.
 */
#ifdef __MACH30__
kern_return_t
#else
enum dyld_debug_return
#endif
server_dyld_debug_monoutput(
mach_port_t debug_port
#ifdef __MACH30__
, dyld_debug_return_t *retval
#endif
)
{
	dyld_monoutput();
#ifdef __MACH30__
	*retval = DYLD_SUCCESS;
	return(KERN_SUCCESS);
#else
	return(DYLD_SUCCESS);
#endif
}
#endif /* DYLD_PROFILING */

/*
 * server_dyld_debug_add_event_subscriber() is the server size of
 * _dyld_debug_add_event_subscriber() which adds the subscriber to the list of
 * event ports that dyld event messages are sent to.  Then all past events are
 * sent to the subscriber.
 */
#ifdef __MACH30__
kern_return_t
#else
enum dyld_debug_return
#endif
server_dyld_debug_add_event_subscriber(
mach_port_t debug_port,
boolean_t inconsistent_data_ok,
mach_port_t subscriber
#ifdef __MACH30__
, dyld_debug_return_t *retval
#endif
)
{
    struct event_port *event_port;
    enum dyld_debug_return return_value;

	/*
	 * For core files server_dyld_debug_subscribe_to_events() is a valid 
	 * call but since the program is not running only past events will be
	 * sent.
	 */

	/*
	 * If the lock is set and the caller does not want inconsistent data
	 * then just return DYLD_INCONSISTENT_DATA and do nothing else.
	 */
	if(dyld_lock == TRUE && inconsistent_data_ok == FALSE){
	    return_value = DYLD_INCONSISTENT_DATA;
	    goto done;
	}

	/*
	 * Send past events to the new subscriber.
	 */
	if(send_past_events(subscriber) != DYLD_SUCCESS){
	    return_value = DYLD_FAILURE;
	    goto done;
	}

	/*
	 * Add this port to the list of event ports.
	 */
	if(event_port_list.port == MACH_PORT_NULL){
	    event_port_list.port = subscriber;
	}
	else{
	    event_port = allocate_with_no_error_check(
			     sizeof(struct event_port));
	    if(event_port == NULL){
		return_value = DYLD_FAILURE;
		goto done;
	    }
	    event_port->port = subscriber;
	    event_port->next = event_port_list.next;
	    event_port_list.next = event_port;
	}

	return_value = DYLD_SUCCESS;
done:
#ifdef __MACH30__
	*retval = return_value;
	return(KERN_SUCCESS);
#else
	return(return_value);
#endif
}

/*
 * send_past_events() is called when a new subscriber port is added.  This
 * routine then sends the current state of the dynamic linker over as a series
 * of events.
 */
static
enum dyld_debug_return
send_past_events(
mach_port_t subscriber)
{
    unsigned long i, j;
    struct object_images *p;
    struct library_images *q;
    struct dyld_event event;
    enum link_state link_state;

	/*
	 * First send all the DYLD_IMAGE_ADDED events for object images and
	 * library images.
	 */
	memset(&event, '\0', sizeof(struct dyld_event));
	event.type = DYLD_IMAGE_ADDED;
	if(dyld_image == TRUE){
	    event.arg[0].header = dyld_image_header;
	    event.arg[0].vmaddr_slide = dyld_image_vmaddr_slide;
	    event_MsgError = FALSE;
	    user_dyld_event_server_callback(subscriber, rcv_timeout,
#ifndef __MACH30__
					    send_timeout,
#endif
					    event);
	    if(event_MsgError == TRUE)
		return(DYLD_FAILURE);
	    
	}
	for(p = &object_images ; ; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state == UNUSED)
		    continue;
		event.arg[0].header = p->images[i].image.mh;
		event.arg[0].vmaddr_slide = p->images[i].image.vmaddr_slide;
		event_MsgError = FALSE;
		user_dyld_event_server_callback(subscriber, rcv_timeout,
#ifndef __MACH30__
						send_timeout,
#endif
						event);
		if(event_MsgError == TRUE)
		    return(DYLD_FAILURE);
	    }
	    if(p->next_images == NULL)
		break;
	}
	for(q = &library_images ; ; q = q->next_images){
	    for(i = 0; i < q->nimages; i++){
		event.arg[0].header = q->images[i].image.mh;
		event.arg[0].vmaddr_slide = q->images[i].image.vmaddr_slide;
		event_MsgError = FALSE;
		user_dyld_event_server_callback(subscriber, rcv_timeout,
#ifndef __MACH30__
						send_timeout,
#endif
						event);
		if(event_MsgError == TRUE)
		    return(DYLD_FAILURE);
	    }
	    if(q->next_images == NULL)
		break;
	}

	/*
	 * Second send all the DYLD_MODULE_BOUND events for object images and
	 * library images.
	 */
	memset(&event, '\0', sizeof(struct dyld_event));
	event.type = DYLD_MODULE_BOUND;
	for(p = &object_images ; ; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		/* skip modules that that are not linked */
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state != LINKED && link_state != FULLY_LINKED)
		    continue;

		event.arg[0].header = p->images[i].image.mh;
		event.arg[0].vmaddr_slide = p->images[i].image.vmaddr_slide;

		event_MsgError = FALSE;
		user_dyld_event_server_callback(subscriber, rcv_timeout,
#ifndef __MACH30__
						send_timeout,
#endif
						event);
		if(event_MsgError == TRUE)
		    return(DYLD_FAILURE);
	    }
	    if(p->next_images == NULL)
		break;
	}
	for(q = &library_images ; ; q = q->next_images){
	    for(i = 0; i < q->nimages; i++){
		for(j = 0; j < q->images[i].image.dyst->nmodtab; j++){
		    link_state = GET_LINK_STATE(q->images[i].modules[j]);
		    /* skip modules that that not being linked */
		    if(link_state != LINKED && link_state != FULLY_LINKED)
			continue;

		    event.arg[0].header = q->images[i].image.mh;
		    event.arg[0].vmaddr_slide = q->images[i].image.vmaddr_slide;
		    event.arg[0].module_index = j;

		    event_MsgError = FALSE;
		    user_dyld_event_server_callback(subscriber, rcv_timeout,
#ifndef __MACH30__
						    send_timeout,
#endif
						    event);
		    if(event_MsgError == TRUE)
			return(DYLD_FAILURE);
		}
	    }
	    if(q->next_images == NULL)
		break;
	}

	/* send end marker for past events */
	memset(&event, '\0', sizeof(struct dyld_event));
	event.type = DYLD_PAST_EVENTS_END;
	event_MsgError = FALSE;
	user_dyld_event_server_callback(subscriber, rcv_timeout,
#ifndef __MACH30__
					send_timeout,
#endif
					event);
	if(event_MsgError == TRUE)
	    return(DYLD_FAILURE);

	return(DYLD_SUCCESS);
}

/*
 * send_event() sends the specified event to all the event ports.
 */
void
send_event(
struct dyld_event *event)
{
    struct event_port *event_port, *prev;

        /*
	 * This loop expects "standard delayed free" behavior by free() as we
	 * will use event_port->next after event_port has been free()'ed.
	 */
	prev = NULL;
	for(event_port = &event_port_list;
	    event_port->port != MACH_PORT_NULL;
	    event_port = event_port->next){
	    event_MsgError = FALSE;
	    user_dyld_event_server_callback(event_port->port, rcv_timeout,
#ifndef __MACH30__
					    send_timeout,
#endif
					    *event);
	    if(event_MsgError == TRUE){
#ifdef __MACH30__
		(void)mach_port_deallocate(mach_task_self(), event_port->port);
#else
		(void)port_deallocate(mach_task_self(), event_port->port);
#endif
		event_port->port = MACH_PORT_NULL;
		if(prev != NULL){
		    prev->next = event_port->next;
		    free(event_port);
		}
	    }
	    else{
		prev = event_port;
	    }
	    if(event_port->next == NULL)
		return;
	}
}

#ifndef __MACH30__
/*
 * dyld_event_MsgError() is called by user_dyld_event_server_callback() when
 * there is an error is sending the mach message via
 * user_dyld_event_server_callback() in send_event() above.
 * This sets the flag event_MsgError to tell send_event() to remove the
 * event_port from the list.
 */
void
dyld_event_MsgError(
msg_return_t msg_result)
{
	event_MsgError = TRUE;
}
#endif
