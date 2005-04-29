/* 
 * threadSpCmd.c --
 *
 * This file implements commands for script-level access to thread 
 * synchronization primitives. Currently, the exclusive mutex, the
 * recursive mutex. the reader/writer mutex and condition variable 
 * objects are exposed to the script programmer.
 *
 * Additionaly, a locked eval is also implemented. This is a practical
 * convenience function which relieves the programmer from the need
 * to take care about unlocking some mutex after evaluating a protected
 * part of code. The locked eval is recursive-savvy since it used the
 * recursive mutex for internal locking. 
 *
 * The Tcl interface to the locking and synchronization primitives 
 * attempts to catch some very common problems in thread programming
 * like attempting to lock an exclusive mutex twice from the same
 * thread (deadlock), and eases garbage collection of mutexes.
 *
 * Copyright (c) 2002 by Zoran Vasiljevic.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: threadSpCmd.c,v 1.19 2004/07/21 20:58:12 vasiljevic Exp $
 * ----------------------------------------------------------------------------
 */

#include "tclThread.h"
#include "threadSpCmd.h"

/*
 * Types of synchronization variables we support.
 */

#define EMUTEXID  'm' /* First letter of the exclusive mutex handle */
#define RMUTEXID  'r' /* First letter of the recursive mutex handle */
#define WMUTEXID  'w' /* First letter of the read/write mutex handle */
#define CONDVID   'c' /* First letter of the condition variable handle */

#define SP_MUTEX   1  /* Any kind of mutex */
#define SP_CONDV   2  /* The condition variable sync type */

/* 
 * Structure representing one sync primitive (mutex, condition variable). 
 * We use buckets to manage Tcl handles to sync primitives. Each bucket
 * is associated with a mutex. Each time we process the Tcl handle of an
 * sync primitive, we compute it's (trivial) hash and use this hash to
 * address one of pre-allocated buckets. Then we lock the bucket, perform
 * the needed operation and unlock the bucket again.
 * The bucket internally utilzes a hash-table to store item pointers.
 * Item pointers are identified by a simple xid1, xid2... counting
 * handle. This format is chosen to simplify distribution of handles
 * accross buckets (natural distribution vs. hash-one as in shared vars).
 */

typedef struct _SpItem {
    SpBucket *bucket;      /* Bucket where this item is stored */
    Tcl_HashEntry *hentry; /* Hash table entry where this item is stored */
} SpItem;

/*
 * Structure representing a mutex.
 */

typedef struct _SpMutex {
    SpBucket *bucket;      /* Bucket where this mutex is stored */
    Tcl_HashEntry *hentry; /* Hash table entry where mutex is stored */
    char type;             /* Type of the mutex */
    void *lock;            /* Exclusive, recursive or read/write mutex */
} SpMutex;

/*
 * Structure representing a condition variable. 
 */

typedef struct _SpCondv {
    SpBucket *bucket;      /* Bucket where this variable is stored */
    Tcl_HashEntry *hentry; /* Hash table entry where variable is stored */
    int waited;            /* Flag: someone waits on the variable */
    Tcl_Condition cond;    /* The condition variable itself */
} SpCondv;

/*
 * This global data is used to map opaque Tcl-level handles 
 * to pointers of their corresponding synchronization objects.
 */

static int        initOnce;    /* Flag for initializing tables below */
static Tcl_Mutex  initMutex;   /* Controls initialization of stuff */
static SpBucket*  muxBuckets;  /* Maps mutex handles */
static SpBucket*  varBuckets;  /* Maps condition variable handles */

/*
 * Functions implementing Tcl commands
 */

static Tcl_ObjCmdProc ThreadMutexObjCmd;
static Tcl_ObjCmdProc ThreadRWMutexObjCmd;
static Tcl_ObjCmdProc ThreadCondObjCmd;
static Tcl_ObjCmdProc ThreadEvalObjCmd;

/*
 * Forward declaration of functions used only within this file
 */

static void      SpFinalizeAll   (ClientData);

static void      SpMutexLock     (SpMutex*);
static void      SpMutexUnlock   (SpMutex*);
static void      SpMutexFinalize (SpMutex*);

static void      SpCondvWait     (SpCondv*, SpMutex*, int);
static void      SpCondvNotify   (SpCondv*);
static void      SpCondvFinalize (SpCondv*);

static void      AddAnyItem (int, char*, int, SpItem*);
static SpItem*   GetAnyItem (int, char*, int);
static void      PutAnyItem (SpItem*);
static void      DelAnyItem (SpItem*);
static Tcl_Obj*  GetHandle  (int, void*);
static SpBucket* GetBucket  (int, char*, int);

/*
 * Function-like macros for some frequently used calls
 */

#define AddMutex(a,b,c)  AddAnyItem(SP_MUTEX, (a), (b), (SpItem*)(c))
#define GetMutex(a,b)    (SpMutex*)GetAnyItem(SP_MUTEX, (a), (b))
#define PutMutex(a)      PutAnyItem((SpItem*)(a))
#define DelMutex(a)      DelAnyItem((SpItem*)(a))

#define AddCondv(a,b,c)  AddAnyItem(SP_CONDV, (a), (b), (SpItem*)(c))
#define GetCondv(a,b)    (SpCondv*)GetAnyItem(SP_CONDV, (a), (b))
#define PutCondv(a)      PutAnyItem((SpItem*)(a))
#define DelCondv(a)      DelAnyItem((SpItem*)(a))

#define IsExclusive(a)   ((a)->type == EMUTEXID)
#define IsRecursive(a)   ((a)->type == RMUTEXID)
#define IsReadWrite(a)   ((a)->type == WMUTEXID)

