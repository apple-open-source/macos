/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.lock;

import java.lang.reflect.Method;
import java.util.LinkedList;
import java.util.HashMap;
import java.util.ArrayList;

import javax.transaction.Transaction;
import javax.transaction.Status;

import org.jboss.invocation.Invocation;
import org.jboss.ejb.Container;
import org.jboss.ejb.EntityContainer;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.monitor.LockMonitor;
import org.jboss.util.deadlock.DeadlockDetector;

/**
 * This class is holds threads awaiting the transactional lock to be free
 * in a fair FIFO transactional queue.  Non-transactional threads
 * are also put in this wait queue as well. Unlike SimplePessimisticEJBLock which notifies all
 * threads on transaction completion, this class pops the next waiting transaction from the queue
 * and notifies only those threads waiting associated with that transaction.  This
 * class should perform better than Simple on high contention loads.
 *
 * Holds all locks for entity beans, not used for stateful. <p>
 *
 * All BeanLocks have a reference count.
 * When the reference count goes to 0, the lock is released from the
 * id -> lock mapping.
 *
 * As of 04/10/2002, you can now specify in jboss.xml method attributes that define
 * methods as read-only.  read-only methods(and read-only beans) will release transactional
 * locks at the end of the invocation.  This decreases likelyhood of deadlock and increases
 * performance.
 *
 * FIXME marcf: we should get solid numbers on this locking, bench in multi-thread environments
 * We need someone with serious SUN hardware to run this lock into the ground
 *
 * @author <a href="marc.fleury@jboss.org">Marc Fleury</a>
 * @author <a href="bill@burkecentral.com">Bill Burke</a>
 * @author <a href="pete@subx.com">Peter Murray</a>
 *
 * @version $Revision: 1.12.2.22 $
 */
public class QueuedPessimisticEJBLock extends BeanLockSupport
{
   private HashMap txLocks = new HashMap();
   private LinkedList txWaitQueue = new LinkedList();
   private boolean isReadOnlyTxLock = true;

   private int txIdGen = 0;
   protected LockMonitor lockMonitor = null;
   /** A flag that disables the deadlock detection check */
   protected boolean deadlockDetection = true;

   public void setContainer(Container container)
   {
      this.container = container;
      lockMonitor = container.getLockManager().getLockMonitor();
   }

   public boolean getDeadlockDetection()
   {
      return deadlockDetection;
   }
   public void setDeadlockDetection(boolean flag)
   {
      this.deadlockDetection = flag;
   }

   private class TxLock
   {

      public Transaction waitingTx = null;
      public int id = 0;
      public String threadName;
      public boolean isQueued;

      /**
       * deadlocker is used by the DeadlockDetector
       * It is the thread if the tx is null.
       */
      public Object deadlocker;

      public TxLock(Transaction trans)
      {
         this.threadName = Thread.currentThread().toString();
         this.waitingTx = trans;
         if (trans == null)
         {
            if (txIdGen < 0) txIdGen = 0;
            this.id = txIdGen++;
            deadlocker = Thread.currentThread();
         }
         else
         {
            deadlocker = trans;
         }
         this.isQueued = true;
      }

      public boolean equals(Object obj)
      {
         if (obj == this) return true;

         TxLock lock = (TxLock) obj;

         if (lock.waitingTx == null && this.waitingTx == null)
         {
            return lock.id == this.id;
         }
         else if (lock.waitingTx != null && this.waitingTx != null)
         {
            return lock.waitingTx.equals(this.waitingTx);
         }
         return false;
      }

      public int hashCode()
      {
         return this.id;
      }


      public String toString()
      {
         StringBuffer buffer = new StringBuffer(100);
         buffer.append("TXLOCK waitingTx=").append(waitingTx);
         buffer.append(" id=").append(id);
         buffer.append(" thread=").append(threadName);
         buffer.append(" queued=").append(isQueued);
         return buffer.toString();
      }
   }

   protected TxLock getTxLock(Transaction miTx)
   {
      TxLock lock = null;
      if (miTx == null)
      {
         // There is no transaction
         lock = new TxLock(null);
         txWaitQueue.addLast(lock);
      }
      else
      {
         TxLock key = new TxLock(miTx);
         lock = (TxLock) txLocks.get(key);
         if (lock == null)
         {
            txLocks.put(key, key);
            txWaitQueue.addLast(key);
            lock = key;
         }
      }
      return lock;
   }

   protected boolean isTxExpired(Transaction miTx) throws Exception
   {
      if (miTx != null && miTx.getStatus() == Status.STATUS_MARKED_ROLLBACK)
      {
         return true;
      }
      return false;
   }


