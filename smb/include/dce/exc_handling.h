/*
 * 
 * (c) Copyright 1991 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1991 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1991 DIGITAL EQUIPMENT CORPORATION
 *
 * Portions Copyright (C) 2005 - 2007 Apple Inc. All rights reserved.
 *
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
#ifndef EXC_HANDLING_H
#define EXC_HANDLING_H
/*
**
**  NAME:
**
**      exc_handling.h
**
**  FACILITY:
**
**      Exceptions
**
**  ABSTRACT:
** 
**  Pthread based exception package support header file.
**
**  This header file defines a TRY/CATCH exception mechanism that runs
**  'on top of' 1003.1 pthreads.
** 
**  The following implementation models all exceptions by converting them
**  into a "cancel" and counting on pthreads to maintain the cancel cleanup
**  handler chain.  So, rather than really maintain our own exception
**  handler list, we just piggyback on the cancel cleanup handler list
**  at the expense of a slightly more expensive RAISE().  There is a per
**  thread global "current exception" that is set to the exception that
**  is being processed.
** 
**  Exception handlers execute with general cancellability disabled.
** 
**  Arbitrary application pthread_cancel(pthread_self()) (i.e. something
**  not part of the exception package; not a RAISE) that are delivered
**  while in the scope of an exception handler will unwind to the exception
**  handler; these "outside" pthread_cancel's are represented as a "cancel"
**  exception.
** 
*/

#include <sys/types.h>
#include <setjmp.h>
#include <pthread.h>

/* --------------------------------------------------------------------------- */

typedef enum _exc_kind_t 
{
    _exc_c_kind_address = 0x02130455,  
    _exc_c_kind_status  = 0x02130456
} _exc_kind_t;

typedef struct _exc_exception
{
    _exc_kind_t kind;
    union match_value {
        int                     value;
        struct _exc_exception   *address;       
    } match;
} EXCEPTION;
 
#define EXCEPTION_INIT(e) { \
    (e).kind = _exc_c_kind_address; \
    (e).match.address = &(e); \
    }

#define exc_set_status(e, s) ( \
    (e)->match.value = (s), \
    (e)->kind = _exc_c_kind_status \
)

extern pthread_key_t _exc_key;

/* --------------------------------------------------------------------------- */

/*
 * An exception handler buffer (state block).
 */
typedef struct _exc_buf
{
    jmp_buf          jb;
    EXCEPTION 	     *_cur_exc;
    struct _exc_buf  *next;
} _exc_buf;

/* --------------------------------------------------------------------------- */

int exc_matches
    (
        EXCEPTION *cur_exc,
        EXCEPTION *exc
    );

void exc_report
    (
        EXCEPTION *exc
    );

void _exc_thread_init
    (
        void
    );

void _exc_raise
    (
        EXCEPTION *exc
    );

/* --------------------------------------------------------------------------- */

/*
 * The following exception is used to report an attempt to raise an
 * uninitialized exception object. It should never be raised by any other
 * code.
 */

extern EXCEPTION exc_uninitexc_e;       /* Uninitialized exception */

/*
 * The following exceptions are common error conditions which may be raised
 * by the thread library or any other facility following this exception
 * specification.
 */

extern EXCEPTION exc_exquota_e;         /* Exceeded quota */
extern EXCEPTION exc_insfmem_e;         /* Insufficient memory */
extern EXCEPTION exc_nopriv_e;          /* No privilege */

/*
 * The following exceptions describe hardware or operating system error
 * conditions that are appropriate for most hardware and operating system
 * platforms. These are raised by the exception facility to report operating
 * system and hardware error conditions.
 */