#define LockCount(a)     ((Sp_AnyMutex*)((a)->lock))->lockcount != 0
#define LockOwner(a)     ((Sp_AnyMutex*)((a)->lock))->owner

#define IsOwner(a)       (LockOwner(a) == Tcl_GetCurrentThread())
#define IsLocked(a)      ((Sp_AnyMutex*)((a)->lock) && LockCount(a))

#define IsWaited(a)      ((SpCondv*)(a))->waited != 0

/* 
 * This macro produces a hash-value for table-lookups given a handle
 * and its length. It is implemented as macro just for speed.
 * It is actually a trivial thing because the handles are simple
 * counting values with a small three-letter prefix.
 */

#define GetHash(a,b) (atoi((a)+((b) < 4 ? 0 : 3)) % NUMSPBUCKETS)


/*
 *----------------------------------------------------------------------
 *
 * ThreadMutexObjCmd --
 *
 *    This procedure is invoked to process "thread::mutex" Tcl command.
 *    See the user documentation for details on what it does.
 *
 * Results:
 *    A standard Tcl result.
 *
 * Side effects:
 *    See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
ThreadMutexObjCmd(dummy, interp, objc, objv)
    ClientData dummy;                   /* Not used. */
    Tcl_Interp *interp;                 /* Current interpreter. */
    int objc;                           /* Number of arguments. */
    Tcl_Obj *CONST objv[];              /* Argument objects. */
{
    int opt, ret, handleLen;
    char *mutexHandle, type;
    SpMutex *mutex;

    static CONST84 char *cmdOpts[] = {
        "create", "destroy", "lock", "unlock", NULL
    };
    enum options {
        m_CREATE, m_DESTROY, m_LOCK, m_UNLOCK,
    };
    
    /* 
     * Syntax:
     *
     *     thread::mutex create ?-recursive?
     *     thread::mutex destroy <mutexHandle>
     *     thread::mutex lock <mutexHandle>
     *     thread::mutex unlock <mutexHandle>
     */

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "option ?args?");
        return TCL_ERROR;
    }
    ret = Tcl_GetIndexFromObj(interp, objv[1], cmdOpts, "option", 0, &opt);
    if (ret != TCL_OK) {
        return TCL_ERROR;
    }

    /*
     * Cover the "create" option first. It needs no existing handle.
     */

    if (opt == (int)m_CREATE) {
        Tcl_Obj *hndObj;
        char *arg;
        
        /*
         * Parse out which type of mutex to create
         */

        if (objc == 2) {
            type = EMUTEXID;
        } else if (objc > 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "?-recursive?");
            return TCL_ERROR;
        } else {
            arg = Tcl_GetStringFromObj(objv[2], NULL);
            if (OPT_CMP(arg, "-recursive")) {
                type = RMUTEXID;
            } else {
                Tcl_WrongNumArgs(interp, 2, objv, "?-recursive?");
                return TCL_ERROR;
            }
        }

        /*
         * Create the requested mutex
         */

        mutex = (SpMutex*)Tcl_Alloc(sizeof(SpMutex));
        mutex->type   = type;
        mutex->bucket = NULL;
        mutex->hentry = NULL;
        mutex->lock   = NULL; /* Will be auto-initialized */

        /*
         * Generate Tcl handle for this mutex
         */

        hndObj = GetHandle(mutex->type, (void*)mutex);
        mutexHandle = Tcl_GetStringFromObj(hndObj, &handleLen);
        AddMutex(mutexHandle, handleLen, mutex);
        Tcl_SetObjResult(interp, hndObj);
        return TCL_OK;
    }

    /*
     * All other options require a valid handle.
     */

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "mutexHandle");
        return TCL_ERROR;
    }

    mutexHandle = Tcl_GetStringFromObj(objv[2], &handleLen);

    /*
     * Get the mutex handle. Assure it is one of the
     * supported types.
     */

    mutex = GetMutex(mutexHandle, handleLen);
    if (mutex == NULL) {
        Tcl_AppendResult(interp, "no such mutex \"", mutexHandle, "\"", NULL);
        return TCL_ERROR;
    }
    if (IsReadWrite(mutex)) {
        PutMutex(mutex);
        Tcl_AppendResult(interp, "wrong mutex type, must be either"
                         " exclusive or recursive", NULL);
        return TCL_ERROR;
    }

    switch ((enum options)opt) {
    case m_DESTROY:
        if (IsLocked(mutex)) {
            PutMutex(mutex);
            /* Should we set POSIX EBUSY here? */
            Tcl_AppendResult(interp, "mutex is already in use", NULL);
            return TCL_ERROR;
        } else {
            DelMutex(mutex);
            SpMutexFinalize(mutex);
            Tcl_Free((char*)mutex);
        }
        break;

    case m_LOCK:
        if (IsLocked(mutex)) {
            if (IsExclusive(mutex) && IsOwner(mutex)) {
                PutMutex(mutex);
                Tcl_AppendResult(interp, "locking the same exclusive mutex "
                                 "twice from the same thread", NULL);
                return TCL_ERROR;
            } else {
                PutMutex(mutex);
                SpMutexLock(mutex);
            }
        } else {
            SpMutexLock(mutex);
            PutMutex(mutex);
        }
        break;

    case m_UNLOCK:
        if (IsLocked(mutex)) {
            SpMutexUnlock(mutex);
            PutMutex(mutex);
        } else {
            PutMutex(mutex);
            Tcl_AppendResult(interp, "mutex is not locked", NULL);
            return TCL_ERROR;
        }
        break;

    default:
        break;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ThreadRwMutexObjCmd --
 *
 *    This procedure is invoked to process "thread::rwmutex" Tcl command.
 *    See the user documentation for details on what it does.
 *
 * Results:
 *    A standard Tcl result.
 *
 * Side effects:
 *    See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
ThreadRWMutexObjCmd(dummy, interp, objc, objv)
    ClientData dummy;                   /* Not used. */
    Tcl_Interp *interp;                 /* Current interpreter. */
    int objc;                           /* Number of arguments. */
    Tcl_Obj *CONST objv[];              /* Argument objects. */
{
    int opt, ret, handleLen;
    char *mutexHandle;
    SpMutex *mutex;

    static CONST84 char *cmdOpts[] = {
        "create", "destroy", "rlock", "wlock", "unlock", NULL
    };
    enum options {
        w_CREATE, w_DESTROY, w_RLOCK, w_WLOCK, w_UNLOCK,
    };
    
    /* 
     * Syntax:
     *
     *     thread::rwmutex create
     *     thread::rwmutex destroy <mutexHandle>
     *     thread::rwmutex rlock <mutexHandle>
     *     thread::rwmutex wlock <mutexHandle>
     *     thread::rwmutex unlock <mutexHandle>
     */

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "option ?args?");
        return TCL_ERROR;
    }
    ret = Tcl_GetIndexFromObj(interp, objv[1], cmdOpts, "option", 0, &opt);
    if (ret != TCL_OK) {
        return TCL_ERROR;
    }

    /*
     * Cover the "create" option first, since it needs no existing handle.
     */

    if (opt == (int)w_CREATE) {
        Tcl_Obj *hndObj;
        if (objc > 2) {
            Tcl_WrongNumArgs(interp, 1, objv, "create");
            return TCL_ERROR;
        }
        mutex = (SpMutex*)Tcl_Alloc(sizeof(SpMutex));
        mutex->type   = WMUTEXID;
        mutex->bucket = NULL;
        mutex->hentry = NULL;
        mutex->lock   = NULL; /* Will be auto-initialized */

        hndObj = GetHandle(mutex->type, (void*)mutex);
        mutexHandle = Tcl_GetStringFromObj(hndObj, &handleLen);
        AddMutex(mutexHandle, handleLen, mutex);
        Tcl_SetObjResult(interp, hndObj);
        return TCL_OK;
    }

    /*
     * All other options require a valid handle.
     */

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "mutexHandle");
        return TCL_ERROR;
    }

    mutexHandle = Tcl_GetStringFromObj(objv[2], &handleLen);

    /*
     * Get the mutex handle. Assure it is one of the
     * supported type.
     */

    mutex = GetMutex(mutexHandle, handleLen);
    if (mutex == NULL) {
        Tcl_AppendResult(interp, "no such mutex \"", mutexHandle, "\"", NULL);
        return TCL_ERROR;
    }
    if (!IsReadWrite(mutex)) {
        PutMutex(mutex);
        Tcl_AppendResult(interp, "wrong mutex type, must be readwrite", NULL);
        return TCL_ERROR;
    }
 
    switch ((enum options)opt) {
    case w_DESTROY:
        if (IsLocked(mutex)) {
            PutMutex(mutex);
            Tcl_AppendResult(interp, "mutex is already in use", NULL);
            return TCL_ERROR;
        } else {
            DelMutex(mutex);
            SpMutexFinalize(mutex);
            Tcl_Free((char*)mutex);
        }
        break;

    case w_RLOCK:
        if (IsLocked(mutex)) {
            PutMutex(mutex);
            Sp_ReadWriteMutexRLock((Sp_ReadWriteMutex*)&mutex->lock);
        } else {
            Sp_ReadWriteMutexRLock((Sp_ReadWriteMutex*)&mutex->lock);
            PutMutex(mutex);
        }
        break;

    case w_WLOCK:
        if (IsLocked(mutex)) {
            if (IsOwner(mutex)) {
                PutMutex(mutex);
                Tcl_AppendResult(interp, "write-locking the same read-write "
                                 "mutex twice from the same thread", NULL);
                return TCL_ERROR;
            }
            PutMutex(mutex);
            Sp_ReadWriteMutexWLock((Sp_ReadWriteMutex*)&mutex->lock);
        } else {
            Sp_ReadWriteMutexWLock((Sp_ReadWriteMutex*)&mutex->lock);
            PutMutex(mutex);
        }
        break;

    case w_UNLOCK:
        if (IsLocked(mutex)) {
            Sp_ReadWriteMutexUnlock((Sp_ReadWriteMutex*)&mutex->lock);
            PutMutex(mutex);
        } else {
            PutMutex(mutex);
            Tcl_AppendResult(interp, "mutex is not locked", NULL);
            return TCL_ERROR;
        }
        break;

    default:
        break;
    }

    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * ThreadCondObjCmd --
 *
 *    This procedure is invoked to process "thread::cond" Tcl command.
 *    See the user documentation for details on what it does.
 *
 * Results:
 *    A standard Tcl result.
 *
 * Side effects:
 *    See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
