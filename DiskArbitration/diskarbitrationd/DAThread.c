/*
 * Copyright (c) 1998-2014 Apple Inc. All rights reserved.
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

#include "DAThread.h"
#include "DAServer.h"

#include <pthread.h>
#include <sysexits.h>
#include <mach/mach.h>

enum
{
    __kDAThreadMachChannelJobKindExecute = 0x00000001
};

typedef UInt32 __DAThreadMachChannelJobKind;

struct __DAThreadMachChannelJob
{
    __DAThreadMachChannelJobKind      kind;
    struct __DAThreadMachChannelJob * next;

    union
    {
        struct
        {
            Boolean                 exited;
            int                     status;
            pthread_t               thread;
            DAThreadExecuteCallback callback;
            void *                  callbackContext;
            DAThreadFunction        function;
            void *                  functionContext;
        } execute;
    };
};

typedef struct __DAThreadMachChannelJob __DAThreadMachChannelJob;

static __DAThreadMachChannelJob * __gDAThreadMachChannelJobs = NULL;
static pthread_mutex_t              __gDAThreadMachChannelLock = PTHREAD_MUTEX_INITIALIZER;
static mach_port_t                __gDAThreadMachChannelPort = NULL;
static dispatch_mach_t            __gDAThreadChannel = NULL;

static void * __DAThreadFunction( void * context )
{
    /*
     * Run a thread.
     */

    __DAThreadMachChannelJob * job;

    pthread_mutex_lock( &__gDAThreadMachChannelLock );

    for ( job = __gDAThreadMachChannelJobs; job; job = job->next )
    {
        assert( job->kind == __kDAThreadMachChannelJobKindExecute );

        if ( pthread_equal( job->execute.thread, pthread_self( ) ) )
        {
            break;
        }
    }

    pthread_mutex_unlock( &__gDAThreadMachChannelLock );

    if ( job )
    {
        mach_msg_header_t message;
        kern_return_t     status;

        job->execute.status = ( ( DAThreadFunction ) job->execute.function )( job->execute.functionContext );

        pthread_mutex_lock( &__gDAThreadMachChannelLock );

        job->execute.exited = TRUE;

        pthread_mutex_unlock( &__gDAThreadMachChannelLock );

        message.msgh_bits        = MACH_MSGH_BITS( MACH_MSG_TYPE_COPY_SEND, 0 );
        message.msgh_id          = 0;
        message.msgh_local_port  = MACH_PORT_NULL;
        message.msgh_remote_port =  __gDAThreadMachChannelPort ;
        message.msgh_reserved    = 0;
        message.msgh_size        = sizeof( message );

        dispatch_mach_msg_t m = dispatch_mach_msg_create(&message,
                    message.msgh_size, DISPATCH_MACH_MSG_DESTRUCTOR_DEFAULT,
                    NULL);
        dispatch_mach_send(__gDAThreadChannel, m, 0);
        dispatch_release(m);
    }

    pthread_detach( pthread_self( ) );

    return NULL;
}

static void __DAThreadMachChannelHandler ( void *context, dispatch_mach_reason_t reason,
                                          dispatch_mach_msg_t msg, mach_error_t error )
{
    /*
     * Process a DAThread CFMachChannel fire.
     */

    if (reason == DISPATCH_MACH_MESSAGE_RECEIVED)
    {
        mach_msg_header_t   *header = dispatch_mach_msg_get_msg(msg, NULL);
        
        __DAThreadMachChannelJob * job     = NULL;
        __DAThreadMachChannelJob * jobLast = NULL;

        pthread_mutex_lock( &__gDAThreadMachChannelLock );

        /*
         * Scan through job list.
         */

        for ( job = __gDAThreadMachChannelJobs; job; jobLast = NULL )
        {
            for ( job = __gDAThreadMachChannelJobs; job; jobLast = job, job = job->next )
            {
                assert( job->kind == __kDAThreadMachChannelJobKindExecute );

                if ( job->execute.exited )
                {
                    /*
                     * Process the job's callback.
                     */

                    if ( jobLast )
                    
                    {
                        jobLast->next = job->next;
                    }
                    else
                    {
                        __gDAThreadMachChannelJobs = job->next;
                    }

                    pthread_mutex_unlock( &__gDAThreadMachChannelLock );

                    /*
                     * Issue the callback.
                     */

                    if ( job->execute.callback )
                    {
                        ( job->execute.callback )( job->execute.status, job->execute.callbackContext );
                    }

                    /*
                     * Release our resources.
                     */

                    free( job );

                    pthread_mutex_lock( &__gDAThreadMachChannelLock );

                    break;
                }
            }
        }

        pthread_mutex_unlock( &__gDAThreadMachChannelLock );
        mach_msg_destroy( header );
    }
}