   public void schedule(Invocation mi) throws Exception
   {
      boolean threadScheduled = false;
      while (!threadScheduled)
      {
         /* loop on lock wakeup and restart trying to schedule */
         threadScheduled = doSchedule(mi);
      }
      // Only set isReadOnlyTxLock if there was a transactional lock
      if (mi.getTransaction() != null)
      {
         // Promote the txlock into a writeLock if we're not a readonly method
         // isReadOnlyTxLock will be reset in nextTransaction()
         Method method = mi.getMethod();
         isReadOnlyTxLock =
                 isReadOnlyTxLock &&
                 (
                 ((EntityContainer) container).isReadOnly() ||
                 (
                 method != null &&
                 container.getBeanMetaData().isMethodReadOnly(method.getName())
                 )
                 );

      }
   }

   /**
    * doSchedule(Invocation)
    *
    * doSchedule implements a particular policy for scheduling the threads coming in.
    * There is always the spec required "serialization" but we can add custom scheduling in here
    *
    * Synchronizing on lock: a failure to get scheduled must result in a wait() call and a
    * release of the lock.  Schedulation must return with lock.
    *
    */
   protected boolean doSchedule(Invocation mi)
           throws Exception
   {
      boolean wasThreadScheduled = false;
      Transaction miTx = mi.getTransaction();
      boolean trace = log.isTraceEnabled();
      this.sync();
      try
      {
         if (trace) log.trace("Begin schedule, key=" + mi.getId());

         if (isTxExpired(miTx))
         {
            log.error("Saw rolled back tx=" + miTx);
            throw new RuntimeException("Transaction marked for rollback, possibly a timeout");
         }

         //Next test is independent of whether the context is locked or not, it is purely transactional
         // Is the instance involved with another transaction? if so we implement pessimistic locking
         long startWait = System.currentTimeMillis();
         try
         {
            wasThreadScheduled = waitForTx(miTx, trace);
            if (wasThreadScheduled && lockMonitor != null)
            {
               long endWait = System.currentTimeMillis() - startWait;
               lockMonitor.finishedContending(endWait);
            }
         }
         catch (Exception throwable)
         {
            if (lockMonitor != null && isTxExpired(miTx))
            {
               synchronized (lockMonitor)
               {
                  lockMonitor.timeouts++;
               }
            }
            if (lockMonitor != null)
            {
               long endWait = System.currentTimeMillis() - startWait;
               lockMonitor.finishedContending(endWait);
            }
            throw throwable;
         }
      }
      finally
      {
         if (miTx == null // non-transactional
                 && wasThreadScheduled)
         {
            // if this non-transctional thread was
            // scheduled in txWaitQueue, we need to call nextTransaction
            // Otherwise, threads in txWaitQueue will never wake up.
            nextTransaction();
         }
         this.releaseSync();
      }

      //If we reach here we are properly scheduled to go through so return true
      return true;
   }

   /**
    * Wait until no other transaction is running with this lock.
    *
    * @return    Returns true if this thread was scheduled in txWaitQueue
    */
   protected boolean waitForTx(Transaction miTx, boolean trace) throws Exception
   {
      boolean wasScheduled = false;
      // Do we have a running transaction with the context?
      // We loop here until either until success or until transaction timeout
      // If we get out of the loop successfully, we can successfully
      // set the transaction on this puppy.
      TxLock txLock = null;
      Object deadlocker = miTx;
      if (deadlocker == null) deadlocker = Thread.currentThread();

      while (getTransaction() != null &&
              // And are we trying to enter with another transaction?
              !getTransaction().equals(miTx))
      {
         // Check for a deadlock on every cycle
         try
         {
            if( deadlockDetection == true )
               DeadlockDetector.singleton.deadlockDetection(deadlocker, this);
         }
         catch (Exception e)
         {
            // We were queued, not any more
            if (txLock != null && txLock.isQueued)
            {
               txLocks.remove(txLock);
               txWaitQueue.remove(txLock);
            }
            throw e;
         }

         wasScheduled = true;
         if (lockMonitor != null) lockMonitor.contending();
         // That's no good, only one transaction per context
         // Let's put the thread to sleep the transaction demarcation will wake them up
         if (trace) log.trace("Transactional contention on context" + id);

         // Only queue the lock on the first iteration
         if (txLock == null)
            txLock = getTxLock(miTx);

         if (trace) log.trace("Begin wait on Tx=" + getTransaction());

         // And lock the threads on the lock corresponding to the Tx in MI
         synchronized (txLock)
         {
            releaseSync();
            try
            {
               txLock.wait(txTimeout);
            }
            catch (InterruptedException ignored)
            {
            }
         } // end synchronized(txLock)

         this.sync();

         if (trace) log.trace("End wait on TxLock=" + getTransaction());
         if (isTxExpired(miTx))
         {
            log.error(Thread.currentThread() + "Saw rolled back tx=" + miTx + " waiting for txLock"
                    // +" On method: " + mi.getMethod().getName()
                    // +" txWaitQueue size: " + txWaitQueue.size()
            );
            if (txLock.isQueued)
            {
               // Remove the TxLock from the queue because this thread is exiting.
               // Don't worry about notifying other threads that share the same transaction.
               // They will timeout and throw the below RuntimeException
               txLocks.remove(txLock);
               txWaitQueue.remove(txLock);
            }
            else if (getTransaction() != null && getTransaction().equals(miTx))
            {
               // We're not qu
               nextTransaction();
            }
            if (miTx != null)
            {
               if( deadlockDetection == true )
                  DeadlockDetector.singleton.removeWaiting(deadlocker);
            }
            throw new RuntimeException("Transaction marked for rollback, possibly a timeout");
         }
      } // end while(tx!=miTx)

      // If we get here, this means that we have the txlock
      if (!wasScheduled)
      {
         setTransaction(miTx);
      }
      return wasScheduled;
   }