ThreadCondObjCmd(dummy, interp, objc, objv)
    ClientData dummy;                   /* Not used. */
    Tcl_Interp *interp;                 /* Current interpreter. */
    int objc;                           /* Number of arguments. */
    Tcl_Obj *CONST objv[];              /* Argument objects. */
{
    int opt, ret, handleLen, timeMsec = 0;
    char *condvHandle, *mutexHandle;
    SpMutex *mutex;
    SpCondv *condv;

    static CONST84 char *cmdOpts[] = {
        "create", "destroy", "notify", "wait", NULL
    };
    enum options {
        c_CREATE, c_DESTROY, c_NOTIFY, c_WAIT
    };

    /* 
     * Syntax:
     *
     *    thread::cond create
     *    thread::cond destroy <condHandle>
     *    thread::cond notify <condHandle>
     *    thread::cond wait <condHandle> <mutexHandle> ?timeout?
     */

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "option ?args?");
        return TCL_ERROR;
    }
    ret = Tcl_GetIndexFromObj(interp, objv[1], cmdOpts, "option", 0, &opt);
    if (ret != TCL_OK) {
        return TCL_ERROR;
    }

    /*
     * Cover the "create" option since it needs no existing handle.
     */

    if (opt == (int)c_CREATE) {
        Tcl_Obj *hndObj;
        if (objc > 2) {
            Tcl_WrongNumArgs(interp, 1, objv, "create");
            return TCL_ERROR;
        }
        condv = (SpCondv*)Tcl_Alloc(sizeof(SpCondv));
        condv->waited = 0;
        condv->bucket = NULL;
        condv->hentry = NULL;
        condv->cond   = NULL; /* Will be auto-initialized */


        hndObj = GetHandle(CONDVID, (void*)condv);
        condvHandle = Tcl_GetStringFromObj(hndObj, &handleLen);
        AddCondv(condvHandle, handleLen, condv);
        Tcl_SetObjResult(interp, hndObj);
        return TCL_OK;
    }

    /*
     * All others require at least a valid handle. 
     */

    if (objc < 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "condHandle ?args?");
        return TCL_ERROR;
    }

    condvHandle = Tcl_GetStringFromObj(objv[2], &handleLen);
    condv = GetCondv(condvHandle, handleLen);
    if (condv == NULL) {
        Tcl_AppendResult(interp, "no such condition variable \"",
                         condvHandle, "\"", NULL);
        return TCL_ERROR;
    }

    switch ((enum options)opt) {
    case c_DESTROY:
        if (IsWaited(condv)) {
            PutCondv(condv);
            Tcl_AppendResult(interp, "condition variable is in use", NULL);
            return TCL_ERROR;
        } else {
            DelCondv(condv);
            SpCondvFinalize(condv);
            Tcl_Free((char*)condv);
        }
        break;

    case c_WAIT:

        /*
         * May improve the Tcl_ConditionWait() to report timeouts so we can
         * inform script programmer about this interesting fact. I think 
         * there is still a place for something like Tcl_ConditionWaitEx()
         * or similar in the core.
         */

        if (objc < 4 || objc > 5) {
            Tcl_WrongNumArgs(interp, 2, objv, "condHandle mutexHandle ?timeout?");
            PutCondv(condv);
            return TCL_ERROR;
        }
        if (objc == 5) {
            if (Tcl_GetIntFromObj(interp, objv[4], &timeMsec) != TCL_OK) {
                PutCondv(condv);
                return TCL_ERROR;
            }
        }
        mutexHandle = Tcl_GetStringFromObj(objv[3], &handleLen);
        mutex = GetMutex(mutexHandle, handleLen);
        if (mutex == NULL) {
            PutCondv(condv);
            Tcl_AppendResult(interp, "no such mutex \"", mutexHandle, 
                             "\"", NULL);
            return TCL_ERROR;
        } else if (!IsExclusive(mutex) || !IsLocked(mutex)) {
            PutCondv(condv);
            PutMutex(mutex);
            Tcl_AppendResult(interp, "mutex not locked or wrong type", NULL);
            return TCL_ERROR;
        } else {
            condv->waited = 1;
            PutCondv(condv);
            PutMutex(mutex);
            SpCondvWait(condv, mutex, timeMsec);
            condv->waited = 0; /* Safe to do, since mutex is locked */
        }
        break;

    case c_NOTIFY:
        SpCondvNotify(condv);
        PutCondv(condv);
        break;

    default:
        break;
    }

    return TCL_OK;
}
/*
 *----------------------------------------------------------------------
 *
 * ThreadEvalObjCmd --
 *
 *    This procedure is invoked to process "thread::eval" Tcl command.
 *    See the user documentation for details on what it does.
 *
 * Results:
 *    A standard Tcl result.
 *
 * Side effects:
 *    See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
ThreadEvalObjCmd(dummy, interp, objc, objv)
    ClientData dummy;                   /* Not used. */
    Tcl_Interp *interp;                 /* Current interpreter. */
    int objc;                           /* Number of arguments. */
    Tcl_Obj *CONST objv[];              /* Argument objects. */
{
    int ret, optx, internal, handleLen;
    char *mutexHandle;
    Tcl_Obj *scriptObj;
    SpMutex *mutex;
    static Sp_RecursiveMutex evalMutex;

    /* 
     * Syntax:
     *
     *     thread::eval ?-lock <mutexHandle>? arg ?arg ...?
     */

    if (objc < 2) {
      syntax:
        Tcl_AppendResult(interp, "wrong # args: should be \"",
                         Tcl_GetString(objv[0]),
                         " ?-lock <mutexHandle>? arg ?arg...?\"", NULL);
        return TCL_ERROR;
    }

    /*
     * Find out wether to use the internal (recursive) mutex
     * or external mutex given on the command line, and lock
     * the corresponding mutex immediately.
     * 
     * We are using recursive internal mutex so we can easily
     * support the recursion w/o danger of deadlocking. If 
     * however, user gives us an exclusive mutex, we will 
     * throw error on attempt to recursively call us.
     */

    if (OPT_CMP(Tcl_GetString(objv[1]), "-lock") == 0) {
        internal = 1;
        optx = 1;
        Sp_RecursiveMutexLock(&evalMutex);
    } else {
        internal = 0;
        optx = 3;
        if ((objc - optx) < 1) {
            goto syntax;
        }
        mutexHandle = Tcl_GetStringFromObj(objv[2], &handleLen);
        mutex = GetMutex(mutexHandle, handleLen);
        if (mutex == NULL) {
            Tcl_AppendResult(interp, "no such mutex \"", mutexHandle, 
                             "\"", NULL);
            return TCL_ERROR;
        }
        if (IsReadWrite(mutex)) {
            PutMutex(mutex);
            Tcl_AppendResult(interp, "wrong mutex type, must be recursive "
                             "or recursive", NULL);
            return TCL_ERROR;
        }
        if (IsLocked(mutex)) {
            if (IsExclusive(mutex) && IsOwner(mutex)) {
                PutMutex(mutex);
                Tcl_AppendResult(interp, "locking the same exclusive mutex "
                                 "twice from the same thread", NULL);
                return TCL_ERROR;
            }
            PutMutex(mutex);
            SpMutexLock(mutex);
        } else {
            SpMutexLock(mutex);
            PutMutex(mutex);
        }
    }

    objc -= optx;

    /*
     * Evaluate passed arguments as Tcl script. Note that
     * Tcl_EvalObjEx throws away the passed object by 
     * doing an decrement reference count on it. This also
     * means we need not build object bytecode rep.
     */
    
    if (objc == 1) {
        scriptObj = Tcl_DuplicateObj(objv[optx]);
    } else {
        scriptObj = Tcl_ConcatObj(objc, objv + optx);
    }

    Tcl_IncrRefCount(scriptObj);
    ret = Tcl_EvalObjEx(interp, scriptObj, TCL_EVAL_DIRECT);
    Tcl_DecrRefCount(scriptObj);

    if (ret == TCL_ERROR) {
        char msg[32 + TCL_INTEGER_SPACE];   
        sprintf(msg, "\n    (\"eval\" body line %d)", interp->errorLine);
        Tcl_AddObjErrorInfo(interp, msg, -1);
    }

    /*
     * Unlock the mutex.
     */

    if (internal) {
        Sp_RecursiveMutexUnlock(&evalMutex);
    } else {
        SpMutexUnlock(mutex);
        PutMutex(mutex);
    }

    return ret;
}