dispatch_mach_t DAThreadCreateMachChannel( void )
{
    /*
     * Initialize our minimal state.
     */

    if ( __gDAThreadMachChannelPort == 0 )
    {
        /*
         * Create the global CFMachPort.  It will be used to post jobs to the run loop.
         */

        kern_return_t status;

        status = mach_port_allocate( mach_task_self( ), MACH_PORT_RIGHT_RECEIVE, &__gDAThreadMachChannelPort );

        if ( status == KERN_SUCCESS )
        {
            status = mach_port_insert_right(mach_task_self(), __gDAThreadMachChannelPort, __gDAThreadMachChannelPort, MACH_MSG_TYPE_MAKE_SEND);
            /*
             * Set up the global CFMachPort.  It requires no more than one queue element.
             */

            if ( status == KERN_SUCCESS )
            {
                mach_port_limits_t limits = { 0 };

                limits.mpl_qlimit = 1;

                mach_port_set_attributes( mach_task_self( ),
                                       __gDAThreadMachChannelPort ,
                                      MACH_PORT_LIMITS_INFO,
                                      ( mach_port_info_t ) &limits,
                                      MACH_PORT_LIMITS_INFO_COUNT );
            }
        }
    }

    /*
     * Obtain the CFMachChannel for our CFMachPort.
     */

    if ( __gDAThreadMachChannelPort )
    {
        __gDAThreadChannel = dispatch_mach_create_f("diskarbitrationd/thread",
                                                DAServerWorkLoop(),
                                                NULL,
                                               __DAThreadMachChannelHandler);
        dispatch_mach_connect( __gDAThreadChannel, __gDAThreadMachChannelPort, __gDAThreadMachChannelPort, NULL);
    }

    return __gDAThreadChannel;
}

void DAThreadExecute( DAThreadFunction function, void * functionContext, DAThreadExecuteCallback callback, void * callbackContext )
{
    /*
     * Execute a thread.
     */

    pthread_t thread;
    int       status;

    /*
     * State our assumptions.
     */

    assert( __gDAThreadMachChannelPort );

    /*
     * Run the thread.
     */

    pthread_mutex_lock( &__gDAThreadMachChannelLock );

    status = pthread_create( &thread, NULL, __DAThreadFunction, NULL );

    if ( status == 0 )
    {
        /*
         * Register this callback job on our queue.
         */

        __DAThreadMachChannelJob * job;

        job = malloc( sizeof( __DAThreadMachChannelJob ) );

        if ( job )
        {
            job->kind = __kDAThreadMachChannelJobKindExecute;
            job->next = __gDAThreadMachChannelJobs;

            job->execute.exited          = FALSE;
            job->execute.status          = 0;
            job->execute.thread          = thread;
            job->execute.callback        = callback;
            job->execute.callbackContext = callbackContext;
            job->execute.function        = function;
            job->execute.functionContext = functionContext;

            __gDAThreadMachChannelJobs = job;
        }
    }

    pthread_mutex_unlock( &__gDAThreadMachChannelLock );

    /*
     * Complete the call in case we had a local failure.
     */

    if ( status )
    {
        if ( callback )
        {
            ( callback )( EX_OSERR, callbackContext );
        }
    }
}
