/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
#if defined(__MWERKS__) && !defined(__private_extern__)
#define __private_extern__ __declspec(private_extern)
#endif

#ifdef SHLIB
#include "shlib.h"
#endif

#import <string.h>
#import <libc.h>
#import <errno.h>
#import <mach/mach.h>
#import <mach/mach_error.h>
#import <mach/machine/vm_param.h>
#include <unistd.h>
#ifdef _POSIX_THREADS
#include <pthread.h>
#else
#import <mach/cthreads.h>
#endif
#import <mach-o/loader.h>
#import "stuff/bool.h"
#import <mach-o/dyld_debug.h>
#import <mach/machine/thread_status.h>
#ifdef hppa
#import <architecture/hppa/cframe.h>
#endif
#ifndef __OPENSTEP__
#include <mach-o/getsect.h>
#endif

/* this header file is created by mig */
#import "_dyld_debug.h"
#import "debug.h"

/*
 * A struct used to pass the two parameters to the server_loop() thread.
 */
struct server_loop_params {
    mach_port_t subscriber;
    void (*dyld_event_routine)(struct dyld_event event);
};

static void server_loop(
    struct server_loop_params *server_loop_params);

static enum dyld_debug_return get_or_start_debug_thread(
    task_port_t target_task,
    struct _dyld_debug_task_state *state);

static enum dyld_debug_return get_set_dyld_data(
    enum bool get, /* TRUE for get, FALSE for set */
    task_port_t target_task,
    struct dyld_data_section *dyld_data);

static enum dyld_debug_return make_dyld_image_data_writable(
    task_port_t target_task);

static enum dyld_debug_return task_suspend_threads(
    task_port_t target_task, 
    thread_port_array_t *threads,
    unsigned int *count);

static enum dyld_debug_return task_resume_threads(
    thread_port_array_t threads,
    unsigned int count);

static enum dyld_debug_return translate_port(
    task_port_t target_task,
    mach_port_name_t target_port,
    mach_port_t *dest_port);

static enum dyld_debug_return thread_start(
    task_port_t task, 
    void *func,
    long arg,
    vm_offset_t stack_size,
    thread_port_t *thread);

static enum dyld_debug_return thread_setup(
    thread_port_t thread,
    void *func,
    long arg,
    vm_address_t stack_copy,
    vm_address_t stack_base,
    vm_offset_t stack_size);

#ifndef __MACH30__
/*
 * This is set by dyld_debug_MsgError() to let the code know it got a mig error
 * and not set the debug error info. It is not thread safe but at this time
 * MacOS X Server S2 has shipped and this code may never be used again so I
 * won't worry about thread safety.
 */
static int mig_error = 0;

/*
 * For dyld_debug_MsgError() to set the mig error _dyld_debug_error_data needs
 * to be file level static.  Again this is not thread safe but given the
 * interfaces for mig errors and that this may never be used again I won't
 * worry about thread safety.
 */
static struct dyld_debug_error_data _dyld_debug_error_data;

/*
 * dyld_debug_MsgError() is called by userdyld_debug() when there is an error
 * in sending the mach message on the debug_port to the target task.
 */
__private_extern__
void
dyld_debug_MsgError(
msg_return_t msg_result)
{
	SET_MACH_DYLD_DEBUG_ERROR(msg_result, 89);
	/* do nothing with this error, just let the message fail */
	mig_error = 1;
}
#endif

/*
 * _dyld_debug_defining_module() is passed a symbol name and returns which
 * module the symbol name is being or would be used from.  If the symbol does
 * not exist in the target_task this returns DYLD_INVALID_ARGUMENTS.
 */
enum dyld_debug_return
_dyld_debug_defining_module(
task_port_t target_task,
unsigned long send_timeout,
unsigned long rcv_timeout,
boolean_t inconsistent_data_ok,
char *name,
struct dyld_debug_module *module)
{
    enum dyld_debug_return d, p;
    struct _dyld_debug_task_state state;
#ifdef __MACH30__
    kern_return_t k;
#endif

	p = _dyld_debug_make_runnable(target_task, &state);
	if(p != DYLD_SUCCESS)
	    return(DYLD_FAILURE);

#ifdef __MACH30__
	k = user_dyld_debug_defining_module(state.debug_port, rcv_timeout,
		inconsistent_data_ok, name, strlen(name), module, &d);
        if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 88);
            d = DYLD_FAILURE;
	}
#else
	d = user_dyld_debug_defining_module(state.debug_port, send_timeout,
		rcv_timeout, inconsistent_data_ok, name, strlen(name), module);
	if(d != DYLD_SUCCESS)
	    set_dyld_debug_error_data(d,0,0,87,__FILE__,__LINE__);
#endif

	p = _dyld_debug_restore_runnable(target_task, &state);
	if(p != DYLD_SUCCESS)
	    return(DYLD_FAILURE);

	return(d);
}

/*
 * _dyld_debug_is_module_bound() is passed a dyld_debug_module struct and
 * sets bound to TRUE if the module is bound and FALSE otherwise.  If the module
 * does not refer to a valid module DYLD_INVALID_ARGUMENTS is returned.
 */
enum dyld_debug_return
_dyld_debug_is_module_bound(
task_port_t target_task,
unsigned long send_timeout,
unsigned long rcv_timeout,
boolean_t inconsistent_data_ok,
struct dyld_debug_module module,
boolean_t *bound)
{
    enum dyld_debug_return d, p;
    struct _dyld_debug_task_state state;
#ifdef __MACH30__
    kern_return_t k;
#endif

	p = _dyld_debug_make_runnable(target_task, &state);
	if(p != DYLD_SUCCESS)
	    return(DYLD_FAILURE);

#ifdef __MACH30__
	k = user_dyld_debug_is_module_bound(state.debug_port, rcv_timeout,
		inconsistent_data_ok, module, bound, &d);
        if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 86);
            d = DYLD_FAILURE;
	}
	else if(d != DYLD_SUCCESS)
	    SET_LOCAL_DYLD_DEBUG_ERROR(85);
#else
	mig_error = 0;
	d = user_dyld_debug_is_module_bound(state.debug_port, send_timeout,
		rcv_timeout, inconsistent_data_ok, module, bound);
	if(mig_error == 0 && d != DYLD_SUCCESS)
	    set_dyld_debug_error_data(d,0,0,84,__FILE__,__LINE__);
#endif

	p = _dyld_debug_restore_runnable(target_task, &state);
	if(p != DYLD_SUCCESS)
	    return(DYLD_FAILURE);

	return(d);
}

/*
 * _dyld_debug_bind_module() binds the modules specified by dyld_debug_module
 * into the program.  If the module does not refer to a valid module
 * DYLD_INVALID_ARGUMENTS is returned.  This will return DYLD_FAILURE if the
 * another thread has the lock and it can't be released because this is from
 * a core or it is in an error handler.
 */
enum dyld_debug_return
_dyld_debug_bind_module(
task_port_t target_task,
unsigned long send_timeout,
unsigned long rcv_timeout,
boolean_t inconsistent_data_ok,
struct dyld_debug_module module)
{
    enum dyld_debug_return d, p;
    struct _dyld_debug_task_state state;
#ifdef __MACH30__
    kern_return_t k;
#endif

	p = _dyld_debug_make_runnable(target_task, &state);
	if(p != DYLD_SUCCESS)
	    return(DYLD_FAILURE);

#ifdef __MACH30__
	k = user_dyld_debug_bind_module(state.debug_port, rcv_timeout,
		inconsistent_data_ok, module, &d);
        if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 83);
            d = DYLD_FAILURE;
	}
#else
	mig_error = 0;
	d = user_dyld_debug_bind_module(state.debug_port, send_timeout,
		rcv_timeout, inconsistent_data_ok, module);
	if(mig_error == 0 && d != DYLD_SUCCESS)
	    set_dyld_debug_error_data(d,0,0,82,__FILE__,__LINE__);
#endif

	p = _dyld_debug_restore_runnable(target_task, &state);
	if(p != DYLD_SUCCESS)
	    return(DYLD_FAILURE);

	return(d);
}

/*
 * _dyld_debug_module_name() is passed a dyld_debug_module struct and
 * sets image_name and module_name as well as the nameCnts.  If the module
 * does not refer to a valid module DYLD_INVALID_ARGUMENTS is returned.
 */
enum dyld_debug_return
_dyld_debug_module_name(
task_port_t target_task,
unsigned long send_timeout,
unsigned long rcv_timeout,
boolean_t inconsistent_data_ok,
struct dyld_debug_module module,
char **image_name,
unsigned long *image_nameCnt,
char **module_name,
unsigned long *module_nameCnt)
{
    enum dyld_debug_return d, p;
    struct _dyld_debug_task_state state;
#ifdef __MACH30__
    kern_return_t k;
#endif

	p = _dyld_debug_make_runnable(target_task, &state);
	if(p != DYLD_SUCCESS)
	    return(DYLD_FAILURE);

#ifdef __MACH30__
	k = user_dyld_debug_module_name(state.debug_port, rcv_timeout,
		inconsistent_data_ok, module, image_name,
		(unsigned int *)image_nameCnt, module_name,
		(unsigned int *)module_nameCnt, &d);
        if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 81);
            d = DYLD_FAILURE;
	}
	else if(d != DYLD_SUCCESS)
	    SET_LOCAL_DYLD_DEBUG_ERROR(80);
