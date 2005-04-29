/*
 * 
 * (c) Copyright 1991 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1991 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1991 DIGITAL EQUIPMENT CORPORATION
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 *                 permission to use, copy, modify, and distribute this
 * file for any purpose is hereby granted without fee, provided that
 * the above copyright notices and this notice appears in all source
 * code copies, and that none of the names of Open Software
 * Foundation, Inc., Hewlett-Packard Company, or Digital Equipment
 * Corporation be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Neither Open Software Foundation, Inc., Hewlett-
 * Packard Company, nor Digital Equipment Corporation makes any
 * representations about the suitability of this software for any
 * purpose.
 * 
 */
/*
 */
/*
**
**  NAME:
**
**      exception_handling.c
**
**  FACILITY:
**
**      Exceptions
**
**  ABSTRACT:
**
**  Pthread based exception package support routines.
** 
**  These support routines help to define a TRY/CATCH exception mechanism
**  that runs 'on top of' 1003.1 pthreads.
** 
**  The following implementation models all exceptions by converting them
**  into pthread "cancel"'s.  The package counts on the pthread cancel
**  mechanism to maintain the cancel cleanup handler chain; it piggyback
**  on the cancel cleanup handler list at the expense of a slightly more
**  expensive RAISE().
** 
**  The exception currently being processed is recorded in per thread
**  data which is set by the excpetion handler package.
** 
**  Exception handlers execute with general cancellability disabled.
** 
**  Arbitrary application pthread_cancel's that are not part of a TRY/CATCH
**  scoped macro will unwind to the mort recent TRY/CATCH exception handler.
**  That is, if an application nests a pthread_cleanup_push/pop() macro
**  set inside a TRY/CATCH macro set the pthread cleanup macros outside
**  the exception package scope are represented as a "cancel" exception.
** 
*/

#include <pthread.h>
#include <dce/exc_handling.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

EXCEPTION exc_uninitexc_e;
EXCEPTION exc_exquota_e;
EXCEPTION exc_insfmem_e;
EXCEPTION exc_nopriv_e;
EXCEPTION exc_illaddr_e;
EXCEPTION exc_illinstr_e;
EXCEPTION exc_resaddr_e;
EXCEPTION exc_privinst_e;
EXCEPTION exc_resoper_e;
EXCEPTION exc_aritherr_e;
EXCEPTION exc_intovf_e;
EXCEPTION exc_intdiv_e;
EXCEPTION exc_fltovf_e;
EXCEPTION exc_fltdiv_e;
EXCEPTION exc_fltund_e;
EXCEPTION exc_decovf_e;
EXCEPTION exc_subrng_e;
EXCEPTION exc_excpu_e;
EXCEPTION exc_exfilsiz_e;
EXCEPTION exc_SIGTRAP_e;
EXCEPTION exc_SIGIOT_e;
#ifdef SIGEMT
EXCEPTION exc_SIGEMT_e;
#endif
#ifdef SIGSYS
EXCEPTION exc_SIGSYS_e;
#endif
EXCEPTION exc_SIGPIPE_e;
EXCEPTION exc_unksyncsig_e;

EXCEPTION pthread_cancel_e;

pthread_key_t _exc_key;
      
/* -------------------------------------------------------------------- */
#ifdef apollo
/*
 * For apollos, tell the compiler to place all static data, declared
 * within the scope of a function, in a section named exc_pure_data$.
 * This section will be loaded as a R/O, shared, initialized data section.
 * All other data, global or statics at the file scope, will be loaded
 * as R/W, per-process, and zero-filled.
 */

#pragma HP_SECTION( , exc_pure_data$)
#endif
/* -------------------------------------------------------------------- */
/*
 * Make sure NULL is defined for init_once_block
 */
#ifndef NULL
#define NULL 0
#endif

static pthread_once_t init_once_block = PTHREAD_ONCE_INIT;
/* -------------------------------------------------------------------- */

/*
 * We're client-only, and if we get a SIGSEGV or SIGBUS or something
 * such as that, we just want to crash, we don't want to have to have
 * exception handlers to catch that.
 */
#if 0
static void sync_signal_handler
    (
        int signal
    );
static void setup_sync_signal_handlers
    (
        void
    );
#endif

static void destroy_exc
    (
        void *_exc_cur
    );
    
static void init_once
    (
        void
    );

/* -------------------------------------------------------------------- */


