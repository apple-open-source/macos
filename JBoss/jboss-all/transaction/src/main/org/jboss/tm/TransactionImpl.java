/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.tm;

import org.jboss.logging.Logger;
import org.jboss.util.timeout.Timeout;
import org.jboss.util.timeout.TimeoutFactory;
import org.jboss.util.timeout.TimeoutTarget;

import javax.transaction.HeuristicMixedException;
import javax.transaction.HeuristicRollbackException;
import javax.transaction.RollbackException;
import javax.transaction.Status;
import javax.transaction.Synchronization;
import javax.transaction.SystemException;
import javax.transaction.Transaction;
import javax.transaction.xa.XAException;
import javax.transaction.xa.XAResource;
import javax.transaction.xa.Xid;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.ArrayList;


/**
 *  Our <code>Transaction</code> implementation.
 *
 *  @see TxManager
 *
 *  @author <a href="mailto:rickard.oberg@telkel.com">Rickard Ãberg</a>
 *  @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @author <a href="mailto:toby.allsopp@peace.com">Toby Allsopp</a>
 *  @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 *  @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 *  @author <a href="mailto:bill@jboss.org">Bill Burke</a>
 *  @version $Revision: 1.5.2.16 $
 */
class TransactionImpl
   implements Transaction, TimeoutTarget
{
   // Constants -----------------------------------------------------

   /**
    * Code meaning "no heuristics seen",
    * must not be XAException.XA_HEURxxx
    */
   private static final int HEUR_NONE = XAException.XA_RETRY;

   // Resource states
   private final static int RS_NEW           = 0; // not yet enlisted
   private final static int RS_ENLISTED      = 1; // enlisted
   private final static int RS_SUSPENDED     = 2; // suspended
   private final static int RS_ENDED         = 3; // not associated
   private final static int RS_VOTE_READONLY = 4; // voted read-only
   private final static int RS_VOTE_OK       = 5; // voted ok
   private final static int RS_FORGOT        = 6; // RM has forgotten     


   // Attributes ----------------------------------------------------

   /** Class logger, we don't want a new logger with every transaction. */
   private static Logger log = Logger.getLogger(TransactionImpl.class);

   /** True if trace messages should be logged. */
   private boolean trace = log.isTraceEnabled();

   /** The ID of this transaction. */
   private Xid xid;

   private ArrayList threads = new ArrayList(1);

   private HashMap transactionLocalMap = new HashMap();

   private Throwable cause;

   /**
    *  The global ID of this transaction.
    *  This is used as a transaction propagation context, and in the
    *  TxManager for mapping transaction IDs to transactions.
    */
   private GlobalId globalId;

   /**
    *  The synchronizations to call back.
    */
   private Synchronization[] sync = new Synchronization[3];

   /**
    *  Size of allocated synchronization array.
    */
   private int syncAllocSize = 3;

   /**
    *  Count of synchronizations for this transaction.
    */
   private int syncCount = 0;

   /**
    *  A list of the XARessources that have participated in this transaction.
    */
   private XAResource[] resources = new XAResource[3];

   /**
    *  The state of the resources.
    */
   private int[] resourceState = new int[3];

   /**
    *  Index of the first XAResource representing the same resource manager,
    *  or <code>-1</code> if this XAResource is the first XAResource in this
    *  transaction that represents its resource manager.
    */
   private int[] resourceSameRM = new int[3];

   /**
    *  A list of the XARessources that have participated in this transaction.
    */
   private Xid[] resourceXids = new Xid[3];

   /**
    *  Size of allocated resource arrays.
    */
   private int resourceAllocSize = 3;

   /**
    *  Count of resources that have participated in this transaction.
    *  This contains a count of all XAResources, not a count of distinct
    *  resource managers.
    *  It is the length of resources and other such arrays.
    */
   private int resourceCount = 0;

   /**
    *  Flags that it is too late to enlist new resources.
    */
   private boolean resourcesEnded = false;

   /**
    *  Last branch id used.
    */
   private long lastBranchId = 0;

   /**
    *  Status of this transaction.
    */
   private int status;

   /**
    *  The heuristics status of this transaction.
    */
   private int heuristicCode = HEUR_NONE;

   /**
    *  The time when this transaction was started.
    */
   private long start;

   /**
    *  The timeout handle for this transaction.
    */
   private Timeout timeout;

   /**
    * Timeout in millisecs
    */
   private long timeoutPeriod;
   /**
    *  Mutex for thread-safety. This should only be changed in the
    *  <code>lock()</code> and <code>unlock()</code> methods.
    */
   private boolean locked = false;

   /**
    *  Flags that we are done with this transaction and that it can be reused.
    */
   private boolean done = false;

   // Static --------------------------------------------------------

   /**
    *  Factory for Xid instances of specified class.
    *  This is set from the <code>TransactionManagerService</code>
    *  MBean.
    */
   static XidFactoryMBean xidFactory;

   static TransactionManagerService txManagerService;

   /**
    * This static code is only present for testing purposes so a
    * tm can be usable without a lot of setup.
    *
    */
   static void defaultXidFactory()
   {
      if (xidFactory == null)
      {
         xidFactory = new XidFactory();
      } // end of if ()
   }


   // Constructors --------------------------------------------------

   TransactionImpl(long timeout)
   {
      xid = xidFactory.newXid();
      globalId = new GlobalId(xid);

      status = Status.STATUS_ACTIVE;

      start = System.currentTimeMillis();
      this.timeout = TimeoutFactory.createTimeout(start+timeout, this);
      this.timeoutPeriod = timeout;
      if (trace)
         log.trace("Created new instance for tx=" + toString());

   }

   // Implements TimeoutTarget --------------------------------------

   /**
    *  Called when our timeout expires.
    */
   public void timedOut(Timeout timeout)
   {
      try
      {
         lock();

         log.warn("Transaction " + toString() + " timed out." +
                  " status=" + getStringStatus(status));

         if (this.timeout == null)
            return; // Don't race with timeout cancellation.
         this.timeout = null;

         switch (status)
         {
            case Status.STATUS_ROLLEDBACK:
            case Status.STATUS_COMMITTED:
            case Status.STATUS_NO_TRANSACTION:
               return; // Transaction done.

            case Status.STATUS_ROLLING_BACK:
               return; // Will be done shortly.

            case Status.STATUS_COMMITTING:
               // This is _very_ bad:
               // We are in the second commit phase, and have decided
               // to commit, but now we get a timeout and should rollback.
               // So we end up with a mixed decision.
               gotHeuristic(-1, XAException.XA_HEURMIX);
               status = Status.STATUS_MARKED_ROLLBACK;
               return; // commit will fail

            case Status.STATUS_PREPARED:
               // This is bad:
               // We are done with the first phase, and are persistifying
               // our decision. Fortunately this case is currently never
               // hit, as we do not release the lock between the two phases.
            case Status.STATUS_ACTIVE:
               status = Status.STATUS_MARKED_ROLLBACK;
               // fall through..
            case Status.STATUS_MARKED_ROLLBACK:
               // don't rollback for now, this messes up with the TxInterceptor.
               interruptThreads();
               return;

            case Status.STATUS_PREPARING:
               status = Status.STATUS_MARKED_ROLLBACK;
               return; // commit will fail

            default:
               log.warn("Unknown status at timeout, tx=" + toString());
               return;
         }
      } finally
      {
         unlock();
      }
   }

   // Implements Transaction ----------------------------------------

   public void commit()
      throws RollbackException,
             HeuristicMixedException,
             HeuristicRollbackException,
             java.lang.SecurityException,
             java.lang.IllegalStateException,
             SystemException
   {
      try
      {
         lock();

         if (trace)
         {
            log.trace("Committing, tx=" + this +
                      ", status=" + getStringStatus(status));
         }

         switch (status)
         {
            case Status.STATUS_PREPARING:
               throw new IllegalStateException("Already started preparing.");
            case Status.STATUS_PREPARED:
               throw new IllegalStateException("Already prepared.");
            case Status.STATUS_ROLLING_BACK:
               throw new IllegalStateException("Already started rolling back.");
            case Status.STATUS_ROLLEDBACK:
               instanceDone();
               checkHeuristics();
               throw new IllegalStateException("Already rolled back.");
            case Status.STATUS_COMMITTING:
               throw new IllegalStateException("Already started committing.");
            case Status.STATUS_COMMITTED:
               instanceDone();
               checkHeuristics();
               throw new IllegalStateException("Already committed.");
            case Status.STATUS_NO_TRANSACTION:
               throw new IllegalStateException("No transaction.");
            case Status.STATUS_UNKNOWN:
               throw new IllegalStateException("Unknown state");
            case Status.STATUS_MARKED_ROLLBACK:
               doBeforeCompletion();
               endResources();
               rollbackResources();
               doAfterCompletion();
               cancelTimeout();
               instanceDone();
               checkHeuristics();
               throw new RollbackException("Already marked for rollback");
            case Status.STATUS_ACTIVE:
               break;
            default:
               throw new IllegalStateException("Illegal status: " + status);
         }

         doBeforeCompletion();

         if (trace)
         {
            log.trace("Before completion done, tx=" + this +
            ", status=" + getStringStatus(status));
         }

         endResources();

         if (status == Status.STATUS_ACTIVE)
         {
            if (resourceCount == 0)
            {
               // Zero phase commit is really fast ;-)
               if (trace)
               {
                  log.trace("Zero phase commit: No resources.");
               }
               status = Status.STATUS_COMMITTED;
            }
            else if (isOneResource())
            {
               // One phase commit
               if (trace)
               {
                  log.trace("One phase commit: One resource.");
               }
               commitResources(true);
            } else
            {
               // Two phase commit
               if (trace)
               {
                  log.trace("Two phase commit: Many resources.");
               }

               if (!prepareResources())
               {
                  boolean commitDecision =
                     status == Status.STATUS_PREPARED &&
                     (heuristicCode == HEUR_NONE ||
                      heuristicCode == XAException.XA_HEURCOM);

                  // TODO: Save decision to stable storage for recovery
                  //       after system crash.

                  if (commitDecision)
                     commitResources(false);
               } else
                  status = Status.STATUS_COMMITTED; // all was read-only
            }
         }

         if (status != Status.STATUS_COMMITTED)
         {
            rollbackResources();
            doAfterCompletion();
            cancelTimeout();

            // save off the cause throwable as Instance done resets it to null
            Throwable causedByThrowable = cause;

            instanceDone();

            // throw jboss rollback exception with the saved off cause
            throw new JBossRollbackException("Unable to commit, tx=" +
                  toString() + " status=" + getStringStatus(status),
                  causedByThrowable);
         }

         cancelTimeout();
         doAfterCompletion();
         instanceDone();
         checkHeuristics();

         if (trace)
         {
            log.trace("Committed OK, tx=" + this);
         }

      } finally {
         transactionLocalMap.clear();
         threads.clear();
         unlock();
      }
   }

   public void rollback()
      throws java.lang.IllegalStateException,
             java.lang.SecurityException,
             SystemException
   {
      try
      {
         lock();

         if (trace)
         {
            log.trace("rollback(): Entered, tx=" + toString() +
            " status=" + getStringStatus(status));
         }

         switch (status)
         {
            case Status.STATUS_ACTIVE:
               status = Status.STATUS_MARKED_ROLLBACK;
               // fall through..
            case Status.STATUS_MARKED_ROLLBACK:
               doBeforeCompletion();
               endResources();
               rollbackResources();
               cancelTimeout();
               doAfterCompletion();
               instanceDone();
               // Cannot throw heuristic exception, so we just have to
               // clear the heuristics without reporting.
               heuristicCode = HEUR_NONE;
               return;
            case Status.STATUS_PREPARING:
               // Set status to avoid race with prepareResources().
               status = Status.STATUS_MARKED_ROLLBACK;
               return; // commit() will do rollback.
            default:
               throw new IllegalStateException("Cannot rollback(), " +
               "tx=" + toString() +
               " status=" +
               getStringStatus(status));
         }
      } finally {
         transactionLocalMap.clear();
         threads.clear();
         Thread.interrupted();// clear timeout that did an interrupt
         unlock();
      }
   }

   public boolean delistResource(XAResource xaRes, int flag)
      throws java.lang.IllegalStateException,
             SystemException
   {
      if (xaRes == null)
         throw new IllegalArgumentException("null xaRes");
      if (flag != XAResource.TMSUCCESS &&
          flag != XAResource.TMSUSPEND &&
          flag != XAResource.TMFAIL)
         throw new IllegalArgumentException("Bad flag: " + flag);

      try
      {
         lock();

         if (trace)
         {
            log.trace("delistResource(): Entered, tx=" +
            toString() + " status=" + getStringStatus(status));
         }

         int idx = findResource(xaRes);

         if (idx == -1)
            throw new IllegalArgumentException("xaRes not enlisted");

         switch (status)
         {
            case Status.STATUS_ACTIVE:
            case Status.STATUS_MARKED_ROLLBACK:
               break;
            case Status.STATUS_PREPARING:
               throw new IllegalStateException("Already started preparing.");
            case Status.STATUS_ROLLING_BACK:
               throw new IllegalStateException("Already started rolling back.");
            case Status.STATUS_PREPARED:
               throw new IllegalStateException("Already prepared.");
            case Status.STATUS_COMMITTING:
               throw new IllegalStateException("Already started committing.");
            case Status.STATUS_COMMITTED:
               throw new IllegalStateException("Already committed.");
            case Status.STATUS_ROLLEDBACK:
               throw new IllegalStateException("Already rolled back.");
            case Status.STATUS_NO_TRANSACTION:
               throw new IllegalStateException("No transaction.");
            case Status.STATUS_UNKNOWN:
               throw new IllegalStateException("Unknown state");
            default:
               throw new IllegalStateException("Illegal status: " + status);
         }

         try
         {
            if (resourceState[idx] == RS_ENDED && !resources[idx].isSameRM(xaRes)) {
               // This RM always returns false on isSameRM.  Further,
               // the last resource has already been delisted.
               log.warn("Resource already delisted.  tx=" + toString());
               return false;
            }
            endResource(idx, flag);
            return true;
         } catch (XAException xae)
         {
            logXAException(xae);
            status = Status.STATUS_MARKED_ROLLBACK;
            cause = xae;
            return false;
         }
      } finally
      {
         unlock();
      }
   }

   public boolean enlistResource(XAResource xaRes)
      throws RollbackException,
             java.lang.IllegalStateException,
             SystemException
   {
      if (xaRes == null)
         throw new IllegalArgumentException("null xaRes");

      try
      {
         lock();

         if (trace)
         {
            log.trace("enlistResource(): Entered, tx=" +
            toString() + " status=" + getStringStatus(status));
         }

         switch (status)
         {
            case Status.STATUS_ACTIVE:
            case Status.STATUS_PREPARING:
               break;
            case Status.STATUS_PREPARED:
               throw new IllegalStateException("Already prepared.");
            case Status.STATUS_COMMITTING:
               throw new IllegalStateException("Already started committing.");
            case Status.STATUS_COMMITTED:
               throw new IllegalStateException("Already committed.");
            case Status.STATUS_MARKED_ROLLBACK:
               throw new RollbackException("Already marked for rollback");
            case Status.STATUS_ROLLING_BACK:
               throw new RollbackException("Already started rolling back.");
            case Status.STATUS_ROLLEDBACK:
               throw new RollbackException("Already rolled back.");
            case Status.STATUS_NO_TRANSACTION:
               throw new IllegalStateException("No transaction.");
            case Status.STATUS_UNKNOWN:
               throw new IllegalStateException("Unknown state");
            default:
               throw new IllegalStateException("Illegal status: " + status);
         }

         if (resourcesEnded)
            throw new IllegalStateException("Too late to enlist resources");

         // Add resource
         try
         {
            int idx = findResource(xaRes);

            if (idx != -1)
            {
               if (resourceState[idx] == RS_ENLISTED)
                  return false; // already enlisted
               if (resourceState[idx] == RS_ENDED && !resources[idx].isSameRM(xaRes)) {
                  // this is a resource that returns false on all calls to
                  // isSameRM.  Further, the last resource enlisted has
                  // already been delisted, so it is time to enlist it again.
                  idx = -1;
               } else {
                  startResource(idx);
                  return true;
               }
            }

            for (int i = 0; i < resourceCount; ++i) {
               if (resourceSameRM[i] == -1 && xaRes.isSameRM(resources[i])) {
                  // The xaRes is new. We register the xaRes with the Xid
                  // that the RM has previously seen from this transaction,
                  // and note that it has the same RM.
                  startResource(addResource(xaRes, resourceXids[i], i));

                  return true;
               }
            }

            // New resource and new RM: Create a new transaction branch.
            startResource(addResource(xaRes, createXidBranch(), -1));
            return true;
         } catch (XAException xae)
         {
            logXAException(xae);
            cause = xae;
            return false;
         }
      } finally
      {
         unlock();
      }

   }

   public int getStatus()
      throws SystemException
   {
      if (done)
         return Status.STATUS_NO_TRANSACTION;
      return status;
   }

   public void registerSynchronization(Synchronization s)
      throws RollbackException,
             java.lang.IllegalStateException,
             SystemException
   {
      if (s == null)
         throw new IllegalArgumentException("Null synchronization");

      try
      {
         lock();

         if (trace)
         {
            log.trace("registerSynchronization(): Entered, " +
            "tx=" + toString() +
            " status=" + getStringStatus(status));
         }

         switch (status) {
         case Status.STATUS_ACTIVE:
         case Status.STATUS_PREPARING:
            break;
         case Status.STATUS_PREPARED:
            throw new IllegalStateException("Already prepared.");
         case Status.STATUS_COMMITTING:
            throw new IllegalStateException("Already started committing.");
         case Status.STATUS_COMMITTED:
            throw new IllegalStateException("Already committed.");
         case Status.STATUS_MARKED_ROLLBACK:
            throw new RollbackException("Already marked for rollback");
         case Status.STATUS_ROLLING_BACK:
            throw new RollbackException("Already started rolling back.");
         case Status.STATUS_ROLLEDBACK:
            throw new RollbackException("Already rolled back.");
         case Status.STATUS_NO_TRANSACTION:
            throw new IllegalStateException("No transaction.");
         case Status.STATUS_UNKNOWN:
            throw new IllegalStateException("Unknown state");
         default:
            throw new IllegalStateException("Illegal status: " + status);
         }

         if (syncCount == syncAllocSize)
         {
            // expand table
            syncAllocSize = 2 * syncAllocSize;

            Synchronization[] sy = new Synchronization[syncAllocSize];
            System.arraycopy(sync, 0, sy, 0, syncCount);
            sync = sy;
         }
         sync[syncCount++] = s;
      } finally
      {
         unlock();
      }
   }

   public void setRollbackOnly()
      throws java.lang.IllegalStateException,
             SystemException
   {
      try {
         lock();

         if (trace)
            log.trace("setRollbackOnly(): Entered, tx=" +
                      toString() + " status=" + getStringStatus(status));

         switch (status) {
            case Status.STATUS_ACTIVE:
            case Status.STATUS_PREPARING:
            case Status.STATUS_PREPARED:
               status = Status.STATUS_MARKED_ROLLBACK;
               // fall through..
            case Status.STATUS_MARKED_ROLLBACK:
            case Status.STATUS_ROLLING_BACK:
               return;
            case Status.STATUS_COMMITTING:
               throw new IllegalStateException("Already started committing.");
            case Status.STATUS_COMMITTED:
               throw new IllegalStateException("Already committed.");
            case Status.STATUS_ROLLEDBACK:
               throw new IllegalStateException("Already rolled back.");
            case Status.STATUS_NO_TRANSACTION:
               throw new IllegalStateException("No transaction.");
            case Status.STATUS_UNKNOWN:
               throw new IllegalStateException("Unknown state");
            default:
               throw new IllegalStateException("Illegal status: " + status);
         }
      } finally {
         unlock();
      }
   }

   // Public --------------------------------------------------------

   public void associateCurrentThread()
   {
      threads.add(Thread.currentThread());
   }

   public void disassociateCurrentThread()
   {
      threads.remove(Thread.currentThread());
      Thread.interrupted();
   }
   public void clearThreads()
   {
      for (int i = 0; i < threads.size(); i++)
      {
         Thread t = (Thread)threads.get(i);
      }
   }
   public int hashCode()
   {
      return globalId.hashCode();
   }

   public String toString()
   {
      return "TransactionImpl:" + xidFactory.toString(xid);
   }

   public boolean equals(Object obj)
   {
      if (obj != null && obj instanceof TransactionImpl)
         return globalId.equals(((TransactionImpl)obj).globalId);
      return false;
   }


   // Package protected ---------------------------------------------

   /**
    *  Getter for property done.
    */
   boolean isDone()
   {
      return done;
   }

   /**
    *  Return the global id of this transaction.
    */
   GlobalId getGlobalId()
   {
      return globalId;
   }


   // Private -------------------------------------------------------

   /**
    * Interrupt all threads involved with transaction
    * This is called on timeout
    */
   private void interruptThreads()
   {
      Iterator it = threads.iterator();
      while (it.hasNext())
      {
         Thread thread = (Thread)it.next();
         try
         {
            thread.interrupt();
         }
         catch (Exception ignored) {}
      }
      threads.clear();
   }

   /**
    *  Return a string representation of the given status code.
    */
   private String getStringStatus(int status)
   {
      switch (status) {
         case Status.STATUS_PREPARING:
            return "STATUS_PREPARING";
         case Status.STATUS_PREPARED:
            return "STATUS_PREPARED";
         case Status.STATUS_ROLLING_BACK:
            return "STATUS_ROLLING_BACK";
         case Status.STATUS_ROLLEDBACK:
            return "STATUS_ROLLEDBACK";
         case Status.STATUS_COMMITTING:
            return "STATUS_COMMITING";
         case Status.STATUS_COMMITTED:
            return "STATUS_COMMITED";
         case Status.STATUS_NO_TRANSACTION:
            return "STATUS_NO_TRANSACTION";
         case Status.STATUS_UNKNOWN:
            return "STATUS_UNKNOWN";
         case Status.STATUS_MARKED_ROLLBACK:
            return "STATUS_MARKED_ROLLBACK";
         case Status.STATUS_ACTIVE:
            return "STATUS_ACTIVE";

         default:
            return "STATUS_UNKNOWN(" + status + ")";
      }
   }

   /**
    *  Return a string representation of the given XA error code.
    */
   private String getStringXAErrorCode(int errorCode)
   {
      switch (errorCode) {
         case XAException.XA_HEURCOM:
            return "XA_HEURCOM";
         case XAException.XA_HEURHAZ:
            return "XA_HEURHAZ";
         case XAException.XA_HEURMIX:
            return "XA_HEURMIX";
         case XAException.XA_HEURRB:
            return "XA_HEURRB";

         case XAException.XA_NOMIGRATE:
            return "XA_NOMIGRATE";

         case XAException.XA_RBCOMMFAIL:
            return "XA_RBCOMMFAIL";
         case XAException.XA_RBDEADLOCK:
            return "XA_RBDEADLOCK";
         case XAException.XA_RBINTEGRITY:
            return "XA_RBINTEGRITY";
         case XAException.XA_RBOTHER:
            return "XA_RBOTHER";
         case XAException.XA_RBPROTO:
            return "XA_RBPROTO";
         case XAException.XA_RBROLLBACK:
            return "XA_RBROLLBACK";
         case XAException.XA_RBTIMEOUT:
            return "XA_RBTIMEOUT";
         case XAException.XA_RBTRANSIENT:
            return "XA_RBTRANSIENT";

         case XAException.XA_RDONLY:
            return "XA_RDONLY";
         case XAException.XA_RETRY:
            return "XA_RETRY";

         case XAException.XAER_ASYNC:
            return "XAER_ASYNC";
         case XAException.XAER_DUPID:
            return "XAER_DUPID";
         case XAException.XAER_INVAL:
            return "XAER_INVAL";
         case XAException.XAER_NOTA:
            return "XAER_NOTA";
         case XAException.XAER_OUTSIDE:
            return "XAER_OUTSIDE";
         case XAException.XAER_PROTO:
            return "XAER_PROTO";
         case XAException.XAER_RMERR:
            return "XAER_RMERR";
         case XAException.XAER_RMFAIL:
            return "XAER_RMFAIL";

         default:
            return "XA_UNKNOWN(" + errorCode + ")";
      }
   }

   private void logXAException(XAException xae)
   {
      log.warn("XAException: tx=" + toString() + " errorCode=" +
               getStringXAErrorCode(xae.errorCode), xae);
      if (txManagerService != null)
      {
         txManagerService.formatXAException(xae, log);
      } // end of if ()
   }

   /**
    *  Lock this instance.
    */
   private synchronized void lock()
   {
      if (done)
         throw new IllegalStateException("Transaction has terminated");

      if (locked) {
         log.warn("Lock contention, tx=" + toString());
         //DEBUG Thread.currentThread().dumpStack();

         while (locked) {
            try {
               // Wakeup happens when:
               // - notify() is called from unlock()
               // - notifyAll is called from instanceDone()
               wait();
            } catch (InterruptedException ex) {
               // ignore
            }

            if (done)
               throw new IllegalStateException("Transaction has now terminated");
         }
      }

      locked = true;
   }

   /**
    *  Unlock this instance.
    */
   private synchronized void unlock()
   {
      if (!locked)
      {
         log.warn("Unlocking, but not locked, tx=" + toString(),
         new Throwable("[Stack trace]"));
      }

      locked = false;

      notify();
   }

   /**
    *  Mark this transaction as non-existing.
    */
   private synchronized void instanceDone()
   {
      TxManager manager = TxManager.getInstance();

      if (status == Status.STATUS_COMMITTED)
         manager.incCommitCount();
      else
         manager.incRollbackCount();

      // Garbage collection
      manager.releaseTransactionImpl(this);

      // Set the status
      status = Status.STATUS_NO_TRANSACTION;

      // Clear tables refering to external objects.
      // Even if a client holds on to this instance forever, the objects
      // that we have referenced may be garbage collected.
      sync = null;
      resources = null;

      // Notify all threads waiting for the lock.
      notifyAll();

      // set the done flag
      done = true;
   }

   /**
    *  Cancel the timeout.
    *  This will release the lock while calling out.
    */
   private void cancelTimeout()
   {
      if (timeout != null) {
         unlock();
         try
         {
            timeout.cancel();
         } catch (Exception e)
         {
            if (trace)
               log.trace("failed to cancel timeout", e);
         } finally
         {
            lock();
         }
         timeout = null;
      }
   }

   /**
    *  Return index of XAResource, or <code>-1</code> if not found.
    */
   private int findResource(XAResource xaRes)
   {
      // A linear search may seem slow, but please note that
      // the number of XA resources registered with a transaction
      // are usually low.
      // Note: This searches backwards intentionally!  It ensures that
      // if this resource was enlisted multiple times, then the last one
      // will be returned.  All others should be in the state RS_ENDED.
      // This allows ResourceManagers that always return false from isSameRM
      // to be enlisted and delisted multiple times.
      for (int idx = resourceCount - 1; idx >= 0; --idx)
         if (xaRes == resources[idx])
            return idx;

      return -1;
   }

   /**
    *  Add a resource, expanding tables if needed.
    *
    *  @param xaRes The new XA resource to add. It is assumed that the
    *         resource is not already in the table of XA resources.
    *  @param branchXid The Xid for the transaction branch that is to
    *         be used for associating with this resource.
    *  @param idxSameRM The index in our XA resource tables of the first
    *         XA resource having the same resource manager as
    *         <code>xaRes</code>, or <code>-1</code> if <code>xaRes</code>
    *         is the first resource seen with this resource manager.
    *
    *  @return The index of the new resource in our internal tables.
    */
   private int addResource(XAResource xaRes, Xid branchXid, int idxSameRM)
   {
      if (resourceCount == resourceAllocSize)
      {
         // expand tables
         resourceAllocSize = 2 * resourceAllocSize;

         XAResource[] res = new XAResource[resourceAllocSize];
         System.arraycopy(resources, 0, res, 0, resourceCount);
         resources = res;

         int[] stat = new int[resourceAllocSize];
         System.arraycopy(resourceState, 0, stat, 0, resourceCount);
         resourceState = stat;

         Xid[] xids = new Xid[resourceAllocSize];
         System.arraycopy(resourceXids, 0, xids, 0, resourceCount);
         resourceXids = xids;

         int[] sameRM = new int[resourceAllocSize];
         System.arraycopy(resourceSameRM, 0, sameRM, 0, resourceCount);
         resourceSameRM = sameRM;
      }
      resources[resourceCount] = xaRes;
      resourceState[resourceCount] = RS_NEW;
      resourceXids[resourceCount] = branchXid;
      resourceSameRM[resourceCount] = idxSameRM;

      return resourceCount++;
   }

   /**
    *  Call <code>start()</code> on a XAResource and update
    *  internal state information.
    *  This will release the lock while calling out.
    *
    *  @param idx The index of the resource in our internal tables.
    */
   private void startResource(int idx)
      throws XAException
   {
      int flags = XAResource.TMJOIN;

      if (resourceSameRM[idx] == -1)
      {
         switch (resourceState[idx])
         {
            case RS_NEW:
               flags = XAResource.TMNOFLAGS;
               break;
            case RS_SUSPENDED:
               flags = XAResource.TMRESUME;
               break;

          default:
             if (trace)
             {
                log.trace("Unhandled resource state: " + resourceState[idx] +
                          " (not RS_NEW or RS_SUSPENDED, using TMJOIN flags)");
             }
         }
      }

      if (trace)
      {
         log.trace("startResource(" +
                   xidFactory.toString(resourceXids[idx]) +
                   ") entered: " + resources[idx].toString() +
                   " flags=" + flags);
      }

      unlock();
      // OSH FIXME: resourceState could be incorrect during this callout.
      try
      {
         try
         {
            resources[idx].start(resourceXids[idx], flags);
         }
         catch(XAException e)
         {
            throw e;
         }
         catch (Throwable t)
         {
            if (trace)
            {
               log.trace("unhandled throwable error in startResource", t);
            }
            status = Status.STATUS_MARKED_ROLLBACK;
            return;
         }

         // Now the XA resource is associated with a transaction.
         resourceState[idx] = RS_ENLISTED;
      }
      finally
      {
         lock();
         if (trace)
         {
            log.trace("startResource(" +
                      xidFactory.toString(resourceXids[idx]) +
                      ") leaving: " + resources[idx].toString() +
                      " flags=" + flags);
         }
      }
   }

   /**
    *  Call <code>end()</code> on the XAResource and update
    *  internal state information.
    *  This will release the lock while calling out.
    *
    *  @param idx The index of the resource in our internal tables.
    *  @param flag The flag argument for the end() call.
    */
   private void endResource(int idx, int flag)
      throws XAException
   {
      if (trace)
      {
         log.trace("endResource(" +
                   xidFactory.toString(resourceXids[idx]) +
         ") entered: " + resources[idx].toString() +
         " flag=" + flag);
      }

      unlock();
      // OSH FIXME: resourceState could be incorrect during this callout.
      try
      {
         try
         {
            resources[idx].end(resourceXids[idx], flag);
         } catch(XAException e)
         {
            throw e;
         } catch (Throwable t)
         {
            if (trace)
            {
               log.trace("unhandled throwable error in endResource", t);
            }
            status = Status.STATUS_MARKED_ROLLBACK;
            // Resource may or may not be ended after illegal exception.
            // We just assume it ended.
            resourceState[idx] = RS_ENDED;
            return;
         }


         // Update our internal state information
         if (flag == XAResource.TMSUSPEND)
            resourceState[idx] = RS_SUSPENDED;
         else
         {
            if (flag == XAResource.TMFAIL)
            {

               status = Status.STATUS_MARKED_ROLLBACK;
            }
            resourceState[idx] = RS_ENDED;
         }
      } finally
      {
         lock();
         if (trace)
         {
            log.trace("endResource(" +
                      xidFactory.toString(resourceXids[idx]) +
                      ") leaving: " + resources[idx].toString() +
                      " flag=" + flag);
         }
      }
   }

   /**
    *  End Tx association for all resources.
    */
   private void endResources()
   {
      for (int idx = 0; idx < resourceCount; idx++) {
         try {
            /*We don't have minerva crap any more!  If your adapter doesn't
              like this, use the matchConnectionWithTx flag to prevent
              your resources from getting suspended.
            if (resourceState[idx] == RS_SUSPENDED) {
               // This is mad, but JTA 1.0.1 spec says on page 41:
               // "If TMSUSPEND is specified in flags, the transaction
               // branch is temporarily suspended in incomplete state.
               // The transaction context is in suspened state and must
               // be resumed via start with TMRESUME specified."
               // Note the _must_ above: It does not say _may_.
               // The above citation also seem to contradict the XA resource
               // state table on pages 17-18 where it is legal to do both
               // end(TMSUCCESS) and end(TMFAIL) when the resource is in
               // a suspended state.
               // But the Minerva XA pool does not like that we call end()
               // two times in a row, so we resume before ending.
               startResource(idx);
            }*/
            if (resourceState[idx] == RS_ENLISTED || resourceState[idx] == RS_SUSPENDED)
            {
               if (trace)
                  log.trace("endresources(" + idx + "): state=" +
                            resourceState[idx]);
               endResource(idx, XAResource.TMSUCCESS);
            }
         } catch(XAException xae)
         {
            logXAException(xae);
            status = Status.STATUS_MARKED_ROLLBACK;
            cause = xae;
         }
      }
      resourcesEnded = true; // Too late to enlist new resources.
   }


   /**
    *  Call synchronization <code>beforeCompletion()</code>.
    *  This will release the lock while calling out.
    */
   private void doBeforeCompletion()
   {
      unlock();
      try
      {
         for (int i = 0; i < syncCount; i++)
         {
            try
            {
               if (trace)
               {
                  log.trace("calling sync " + i + ", " + sync[i]);
               } // end of if ()
               sync[i].beforeCompletion();
            } catch (Throwable t)
            {
               if (trace)
               {
                  log.trace("failed before completion", t);
               }
               status = Status.STATUS_MARKED_ROLLBACK;

               // save the cause off so the user can inspect it
               cause = t;
               break;
            }
         }
      } finally
      {
         lock();
      }
   }

   /**
    *  Call synchronization <code>afterCompletion()</code>.
    *  This will release the lock while calling out.
    */
   private void doAfterCompletion()
   {
      // Assert: Status indicates: Too late to add new synchronizations.
      unlock();
      try
      {
         for (int i = 0; i < syncCount; i++)
         {
            try
            {
               sync[i].afterCompletion(status);
            } catch (Throwable t)
            {
               if (trace)
               {
                  log.trace("failed after completion", t);
               }
            }
         }
      } finally
      {
         lock();
      }
   }

   /**
    *  We got another heuristic.
    *
    *  Promote <code>heuristicCode</code> if needed and tell
    *  the resource to forget the heuristic.
    *  This will release the lock while calling out.
    *
    *  @param resIdx The index of the XA resource that got a
    *         heurictic in our internal tables, or <code>-1</code>
    *         if the heuristic came from here.
    *  @param code The heuristic code, one of
    *         <code>XAException.XA_HEURxxx</code>.
    */
   private void gotHeuristic(int resIdx, int code)
   {
      switch (code)
      {
         case XAException.XA_HEURMIX:
            heuristicCode = XAException.XA_HEURMIX;
            break;
         case XAException.XA_HEURRB:
            if (heuristicCode == HEUR_NONE)
               heuristicCode = XAException.XA_HEURRB;
            else if (heuristicCode == XAException.XA_HEURCOM ||
            heuristicCode == XAException.XA_HEURHAZ)
               heuristicCode = XAException.XA_HEURMIX;
            break;
         case XAException.XA_HEURCOM:
            if (heuristicCode == HEUR_NONE)
               heuristicCode = XAException.XA_HEURCOM;
            else if (heuristicCode == XAException.XA_HEURRB ||
            heuristicCode == XAException.XA_HEURHAZ)
               heuristicCode = XAException.XA_HEURMIX;
            break;
         case XAException.XA_HEURHAZ:
            if (heuristicCode == HEUR_NONE)
               heuristicCode = XAException.XA_HEURHAZ;
            else if (heuristicCode == XAException.XA_HEURCOM ||
            heuristicCode == XAException.XA_HEURRB)
               heuristicCode = XAException.XA_HEURMIX;
            break;
         default:
            throw new IllegalArgumentException();
      }

      if (resIdx != -1)
      {
         try
         {
            unlock();
            resources[resIdx].forget(resourceXids[resIdx]);
         } catch (XAException xae)
         {
            logXAException(xae);
            cause = xae;
         } finally
         {
            lock();
         }
         resourceState[resIdx] = RS_FORGOT;
      }
   }

   /**
    *  Check for heuristics, clear and throw exception if any found.
    */
   private void checkHeuristics()
      throws HeuristicMixedException, HeuristicRollbackException
   {
      switch (heuristicCode)
      {
         case XAException.XA_HEURHAZ:
         case XAException.XA_HEURMIX:
            heuristicCode = HEUR_NONE;
            if (trace)
            {
               log.trace("Throwing HeuristicMixedException, " +
               "status=" + getStringStatus(status));
            }
            throw new HeuristicMixedException();
         case XAException.XA_HEURRB:
            heuristicCode = HEUR_NONE;
            if (trace)
            {
               log.trace("Throwing HeuristicRollbackException, " +
               "status=" + getStringStatus(status));
            }
            throw new HeuristicRollbackException();
         case XAException.XA_HEURCOM:
            heuristicCode = HEUR_NONE;
            // Why isn't HeuristicCommitException used in JTA ?
            // And why define something that is not used ?
            // For now we just have to ignore this failure, even if it happened
            // on rollback.
            if (trace)
            {
               log.trace("NOT Throwing HeuristicCommitException, " +
               "status=" + getStringStatus(status));
            }
            return;
      }
   }


   /**
    *  Prepare all enlisted resources.
    *  If the first phase of the commit process results in a decision
    *  to commit the <code>status</code> will be
    *  <code>Status.STATUS_PREPARED</code> on return.
    *  Otherwise the <code>status</code> will be
    *  <code>Status.STATUS_MARKED_ROLLBACK</code> on return.
    *  This will release the lock while calling out.
    *
    *  @returns True iff all resources voted read-only.
    */
   private boolean prepareResources()
   {
      boolean readOnly = true;

      status = Status.STATUS_PREPARING;

      for (int i = 0; i < resourceCount; i++)
      {
         // Abort prepare on state change.
         if (status != Status.STATUS_PREPARING)
            return false;

         if (resourceSameRM[i] != -1)
            continue; // This RM already prepared.

         XAResource resource = resources[i];

         try
         {
            int vote;

            unlock();
            try
            {
               vote = resources[i].prepare(resourceXids[i]);
            } finally
            {
               lock();
            }

            if (vote == XAResource.XA_OK)
            {
               readOnly = false;
               resourceState[i] = RS_VOTE_OK;
            } else if (vote == XAResource.XA_RDONLY)
               resourceState[i] = RS_VOTE_READONLY;
            else
            {
               // Illegal vote: rollback.
               if (trace)
               {
                  log.trace("illegal vote in prepare resources", new Exception());
               } // end of if ()
               status = Status.STATUS_MARKED_ROLLBACK;
               return false;
            }
         } catch (XAException e)
         {
            readOnly = false;

            logXAException(e);

            switch (e.errorCode)
            {
               case XAException.XA_HEURCOM:
                  // Heuristic commit is not that bad when preparing.
                  // But it means trouble if we have to rollback.
                  gotHeuristic(i, e.errorCode);
                  break;
               case XAException.XA_HEURRB:
               case XAException.XA_HEURMIX:
               case XAException.XA_HEURHAZ:
                  gotHeuristic(i, e.errorCode);
                  if (status == Status.STATUS_PREPARING)
                     status = Status.STATUS_MARKED_ROLLBACK;
                  break;
               default:
                  cause = e;
                  if (status == Status.STATUS_PREPARING)
                     status = Status.STATUS_MARKED_ROLLBACK;
                  break;
            }
         } catch (Throwable t)
         {
            if (trace)
            {
               log.trace("unhandled throwable in prepareResources", t);
            }
            if (status == Status.STATUS_PREPARING)
               status = Status.STATUS_MARKED_ROLLBACK;
            cause = t;
         }
      }

      if (status == Status.STATUS_PREPARING)
         status = Status.STATUS_PREPARED;

      return readOnly;
   }

   /**
    *  Commit all enlisted resources.
    *  This will release the lock while calling out.
    */
   private void commitResources(boolean onePhase)
   {
      status = Status.STATUS_COMMITTING;

      for (int i = 0; i < resourceCount; i++)
      {
         if (trace)
         {
            log.trace("Committing resources, resourceStates["+i+"]=" +
                      resourceState[i]);
         }

         if (!onePhase && resourceState[i] != RS_VOTE_OK)
            continue; // Voted read-only at prepare phase.

         if (resourceSameRM[i] != -1)
            continue; // This RM already committed.

         // Abort commit on state change.
         if (status != Status.STATUS_COMMITTING)
            return;

         try
         {
            unlock();
            try
            {
               resources[i].commit(resourceXids[i], onePhase);
            } finally
            {
               lock();
            }
         } catch (XAException e) {
            logXAException(e);
            switch (e.errorCode) {
               case XAException.XA_HEURRB:
               case XAException.XA_HEURCOM:
               case XAException.XA_HEURMIX:
               case XAException.XA_HEURHAZ:
                  //usually throws an exception, but not for a couple of cases.
                  gotHeuristic(i, e.errorCode);
                  //May not be correct for HEURCOM
                  //Two phase commit is committed after prepare is logged.
                  if (onePhase)
                  {
                     status = Status.STATUS_MARKED_ROLLBACK;
                  } // end of if ()

                  break;
               default:
                  cause = e;
                  if (onePhase)
                  {
                     status = Status.STATUS_MARKED_ROLLBACK;
                     break;
                  } // end of if ()
                  //Not much we can do if there is an RMERR in the
                  //commit phase of 2pc. I guess we try the other rms.
            }
         } catch (Throwable t)
         {
            if (trace)
            {
               log.trace("unhandled throwable in commitResources", t);
            }
         }
      }

      if (status == Status.STATUS_COMMITTING)
         status = Status.STATUS_COMMITTED;
   }

   /**
    *  Rollback all enlisted resources.
    *  This will release the lock while calling out.
    */
   private void rollbackResources()
   {
      status = Status.STATUS_ROLLING_BACK;

      for (int i = 0; i < resourceCount; i++)
      {
         if (resourceState[i] == RS_VOTE_READONLY)
         {
            continue;
         }
         // Already forgotten
         if (resourceState[i] == RS_FORGOT)
            continue;
         if (resourceSameRM[i] != -1)
         {
            continue; // This RM already rolled back.
         }
         try
         {
            unlock();
            try
            {
               resources[i].rollback(resourceXids[i]);
            } finally
            {
               lock();
            }
         } catch (XAException e)
         {
            logXAException(e);
            switch (e.errorCode)
            {
               case XAException.XA_HEURRB:
                  // Heuristic rollback is not that bad when rolling back.
                  gotHeuristic(i, e.errorCode);
                  continue;
               case XAException.XA_HEURCOM:
               case XAException.XA_HEURMIX:
               case XAException.XA_HEURHAZ:
                  gotHeuristic(i, e.errorCode);
                  continue;
               default:
                  cause = e;
                  break;
            }
         } catch (Throwable t) {
            if (trace)
               log.trace("unhandled throwable in rollbackResources", t);
         }
      }

      status = Status.STATUS_ROLLEDBACK;
   }

   /**
    *  Create an Xid representing a new branch of this transaction.
    */
   private Xid createXidBranch()
   {
      long branchId = ++lastBranchId;

      return xidFactory.newBranch(xid, branchId);
   }

   /**
    *  Check if we can do one-phase optimization.
    *  We can do that only if no more than a single resource manager
    *  is involved in this transaction.
    */
   private boolean isOneResource()
   {
      if (resourceCount == 1)
         return true;

      // first XAResource surely has -1, it's the first!
      for (int i = 1; i < resourceCount; i++) {
         if (resourceSameRM[i] == -1) {
            // this one is not the same rm as previous ones,
            // there must be at least 2
            return false;
         }

      }
      // all rms are the same one, one phase commit is ok.
      return true;
   }


   public long getTimeLeftBeforeTimeout()
   {
      return (start + timeoutPeriod) - System.currentTimeMillis();
   }

   Object getTransactionLocalValue(TransactionLocal tlocal)
   {
      return transactionLocalMap.get(tlocal);
   }

   void putTransactionLocalValue(TransactionLocal tlocal, Object value)
   {
      transactionLocalMap.put(tlocal, value);
   }

   boolean containsTransactionLocal(TransactionLocal tlocal)
   {
      return transactionLocalMap.containsKey(tlocal);
   }

   // Inner classes -------------------------------------------------
}