#else
	mig_error = 0;
	d = user_dyld_debug_module_name(state.debug_port, send_timeout,
		rcv_timeout, inconsistent_data_ok, module, image_name,
		(unsigned int *)image_nameCnt, module_name,
		(unsigned int *)module_nameCnt);
	if(mig_error == 0 && d != DYLD_SUCCESS)
	    set_dyld_debug_error_data(d,0,0,79,__FILE__,__LINE__);
#endif

	p = _dyld_debug_restore_runnable(target_task, &state);
	if(p != DYLD_SUCCESS)
	    return(DYLD_FAILURE);

	return(d);
}

#ifdef DYLD_PROFILING
/*
 * _dyld_debug_monoutput() is causes dyld's gmon.out file to be written out.
 */
enum dyld_debug_return
_dyld_debug_monoutput(
task_port_t target_task,
unsigned long send_timeout,
unsigned long rcv_timeout)
{
    enum dyld_debug_return d, p;
    struct _dyld_debug_task_state state;
#ifdef __MACH30__
    kern_return_t k;
#endif

	p = _dyld_debug_make_runnable(target_task, &state);
	if(p != DYLD_SUCCESS)
	    return(DYLD_FAILURE);

#ifdef __MACH30__
	k = user_dyld_debug_monoutput(state.debug_port, rcv_timeout, &d);
        if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 78);
            d = DYLD_FAILURE;
	}
	else if(d != DYLD_SUCCESS)
	    SET_LOCAL_DYLD_DEBUG_ERROR(77);
#else
	mig_error = 0;
	d = user_dyld_debug_monoutput(state.debug_port, send_timeout,
		rcv_timeout);
	if(mig_error == 0 && d != DYLD_SUCCESS)
	    set_dyld_debug_error_data(d,0,0,76,__FILE__,__LINE__);
#endif

	p = _dyld_debug_restore_runnable(target_task, &state);
	if(p != DYLD_SUCCESS)
	    return(DYLD_FAILURE);

	return(d);
}
#endif /* DYLD_PROFILING */


#ifdef _POSIX_THREADS
/*
 * This is the posix thread to set up the per thread data.
 */
static pthread_key_t thread_data_key;
static pthread_once_t once_control = PTHREAD_ONCE_INIT;
static
void
init_routine(void)
{
	/* can't do anything with the error code from this */
	(void)pthread_key_create(&thread_data_key, NULL);
}
#endif

/*
 * _dyld_debug_subscribe_to_events creates a new thread that is will call the
 * specified dyld_event_routine when dynamic link events occur in the target
 * task.  This uses _dyld_debug_add_event_subscriber() and is just a different
 * interface to get events.
 */
enum dyld_debug_return
_dyld_debug_subscribe_to_events(
task_port_t target_task,
unsigned long send_timeout,
unsigned long rcv_timeout,
boolean_t inconsistent_data_ok,
void (*dyld_event_routine)(struct dyld_event event))
{
    kern_return_t k;
#ifdef _POSIX_THREADS
    pthread_t server_loop_thread;
#else
    cthread_t server_loop_thread;
#endif
    mach_port_t subscriber;
    enum dyld_debug_return p;
    struct _dyld_debug_task_state state;
#ifdef __MACH30__
    enum dyld_debug_return d;
#endif
    struct server_loop_params *server_loop_params;

	/*
	 * Allocate a block of memory to pass the two parameters to the
	 * server_loop() routine.  This has to be allocated memory and can't
	 * be on the stack.  This is all needed because pthread_setspecific()
	 * can only set data for it's own thread unlike cthread_set_data().
	 * So that has to be done in the server_loop() routine that only takes
	 * one parameter so we need this parameter block to pass both
	 * parameters.
	 */ 
	server_loop_params = malloc(sizeof(struct server_loop_params));
	if(server_loop_params == NULL){
	    SET_ERRNO_DYLD_DEBUG_ERROR(errno, 75);
	    return(DYLD_FAILURE);
	}

	/*
	 * Allocate a subscriber port for event messages from dyld.
	 */
#ifdef __MACH30__
	if((k = mach_port_allocate(mach_task_self(),
		MACH_PORT_RIGHT_RECEIVE, &subscriber)) != KERN_SUCCESS)
#else
	if((k = port_allocate(mach_task_self(),
			      (int *)&subscriber)) != KERN_SUCCESS)
#endif
	{
	    free(server_loop_params);
	    SET_MACH_DYLD_DEBUG_ERROR(k, 74);
	    return(DYLD_FAILURE);
	}
	server_loop_params->subscriber = subscriber;
	server_loop_params->dyld_event_routine = dyld_event_routine;

	/*
	 * Start a new thread and send it off to the server loop to receive
	 * messages on the subscriber port and call the dyld_event_routine
	 * when a dyld_event message is received.
	 */

#ifdef _POSIX_THREADS
	if(pthread_once(&once_control, init_routine) != 0){
	    SET_ERRNO_DYLD_DEBUG_ERROR(errno, 73);
	    return(DYLD_FAILURE);
	}
	if(pthread_create(&server_loop_thread, NULL,
		          (void *(*)(void *))server_loop,
			  (void *)server_loop_params) != 0){
	    SET_ERRNO_DYLD_DEBUG_ERROR(errno, 72);
	    return(DYLD_FAILURE);
	}
	pthread_detach(server_loop_thread);
#else
	server_loop_thread = cthread_fork((cthread_fn_t)server_loop,
					  (any_t)server_loop_params);
	cthread_detach(server_loop_thread);
#endif

	p = _dyld_debug_make_runnable(target_task, &state);
	if(p != DYLD_SUCCESS)
	    return(DYLD_FAILURE);

#ifdef __MACH30__
	k = user_dyld_debug_add_event_subscriber(state.debug_port, rcv_timeout,
		inconsistent_data_ok, subscriber, &d);
        if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 71);
            d = DYLD_FAILURE;
	}
	else if(d != DYLD_SUCCESS)
	    SET_LOCAL_DYLD_DEBUG_ERROR(70);
#else
	user_dyld_debug_add_event_subscriber(state.debug_port, send_timeout,
		rcv_timeout, inconsistent_data_ok, subscriber);
#endif

	p = _dyld_debug_restore_runnable(target_task, &state);
	if(p != DYLD_SUCCESS)
	    return(DYLD_FAILURE);

#ifdef __MACH30__
	if(d == DYLD_FAILURE)
	    return(DYLD_FAILURE);
#endif
	return(DYLD_SUCCESS);
}

/*
 * _dyld_debug_add_event_subscriber() adds the subscriber to the list of event
 * ports that dyld event messages are sent to.  Then all past events are sent
 * to the subscriber.
 */
enum dyld_debug_return
_dyld_debug_add_event_subscriber(
task_port_t target_task,
unsigned long send_timeout,
unsigned long rcv_timeout,
boolean_t inconsistent_data_ok,
mach_port_t subscriber)
{
    enum dyld_debug_return p;
    struct _dyld_debug_task_state state;
#ifdef __MACH30__
    enum dyld_debug_return d;
    kern_return_t k;
#endif

	p = _dyld_debug_make_runnable(target_task, &state);
	if(p != DYLD_SUCCESS)
	    return(DYLD_FAILURE);

#ifdef __MACH30__
	k = user_dyld_debug_add_event_subscriber(state.debug_port, rcv_timeout,
		inconsistent_data_ok, subscriber, &d);
        if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 69);
            d = DYLD_FAILURE;
	}
	else if(d != DYLD_SUCCESS)
	    SET_LOCAL_DYLD_DEBUG_ERROR(68);
#else
	user_dyld_debug_add_event_subscriber(state.debug_port, send_timeout,
		rcv_timeout, inconsistent_data_ok, subscriber);
#endif

	p = _dyld_debug_restore_runnable(target_task, &state);
	if(p != DYLD_SUCCESS)
	    return(DYLD_FAILURE);

#ifdef __MACH30__
	if(d == DYLD_FAILURE)
	    return(DYLD_FAILURE);
#endif
	return(DYLD_SUCCESS);
}

/*
 * server_loop() is the starting point of the thread created by
 * dyld_debug_subscribe_to_events() and is passed the subscriber port it
 * created to receive event messages.
 */
static
void
server_loop(
struct server_loop_params *server_loop_params)
{
    struct _dyld_event_message_request request;
    struct _dyld_event_message_reply reply;
    kern_return_t r;
    boolean_t b;

#ifdef _POSIX_THREADS
	pthread_setspecific(thread_data_key,
			    server_loop_params->dyld_event_routine);
#else
	cthread_set_data(cthread_self(),
			 (any_t)server_loop_params->dyld_event_routine);
#endif

	/* enter server loop */
	while(TRUE){
	    /* receive an event message */
#ifdef __MACH30__
	    r = mach_msg(
		&request.head,		/* msg */
		MACH_RCV_MSG,		/* option */
		0,			/* send_size */
		sizeof(struct _dyld_event_message_request), /* receive_limit */
		server_loop_params->subscriber,		/* receive_name */
		MACH_MSG_TIMEOUT_NONE,	/* timeout */
		MACH_PORT_NULL);	/* notify */
	    if(r != MACH_MSG_SUCCESS){
		/* ignore errors and wait for another message */
		continue;
	    }
	    /*
	     * Feed into server (_dyld_event_server() is created by mig).
	     */
	    b = _dyld_event_server(&request.head, &reply.head);
#else
	    /*
	     * Set the port the message will be received on to subscriber, and
	     * the maximum size of the message that will be received to the size
	     * of an event message.
	     */
	    request.head.msg_local_port = server_loop_params->subscriber;
	    request.head.msg_size = sizeof(struct _dyld_event_message_request);

	    r = msg_receive(&request.head, MACH_MSG_OPTION_NONE, 0);
	    if(r != RCV_SUCCESS){
		/* ignore errors and wait for another message */
		continue;
	    }
	    /*
	     * Feed into server (_dyld_event_server() is created by mig).
	     */
	    b = _dyld_event_server(&request, &reply);
#endif
	    if(b == FALSE){
		/* ignore errors and wait for another message */
		continue;
	    }

	    /* send the reply */
#ifdef __MACH30__
            /* message is one-way, no reply needed */
#else
	    reply.head.msg_local_port = server_loop_params->subscriber;
	    msg_send(&reply.head, MACH_MSG_OPTION_NONE, 0);
#endif
	}
}