#if 0
/*
 * S Y N C _ S I G N A L _ H A N D L E R
 *
 * This synchronous signal handler is already running on the correct
 * thread stack. A RAISE is all that's required to initiate the unwind.
 * 
 * Opon receipt of a synchronous signal, a pthread_cancel() is posted
 * to the thread in which the synchronous signal occurred cancel delivery
 * will be forced.  The cancel initiates a cleanup handler in the context
 * of the synchronous signal handler.  The cleanup handler then does
 * a _exc_longjmp_np from the context of the signal handler back to
 * the most recent exception handler.
 * 
 * NOTE:
 *
 * It is assumed that it is okay to do a RAISE from a SYNCHRONOUS signal
 * handler without any ill effects.  While RAISE is doing a couple of
 * pthread operations, the fact that these whould only performed in the
 * thread that had the fault due to a synchronous fault in the thread
 * (i.e. we were in user code, not the pthread library when the fault
 * occurred) should mean that there are no pthread re-entrency problems.
 */
static void sync_signal_handler(int signal)
{
    EXCEPTION *exc;

    switch (signal) {
        case SIGILL:    exc = &exc_illinstr_e;      break;
        case SIGTRAP:   exc = &exc_SIGTRAP_e;       break;
        case SIGIOT:    exc = &exc_SIGIOT_e;        break;
#ifdef SIGEMT
        case SIGEMT:    exc = &exc_SIGEMT_e;        break;
#endif        
        case SIGFPE:    exc = &exc_aritherr_e;      break;
        case SIGBUS:    exc = &exc_illaddr_e;       break;
        case SIGSEGV:   exc = &exc_illaddr_e;       break;
#ifdef SIGSYS
        case SIGSYS:    exc = &exc_SIGSYS_e;        break;
#endif        
        case SIGPIPE:   exc = &exc_SIGPIPE_e;       break;
        default:        exc = &exc_unksyncsig_e;    break;
    }

    RAISE(*exc);
}


/* 
 * S E T U P _ S Y N C _ S I G N A L _ H A N D L E R S
 *
 * Setup a signal handler to catch all synchronous signals and convert
 * them to exceptions.  The occurance of a synchronous signal results
 * in an immediate exception unwind on the stack of the thrad that caused
 * the signal.
 */
static void setup_sync_signal_handlers()
{

    /*
     * Setup synchronous handlers.  Note that we get the current state of
     * each signal and then just change the handler field.  Reputed to
     * be better for some implementations.
     */

#define SIGACTION(_sig) \
{ \
    struct sigaction action; \
    (void)sigaction((_sig), (struct sigaction *) 0, &action); \
    if (action.sa_handler == SIG_DFL) \
        action.sa_handler = sync_signal_handler; \
    (void)sigaction((_sig), &action, (struct sigaction *) 0); \
}

    SIGACTION(SIGILL);
    SIGACTION(SIGTRAP);
    SIGACTION(SIGIOT);
#ifdef SIGEMT
    SIGACTION(SIGEMT);
#endif    
    SIGACTION(SIGFPE);
    SIGACTION(SIGBUS);
    SIGACTION(SIGSEGV);
#ifdef SIGSYS
    SIGACTION(SIGSYS);
#endif    
    SIGACTION(SIGPIPE);

#undef SIGACTION
}
#endif


/*
 * D E S T R O Y _ E X C
 * 
 * Called at thread exit to destroy the thread specific exception state
 * storage.
 */
static void destroy_exc(_exc_cur)
void *_exc_cur;
{
    free(_exc_cur);
}


/*
 * I N I T _ O N C E 
 * 
 * Initialize the exception package. This function is run through pthread_once().
 *      Create the key for the thread specific exception state.
 *      Setup signal handlers for the process wide asynchronous signals.
 */
