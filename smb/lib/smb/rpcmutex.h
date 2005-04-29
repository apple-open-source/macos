/*
 * 
 * (c) Copyright 1989 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989 DIGITAL EQUIPMENT CORPORATION
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
#ifndef _RPCMUTEX_H
#define _RPCMUTEX_H	1
/*
**
**  NAME:
**
**      rpcmutex.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  A veneer over CMA (or anything else for that matter) to assist with
**  mutex and condition variable support for the runtime.  This provides
**  some isolation from the underlying mechanisms to allow us to (more)
**  easily slip in alternate facilities, and more importantly add features
**  that we'd like to have in the areas of correctness and debugging
**  of code using these locks as well as statistics gathering.  Note
**  that this package contains a "condition variable" veneer as well,
**  since condition variables and mutexes are tightly integrated (i.e.
**  condition variables have an associated mutex).
**
**  This package provides the following PRIVATE data types and operations:
**  
**      rpc_mutex_t m;
**      rpc_cond_t c;
**  
**      void RPC_MUTEX_INIT(m)
**      void RPC_MUTEX_DELETE(m)
**      void RPC_MUTEX_LOCK(m)
**      void RPC_MUTEX_TRY_LOCK(m,bp)
**      void RPC_MUTEX_UNLOCK(m)
**      void RPC_MUTEX_LOCK_ASSERT(m)
**      void RPC_MUTEX_UNLOCK_ASSERT(m)
**
**      void RPC_COND_INIT(c,m)
**      void RPC_COND_DELETE(c,m)
**      void RPC_COND_WAIT(c,m)
**      void RPC_COND_TIMED_WAIT(c,m,t)
**      void RPC_COND_SIGNAL(c,m)
**
**  This code is controlled by debug switch "rpc_e_dbg_mutex".  At levels 1
**  or higher, statistics gathering is enabled.  At levels 5 or higher,
**  assertion checking, ownership tracking, deadlock detection, etc. are
**  enabled.
**
**
*/

/*
 * The rpc_mutex_t data type.  
 *
 * If debugging isn't configured, this is just a unadorned mutex, else we adorn
 * the struct with goodies to enable assertion checking, deadlock detection
 * and statistics gathering.
 */

typedef struct rpc_mutex_stats_t 
{
    unsigned32 busy;            /* total lock requests when already locked */
    unsigned32 lock;            /* total locks */
    unsigned32 try_lock;        /* total try_locks */
    unsigned32 unlock;          /* total unlocks */
    unsigned32 init;            /* total inits */
    unsigned32 delete;          /* total deletes */
    unsigned32 lock_assert;     /* total lock_asserts */
    unsigned32 unlock_assert;
} rpc_mutex_stats_t, *rpc_mutex_stats_p_t;


typedef struct rpc_mutex_t 
{
    pthread_mutex_t m;          /* the unadorned mutex lock */
#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)
    unsigned is_locked:1;       /* T=> locked */
    pthread_t owner;            /* current owner if locked */
    char *locker_file;          /* last locker */
    int locker_line;            /* last locker */
    rpc_mutex_stats_t stats;
#endif /* RPC_MUTEX_DEBUG OR RPC_MUTEX_STATS */
} rpc_mutex_t, *rpc_mutex_p_t;


/*
 * The rpc_cond_t (condition variable) data type.
 */

typedef struct rpc_cond_stats_t
{
    unsigned32 init;            /* total inits */
    unsigned32 delete;          /* total deletes */
    unsigned32 wait;            /* total waits */
    unsigned32 signals;         /* total signals + broadcasts */
} rpc_cond_stats_t, *rpc_cond_stats_p_t;

typedef struct rpc_cond_t
{
    pthread_cond_t c;           /* the unadorned condition variable */
    rpc_mutex_p_t mp;           /* the cv's associated mutex */
    rpc_cond_stats_t stats;
} rpc_cond_t, *rpc_cond_p_t;



/*
 * Some relatively efficient generic mutex operations that are controllable
 * at run time as well as compile time.  The "real" support routines
 * can be found in rpcmutex.c .
 */


/*
 * R P C _ M U T E X _ I N I T
 *
 * We always need to call the support routine so that the stats, etc
 * get initialized.
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_MUTEX_INIT(mutex)  \
    { \
        if (! rpc__mutex_init(&(mutex))) \
        { \
	    RPC_DCE_SVC_PRINTF (( \
		DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		rpc_svc_mutex, \
		svc_c_sev_fatal | svc_c_action_abort, \
		rpc_m_call_failed_no_status, \
		"RPC_MUTEX_INIT/rpc__mutex_init" )); \
        } \
    }
#else
#  define RPC_MUTEX_INIT(mutex) \
    { \
        RPC_LOG_MUTEX_INIT_NTR; \
        pthread_mutex_init(&(mutex).m, NULL); \
        RPC_LOG_MUTEX_INIT_XIT; \
    }
#endif /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */



/*
 * R P C _ M U T E X _ D E L E T E
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)
#  define RPC_MUTEX_DELETE(mutex)  \
    { \
        if (RPC_DBG(rpc_es_dbg_mutex, 1)) \
        { \
            if (! rpc__mutex_delete(&(mutex))) \
            { \
		RPC_DCE_SVC_PRINTF (( \
		    DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		    rpc_svc_mutex, \
		    svc_c_sev_fatal | svc_c_action_abort, \
		    rpc_m_call_failed_no_status, \
		    "RPC_MUTEX_DELETE/rpc__mutex_delete" )); \
            } \
        } else { \
            pthread_mutex_destroy(&(mutex).m); \
        } \
    }
#else
#  define RPC_MUTEX_DELETE(mutex) \
    { \
        RPC_LOG_MUTEX_DELETE_NTR; \
        pthread_mutex_destroy(&(mutex).m); \
        RPC_LOG_MUTEX_DELETE_XIT; \
    }
#endif  /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */



/*
 * R P C _ M U T E X _ L O C K
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_MUTEX_LOCK(mutex)  \
    { \
        if (RPC_DBG(rpc_es_dbg_mutex, 1)) \
        { \
            if (! rpc__mutex_lock(&(mutex), __FILE__, __LINE__)) \
            { \
		RPC_DCE_SVC_PRINTF (( \
		    DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		    rpc_svc_mutex, \
		    svc_c_sev_fatal | svc_c_action_abort, \
		    rpc_m_call_failed_no_status, \
		    "RPC_MUTEX_LOCK/rpc__mutex_lock" )); \
            } \
        } else { \
            RPC_LOG_MUTEX_LOCK_NTR; \
            pthread_mutex_lock(&(mutex).m); \
            RPC_LOG_MUTEX_LOCK_XIT; \
        } \
    }
#else
#  define RPC_MUTEX_LOCK(mutex) \
    { \
        RPC_LOG_MUTEX_LOCK_NTR; \
        pthread_mutex_lock(&(mutex).m); \
        RPC_LOG_MUTEX_LOCK_XIT; \
    }
#endif  /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */



/*
 * R P C _ M U T E X _ T R Y _ L O C K
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_MUTEX_TRY_LOCK(mutex,bp)  \
    { \
        if (RPC_DBG(rpc_es_dbg_mutex, 1)) \
        { \
            if (! rpc__mutex_try_lock(&(mutex), (bp), __FILE__, __LINE__)) \
            { \
		RPC_DCE_SVC_PRINTF (( \
		    DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		    rpc_svc_mutex, \
		    svc_c_sev_fatal | svc_c_action_abort, \
		    rpc_m_call_failed_no_status, \
		    "RPC_MUTEX_TRY_LOCK/rpc__mutex_try_lock" )); \
            } \
        } else { \
            *(bp) = pthread_mutex_trylock(&(mutex).m); \
        } \
    }
#else
#  define RPC_MUTEX_TRY_LOCK(mutex,bp) \
    { \
        RPC_LOG_MUTEX_TRY_LOCK_NTR; \
        *(bp) = pthread_mutex_trylock(&(mutex).m); \
        RPC_LOG_MUTEX_TRY_LOCK_XIT; \
    }
#endif  /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */



/*
 * R P C _ M U T E X _ U N L O C K
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_MUTEX_UNLOCK(mutex)  \
    { \
        if (RPC_DBG(rpc_es_dbg_mutex, 1)) \
        { \
            if (! rpc__mutex_unlock(&(mutex))) \
            { \
		RPC_DCE_SVC_PRINTF (( \
		    DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		    rpc_svc_mutex, \
		    svc_c_sev_fatal | svc_c_action_abort, \
		    rpc_m_call_failed_no_status, \
		    "RPC_MUTEX_UNLOCK/rpc__mutex_unlock" )); \
            } \
        } else { \
            pthread_mutex_unlock(&(mutex).m); \
        } \
    }
#else
#  define RPC_MUTEX_UNLOCK(mutex) \
    { \
        RPC_LOG_MUTEX_UNLOCK_NTR; \
        pthread_mutex_unlock(&(mutex).m); \
        RPC_LOG_MUTEX_UNLOCK_XIT; \
    }
#endif  /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */



/*
 * R P C _ M U T E X _ L O C K _ A S S E R T
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_MUTEX_LOCK_ASSERT(mutex)  \
    { \
        if (RPC_DBG(rpc_es_dbg_mutex, 1)) \
        { \
            if (! rpc__mutex_lock_assert(&(mutex))) \
            { \
		RPC_DCE_SVC_PRINTF (( \
		    DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		    rpc_svc_mutex, \
		    svc_c_sev_fatal | svc_c_action_abort, \
		    rpc_m_call_failed_no_status, \
		    "RPC_MUTEX_LOCK_ASSERT/rpc__mutex_lock_assert" )); \
            } \
        } \
    }
#else
#  define RPC_MUTEX_LOCK_ASSERT(mutex)      { }
#endif  /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */



/*
 * R P C _ M U T E X _ U N L O C K _ A S S E R T
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_MUTEX_UNLOCK_ASSERT(mutex)  \
    { \
        if (RPC_DBG(rpc_es_dbg_mutex, 1)) \
        { \
            if (! rpc__mutex_unlock_assert(&(mutex))) \
            { \
		RPC_DCE_SVC_PRINTF (( \
		    DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		    rpc_svc_mutex, \
		    svc_c_sev_fatal | svc_c_action_abort, \
		    rpc_m_call_failed_no_status, \
		    "RPC_MUTEX_UNLOCK_ASSERT/rpc__mutex_unlock_assert" )); \
            } \
        } \
    }
#else
#  define RPC_MUTEX_UNLOCK_ASSERT(mutex)    { }
#endif  /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */


/*
 *  R P C _ C O N D _ I N I T
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_COND_INIT(cond,mutex)  \
    { \
        if (! rpc__cond_init(&(cond), &(mutex))) \
        { \
	    RPC_DCE_SVC_PRINTF (( \
		DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		rpc_svc_mutex, \
		svc_c_sev_fatal | svc_c_action_abort, \
		rpc_m_call_failed_no_status, \
		"RPC_COND_INIT/rpc__cond_init" )); \
        } \
    }
#else
#  define RPC_COND_INIT(cond,mutex) \
    { \
        RPC_LOG_COND_INIT_NTR; \
        pthread_cond_init(&(cond).c, NULL); \
        RPC_LOG_COND_INIT_XIT; \
    }
#endif  /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */



/*
 *  R P C _ C O N D _ D E L E T E
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_COND_DELETE(cond,mutex)  \
    { \
        if (RPC_DBG(rpc_es_dbg_mutex, 1)) \
        { \
            if (! rpc__cond_delete(&(cond), &(mutex))) \
            { \
		RPC_DCE_SVC_PRINTF (( \
		    DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		    rpc_svc_mutex, \
		    svc_c_sev_fatal | svc_c_action_abort, \
		    rpc_m_call_failed_no_status, \
		    "RPC_COND_DELETE/rpc__cond_delete" )); \
            } \
        } else { \
            pthread_cond_destroy(&(cond).c); \
        } \
    }
#else
#  define RPC_COND_DELETE(cond,mutex) \
    { \
      RPC_LOG_COND_DELETE_NTR; \
      pthread_cond_destroy(&(cond).c); \
      RPC_LOG_COND_DELETE_XIT; \
  }
#endif  /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */



/*
 *  R P C _ C O N D _ W A I T
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_COND_WAIT(cond,mutex)  \
    { \
        if (RPC_DBG(rpc_es_dbg_mutex, 1)) \
        { \
            if (! rpc__cond_wait(&(cond), &(mutex), __FILE__, __LINE__)) \
            { \
		RPC_DCE_SVC_PRINTF (( \
		    DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		    rpc_svc_mutex, \
		    svc_c_sev_fatal | svc_c_action_abort, \
		    rpc_m_call_failed_no_status, \
		    "RPC_COND_WAIT/rpc__cond_wait" )); \
            } \
        } else { \
            pthread_cond_wait(&(cond).c, &(mutex).m); \
        } \
    }
#else
#  define RPC_COND_WAIT(cond,mutex) \
    { \
        RPC_LOG_COND_WAIT_NTR; \
        pthread_cond_wait(&(cond).c, &(mutex).m); \
        RPC_LOG_COND_WAIT_XIT; \
    }
#endif


/*
 *  R P C _ C O N D _ T I M E D _ W A I T
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_COND_TIMED_WAIT(cond,mutex,time)  \
    { \
        if (RPC_DBG(rpc_es_dbg_mutex, 1)) \
        { \
            if (! rpc__cond_timed_wait(&(cond), &(mutex), (time), __FILE__, __LINE__)) \
            { \
		RPC_DCE_SVC_PRINTF (( \
		    DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		    rpc_svc_mutex, \
		    svc_c_sev_fatal | svc_c_action_abort, \
		    rpc_m_call_failed_no_status, \
		    "RPC_COND_TIMED_WAIT/rpc__cond_timed_wait" )); \
            } \
        } else { \
            pthread_cond_timedwait(&(cond).c, &(mutex).m, (time)); \
        } \
    }
#else
#  define RPC_COND_TIMED_WAIT(cond,mutex,time) \
    { \
        RPC_LOG_COND_TIMED_WAIT_NTR; \
        pthread_cond_timedwait(&(cond).c, &(mutex).m, (time)); \
        RPC_LOG_COND_TIMED_WAIT_XIT; \
    }
#endif  /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */


/*
 *  R P C _ _ C O N D _ S I G N A L
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_COND_SIGNAL(cond,mutex)  \
    { \
        if (RPC_DBG(rpc_es_dbg_mutex, 1)) \
        { \
            if (! rpc__cond_signal(&(cond), &(mutex))) \
            { \
		RPC_DCE_SVC_PRINTF (( \
		    DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		    rpc_svc_mutex, \
		    svc_c_sev_fatal | svc_c_action_abort, \
		    rpc_m_call_failed_no_status, \
		    "RPC_COND_SIGNAL/rpc__cond_signal" )); \
            } \
        } else { \
            pthread_cond_signal(&(cond).c); \
        } \
    }
#else
#  define RPC_COND_SIGNAL(cond,mutex) \
    { \
        RPC_LOG_COND_SIGNAL_NTR; \
        pthread_cond_signal(&(cond).c); \
        RPC_LOG_COND_SIGNAL_XIT; \
    }
#endif  /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */


/*
 *  R P C _ _ C O N D _ B R O A D C A S T
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_COND_BROADCAST(cond,mutex)  \
    { \
        if (RPC_DBG(rpc_es_dbg_mutex, 1)) \
        { \
            if (! rpc__cond_broadcast(&(cond), &(mutex))) \
            { \
		RPC_DCE_SVC_PRINTF (( \
		    DCE_SVC(RPC__SVC_HANDLE, "%s"), \
		    rpc_svc_mutex, \
		    svc_c_sev_fatal | svc_c_action_abort, \
		    rpc_m_call_failed_no_status, \
		    "RPC_COND_BROADCAST/rpc__cond_broadcast" )); \
            } \
        } else { \
            pthread_cond_broadcast(&(cond).c); \
        } \
    }
#else
#  define RPC_COND_BROADCAST(cond,mutex) \
    { \
        RPC_LOG_COND_BROADCAST_NTR; \
        pthread_cond_broadcast(&(cond).c); \
        RPC_LOG_COND_BROADCAST_XIT; \
    }
#endif  /* RPC_MUTEX_DEBUG or RPC_MUTEX_STATS */


/* ===================================================================== */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

/*
 * Prototypes for the support routines.
 */

boolean rpc__mutex_init( rpc_mutex_p_t  /*mp*/ );

boolean rpc__mutex_delete( rpc_mutex_p_t  /*mp*/ );

boolean rpc__mutex_lock(
        rpc_mutex_p_t  /*mp*/,
        char * /*file*/,
        int  /*line*/
    );

boolean rpc__mutex_try_lock(
        rpc_mutex_p_t  /*mp*/,
        boolean * /*bp*/,
        char * /*file*/,
        int  /*line*/
    );

boolean rpc__mutex_unlock( rpc_mutex_p_t  /*mp*/ );

boolean rpc__mutex_lock_assert( rpc_mutex_p_t  /*mp*/);

boolean rpc__mutex_unlock_assert( rpc_mutex_p_t  /*mp*/);


boolean rpc__cond_init(
        rpc_cond_p_t  /*cp*/,
        rpc_mutex_p_t  /*mp*/
    );

boolean rpc__cond_delete(
        rpc_cond_p_t  /*cp*/,
        rpc_mutex_p_t  /*mp*/
    );

boolean rpc__cond_wait(
        rpc_cond_p_t  /*cp*/,
        rpc_mutex_p_t  /*mp*/,
        char * /*file*/,
        int  /*line*/
    );

boolean rpc__cond_timed_wait(
        rpc_cond_p_t  /*cp*/,
        rpc_mutex_p_t  /*mp*/,
        struct timespec * /*wtime*/,
        char * /*file*/,
        int  /*line*/
    );

boolean rpc__cond_signal(
        rpc_cond_p_t  /*cp*/,
        rpc_mutex_p_t  /*mp*/
    );

boolean rpc__cond_broadcast(
        rpc_cond_p_t  /*cp*/,
        rpc_mutex_p_t  /*mp*/
    );

#endif /* defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS) */

/* ===================================================================== */

#endif /* RPCMUTEX_H */