/*
 * _dyld_event_server_callback() is the server side of dyld_event() and is
 * passed the event.  This then retrieves the function pointer to the
 * dyld_event_routine() that was set into the cthread data by
 * dyld_debug_subscribe_to_events() and calls it with the event. 
 * _dyld_event_server_callback() is called indirectly through the mig generated
 * function _dyld_event_server() which is called by the message receive loop in
 * server_loop().
 */
__private_extern__
#ifdef __MACH30__
kern_return_t
#else
void
#endif
_dyld_event_server_callback(
#ifdef __MACH30__
mach_port_t subscriber,
#else
port_t subscriber,
#endif
struct dyld_event event)
{
    void (*dyld_event_routine)(struct dyld_event event);

#ifdef _POSIX_THREADS
	dyld_event_routine = pthread_getspecific(thread_data_key);
#else
	dyld_event_routine = cthread_data(cthread_self());
#endif
	dyld_event_routine(event);

#ifdef __MACH30__
	return(KERN_SUCCESS);
#endif
}

/*
 * _dyld_debug_make_runnable() is called before sending messages to the
 * dynamic link editor.  Basically it assures that the debugging
 * thread is the only runnable thread in the task to receive the
 * message.  It also assures that the debugging thread is indeed
 * runnable if it was suspended.  The function will make sure each 
 * thread in the remote task is suspended and resumed the same number
 * of times, so in the end the suspend count of each individual thread
 * is the same.
 */
enum dyld_debug_return
_dyld_debug_make_runnable(
task_port_t target_task,
struct _dyld_debug_task_state *state)
{
    enum dyld_debug_return d;
    struct thread_basic_info info;
    unsigned int info_count;
    kern_return_t k;
    unsigned int i;

	/*
	 * Call get_or_start_debug_thread() which will set the debug_thread and
	 * debug_port in the _dyld_debug_task_state passed to it.  It will start
	 * the debug thread and allocate the debug port if the task has not
	 * already had one started in it.
	 */
	d = get_or_start_debug_thread(target_task, state);
	if(d != DYLD_SUCCESS)
	    return(d);

	/*
	 * Suspend the target task, so that there is less chance that
	 * something happens while we're preparing the msg_send.
	 */
	k = task_suspend(target_task);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 67);
	    return(DYLD_FAILURE);
	}

	/*
	 * Suspend all threads but debug_thread.  This may actually cause 
	 * control in the target task to block a thread that has the dyld lock,
	 * but was resumed by the debug thread.  That's handled inside dyld.
	 */ 
	k = task_threads(target_task, &state->threads, &state->thread_count);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 66);
	    return(DYLD_FAILURE);
	}
	for(i = 0; i < state->thread_count; i++){
	    if(state->threads[i] != state->debug_thread){
		k = thread_suspend(state->threads[i]);
		if(k != KERN_SUCCESS){
		    SET_MACH_DYLD_DEBUG_ERROR(k, 65);
		    return(DYLD_FAILURE);
		}
	    }
	}

	/* make sure the debug_thread is runnable */
	info_count = THREAD_BASIC_INFO_COUNT;
	k = thread_info(state->debug_thread, THREAD_BASIC_INFO,
			(thread_info_t)&info, &info_count);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 64);
	    return(DYLD_FAILURE);
	}
	state->debug_thread_resume_count = 0;
	for(i = 0; i < (unsigned int)(info.suspend_count); i++){
	    k = thread_resume(state->debug_thread);
	    if(k == KERN_SUCCESS)
		/* the thread has been resumed */
		state->debug_thread_resume_count += 1;
	    else if(k == KERN_FAILURE)
		/* the suspend count is already 0 */
		break;
	    else{
		SET_MACH_DYLD_DEBUG_ERROR(k, 63);
		return(DYLD_FAILURE);
	    }
	}

	/* resume the target task */
	k = task_resume(target_task);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 62);
	    return(DYLD_FAILURE);
	}

	state->task_resume_count = 0;
	while(task_resume(target_task) == KERN_SUCCESS)
	    state->task_resume_count++;

	return(DYLD_SUCCESS);
}  

/*
 * _dyld_debug_restore_runnable() is called after sending messages to the
 * dynamic link editor.  It undoes what _dyld_debug_make_runnable() did to the
 * task and put it back the way it was.
 */
enum dyld_debug_return
_dyld_debug_restore_runnable(
task_port_t target_task,
struct _dyld_debug_task_state *state)
{
    unsigned int i;
    kern_return_t k;

	while(state->task_resume_count--){
	    k = task_suspend(target_task);
	    if(k != KERN_SUCCESS){
		SET_MACH_DYLD_DEBUG_ERROR(k, 61);
		return(DYLD_FAILURE);
	    }
	}

	/* re-suspend the debug thread if we resumed it above */
	for(i = 0; i < state->debug_thread_resume_count; i++){
	    k = thread_suspend(state->debug_thread);
	    if(k != KERN_SUCCESS){
		SET_MACH_DYLD_DEBUG_ERROR(k, 60);
		return(DYLD_FAILURE);
	    }
	}

	/* resume all the same threads we suspended before */
	for(i = 0; i < state->thread_count; i++){
	    if(state->threads[i] != state->debug_thread){
		k = thread_resume(state->threads[i]);
		if(k != KERN_SUCCESS){
		    SET_MACH_DYLD_DEBUG_ERROR(k, 59);
		    return(DYLD_FAILURE);
		}
	    }
	}

	k = vm_deallocate(mach_task_self(), (vm_address_t)state->threads,
			  sizeof(state->threads[0]) * state->thread_count);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 58);
	    return(DYLD_FAILURE);
	}

	return(DYLD_SUCCESS);
}
#ifdef CORE_DEBUG
#ifdef __i386__
void
printf_i386_thread_state_t(
i386_thread_state_t *cpu)
{
	printf(
	   "\t    eax 0x%08x ebx    0x%08x ecx 0x%08x edx 0x%08x\n"
	   "\t    edi 0x%08x esi    0x%08x ebp 0x%08x esp 0x%08x\n"
	   "\t    ss  0x%08x eflags 0x%08x eip 0x%08x cs  0x%08x\n"
	   "\t    ds  0x%08x es     0x%08x fs  0x%08x gs  0x%08x\n",
	    cpu->eax, cpu->ebx, cpu->ecx, cpu->edx, cpu->edi, cpu->esi,
	    cpu->ebp, cpu->esp, cpu->ss, cpu->eflags, cpu->eip, cpu->cs,
	    cpu->ds, cpu->es, cpu->fs, cpu->gs);
}
#endif /* __i386__ */
#ifdef __ppc__
void
printf_ppc_thread_state_t(
ppc_thread_state_t *cpu)
{
	printf("      r0  0x%08x r1  0x%08x r2  0x%08x r3  0x%08x "
	       "r4  0x%08x\n"
	       "      r5  0x%08x r6  0x%08x r7  0x%08x r8  0x%08x "
	       "r9  0x%08x\n"
	       "      r10 0x%08x r11 0x%08x r12 0x%08x r13 0x%08x "
	       "r14 0x%08x\n"
	       "      r15 0x%08x r16 0x%08x r17 0x%08x r18 0x%08x "
	       "r19 0x%08x\n"
	       "      r20 0x%08x r21 0x%08x r22 0x%08x r23 0x%08x "
	       "r24 0x%08x\n"
	       "      r25 0x%08x r26 0x%08x r27 0x%08x r28 0x%08x "
	       "r29 0x%08x\n"
	       "      r30 0x%08x r31 0x%08x lr  0x%08x ctr 0x%08x "
	       "srr0 0x%08x\n",
	       cpu->r0, cpu->r1, cpu->r2, cpu->r3, cpu->r4, cpu->r5,
	       cpu->r6, cpu->r7, cpu->r8, cpu->r9, cpu->r10, cpu->r11,
	       cpu->r12, cpu->r13, cpu->r14, cpu->r15, cpu->r16, cpu->r17,
	       cpu->r18, cpu->r19, cpu->r20, cpu->r21, cpu->r22, cpu->r23,
	       cpu->r24, cpu->r25, cpu->r26, cpu->r27, cpu->r28, cpu->r29,
	       cpu->r30, cpu->r31, cpu->lr, cpu->ctr, cpu->srr0);
	printf("      cr   0x%08x\n", cpu->cr);
	printf("      xer  0x%08x\n", cpu->xer);
	printf("      srr1 0x%08x\n", cpu->srr1);
}
#endif /* __ppc__ */
#ifdef hppa
void
printf_hp_pa_integer_thread_state(
struct hp_pa_integer_thread_state *frame)
{
	printf(
		 "r1  0x%08lx  r2  0x%08lx  r3  0x%08lx  r4  0x%08lx\n"
		 "r5  0x%08lx  r6  0x%08lx  r7  0x%08lx  r8  0x%08lx\n"
		 "r9  0x%08lx  r10 0x%08lx  r11 0x%08lx  r12 0x%08lx\n"
		 "r13 0x%08lx  r14 0x%08lx  r15 0x%08lx  r16 0x%08lx\n"
		 "r17 0x%08lx  r18 0x%08lx  r19 0x%08lx  r20 0x%08lx\n"
		 "r21 0x%08lx  r22 0x%08lx  r23 0x%08lx  r24 0x%08lx\n"
		 "r25 0x%08lx  r26 0x%08lx  r27 0x%08lx  r28 0x%08lx\n"
		 "r29 0x%08lx  r30 0x%08lx  r31 0x%08lx\n"
		 "sr0 0x%08lx  sr1 0x%08lx  sr2 0x%08lx  sar 0x%08lx\n",
	   frame->ts_gr1,  frame->ts_gr2,  frame->ts_gr3,  frame->ts_gr4,
	   frame->ts_gr5,  frame->ts_gr6,  frame->ts_gr7,  frame->ts_gr8,
	   frame->ts_gr9,  frame->ts_gr10, frame->ts_gr11, frame->ts_gr12,
	   frame->ts_gr13, frame->ts_gr14, frame->ts_gr15, frame->ts_gr16,
	   frame->ts_gr17, frame->ts_gr18, frame->ts_gr19, frame->ts_gr20,
	   frame->ts_gr21, frame->ts_gr22, frame->ts_gr23, frame->ts_gr24,
	   frame->ts_gr25, frame->ts_gr26, frame->ts_gr27, frame->ts_gr28,
	   frame->ts_gr29, frame->ts_gr30, frame->ts_gr31,
	   frame->ts_sr0,  frame->ts_sr1,  frame->ts_sr2,  frame->ts_sar);
}
void
print_hp_pa_frame_thread_state(
struct hp_pa_frame_thread_state *frame)
{
	printf("pcsq_front  0x%08lx pcsq_back  0x%08lx\n"
	       "pcoq_front  0x%08lx pcoq_back  0x%08lx\n"
	       "       psw  0x%08lx\n",
	       frame->ts_pcsq_front, frame->ts_pcsq_back,
	       frame->ts_pcoq_front, frame->ts_pcoq_back,
	       frame->ts_psw);
}
#endif /* hppa */
#endif /* CORE_DEBUG */