static void init_once()
{
    EXCEPTION_INIT(exc_uninitexc_e);
    EXCEPTION_INIT(exc_exquota_e);
    EXCEPTION_INIT(exc_insfmem_e);
    EXCEPTION_INIT(exc_nopriv_e);
    EXCEPTION_INIT(exc_illaddr_e);
    EXCEPTION_INIT(exc_illinstr_e);
    EXCEPTION_INIT(exc_resaddr_e);
    EXCEPTION_INIT(exc_privinst_e);
    EXCEPTION_INIT(exc_resoper_e);
    EXCEPTION_INIT(exc_aritherr_e);
    EXCEPTION_INIT(exc_intovf_e);
    EXCEPTION_INIT(exc_intdiv_e);
    EXCEPTION_INIT(exc_fltovf_e);
    EXCEPTION_INIT(exc_fltdiv_e);
    EXCEPTION_INIT(exc_fltund_e);
    EXCEPTION_INIT(exc_decovf_e);
    EXCEPTION_INIT(exc_subrng_e);
    EXCEPTION_INIT(exc_excpu_e);
    EXCEPTION_INIT(exc_exfilsiz_e);
    EXCEPTION_INIT(exc_SIGTRAP_e);
    EXCEPTION_INIT(exc_SIGIOT_e);
#ifdef SIGEMT
    EXCEPTION_INIT(exc_SIGEMT_e);
#endif
#ifdef SIGSYS
    EXCEPTION_INIT(exc_SIGSYS_e);
#endif    
    EXCEPTION_INIT(exc_SIGPIPE_e);
    EXCEPTION_INIT(exc_unksyncsig_e);

    EXCEPTION_INIT(pthread_cancel_e);

    pthread_key_create(&_exc_key, destroy_exc);
}


/*
 * _ E X C _ T H R E A D _ I N I T
 * 
 * Initialize the exception package for per_thread stuff.
 */
void _exc_thread_init()
{
    /*
     * One time initialization for all threads.
     */
    pthread_once(&init_once_block, init_once);

#if 0
    /*
     * Set up signal handlers for this thread.
     * XXX - should only be done once per thread, or be idempotent.
     */
    setup_sync_signal_handlers();
#endif
}


/* 
 * _ E X C _ R A I S E 
 * 
 * RAISE operation.  All exceptions are mapped into a pthread_cancel()
 * to start the cancel cleanup handler popping. Before starting the unwind,
 * setup the thread's current exception.
 * 
 * THIS IS CALLED FROM A SYNCHRONOUS SIGNAL HANDLER (see above).
 */
void _exc_raise(exc)
EXCEPTION *exc;
{
    _exc_buf  *eb, *eb_next;
    static _exc_buf unset_exc_buf;

#define UNHANDLED_EXC_MSG "Unhandled exception; exiting!\n"

    for (;;)
    {
        eb = pthread_getspecific(_exc_key);
        if (eb == NULL)
            break;
        /*
         * We have a handler here, but has it been initialized?
         */
        if (memcmp(&eb->jb, &unset_exc_buf.jb, sizeof eb->jb) != 0)
        {
            /*
             * Yes - there's at least one non-zero value in the jmp_buf.
             * (It contains the SP, so at least one value will be
             * non-zero if it's been initialized with _setjmp.)
             */
            break;
        }

        /*
         * That one's uninitialized; pop it off and free it.
         */
        eb_next = eb->next;
        free(eb);
        pthread_setspecific(_exc_key, eb_next);
    }
    if (eb == NULL)
    {
        /*
         * There are no more exception handlers; cause the process to exit.
         *
         * The reason we do this is because (a) it seems like not such a
	 * bad idea (unhandled exception => process exit), and (b) if an
	 * exception goes unhandled in the initial thread and we didn't do
	 * this, the initial thread would exit but the process would hang
	 * around, making things pretty confusing for the poor user.
	 */
        write(2, UNHANDLED_EXC_MSG, sizeof UNHANDLED_EXC_MSG);
        abort();
    }

    /* Set the current exception and longjmp out. */
    eb->_cur_exc = exc;
    _longjmp(eb->jb, 1);
}


/*
 * E X C _ M A T C H E S
 * 
 * Return true iff two exceptions match.
 */
int exc_matches(cur_exc, exc)
EXCEPTION *cur_exc;
EXCEPTION *exc;
{
    if (cur_exc->kind == exc->kind 
        && (cur_exc->kind == _exc_c_kind_status ?
               cur_exc->match.value == exc->match.value : 
               cur_exc->match.address == exc->match.address))
        return 1;

    return 0;
}


/*
 * E X C _ R E P O R T
 * 
 * 
 */
void exc_report(exc)
EXCEPTION *exc;
{
    fflush(stdout);

    if (exc->kind == _exc_c_kind_status) 
    {
        fprintf(stderr, "%%Exception report; value is 0x%x.\n",
            exc->match.value);
    }
    else
    {
        fprintf(stderr, "%%Exception report; exception address is %p.\n",
            exc->match.address);
    }
}