   /*
    * nextTransaction()
    *
    * nextTransaction will
    * - set the current tx to null
    * - schedule the next transaction by notifying all threads waiting on the transaction
    * - setting the thread with the new transaction so there is no race with incoming calls
    */
   protected void nextTransaction()
   {
      if (synched == null)
      {
         throw new IllegalStateException("do not call nextTransaction while not synched!");
      }

      setTransaction(null);
      this.isReadOnlyTxLock = true;
      // is there a waiting list?
      if (!txWaitQueue.isEmpty())
      {
         TxLock thelock = (TxLock) txWaitQueue.removeFirst();
         txLocks.remove(thelock);
         thelock.isQueued = false;
         // The new transaction is the next one, important to set it up to avoid race with
         // new incoming calls
         setTransaction(thelock.waitingTx);
         //         log.debug(Thread.currentThread()+" handing off to "+lock.threadName);
         if( deadlockDetection == true )
            DeadlockDetector.singleton.removeWaiting(thelock.deadlocker);

         synchronized (thelock)
         {
            // notify All threads waiting on this transaction.
            // They will enter the methodLock wait loop.
            thelock.notifyAll();
         }
      }
      else
      {
         //         log.debug(Thread.currentThread()+" handing off to empty queue");
      }
   }

   public void endTransaction(Transaction transaction)
   {
      nextTransaction();
   }

   public void wontSynchronize(Transaction trasaction)
   {
      nextTransaction();
   }

   /**
    * releaseMethodLock
    *
    * if we reach the count of zero it means the instance is free from threads (and reentrency)
    * we wake up the next thread in the currentLock
    */
   public void endInvocation(Invocation mi)
   {
      // Do we own the lock?
      Transaction tx = mi.getTransaction();
      if (tx != null && tx.equals(getTransaction()))
      {
         // If there is no context or synchronization, release the lock
         EntityEnterpriseContext ctx = (EntityEnterpriseContext) mi.getEnterpriseContext();
         if (ctx == null || ctx.hasTxSynchronization() == false)
            endTransaction(tx);
      }
   }

   public void removeRef()
   {
      refs--;
      if (refs == 0 && txWaitQueue.size() > 0)
      {
         log.error("removing bean lock and it has tx's in QUEUE! " + toString());
         throw new IllegalStateException("removing bean lock and it has tx's in QUEUE!");
      }
      else if (refs == 0 && getTransaction() != null)
      {
         log.error("removing bean lock and it has tx set! " + toString());
         throw new IllegalStateException("removing bean lock and it has tx set!");
      }
      else if (refs < 0)
      {
         log.error("negative lock reference count should never happen !");
         throw new IllegalStateException("negative lock reference count !");
      }
   }

   public String toString()
   {
      StringBuffer buffer = new StringBuffer(100);
      buffer.append(super.toString());
      buffer.append(", bean=").append(container.getBeanMetaData().getEjbName());
      buffer.append(", id=").append(id);
      buffer.append(", refs=").append(refs);
      buffer.append(", tx=").append(getTransaction());
      buffer.append(", synched=").append(synched);
      buffer.append(", timeout=").append(txTimeout);
      buffer.append(", queue=").append(new ArrayList(txWaitQueue));
      return buffer.toString();
   }
}