/*
 * get_or_start_debug_thread() gets or starts the debug thread of the target
 * task.  It fills in the debug_thread and debug_port of the target task state
 * passed to it.
 */
static
enum dyld_debug_return
get_or_start_debug_thread(
task_port_t target_task,
struct _dyld_debug_task_state *state)
{
    enum dyld_debug_return d;
    struct dyld_data_section data;
    thread_port_array_t threads;
    unsigned int count;
    unsigned int count1, trys;
    boolean_t task_resumed;
    thread_port_t debug_thread;
    kern_return_t k;
    struct host_sched_info info;
    enum bool core_task;
    mach_port_t my_mach_host_self;

	state->debug_thread = MACH_PORT_NULL;
	state->debug_port = MACH_PORT_NULL;

	d = make_dyld_image_data_writable(target_task);
	if(d != DYLD_SUCCESS)
	    return(d);

	/*
	 * get the contents of the (__DATA,__dyld) section from the target
	 * task.  If it has none we can't proceed.
	 */
	d = get_dyld_data(target_task, &data);
	if(d != DYLD_SUCCESS)
	    return(d);

	/*
	 * We need to know if this task was recreated from a core file.  If it
	 * has the debug_port and debug_thread of the (__DATA,__dyld) section 
	 * have been set to CORE_DEBUG_PORT and CORE_DEBUG_THREAD to indicate
	 * this to us.
	 */
	if(data.debug_port == CORE_DEBUG_PORT &&
	   data.debug_thread == CORE_DEBUG_THREAD){
	    core_task = TRUE;
	    /*
	     * Now that we know this is a core task we can set the debug_port
	     * and debug_thread to MACH_PORT_NULL to allow the rest of the
	     * code to start the debug thread.
	     */
	    data.debug_port = MACH_PORT_NULL;
	    data.debug_thread = MACH_PORT_NULL;
	    d = set_dyld_data(target_task, &data);
	    if(d != DYLD_SUCCESS)
		return(d);
	}
	else
	    core_task = FALSE;

	/*
	 * If the address of the starting point of the debug has not yet been
	 * set by the dynamic linker the thread can't be started.
	 */
	if(data.start_debug_thread == 0){
#ifdef CORE_DEBUG
	    printf("data.stub_binding_helper_interface 0x%x\n",
		   (unsigned int)data.stub_binding_helper_interface);
	    printf("data._dyld_func_lookup 0x%x\n",
		   (unsigned int)data._dyld_func_lookup);
	    printf("data.start_debug_thread 0x%x\n",
		   (unsigned int)data.start_debug_thread);
	    printf("data.debug_thread %d\n", data.debug_thread);
	    printf("data.debug_port %d\n", data.debug_port);
#ifdef __ppc__
	    printf("data.dyld_stub_binding_helper 0x%x\n",
		   (unsigned int)data.dyld_stub_binding_helper);
#endif
	    printf("data.core_debug %lu\n", data.core_debug);
#endif
	    SET_LOCAL_DYLD_DEBUG_ERROR(57);
	    return(DYLD_FAILURE);
	}

	/*
	 * If there is not already a debug thread in the target task start one.
	 */
	if(data.debug_thread == MACH_PORT_NULL ||
	   data.debug_port == MACH_PORT_NULL){
	    if(core_task == FALSE){
		d = task_suspend_threads(target_task, &threads, &count);
		if(d != DYLD_SUCCESS)
		    return(d);
	    }

	    task_resumed = FALSE;
	    if(task_resume(target_task) == KERN_SUCCESS)
		task_resumed = TRUE;
#ifdef CORE_DEBUG
	    k = task_threads(target_task, &threads, &count);
	    if(k != KERN_SUCCESS)
		printf("task_threads failed (before thread_start())\n");
	    else
		printf("task_threads count = %d (before thread_start())\n",
		       count);
#endif

	    /* start the debugging thread */
	    d = thread_start(target_task,
			     data.start_debug_thread, /* function address */
			     core_task, /* function parameter,start_from_core */
			64 * vm_page_size, /* TODO: will this do? stack size */
			     &debug_thread);
	    if(d != DYLD_SUCCESS)
		return(d);
#ifdef CORE_DEBUG
	    k = task_threads(target_task, &threads, &count);
	    if(k != KERN_SUCCESS)
		printf("task_threads failed %d (%s)\n",
		       k, mach_error_string(k));
	    else{
	        int i;
#ifdef hppa
		struct hp_pa_frame_thread_state frame_state;
		struct hp_pa_integer_thread_state integer_state;
		unsigned int frame_state_count, integer_state_count;
		memset(&frame_state, '\0', sizeof(frame_state));
		memset(&integer_state, '\0', sizeof(integer_state));
#endif /* hppa */
#ifdef __ppc__
		ppc_thread_state_t state;
		unsigned int state_count;
		memset(&state, '\0', sizeof(state));
#endif /* __ppc__ */
#ifdef __i386__
		i386_thread_state_t state;
		unsigned int state_count;
		memset(&state, '\0', sizeof(state));
#endif /* __i386__ */

		printf("task_threads count = %d (after thread_start())\n", 
		       count);

		for(i = 0; i < count; i++){
#ifdef __i386__
		    state_count = i386_THREAD_STATE_COUNT;
		    k = thread_get_state(threads[i],
					 i386_THREAD_STATE, 
					 (thread_state_t)&state, 
					 &state_count);
		    if(k == KERN_SUCCESS){
			printf("thread_get_state worked\n");
			printf_i386_thread_state_t(&state);
		    }
		    else
			printf("thread_get_state failed %d (s)\n", k
			       mach_error_string(k));
#endif /* __i386__ */
#ifdef __ppc__
		    state_count = PPC_THREAD_STATE_COUNT;
		    k = thread_get_state(threads[i],
					 PPC_THREAD_STATE, 
					 (thread_state_t)&state, 
					 &state_count);
		    if(k == KERN_SUCCESS){
			printf("thread_get_state worked\n");
			printf_ppc_thread_state_t(&state);
		    }
		    else
			printf("thread_get_state failed %d (%s)\n", k,
			       mach_error_string(k));
#endif /* __ppc__ */

#ifdef hppa
		    integer_state_count = HPPA_INTEGER_THREAD_STATE_COUNT;
		    k = thread_get_state(threads[i], 
					 HPPA_INTEGER_THREAD_STATE, 
					 (thread_state_t)&integer_state, 
					 &integer_state_count);
		    if(k == KERN_SUCCESS){
			printf("thread_get_state INTEGER worked\n");
			printf_hp_pa_integer_thread_state(&integer_state);
		    }
		    else
			printf("thread_get_state INTEGER failed %d (s)\n", k
			       mach_error_string(k));

		    frame_state_count = HPPA_FRAME_THREAD_STATE_COUNT;
		    k = thread_get_state(threads[i], 
					 HPPA_FRAME_THREAD_STATE, 
					 (thread_state_t)&frame_state, 
					 &frame_state_count);
		    if(k != KERN_SUCCESS){
			printf("thread_get_state FRAME worked\n");
			print_hp_pa_frame_thread_state(&frame_state);
		    }
		    else
			printf("thread_get_state FRAME failed\n");
#endif /* hppa */
		}
	    }
#endif /* CORE_DEBUG */

	    /*
	     * Wait for the debug thread in the target task to be up running
	     * where it has set the debug_thread field in the (__DATA,__dyld)
	     * section of that task.
	     */
	    count1 = HOST_SCHED_INFO_COUNT;
	    my_mach_host_self = mach_host_self();
	    k = host_info(my_mach_host_self, HOST_SCHED_INFO,
			  (host_info_t)(&info), &count1);
	    mach_port_deallocate(mach_task_self(), my_mach_host_self);
	    if(k != KERN_SUCCESS){
		SET_MACH_DYLD_DEBUG_ERROR(k, 56);
		return(DYLD_FAILURE);
	    }
	    trys = 0;
	    do{
	    	k = thread_switch(MACH_PORT_NULL, SWITCH_OPTION_DEPRESS,
				  info.min_timeout * 10000);
		if(k != KERN_SUCCESS){
		    SET_MACH_DYLD_DEBUG_ERROR(k, 55);
		    return(DYLD_FAILURE);
		}

		d = get_dyld_data(target_task, &data);
		if(d != DYLD_SUCCESS)
		    return(d);
		trys++;
	    }while((data.debug_thread == MACH_PORT_NULL ||
	            data.debug_port == MACH_PORT_NULL) &&
		   trys < 100);

	    if(task_resumed){
		k = task_suspend(target_task);
		if(k != KERN_SUCCESS){
		    SET_MACH_DYLD_DEBUG_ERROR(k, 54);
		    return(DYLD_FAILURE);
		}
	    }

	    if(core_task == FALSE){
		d = task_resume_threads(threads, count);
		if(d != DYLD_SUCCESS)
		    return(d);
	    }

	    if(trys >= 100){
#ifdef CORE_DEBUG
		printf("data.stub_binding_helper_interface 0x%x\n",
		       (unsigned int)data.stub_binding_helper_interface);
		printf("data._dyld_func_lookup 0x%x\n",
		       (unsigned int)data._dyld_func_lookup);
		printf("data.start_debug_thread 0x%x\n",
		       (unsigned int)data.start_debug_thread);
		printf("data.debug_thread %d\n", data.debug_thread);
		printf("data.debug_port %d\n", data.debug_port);
#ifdef __ppc__
		printf("data.dyld_stub_binding_helper 0x%x\n",
		       (unsigned int)data.dyld_stub_binding_helper);
#endif
		printf("data.core_debug %lu\n", data.core_debug);
#endif
		SET_LOCAL_DYLD_DEBUG_ERROR(53);
		return(DYLD_FAILURE);
	    }
	}