/*
 *----------------------------------------------------------------------
 *
 * GetHandle --
 *
 *      Construct a Tcl handle for the given sync primitive.
 *      The handle is in the simple counted form: xidN
 *      where "x" designates the type of the handle and N
 *      is a increasing integer.
 *      We choose this simple format because it greatly 
 *      simplifies the distribution of handles accross buckets.
 *
 * Results:
 *      Tcl string object with the handle.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj*
GetHandle(int type, void *addrPtr)
{
    char handle[32];
    unsigned int id;
    static unsigned int idcounter;

    Tcl_MutexLock(&initMutex);
    id = idcounter++;
    Tcl_MutexUnlock(&initMutex);
        
    sprintf(handle, "%cid%d", type, id);
    return Tcl_NewStringObj(handle, -1);
}

/*
 *----------------------------------------------------------------------
 *
 * GetBucket --
 *
 *      Returns the bucket for the handle.
 *
 * Results:
 *      Pointer to bucket.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static SpBucket*
GetBucket(int type, char *handle, int len)
{
    switch (type) {
    case SP_MUTEX: return &muxBuckets[GetHash(handle, len)];
    case SP_CONDV: return &varBuckets[GetHash(handle, len)];
    }

    return NULL; /* Never reached */
}

/*
 *----------------------------------------------------------------------
 *
 * GetAnyItem --
 *
 *      Retrieves the item structure from it's corresponding bucket.
 *      If found, it's address is optionaly left in the itemPtrPtr.
 *
 * Results:
 *      0 - item was found
 *     -1 - item was not found
 *
 * Side effects:
 *      Bucket where the item is stored is left locked.
 *
 *----------------------------------------------------------------------
 */

