/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */


package org.jboss.ejb.plugins.lock;

import org.jboss.logging.Logger;
import org.jboss.util.deadlock.ApplicationDeadlockException;
import org.jboss.util.deadlock.Resource;
import org.jboss.util.deadlock.DeadlockDetector;

import javax.transaction.Transaction;
import java.util.HashMap;
import java.util.HashSet;

/**
 * Implementents a non reentrant lock with deadlock detection
 *
 * It will throw a ReentranceException if the same thread tries to acquire twice
 * or the same transaction tries to acquire twice
 *
 *
 * @author <a href="bill@jboss.org">Bill Burke</a>
 * @version $Revision: 1.1.2.1 $
 */
public class NonReentrantLock implements Resource
{
   public static class ReentranceException extends Exception
   {
   }


   protected Thread lockHolder;
   protected Object lock = new Object();
   protected volatile int held = 0;
   protected Transaction holdingTx = null;

   public Object getResourceHolder()
   {
      if (holdingTx != null) return holdingTx;
      return lockHolder;
   }

   /** Logger instance */
   static Logger log = Logger.getLogger(NonReentrantLock.class);

   public Thread getLockHolder()
   {
      return lockHolder;
   }

   public void acquire(Transaction miTx) throws ApplicationDeadlockException, InterruptedException, ReentranceException
   {
      acquire(0, miTx);
   }

   protected boolean acquire(long waitTime, Transaction miTx) throws ApplicationDeadlockException, InterruptedException, ReentranceException
   {
      synchronized (lock)
      {
         if (lockHolder != null)
         {
            if (lockHolder == Thread.currentThread()) throw new ReentranceException();
            if (miTx != null && miTx.equals(holdingTx)) throw new ReentranceException();

            // Always upgrade deadlock holder to Tx so that we can detect lock properly
            Object deadlocker = Thread.currentThread();
            if (miTx != null) deadlocker = miTx;
            try
            {
               DeadlockDetector.singleton.deadlockDetection(deadlocker, this);
               while (lockHolder != null)
               {
                  if (waitTime < 1)
                     lock.wait();
                  else
                     lock.wait(waitTime);
                  // If we waited and never got lock, abort
                  if (waitTime > 0 && lockHolder != null) return false;
               }
            }
            finally
            {
               DeadlockDetector.singleton.removeWaiting(deadlocker);
            }
         }
         held++;
         if (held > 1)
         {
            held--;
            throw new IllegalStateException("Should only be able to acquire lock 1 time");
         }
         lockHolder = Thread.currentThread();
         holdingTx = miTx;
      }
      return true;
   }

   public boolean attempt(long waitTime, Transaction miTx) throws ApplicationDeadlockException, InterruptedException, ReentranceException
   {
      return acquire(waitTime, miTx);
   }

   public void release()
   {
      synchronized (lock)
      {
         held--;
         if (held < 0) throw new IllegalStateException("Released lock too many times");
         lockHolder = null;
         holdingTx = null;
         lock.notify();
      }
   }

}