	/* translate to this task's port space */
	d = translate_port(target_task, data.debug_thread,&state->debug_thread);
	if(d != DYLD_SUCCESS)
	    return(d);

	d = translate_port(target_task, data.debug_port, &state->debug_port);
	if(d != DYLD_SUCCESS)
	    return(d);

	return(DYLD_SUCCESS);
}

/*
 * make_dyld_image_data_writable() makes the data segment in for the dyld image
 * in the target task it is not writable.  The data segment of the dyld image
 * may be read-only if the task is using the DYLD_MEM_PROTECT feature.
 */
static
enum dyld_debug_return
make_dyld_image_data_writable(
task_port_t target_task)
{
    kern_return_t k;
    struct mach_header *mh;
    vm_address_t address;
    vm_size_t size;
#ifdef __MACH30__
    vm_region_basic_info_data_t info;
    mach_msg_type_number_t infoCnt;
#else
    vm_prot_t max_protection, protection;
    vm_inherit_t inheritance;
    boolean_t shared;
    vm_offset_t	offset;
#endif
    vm_prot_t data_seg_initprot;
    mach_port_t objectName;
    pointer_t data;
    unsigned int data_count;
    struct segment_command *sg, *text_seg, *data_seg;
    unsigned long i, mh_address, dyld_vmslide,
		  data_seg_vmaddr, data_seg_vmsize;

	/*
	 * Look through the target task's regions looking for the region that
	 * has the mach header and load commands for the dyld image in the task
	 * This is a guess.  It should be possible to ask the kernel where the
	 * address of the mach header for dyld is in a task
	 *
	 * As it turns out this is a very dangerous thing to try to do because
	 * of the way the VM system works on Mach.  If the target task has some
	 * memory from a memory mapped file which has been removed the vm_region
	 * and the vm_read will succeed.  Then when we read the memory here we
	 * crash.  There is no Mach interface to test if the memory is valid.
	 * So we improve our odds by first looking at the address the dynamic
	 * linker is usually loaded at.
	 */
	mh = NULL;
	mh_address = 0;
	address = 0x8fe00000;
#ifdef __MACH30__
	infoCnt = VM_REGION_BASIC_INFO_COUNT;
	k = vm_region(target_task, &address, &size, VM_REGION_BASIC_INFO,
                      (vm_region_info_t)&info, &infoCnt, &objectName);
#else
	k = vm_region(target_task, &address, &size, &protection,
		      &max_protection, &inheritance, &shared, &objectName,
		      &offset);
#endif
	if(k == KERN_SUCCESS){
	    k = vm_read(target_task, address, size, &data, &data_count);
	    if(k == KERN_SUCCESS){
		if(data_count > sizeof(struct mach_header)){
		    mh = (struct mach_header *)data;
		    mh_address = address;
		    /*
		     * If the magic number is right and the size of this
		     * region is big enough to cover the mach header and
		     * load commands assume it is correct.
		     */
		    if(mh->magic != MH_MAGIC ||
		       mh->filetype != MH_DYLINKER ||
		       data_count < sizeof(struct mach_header) +
				    mh->sizeofcmds){
			mh = NULL;
		    }
		}
		if(mh == NULL){
		    k = vm_deallocate(mach_task_self(), data, data_count);
		    if(k != KERN_SUCCESS){
			SET_MACH_DYLD_DEBUG_ERROR(k, 52);
			return(DYLD_FAILURE);
		    }
		}
		else
		    goto down;
	    }
	}

	mh = NULL;
	mh_address = 0;
	address = VM_MIN_ADDRESS;
	do{
#ifdef __MACH30__
	    infoCnt = VM_REGION_BASIC_INFO_COUNT;
	    k = vm_region(target_task, &address, &size, VM_REGION_BASIC_INFO,
			  (vm_region_info_t)&info, &infoCnt, &objectName);
#else
	    k = vm_region(target_task, &address, &size, &protection,
			  &max_protection, &inheritance, &shared, &objectName,
			  &offset);
#endif
	    if(k == KERN_SUCCESS){
		k = vm_read(target_task, address, size, &data, &data_count);
	        if(k == KERN_SUCCESS){
		    if(data_count > sizeof(struct mach_header)){
			mh = (struct mach_header *)data;
			mh_address = address;
			/*
			 * If the magic number is right and the size of this
			 * region is big enough to cover the mach header and
			 * load commands assume it is correct.
			 */
			if(mh->magic != MH_MAGIC ||
			   mh->filetype != MH_DYLINKER ||
			   data_count < sizeof(struct mach_header) +
					mh->sizeofcmds){
			    mh = NULL;
			}
		    }
		    if(mh == NULL){
			k = vm_deallocate(mach_task_self(), data, data_count);
			if(k != KERN_SUCCESS){
			    SET_MACH_DYLD_DEBUG_ERROR(k, 51);
			    return(DYLD_FAILURE);
			}
		    }
		}
		else if(k != KERN_PROTECTION_FAILURE){
		    SET_MACH_DYLD_DEBUG_ERROR(k, 50);
		    return(DYLD_FAILURE);
		}
		address += size;
	    }
	}while(k != KERN_NO_SPACE && mh == NULL);

down:
	if(mh == NULL){
	    SET_LOCAL_DYLD_DEBUG_ERROR(49);
	    return(DYLD_FAILURE);
	}

	/*
	 * Determine the vmslide of dyld by comparing the address we found
	 * the mach header at and the vmaddr of the text segment.
	 */
	text_seg = NULL;
	sg = (struct segment_command *)
              ((char *)mh + sizeof(struct mach_header));
	for(i = 0; i < mh->ncmds; i++){
	    if(sg->cmd == LC_SEGMENT){
		if(strncmp(sg->segname, "__TEXT", sizeof(sg->segname)) == 0){
                    text_seg = sg;
		    break;
		}
	    }
	    sg = (struct segment_command *)((char *)sg + sg->cmdsize);
	}
	if(text_seg == NULL){
	    SET_LOCAL_DYLD_DEBUG_ERROR(48);
	    return(DYLD_FAILURE);
	}
	dyld_vmslide = mh_address - text_seg->vmaddr;

	/*
	 * The data segment may be read-only if the task is using the
	 * DYLD_MEM_PROTECT feature.  So walk the mach header and save
	 * away the load command for the data segment.
	 */
	data_seg = NULL;
	sg = (struct segment_command *)
              ((char *)mh + sizeof(struct mach_header));
	for(i = 0; i < mh->ncmds; i++){
	    if(sg->cmd == LC_SEGMENT){
		if(strncmp(sg->segname, "__DATA", sizeof(sg->segname)) == 0){
                    data_seg = sg;
		    break;
		}
	    }
	    sg = (struct segment_command *)((char *)sg + sg->cmdsize);
	}
	if(data_seg == NULL){
	    SET_LOCAL_DYLD_DEBUG_ERROR(47);
	    return(DYLD_FAILURE);
	}
	data_seg_vmaddr = data_seg->vmaddr;
	data_seg_vmsize = data_seg->vmsize;
	data_seg_initprot = data_seg->initprot;

	/*
	 * Now that the needed values from the headers are saved get rid of
	 * the memory for the mach header and load commands.
	 */
	k = vm_deallocate(mach_task_self(), data, data_count);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 46);
	    return(DYLD_FAILURE);
	}

	/*
	 * Now deal with the data segment being read-only if the task is using
	 * the DYLD_MEM_PROTECT feature.  Get the region for the data segment
	 * of the dyld image and see if it is writable.  If it not writable
	 * make it writable.
	 */
	address = data_seg_vmaddr + dyld_vmslide;