extern EXCEPTION exc_illaddr_e;         /* Illegal address */
extern EXCEPTION exc_illinstr_e;        /* Illegal instruction */
extern EXCEPTION exc_resaddr_e;         /* Reserved addressing mode */
extern EXCEPTION exc_privinst_e;        /* Privileged instruction */
extern EXCEPTION exc_resoper_e;         /* Reserved operand */
extern EXCEPTION exc_aritherr_e;        /* Arithmetic error */
extern EXCEPTION exc_intovf_e;          /* Integer overflow */
extern EXCEPTION exc_intdiv_e;          /* Integer divide by zero */
extern EXCEPTION exc_fltovf_e;          /* Floating overflow */
extern EXCEPTION exc_fltdiv_e;          /* Floating divide by zero */
extern EXCEPTION exc_fltund_e;          /* Floating underflow */
extern EXCEPTION exc_decovf_e;          /* Decimal overflow */
extern EXCEPTION exc_subrng_e;          /* Subrange */
extern EXCEPTION exc_excpu_e;           /* Exceeded CPU quota */
extern EXCEPTION exc_exfilsiz_e;        /* Exceeded file size */

/*
 * The following exceptions correspond directly to UNIX synchronous
 * terminating signals.  This is distinct from the prior list in that those
 * are generic and likely to have equivalents on most any operating system,
 * whereas these are highly specific to UNIX platforms.
 */

extern EXCEPTION exc_SIGTRAP_e;         /* SIGTRAP received */
extern EXCEPTION exc_SIGIOT_e;          /* SIGIOT received */
extern EXCEPTION exc_SIGEMT_e;          /* SIGEMT received */
extern EXCEPTION exc_SIGSYS_e;          /* SIGSYS received */
extern EXCEPTION exc_SIGPIPE_e;         /* SIGPIPE received */
extern EXCEPTION exc_unksyncsig_e;      /* Unknown synchronous signal */

/*
 * The following exception is raised in the target of a cancel
 */

extern EXCEPTION pthread_cancel_e;      

/*
 * The following aliases exist for backward compatibility with CMA.
 */

#define cma_e_alerted           pthread_cancel_e
                                                    
/* --------------------------------------------------------------------------- */

/*
 * The following aliases exist for backward compatibility with CMA.
 */

#define exc_e_uninitexc         exc_uninitexc_e
#define exc_e_illaddr           exc_illaddr_e
#define exc_e_exquota           exc_exquota_e
#define exc_e_insfmem           exc_insfmem_e
#define exc_e_nopriv            exc_nopriv_e
#define exc_e_illinstr          exc_illinstr_e
#define exc_e_resaddr           exc_resaddr_e
#define exc_e_privinst          exc_privinst_e
#define exc_e_resoper           exc_resoper_e
#define exc_e_SIGTRAP           exc_SIGTRAP_e
#define exc_e_SIGIOT            exc_SIGIOT_e
#define exc_e_SIGEMT            exc_SIGEMT_e
#define exc_e_aritherr          exc_aritherr_e
#define exc_e_SIGSYS            exc_SIGSYS_e
#define exc_e_SIGPIPE           exc_SIGPIPE_e
#define exc_e_excpu             exc_excpu_e
#define exc_e_exfilsiz          exc_exfilsiz_e
#define exc_e_intovf            exc_intovf_e
#define exc_e_intdiv            exc_intdiv_e
#define exc_e_fltovf            exc_fltovf_e
#define exc_e_fltdiv            exc_fltdiv_e
#define exc_e_fltund            exc_fltund_e
#define exc_e_decovf            exc_decovf_e
#define exc_e_subrng            exc_subrng_e

#define exc_e_accvio            exc_e_illaddr
#define exc_e_SIGILL            exc_e_illinstr
#define exc_e_SIGFPE            exc_e_aritherr
#define exc_e_SIGBUS            exc_e_illaddr
#define exc_e_SIGSEGV           exc_e_illaddr
#define exc_e_SIGXCPU           exc_e_excpu
#define exc_e_SIGXFSZ           exc_e_exfilsiz

/* --------------------------------------------------------------------------- */