static SpItem*
GetAnyItem(int type, char *handle, int len)
{
    SpBucket *bucketPtr = GetBucket(type, handle, len);
    Tcl_HashEntry *hashEntryPtr = NULL;

    Tcl_MutexLock(&bucketPtr->lock);

    hashEntryPtr = Tcl_FindHashEntry(&bucketPtr->handles, handle);
    if (hashEntryPtr == (Tcl_HashEntry*)NULL) {
        Tcl_MutexUnlock(&bucketPtr->lock);
        return NULL;
    }

    return (SpItem*)Tcl_GetHashValue(hashEntryPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * PutAnyItem --
 *
 *      Returns the item back to its bucket. This unlocks the bucket.
 *      If the item was never a part of the bucket, it is put there.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Bucket where the item is stored is unlocked.
 *
 *----------------------------------------------------------------------
 */

static void
PutAnyItem(SpItem *itemPtr)
{
    Tcl_MutexUnlock(&itemPtr->bucket->lock);
}

/*
 *----------------------------------------------------------------------
 *
 * AddAnyItem --
 *
 *      Puts any item in the corresponding bucket.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
AddAnyItem(int type, char *handle, int len, SpItem *itemPtr)
{
    int new;
    SpBucket *bucketPtr = GetBucket(type, handle, len);
    Tcl_HashEntry *hashEntryPtr;

    Tcl_MutexLock(&bucketPtr->lock);

    hashEntryPtr = Tcl_CreateHashEntry(&bucketPtr->handles, handle, &new);
    Tcl_SetHashValue(hashEntryPtr, (ClientData)itemPtr);

    itemPtr->bucket = bucketPtr;
    itemPtr->hentry = hashEntryPtr;

    Tcl_MutexUnlock(&bucketPtr->lock);
}

/*
 *----------------------------------------------------------------------
 *
 * DelAnyItem --
 *
 *      Deletes any item from it's corresponding bucket.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
DelAnyItem(SpItem *itemPtr)
{
    SpBucket *bucketPtr = itemPtr->bucket;
    Tcl_HashEntry *hashEntryPtr = itemPtr->hentry;

    Tcl_DeleteHashEntry(hashEntryPtr);
    Tcl_MutexUnlock(&bucketPtr->lock);
}

/*
 *----------------------------------------------------------------------
 *
 * Sp_Init --
 *
 *      Create commands in current interpreter.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Creates new commands in current interpreter, initializes 
 *      shared hash table for storing sync primitives handles/pointers.
 *
 *----------------------------------------------------------------------
 */

int
Sp_Init (interp)
    Tcl_Interp *interp;                 /* Interp where to create cmds */
{
    SpBucket *bucketPtr;

    if (!initOnce) {
        Tcl_MutexLock(&initMutex);
        if (!initOnce) {
            int ii, buflen = sizeof(SpBucket) * (NUMSPBUCKETS);
            char *buf  = Tcl_Alloc(2 * buflen);
            muxBuckets = (SpBucket*)(buf);
            varBuckets = (SpBucket*)(buf + buflen);
            for (ii = 0; ii < 2 * (NUMSPBUCKETS); ii++) {
                bucketPtr = &muxBuckets[ii];
                memset(bucketPtr, 0, sizeof(SpBucket));
                Tcl_InitHashTable(&bucketPtr->handles, TCL_STRING_KEYS);
            }
            initOnce = 1;
        }
        Tcl_MutexUnlock(&initMutex);
    }

    TCL_CMD(interp, THNS"::mutex",   ThreadMutexObjCmd);
    TCL_CMD(interp, THNS"::rwmutex", ThreadRWMutexObjCmd);
    TCL_CMD(interp, THNS"::cond",    ThreadCondObjCmd);
    TCL_CMD(interp, THNS"::eval",    ThreadEvalObjCmd);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SpFinalizeAll --
 *
 *      Garbage-collect hash table on application exit. 
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      Memory gets reclaimed.  
 *
 *----------------------------------------------------------------------
 */

static void
SpFinalizeAll(ClientData clientData)
{
    return; /* Does nothing since it is not safe! */
}

/*
 *----------------------------------------------------------------------
 *
 * SpMutexLock --
 *
 *      Locks the typed mutex.
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      None.  
 *
 *----------------------------------------------------------------------
 */

static void 
SpMutexLock(SpMutex *mutexPtr)
{
    
    switch (mutexPtr->type) {
    case EMUTEXID: 
        Sp_ExclusiveMutexLock((Sp_ExclusiveMutex*)&mutexPtr->lock);
        break;
    case RMUTEXID: 
        Sp_RecursiveMutexLock((Sp_RecursiveMutex*)&mutexPtr->lock);
        break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SpMutexUnlock --
 *
 *      Unlocks the typed mutex.
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      None.  
 *
 *----------------------------------------------------------------------
 */

static void 
SpMutexUnlock(SpMutex *mutexPtr)
{
    switch (mutexPtr->type) {
    case EMUTEXID: 
        Sp_ExclusiveMutexUnlock((Sp_ExclusiveMutex*)&mutexPtr->lock);
        break;
    case RMUTEXID:
        Sp_RecursiveMutexUnlock((Sp_RecursiveMutex*)&mutexPtr->lock);
        break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SpMutexFinalize --
 *
 *      Finalizes the typed mutex.
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      None.  
 *
 *----------------------------------------------------------------------
 */

static void
SpMutexFinalize(SpMutex *mutexPtr)
{
    switch (mutexPtr->type) {
    case EMUTEXID:
        Sp_ExclusiveMutexFinalize((Sp_ExclusiveMutex*)&mutexPtr->lock);
        break;
    case RMUTEXID:
        Sp_RecursiveMutexFinalize((Sp_RecursiveMutex*)&mutexPtr->lock);
        break;
    case WMUTEXID:
        Sp_ReadWriteMutexFinalize((Sp_ReadWriteMutex*)&mutexPtr->lock);
        break;
    default:
        break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SpCondvWait --
 *
 *      Waits on the condition variable.
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      None.  
 *
 *----------------------------------------------------------------------
 */

static void 
SpCondvWait(SpCondv *condvPtr, SpMutex *mutexPtr, int msec)
{
    Sp_ExclusiveMutex_ *emPtr = *(Sp_ExclusiveMutex_**)&mutexPtr->lock;
    Tcl_Time waitTime, *wt = NULL;

    if (msec > 0) {
        wt = &waitTime;
        wt->sec  = (msec/1000);
        wt->usec = (msec%1000) * 1000;
    }

    Tcl_ConditionWait(&condvPtr->cond, &emPtr->lock, wt);
}

/*
 *----------------------------------------------------------------------
 *
 * SpCondvNotify --
 *
 *      Signalizes the condition variable.
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      None.  
 *
 *----------------------------------------------------------------------
 */

static void 
SpCondvNotify(SpCondv *condvPtr)
{
    Tcl_ConditionNotify(&condvPtr->cond);
}

/*
 *----------------------------------------------------------------------
 *
 * SpCondvFinalize--
 *
 *      Finalizes the condition variable.
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      None.  
 *
 *----------------------------------------------------------------------
 */

static void 
SpCondvFinalize(SpCondv *condvPtr)
{
    if (condvPtr->cond) {
        Tcl_ConditionFinalize(&condvPtr->cond);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Sp_ExclusiveMutexLock --
 *
 *      Locks the exclusive mutex.
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      None.  
 *
 *----------------------------------------------------------------------
 */

void 
Sp_ExclusiveMutexLock(Sp_ExclusiveMutex *muxPtr)
{
    Sp_ExclusiveMutex_ *emPtr;
    Tcl_ThreadId thisThread = Tcl_GetCurrentThread();

    /*
     * Allocate the mutex structure on first access
     */

    if (*muxPtr == (Sp_ExclusiveMutex_*)0) {
        Tcl_MutexLock(&initMutex);
        if (*muxPtr == (Sp_ExclusiveMutex_*)0) {
            *muxPtr = (Sp_ExclusiveMutex_*)
                Tcl_Alloc(sizeof(Sp_ExclusiveMutex_));
            memset(*muxPtr, 0, sizeof(Sp_ExclusiveMutex_));
        }
        Tcl_MutexUnlock(&initMutex);
    }

    emPtr = *(Sp_ExclusiveMutex_**)muxPtr;

    Tcl_MutexLock(&emPtr->lock);

    emPtr->owner = thisThread;
    emPtr->lockcount++;
}

/*
 *----------------------------------------------------------------------
 *
 * Sp_ExclusiveMutexUnlock --
 *
 *      Unlock the exclusive mutex.
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      None.  
 *
 *----------------------------------------------------------------------
 */

void 
Sp_ExclusiveMutexUnlock(Sp_ExclusiveMutex *muxPtr)
{
    Sp_ExclusiveMutex_ *emPtr;

    if (*muxPtr == (Sp_ExclusiveMutex_*)0) {
        return; /* Never locked before */
    }

    emPtr = *(Sp_ExclusiveMutex_**)muxPtr;

    emPtr->owner = (Tcl_ThreadId)0;
    emPtr->lockcount--;

    Tcl_MutexUnlock(&emPtr->lock);
}

/*
 *----------------------------------------------------------------------
 *
 * Sp_ExclusiveMutexFinalize --
 *
 *      Finalize the exclusive mutex.
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      None.  
 *
 *----------------------------------------------------------------------
 */

void
Sp_ExclusiveMutexFinalize(Sp_ExclusiveMutex *muxPtr)
{
    if (*muxPtr != (Sp_ExclusiveMutex_*)0) {
        Sp_ExclusiveMutex_ *emPtr = *(Sp_ExclusiveMutex_**)muxPtr;
        if (emPtr->lock) {
            Tcl_MutexFinalize(&emPtr->lock);
        }
        Tcl_Free((char*)*muxPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Sp_RecursiveMutexLock --
 *
 *      Locks the recursive mutex.
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      None.  
 *
 *----------------------------------------------------------------------
 */

void 
Sp_RecursiveMutexLock(Sp_RecursiveMutex *muxPtr)
{
    Sp_RecursiveMutex_ *rmPtr;
    Tcl_ThreadId thisThread = Tcl_GetCurrentThread();

    /*
     * Allocate the mutex structure on first access
     */

    if (*muxPtr == (Sp_RecursiveMutex_*)0) {
        Tcl_MutexLock(&initMutex);
        if (*muxPtr == (Sp_RecursiveMutex_*)0) {
            *muxPtr = (Sp_RecursiveMutex_*)
                Tcl_Alloc(sizeof(Sp_RecursiveMutex_));
            memset(*muxPtr, 0, sizeof(Sp_RecursiveMutex_));
        }
        Tcl_MutexUnlock(&initMutex);
    }

    rmPtr = *(Sp_RecursiveMutex_**)muxPtr;
    Tcl_MutexLock(&rmPtr->lock);
    
    if (rmPtr->owner == thisThread) {
        /*
         * We are already holding the mutex
         * so just count one more lock.
         */
    	rmPtr->lockcount++;
    } else {
    	if (rmPtr->owner == (Tcl_ThreadId)0) {
            /*
             * Nobody holds the mutex, we do now.
             */
    		rmPtr->owner = thisThread;
    		rmPtr->lockcount = 1;
    	} else {
            /*
             * Somebody else holds the mutex; wait.
             */
    		while (1) {
                Tcl_ConditionWait(&rmPtr->cond, &rmPtr->lock, NULL);
    			if (rmPtr->owner == (Tcl_ThreadId)0) {
    				rmPtr->owner = thisThread;
    				rmPtr->lockcount = 1;
    				break;
    			}
    		}
    	}
    }

    Tcl_MutexUnlock(&rmPtr->lock);
}

/*
 *----------------------------------------------------------------------
 *
 * Sp_RecursiveMutexUnlock --
 *
 *      Unlock the recursive mutex.
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      None.  
 *
 *----------------------------------------------------------------------
 */

void 
Sp_RecursiveMutexUnlock(Sp_RecursiveMutex *muxPtr)
{
    Sp_RecursiveMutex_ *rmPtr;

    if (*muxPtr == (Sp_RecursiveMutex_*)0) {
        return; /* Never locked before */
    }

    rmPtr = *(Sp_RecursiveMutex_**)muxPtr;
    Tcl_MutexLock(&rmPtr->lock);

    if (--rmPtr->lockcount <= 0) {
        rmPtr->lockcount = 0;
        rmPtr->owner = (Tcl_ThreadId)0;
        Tcl_ConditionNotify(&rmPtr->cond);
    }

    Tcl_MutexUnlock(&rmPtr->lock);
}

/*
 *----------------------------------------------------------------------
 *
 * Sp_RecursiveMutexFinalize --
 *
 *      Finalize the recursive mutex.
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      None.  
 *
 *----------------------------------------------------------------------
 */

void
Sp_RecursiveMutexFinalize(Sp_RecursiveMutex *muxPtr)
{
    if (*muxPtr != (Sp_RecursiveMutex_*)0) {
        Sp_RecursiveMutex_ *rmPtr = *(Sp_RecursiveMutex_**)muxPtr;
        if (rmPtr->lock) {
            Tcl_MutexFinalize(&rmPtr->lock);
        }
        if (rmPtr->cond) {
            Tcl_ConditionFinalize(&rmPtr->cond);
        }
        Tcl_Free((char*)*muxPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Sp_ReadWriteMutexRLock --
 *
 *      Read-locks the reader/writer mutex.
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      None.  
 *
 *----------------------------------------------------------------------
 */

void
Sp_ReadWriteMutexRLock(Sp_ReadWriteMutex *muxPtr)
{
    Sp_ReadWriteMutex_ *rwPtr;

    /*
     * Allocate the mutex structure on first access
     */

    if (*muxPtr == (Sp_ReadWriteMutex_*)0) {
        Tcl_MutexLock(&initMutex);
        if (*muxPtr == (Sp_ReadWriteMutex_*)0) {
            *muxPtr = (Sp_ReadWriteMutex_*)
                Tcl_Alloc(sizeof(Sp_ReadWriteMutex_));
            memset(*muxPtr, 0, sizeof(Sp_ReadWriteMutex_));
        }
        Tcl_MutexUnlock(&initMutex);
    }

    rwPtr = *(Sp_ReadWriteMutex_**)muxPtr;
    Tcl_MutexLock(&rwPtr->lock);

    while (rwPtr->lockcount < 0 || rwPtr->numwr > 0) {
        rwPtr->numrd++;
        Tcl_ConditionWait(&rwPtr->rcond, &rwPtr->lock, NULL);
        rwPtr->numrd--;
    }
    rwPtr->lockcount++;
    rwPtr->owner = (Tcl_ThreadId)0;

    Tcl_MutexUnlock(&rwPtr->lock);
}

/*
 *----------------------------------------------------------------------
 *
 * Sp_ReadWriteMutexWLock --
 *
 *      Write-locks the reader/writer mutex.
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      None.  
 *
 *----------------------------------------------------------------------
 */

void
Sp_ReadWriteMutexWLock(Sp_ReadWriteMutex *muxPtr)
{
    Sp_ReadWriteMutex_ *rwPtr;
    Tcl_ThreadId thisThread = Tcl_GetCurrentThread();

    /*
     * Allocate the mutex structure on first access
     */

    if (*muxPtr == (Sp_ReadWriteMutex_*)0) {
        Tcl_MutexLock(&initMutex);
        if (*muxPtr == (Sp_ReadWriteMutex_*)0) {
            *muxPtr = (Sp_ReadWriteMutex_*)
                Tcl_Alloc(sizeof(Sp_ReadWriteMutex_));
            memset(*muxPtr, 0, sizeof(Sp_ReadWriteMutex_));
        }
        Tcl_MutexUnlock(&initMutex);
    }

    rwPtr = *(Sp_ReadWriteMutex_**)muxPtr;
    Tcl_MutexLock(&rwPtr->lock);

    while (rwPtr->lockcount != 0) {
        rwPtr->numwr++;
        Tcl_ConditionWait(&rwPtr->wcond, &rwPtr->lock, NULL);
        rwPtr->numwr--;
    }
    rwPtr->lockcount = -1; /* This designates the sole writer */
    rwPtr->owner = thisThread;

    Tcl_MutexUnlock(&rwPtr->lock);
}

/*
 *----------------------------------------------------------------------
 *
 * Sp_ReadWriteMutexUnlock --
 *
 *      Unlock the reader/writer mutex.
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

void
Sp_ReadWriteMutexUnlock(Sp_ReadWriteMutex *muxPtr)
{
    Sp_ReadWriteMutex_ *rwPtr;

    if (*muxPtr == (Sp_ReadWriteMutex_*)0) {
        return; /* Never locked before */
    }
    
    rwPtr = *(Sp_ReadWriteMutex_**)muxPtr;
    Tcl_MutexLock(&rwPtr->lock);

    if (--rwPtr->lockcount < 0) {
        rwPtr->lockcount = 0;
        rwPtr->owner = (Tcl_ThreadId)0;
    }
    if (rwPtr->numwr) {
        Tcl_ConditionNotify(&rwPtr->wcond);
    } else if (rwPtr->numrd) {
        Tcl_ConditionNotify(&rwPtr->rcond);
    }

    Tcl_MutexUnlock(&rwPtr->lock);
}

/*
 *----------------------------------------------------------------------
 *
 * Sp_ReadWriteMutexFinalize --
 *
 *      Finalizes the reader/writer mutex.
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      None.  
 *
 *----------------------------------------------------------------------
 */

void
Sp_ReadWriteMutexFinalize(Sp_ReadWriteMutex *muxPtr)
{
    if (*muxPtr != (Sp_ReadWriteMutex_*)0) {
        Sp_ReadWriteMutex_ *rwPtr = *(Sp_ReadWriteMutex_**)muxPtr;
        if (rwPtr->lock) {
            Tcl_MutexFinalize(&rwPtr->lock);
        }
        if (rwPtr->rcond) {
            Tcl_ConditionFinalize(&rwPtr->rcond);
        }
        if (rwPtr->wcond) {
            Tcl_ConditionFinalize(&rwPtr->wcond);
        }
        Tcl_Free((char*)*muxPtr);
    }
}


/* EOF $RCSfile: threadSpCmd.c,v $ */

/* Emacs Setup Variables */
/* Local Variables:      */
/* mode: C               */
/* indent-tabs-mode: nil */
/* c-basic-offset: 4     */
/* End:                  */