#ifdef __MACH30__
	infoCnt = VM_REGION_BASIC_INFO_COUNT;
	k = vm_region(target_task, &address, &size, VM_REGION_BASIC_INFO,
                      (vm_region_info_t)&info, &infoCnt, &objectName);
#else
	k = vm_region(target_task, &address, &size, &protection,
		      &max_protection, &inheritance, &shared, &objectName,
		      &offset);
#endif
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 45);
	    return(DYLD_FAILURE);
	}
	if(address != data_seg_vmaddr + dyld_vmslide){
	    SET_LOCAL_DYLD_DEBUG_ERROR(44);
	    return(DYLD_FAILURE);
	}
#ifdef __MACH30__
	if((info.protection & VM_PROT_WRITE) != VM_PROT_WRITE)
#else
	if((protection & VM_PROT_WRITE) != VM_PROT_WRITE)
#endif
	{
	    k = vm_protect(target_task, address, (vm_size_t)data_seg_vmsize,
			   FALSE, data_seg_initprot);
	    if(k != KERN_SUCCESS){
		SET_MACH_DYLD_DEBUG_ERROR(k, 43);
		return(DYLD_FAILURE);
	    }
	}

	return(DYLD_SUCCESS);
}

/*
 * get_dyld_data() gets the contents of the (__DATA,__dyld) section from the
 * target task if it has one.  If it does, the contents are copied into
 * the memory pointed to by data and DYLD_SUCCESS is returned otherwise
 * DYLD_FAILURE is returned.
 */
enum dyld_debug_return
get_dyld_data(
task_port_t target_task,
struct dyld_data_section *dyld_data)
{
	return(get_set_dyld_data(TRUE, target_task, dyld_data));
}

/*
 * set_dyld_data() set the contents of the (__DATA,__dyld) section in the
 * target task if it has one.  If it does, the contents in data are copied
 * into the task and DYLD_SUCCESS is returned otherwise DYLD_FAILURE is
 * returned.
 */
enum dyld_debug_return
set_dyld_data(
task_port_t target_task,
struct dyld_data_section *dyld_data)
{
	return(get_set_dyld_data(FALSE, target_task, dyld_data));
}

static
enum dyld_debug_return
get_set_dyld_data(
enum bool get, /* TRUE for get, FALSE for set */
task_port_t target_task,
struct dyld_data_section *dyld_data)
{
    kern_return_t k;
    struct mach_header *mh;
    vm_address_t address;
    vm_size_t size;
#ifdef __MACH30__
    vm_region_basic_info_data_t info;
    mach_msg_type_number_t infoCnt;
#else
    vm_prot_t protection, max_protection;
    vm_inherit_t inheritance;
    boolean_t shared;
    vm_offset_t	offset;
#endif
    mach_port_t objectName;
    pointer_t data;
    unsigned int data_count;
    const struct section *s;
    unsigned long addr;

	if(get == TRUE)
	    memset(dyld_data, '\0', sizeof(struct dyld_data_section));

	/*
	 * Look through the target task's region looking for the region that
	 * has the mach header and load commands for the executable.  This is
	 * a guess.  It should be possible to ask the kernel where the address
	 * of the mach header for the executable is in a task if it has been
	 * mapped.
	 */
	mh = NULL;
	address = VM_MIN_ADDRESS;
	do{
#ifdef __MACH30__
	    infoCnt = VM_REGION_BASIC_INFO_COUNT;
	    k = vm_region(target_task, &address, &size, VM_REGION_BASIC_INFO,
			  (vm_region_info_t)&info, &infoCnt, &objectName);
#else
	    k = vm_region(target_task, &address, &size, &protection,
			  &max_protection, &inheritance, &shared, &objectName,
			  &offset);
#endif
	    if(k == KERN_SUCCESS){
		k = vm_read(target_task, address, size, &data, &data_count);
	        if(k == KERN_SUCCESS){
		    if(data_count > sizeof(struct mach_header)){
			mh = (struct mach_header *)data;
			/*
			 * If the magic number is right and the size of this
			 * region is big enough to cover the mach header and
			 * load commands assume it is correct.
			 */
			if(mh->magic != MH_MAGIC ||
			   mh->filetype != MH_EXECUTE ||
			   data_count < sizeof(struct mach_header) +
					mh->sizeofcmds){
			    /*
			     * If the magic number and filetype are right
			     * try a vm_read of the expected size as the
			     * headers may be split between regions.
			     */
			    if(mh->magic == MH_MAGIC &&
			       mh->filetype == MH_EXECUTE){
				size = sizeof(struct mach_header) +
				       mh->sizeofcmds;
				size = round_page(size);
				k = vm_deallocate(mach_task_self(),data,
						  data_count);
				if(k != KERN_SUCCESS){
				    SET_MACH_DYLD_DEBUG_ERROR(k, 42);
				    return(DYLD_FAILURE);
				}
				k = vm_read(target_task, address, size,
					    &data, &data_count);
				if(k != KERN_SUCCESS){
				    mh = NULL;
				    data = (pointer_t)NULL;
				    data_count = 0;
				}
				else if(data_count < size)
				    mh = NULL;
				else
				    mh = (struct mach_header *)data;
			    }
			    else{
				mh = NULL;
			    }
			}
		    }
		    if(mh == NULL){
			k = vm_deallocate(mach_task_self(), data, data_count);
			if(k != KERN_SUCCESS){
			    SET_MACH_DYLD_DEBUG_ERROR(k, 41);
			    return(DYLD_FAILURE);
			}
		    }
		}
		else if(k != KERN_PROTECTION_FAILURE){
		    SET_MACH_DYLD_DEBUG_ERROR(k, 40);
		    return(DYLD_FAILURE);
		}
		address += size;
	    }
	    else if(k != KERN_NO_SPACE){
		SET_MACH_DYLD_DEBUG_ERROR(k, 39);
		return(DYLD_FAILURE);
	    }
	}while(k != KERN_NO_SPACE && mh == NULL);

	if(mh == NULL){
	    SET_LOCAL_DYLD_DEBUG_ERROR(38);
	    return(DYLD_FAILURE);
	}

	/*
	 * Walk the mach header looking for the (__DATA,__dyld) section.
	 */
	s = getsectbynamefromheader(mh, "__DATA", "__dyld");
	if(s == NULL){
	    SET_LOCAL_DYLD_DEBUG_ERROR(37);
	    return(DYLD_FAILURE);
	}

	/* if the size of this section is too small return failure. */
	if(s->size < sizeof(struct dyld_data_section)){
	    k = vm_deallocate(mach_task_self(), data, data_count);
	    SET_MACH_DYLD_DEBUG_ERROR(k, 36);
	    return(DYLD_FAILURE);
	}

	/*
	 * Save the address of the (__DATA,__dyld) section and get rid of
	 * the memory for the mach header and load commands.
	 */
	addr = s->addr;
	s = NULL;
	k = vm_deallocate(mach_task_self(), data, data_count);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 35);
	    return(DYLD_FAILURE);
	}

	/* get the region for the (__DATA,__dyld) section */
	address = addr;
#ifdef __MACH30__
	infoCnt = VM_REGION_BASIC_INFO_COUNT;
	k = vm_region(target_task, &address, &size, VM_REGION_BASIC_INFO,
		      (vm_region_info_t)&info, &infoCnt, &objectName);
#else
	k = vm_region(target_task, &address, &size, &protection,
		      &max_protection, &inheritance, &shared, &objectName,
		      &offset);
#endif
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 34);
	    return(DYLD_FAILURE);
	}

	/*
	 * Make sure the address of the region we got is not past the
	 * (__DATA,__dyld) section.
	 */
	if(address > addr + sizeof(struct dyld_data_section)){
	    SET_LOCAL_DYLD_DEBUG_ERROR(33);
	    return(DYLD_FAILURE);
	}

	/* read the region that contains the (__DATA,__dyld) section */
	k = vm_read(target_task, address, size, &data, &data_count);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 32);
	    return(DYLD_FAILURE);
	}

	/* make sure the region is large enough */
	if(size - (addr - address) < sizeof(struct dyld_data_section))
	    return(DYLD_FAILURE);

	if(get == TRUE){
	    /* copy the (__DATA,__dyld) section contents */
	    memcpy(dyld_data, (char *)data + addr - address,
		   sizeof(struct dyld_data_section));
	}
	else{
	    /* copy the (__DATA,__dyld) section contents */
/*
printf("before memcpy dyld_data->stub_binding_helper_interface = 0x%x\n",
dyld_data->stub_binding_helper_interface);
printf("before memcpy dyld_data->debug_thread = 0x%x\n", dyld_data->debug_thread);
*/
	    memcpy((char *)data + addr - address, dyld_data,
		   sizeof(struct dyld_data_section));
/*
printf("vm_write address = 0x%x first word of data 0x%x data_count = 0x%x\n",
address, *((unsigned int *)data), data_count);
*/
	    k = vm_write(target_task, address, data, data_count);
	    if(k != KERN_SUCCESS){
		SET_MACH_DYLD_DEBUG_ERROR(k, 31);
		return(DYLD_FAILURE);
	    }
	}

	/*
	 * Get rid of the memory for the region that contains the 
	 * (__DATA,__dyld) section.
	 */
	k = vm_deallocate(mach_task_self(), data, data_count);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 30);
	    return(DYLD_FAILURE);
	}