/*
 * _ E X C _ S E T J M P / L O N G J M P 
 * 
 * Similar to setjmp(2) and longjmp(2) these macros also save/restore
 * the cancel state of a canceled thread providing a means to "handle"
 * a cancel.
 * 
 * These macros gives us the basic mechanism to create in-line exception
 * handlers (i.e. TRY / CATCH) and integrate the exception package with
 * the pthread cancel mechanism.
 * 
 * _exc_longjmp() must be defined to be legal to use from within a
 * pthread cleanup handler.
 * 
 * Notes:
 *
 * (1) _exc_longjmp() is NOT integrated with pthread cleanup handlers.
 * Specifically, calling _exc_longjmp() will not induce behaviour as
 * if a cancel had been delivered to the thread.  The intent is that
 * _exc_{setjmp,longjmp}() are to be used by exception package developers.
 *
 * (2) cancels must be turned off prior to calling setjmp to set up the
 * jmpbuf for the exception handler.  Therefore the setcancel calls around
 * setjmp are not clearly separable into an exception prelude function.
 */

/* --------------------------------------------------------------------------- */

/*
 * T R Y
 *
 * Define the beginning of an exception handler scope and the non-exception
 * code path.
 *
 * - Push a cancel cleanup handler for this exception handler
 * - Arrange it so that this cancel cleanup handler gets us back to this 
 *   code when an exception occurs (_exc_setjmp).
 * - If an exception occurred, get a handle to the thread's "current
 *   exception" value (so CATCH clauses can RERAISE the exception).
 *
 * Note that the rough schema for the exception handler is:
 *
 *      do {
 *          pthread_cleanup_push("routine that will longjmp back here");
 *          val = setjmp(...);
 *          if (val == 0)
 *              ...normal code...
 *          else
 *              ...exception code...
 *          ...finally code...
 *          if ("exception happened")
 *              if ("exception handled")
 *                  break;
 *              else
 *                  ...re-raise exception...
 *          pthread_cleanup_pop(...);
 *      } while (0);
 *
 * Exceptions are raised by doing a pthread_cancel against one's self
 * and then doing a pthread_testcancel.  This causes the topmost cleanup
 * routine to be popped and called.  This routine (_exc_cleanup_handler)
 * longjmp's back to the exception handler.  This approach means we can
 * leverage off the fact that the push/pop routines are maintaining some
 * per-thread state (hopefully [but likely not] more efficiently than
 * we could ourselves).  We need this state to string together the dynamic
 * stack of exception handlers.
 *
 * Most of this trickery is so we can avoid doing the pop in the "exception
 * handled" path (since the cleanup routine will already have been popped).
 * (We also need to keep the push and pop appropriately lexically scoped
 * per the pthread spec.)
 *
 * Note that there's some tricky stuff that deals with the case of an
 * exception being raised in a FINALLY clause that was entered in the
 * normal (i.e., non-exception) case.  The problem is that we haven't
 * yet popped the current cleanup routine so when the exception gets
 * raised, we execute it and longjmp back to the CURRENT exception handler,
 * not the next one.  We deal with this by initializing a flag
 * (_exc_in_finally) to "false" and set it to "true" in the FINALLY clause.
 * If we ever jump back and see the flag is "true", we know we should
 * just re-raise right away.
 */
#define _exc_longjmp(jmpbuf, val) _longjmp((jmpbuf)->jb, (val))