#ifdef CORE_DEBUG
	printf("got dyld data from 0x%x\n", (unsigned int)addr);
#endif 
	return(DYLD_SUCCESS);
}

/*
 * task_suspend_threads() does a single thread_supend() on all the threads in
 * the target task.  It also returns indirectly the thread array and count of
 * all the threads in the task.
 */
static
enum dyld_debug_return
task_suspend_threads(
task_port_t target_task, 
thread_port_array_t *threads,
unsigned int *count)
{
    kern_return_t k;
    unsigned int i;

	k = task_threads(target_task, threads, count);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 29);
	    return(DYLD_FAILURE);
	}

	for(i = 0; i < *count; i++){
	    k = thread_suspend((*threads)[i]);
	    if(k != KERN_SUCCESS){
		SET_MACH_DYLD_DEBUG_ERROR(k, 28);
		return(DYLD_FAILURE);
	    }
	}

	return(DYLD_SUCCESS);
}

/*
 * task_resume_threads() does a single thread_resume on all the threads in the
 * thread array of size count.
 */
static
enum dyld_debug_return
task_resume_threads(
thread_port_array_t threads,
unsigned int count)
{
    kern_return_t k;
    unsigned int i;

	for(i = 0; i < count; i++){
	    k = thread_resume((threads)[i]);
	    if(k != KERN_SUCCESS){
		SET_MACH_DYLD_DEBUG_ERROR(k, 27);
		return(DYLD_FAILURE);
	    }
	}

	k = vm_deallocate(mach_task_self(), (vm_address_t)threads,
		 sizeof(threads[0]) * count);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 26);
	    return(DYLD_FAILURE);
	}

	return(DYLD_SUCCESS);
}

/*
 * translate_port() sets dest_port to the port in this task's name space
 * for target_port in target_task when target_port in target_task is valid and
 * returns DYLD_SUCCESS.  Otherwise it sets dest_port is to MACH_PORT_NULL and
 * returns DYLD_FAILURE.
 */
static
enum dyld_debug_return
translate_port(
task_port_t target_task,
mach_port_name_t target_port,
mach_port_t *dest_port)
#ifdef __MACH30__
{
    kern_return_t result;
    mach_port_type_t target_port_type;
    mach_port_t local_port = MACH_PORT_NULL;
    mach_port_type_t local_type;

	*dest_port = MACH_PORT_NULL;

	result = mach_port_type(target_task, target_port, &target_port_type);
	if(result != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(result, 25);
	    return(DYLD_FAILURE);
	}

	if((target_port_type & MACH_PORT_TYPE_RECEIVE) != 0)
	    result = mach_port_extract_right(target_task, target_port,
			MACH_MSG_TYPE_MAKE_SEND, &local_port, &local_type);
	else if((target_port_type & MACH_PORT_TYPE_SEND) != 0)
	    result = mach_port_extract_right(target_task, target_port,
			MACH_MSG_TYPE_COPY_SEND, &local_port, &local_type);

	if(result != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(result, 24);
	    return(DYLD_FAILURE);
	}

	*dest_port = local_port;
	return(DYLD_SUCCESS);
}
#else
{
    mach_port_name_t *names; /* should be mach_port_name_array_t */
    mach_port_type_array_t types;
    unsigned int i, namesCount, typesCount;
    kern_return_t k;

    *dest_port = MACH_PORT_NULL;

    k = port_names(target_task, &names, &namesCount, &types, &typesCount);
    if(k != KERN_SUCCESS){
	SET_MACH_DYLD_DEBUG_ERROR(k, 23);
        return(DYLD_FAILURE);
    }

    for(i = 0; i < namesCount; i++){
        if(names[i] == target_port){
            if(types[i] == PORT_TYPE_RECEIVE_OWN){
                k = port_extract_receive(target_task, names[i],
                                         (int *)dest_port);
                if(k != KERN_SUCCESS){
		    SET_MACH_DYLD_DEBUG_ERROR(k, 22);
                    return(DYLD_FAILURE);
		}
                k = port_insert_receive(target_task, *dest_port, names[i]);
                if(k != KERN_SUCCESS){
		    SET_MACH_DYLD_DEBUG_ERROR(k, 21);
                    return(DYLD_FAILURE);
		}
                break;
            }
            else if(types[i] == PORT_TYPE_SEND){
                k = port_extract_send(target_task, names[i],
                                      (int *)dest_port);
                if(k != KERN_SUCCESS){
		    SET_MACH_DYLD_DEBUG_ERROR(k, 20);
                    return(DYLD_FAILURE);
		}
                k = port_insert_send(target_task, *dest_port, names[i]);
                if(k != KERN_SUCCESS){
		    SET_MACH_DYLD_DEBUG_ERROR(k, 19);
                    return(DYLD_FAILURE);
		}
                break;
            }
        }
    }

    k = vm_deallocate(mach_task_self(), (vm_address_t)names,
                      sizeof(*names) * namesCount);
    if(k != KERN_SUCCESS){
	SET_MACH_DYLD_DEBUG_ERROR(k, 18);
        return(DYLD_FAILURE);
    }
    k = vm_deallocate(mach_task_self(), (vm_address_t)types,
                      sizeof(*types) * typesCount);
    if(k != KERN_SUCCESS){
	SET_MACH_DYLD_DEBUG_ERROR(k, 17);
        return(DYLD_FAILURE);
    }

    return(DYLD_SUCCESS);
}
#endif /* __MACH30__ */

/*
 * thread_start() starts a thread in the specified task at the function passing
 * the arg using stack_size as it's stack size.  It returns the created thread
 * indirectly through thread.
 */
static
enum dyld_debug_return
thread_start(
task_port_t task, 
void *func,
long arg,
vm_offset_t stack_size,
thread_port_t *thread)
{
    vm_address_t stack_base, stack_copy;
    kern_return_t k;
    enum dyld_debug_return d;

	/* create remote stack */
	k = vm_allocate(task, &stack_base, stack_size, TRUE);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 16);
	    return(DYLD_FAILURE);
	}

	/* create local stack */
	k = vm_allocate(mach_task_self(), &stack_copy, stack_size, TRUE);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 15);
	    return(DYLD_FAILURE);
	}

	/* create a thread */
	k = thread_create(task, thread);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 14);
	    return(DYLD_FAILURE);
	}

	/* setup the thread */
	d = thread_setup(*thread, func, arg, stack_copy, stack_base,stack_size);
	if(d != DYLD_SUCCESS)
	    return(d);

	/* copy stack to remote task */
	k = vm_write(task, stack_base, stack_copy, stack_size);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 13);
	    return(DYLD_FAILURE);
	}

	/* release local copy of stack */
	k = vm_deallocate(mach_task_self(), stack_copy, stack_size);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 12);
	    return(DYLD_FAILURE);
	}

	/* really start the remote thread */
	k = thread_resume(*thread);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 11);
	    return(DYLD_FAILURE);
	}

	return(DYLD_SUCCESS);
}

/*
 * thread_setup() sets up the machine specific thread and stack for the func
 * with the arg.
 */
#ifdef __i386__
static
enum dyld_debug_return
thread_setup(
thread_port_t thread,
void *func,
long arg,
vm_address_t stack_copy,
vm_address_t stack_base,
vm_offset_t stack_size)
{
    long *ltop, *rtop;
    i386_thread_state_t state, *ts;
    unsigned int state_count;
    kern_return_t k;

	/*
	 * On the i386 stack grows down, so it starts out at:
	 * stack_base + stack top.
	 */
	ltop = (long *)((char *)stack_copy + stack_size);
	rtop = (long *)((char *)stack_base + stack_size);

	/* get the current state of the thread */
	ts = &state;
	state_count = i386_THREAD_STATE_COUNT;
	k = thread_get_state(thread,
			     i386_THREAD_STATE, 
			     (thread_state_t)&state, 
			     &state_count);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 10);
	    return(DYLD_FAILURE);
	}

	/* setup the instruction pointer */
	ts->eip = (long)func;

	/* push the argument on the stack */
	*--ltop = (long)arg;
	rtop--;

	/*
	 * The return address is set to 0, so we get a segfault
	 * if the thread tries to return.
	 */
	*--ltop = 0;
	rtop--;

	/* setup the stack pointer */
	ts->esp = (long)rtop;

	/* set the state of the thread */
	k = thread_set_state(thread, 
			     i386_THREAD_STATE, 
			     (thread_state_t)&state, 
			     state_count);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 9);
	    return(DYLD_FAILURE);
	}
#ifdef CORE_DEBUG
	printf_i386_thread_state_t(&state);
#endif

	return(DYLD_SUCCESS);
}
#endif /* i386 */