#define TRY \
do \
{ \
    _exc_buf *volatile _eb, *volatile _eb_next; \
    EXCEPTION *volatile _exc_cur; \
    volatile char _exc_cur_handled = 0; \
    volatile char _exc_in_finally = 0; \
    volatile int _setjmp_res; \
    \
    _exc_thread_init(); \
    _eb = malloc(sizeof(*_eb)); \
    memset(_eb, 0, sizeof(*_eb)); \
    _eb->next = pthread_getspecific(_exc_key); \
    pthread_setspecific(_exc_key, _eb); \
    _setjmp_res = _setjmp(_eb->jb); \
    if (_setjmp_res != 0) \
    { \
        _eb = pthread_getspecific(_exc_key); \
        _exc_cur = _eb->_cur_exc; \
        if (_exc_in_finally) \
        { \
            /* \
             * Pop the current exception handler off the stack and free it. \
             */ \
            _eb = pthread_getspecific(_exc_key); \
            _eb_next = _eb->next; \
            pthread_setspecific(_exc_key, _eb_next); \
            free(_eb); \
            _exc_raise(_exc_cur); \
        } \
    } \
    if (_setjmp_res == 0) \
    { \
        /* normal code here  */

/* --------------------------------------------------------------------------- */

/*
 * C A T C H ( e x c )
 *
 * Define the beginning of an exception handler scope for the exception
 * "exc".  The exception handler code must either "fall through" to
 * ENDTRY (indicating that the exception has been handled and to resume
 * execution after the ENDTRY), RERAISE the current exception or RAISE
 * a different exception (in both cases, propagating an unwind to the next
 * higher exception handler).
 */
#define CATCH(exc) \
    } \
    else if (exc_matches(_exc_cur, &(exc))) \
    { \
        _exc_cur_handled = 1; \
        /* exception code here */

/*
 * C A T C H _ A L L
 *
 * Define the beginning of an exception handler scope for any exception
 * not explicitly caught by a CATCH(exc).  Everything else is just like
 * CATCH(exc).
 */
#define CATCH_ALL \
    } \
    else \
    { \
        EXCEPTION *THIS_CATCH __attribute((unused)) = _exc_cur; \
        _exc_cur_handled = 1; \
        /* exception code here */

/* --------------------------------------------------------------------------- */

/*
 * F I N A L L Y
 *
 * Define the beginning of a code block that is to be executed regardless
 * of whether or not an exception occurs.  FINALLY should NOT be used
 * if a CATCH or CATCH_ALL is used.
 */
#define FINALLY \
    } \
    { \
        _exc_in_finally = 1; \
        /* user finally code here */

/* --------------------------------------------------------------------------- */

/*
 * E N D T R Y
 *
 * Terminate an exception handler scope.
 *
 * We can reach the ENDTRY under several conditions.  Note that we will
 * not reach here if an exception occurred, was caught and was explicitly
 * reraised.
 *
 * If we got here because things went fine (i.e., there was no exception)
 * then we need to pop the cancel cleanup handler and we're done (fall
 * out the bottom).
 *
 * If we got here because an exception occurred AND the exception was
 * NOT handled, we just then we propagate the exception to the next higher
 * exception handler (no need to restore the original cancel state as
 * the induced _exc_longjmp() will reset the state).
 *
 * If we got here because an exception occurred (i.e., the cancel cleanup
 * handler was implicitly popped) AND the exception WAS handled, we just
 * want to break out (NOT do the pop).  Reset the current exception to
 * "cancel" so that we can detect an unwind from outside of the exception
 * package (i.e., via the thread's being cancelled).
 */
#define ENDTRY \
    } \
    if (_setjmp_res != 0) \
    { \
        if (! _exc_cur_handled) { \
            /* \
             * Pop the current exception handler off the stack and free it. \
             */ \
            _eb = pthread_getspecific(_exc_key); \
            _eb_next = _eb->next; \
            pthread_setspecific(_exc_key, _eb_next); \
            free(_eb); \
            /* \
             * Now re-raise the exception, so the previous exception \
             * handler handles it. \
             */ \
            _exc_raise(_exc_cur); \
        } \
        *_exc_cur = pthread_cancel_e; \
        _eb->_cur_exc = &pthread_cancel_e; \
        break; \
    } \
    /* \
     * Pop the current exception handler off the stack and free it. \
     */ \
    _eb = pthread_getspecific(_exc_key); \
    _eb_next = _eb->next; \
    pthread_setspecific(_exc_key, _eb_next); \
    free(_eb); \
} while (0); \
/* End of exception handler scope; continue execution */


/*
 * R A I S E ( e x c )
 *
 * Raise an exception - i.e. initiate an unwind to the next
 * exception handler.
 */
#define RAISE(exc)  _exc_raise(&exc)

/* --------------------------------------------------------------------------- */

/*
 * R E R A I S E
 *
 * Raise the current exception - i.e. initiate an unwind to next exception
 * handler.  Note: RERAISE is legal only within a CATCH or a CATCH_ALL.
 * Note _exc_cur may be NULL due to an "outside" pthread_cancel().
 */
#define RERAISE     _exc_raise(_exc_cur)

/* --------------------------------------------------------------------------- */

#endif