#ifdef m68k
static
enum dyld_debug_return
thread_setup(
thread_port_t thread,
void *func,
long arg,
vm_address_t stack_copy,
vm_address_t stack_base,
vm_offset_t stack_size)
{
    long *ltop, *rtop;
    struct m68k_thread_state_regs state, *ts;
    unsigned int state_count;
    kern_return_t k;

	/*
	 * On the m68k stack grows down, so it starts out at:
	 * stack_base + stack top.
	 */
	ltop = (long *)((char *)stack_copy + stack_size);
	rtop = (long *)((char *)stack_base + stack_size);

	/* get the current state of the thread */
	ts = &state;
	state_count = M68K_THREAD_STATE_REGS_COUNT;
	k = thread_get_state(thread,
			     M68K_THREAD_STATE_REGS, 
			     (thread_state_t)&state, 
			     &state_count);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 8);
	    return(DYLD_FAILURE);
	}

	/* setup the instruction pointer */
	ts->pc = (long)func;

	/* push the argument on the stack */
	*--ltop = (long)arg;
	rtop--;

	/*
	 * The return address is set to 0, so we get a segfault
	 * if the thread tries to return.
	 */
	*--ltop = 0;
	rtop--;

	/* setup the frame and stack pointer */
	ts->areg[6] = (long) 0;
	ts->areg[7] = (long) rtop;

	/* set the state of the thread */
	k = thread_set_state(thread, 
			     M68K_THREAD_STATE_REGS, 
			     (thread_state_t)&state, 
			     state_count);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 7);
	    return(DYLD_FAILURE);
	}

	return(DYLD_SUCCESS);
}
#endif /* m68k */

#ifdef hppa
static
enum dyld_debug_return
thread_setup(
thread_port_t thread,
void *func,
long arg,
vm_address_t stack_copy,
vm_address_t stack_base,
vm_offset_t stack_size)
{
    long rtop;
    struct hp_pa_frame_thread_state frame_state;
    struct hp_pa_integer_thread_state integer_state;
    unsigned int frame_state_count, integer_state_count;
    kern_return_t k;

	/* on hppa the stack grows op, so it starts out at stack_base */
	rtop = (long)stack_base;

	/* get the current state of the thread */
	/* TODO: is this right?  is a thread_get_state() call needed? */
	memset(&frame_state, 0, sizeof (frame_state));

	/* setup the instruction pointer */
	frame_state.ts_pcoq_front = (long) func;
	frame_state.ts_pcoq_back = frame_state.ts_pcoq_front + 4;

	/* store the instruction pointer back into the thread */
	frame_state_count = HPPA_FRAME_THREAD_STATE_COUNT;
	k = thread_set_state(thread, 
			     HPPA_FRAME_THREAD_STATE, 
			     (thread_state_t)&frame_state, 
			     frame_state_count);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 6);
	    return(DYLD_FAILURE);
	}

	/* get the general purpose registers */
	/* TODO: is this right?  is a thread_get_state() call needed? */
	memset(&integer_state, 0, sizeof (integer_state));

	/* put argument in first argument register */
	integer_state.ts_gr26 = (unsigned long)arg;

	/*
	 * The return address is set to 0, so we get a segfault
	 * if the thread tries to return.
	 */
	integer_state.ts_gr2 = 0;

	/* set stack pointer so that there is room for frame */
	rtop += C_ARGSAVE_LEN;
	integer_state.ts_gr30 = (rtop + C_STACK_ALIGN-1) & ~(C_STACK_ALIGN - 1);

	/* set the general regs of the thread */
	integer_state_count = HPPA_INTEGER_THREAD_STATE_COUNT;
	k = thread_set_state (thread, 
			      HPPA_INTEGER_THREAD_STATE, 
			      (thread_state_t)&integer_state, 
			      integer_state_count);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 5);
	    return(DYLD_FAILURE);
	}

#ifdef CORE_DEBUG
	printf_hp_pa_integer_thread_state(&integer_state);
	print_hp_pa_frame_thread_state(&frame_state);
#endif
	return(DYLD_SUCCESS);
}
#endif /* hppa */

/*
 * thread_setup() sets up the machine specific thread and stack for the func
 * with the arg.
 */
#ifdef sparc
static
enum dyld_debug_return
thread_setup(
thread_port_t thread,
void *func,
long arg,
vm_address_t stack_copy,
vm_address_t stack_base,
vm_offset_t stack_size)
{
    long *ltop, *rtop;
    struct sparc_thread_state_regs state, *ts;
    unsigned int state_count;
    kern_return_t k;

	/*
	 * On the sparc stack grows down, so it starts out at:
	 * stack_base + stack top.
	 */
	ltop = (long *)((char *)stack_copy + stack_size);
	rtop = (long *)((char *)stack_base + stack_size);

	/*
	 * 64 bytes is need above the stack pointer to execute the "save"
	 * instruction of the function the pc will start in.
	 */
	ltop -= 64;
	rtop -= 64;

	/* get the current state of the thread */
	ts = &state;
	state_count = SPARC_THREAD_STATE_REGS_COUNT;
	k = thread_get_state(thread,
			     SPARC_THREAD_STATE_REGS, 
			     (thread_state_t)&state, 
			     &state_count);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 4);
	    return(DYLD_FAILURE);
	}

	/* setup the instruction pointer */
	ts->regs.r_pc  = (int)func;
        ts->regs.r_npc = ts->regs.r_pc + 4;

	/* push the argument on the stack */
        ts->regs.r_o0 = (int)arg;

	/*
	 * The return address is set to 0, so we get a segfault
	 * if the thread tries to return.
	 */
        ts->regs.r_o7 = (int) 0;
	/* setup the frame and stack pointer */
	ts->regs.r_o6 = (int) rtop;

	/* set the state of the thread */
	k = thread_set_state(thread, 
			     SPARC_THREAD_STATE_REGS, 
			     (thread_state_t)&state, 
			     state_count);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 3);
	    return(DYLD_FAILURE);
	}

	return(DYLD_SUCCESS);
}
#endif /* sparc */

#ifdef __ppc__
static
enum dyld_debug_return
thread_setup(
thread_port_t thread,
void *func,
long arg,
vm_address_t stack_copy,
vm_address_t stack_base,
vm_offset_t stack_size)
{
    long *ltop, *rtop;
    ppc_thread_state_t state, *ts;
    unsigned int state_count;
    kern_return_t k;

	/*
	 * On the ppc stack grows down, so it starts out at:
	 * stack_base + stack top.
	 */
	ltop = (long *)((char *)stack_copy + stack_size);
	rtop = (long *)((char *)stack_base + stack_size);

	/* get the current state of the thread */
	ts = &state;
	state_count = PPC_THREAD_STATE_COUNT;
	k = thread_get_state(thread,
			     PPC_THREAD_STATE, 
			     (thread_state_t)&state, 
			     &state_count);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 2);
	    return(DYLD_FAILURE);
	}

#ifdef CORE_DEBUG
	state.r0 = 0;

	state.r2 = 2;

	state.r4 = 4;
	state.r5 = 5;
	state.r6 = 6;
	state.r7 = 7;
	state.r8 = 8;
	state.r9 = 9;

	state.r10 = 10;
	state.r11 = 11;
	state.r12 = 12;
	state.r13 = 13;
	state.r14 = 14;
	state.r15 = 15;
	state.r16 = 16;
	state.r17 = 17;
	state.r18 = 18;
	state.r19 = 19;

	state.r20 = 20;
	state.r21 = 21;
	state.r22 = 22;
	state.r23 = 23;
	state.r24 = 24;
	state.r25 = 25;
	state.r26 = 26;
	state.r27 = 27;
	state.r28 = 28;
	state.r29 = 29;

	state.r30 = 30;
	state.r31 = 31;
	state.ctr = 0;
	state.xer = 0;
	state.srr1 = 0;
#endif

	/* setup the instruction pointer */
	state.srr0 = (long)func;

	/* put argument in first argument register */
	state.r3 = (unsigned long)arg;

	/*
	 * The return address is set to 0, so we get a segfault
	 * if the thread tries to return.
	 */
	state.lr = 0;

	/* set stack pointer so that there is room for frame */
	rtop -= 64;
	state.r1 = (unsigned int)rtop;

	/* set the state of the thread */
	k = thread_set_state(thread, 
			     PPC_THREAD_STATE, 
			     (thread_state_t)&state, 
			     state_count);
	if(k != KERN_SUCCESS){
	    SET_MACH_DYLD_DEBUG_ERROR(k, 1);
	    return(DYLD_FAILURE);
	}

#ifdef CORE_DEBUG
	printf_ppc_thread_state_t(&state);
#endif

	return(DYLD_SUCCESS);
}
#endif /* __ppc__ */

/*
 * When a error occurs in the dyld debug API's the user registered function
 * is called if set.  This is the pointer to that user registered function.
 */
static void (*user_error_func)(struct dyld_debug_error_data *e) = NULL;

/*
 * set_dyld_debug_error_func() is called to register a function to be called
 * when error occurs in the dyld debug API's.
 */
void
_dyld_debug_set_error_func(
void (*func)(struct dyld_debug_error_data *e))
{
	user_error_func = func;
}

/*
 * set_dyld_debug_error_data() is called to set the values into the global
 * _dyld_debug_error_data structure.  It fills in the error values into the
 * global struct and then if the user registered function to be called it
 * calls that with a pointer to the global struct.
 */
void
set_dyld_debug_error_data(
enum dyld_debug_return dyld_debug_return,
kern_return_t mach_error,
int dyld_debug_errno,
unsigned long local_error,
char *file_name,
unsigned long line_number)
{
#ifdef __MACH30__
    /*
     * See the comments above near the mig error routine dyld_debug_MsgError()
     * as to why this is a file level static for pre-Mach 3.0 systems.
     */
    struct dyld_debug_error_data _dyld_debug_error_data;
#endif

	_dyld_debug_error_data.dyld_debug_return = dyld_debug_return;
	_dyld_debug_error_data.mach_error = mach_error;
	_dyld_debug_error_data.dyld_debug_errno = dyld_debug_errno;
	_dyld_debug_error_data.local_error = local_error;
	_dyld_debug_error_data.file_name = file_name;
	_dyld_debug_error_data.line_number = line_number;
	if(user_error_func != NULL)
	    user_error_func(&_dyld_debug_error_data);
}
