
package org.jboss.ejb.plugins.lock;


import javax.transaction.Transaction;

import org.jboss.invocation.Invocation;

/**
 * No locking what-so-ever
 * 
 * 
 * Holds all locks for entity beans, not used for stateful. <p>
 *
 * All BeanLocks have a reference count.
 * When the reference count goes to 0, the lock is released from the
 * id -> lock mapping.
 *
 * @author <a href="bill@burkecentral.com">Bill Burke</a>
 *
 * @version $Revision: 1.1.4.2 $
 *
 * <p><b>Revisions:</b><br>
 * <p><b>2001/08/08: billb</b>
 *  <ol>
 *  <li>Initial revision
 *  </ol>
 */
public class NoLock extends BeanLockSupport
{
   /**
    * Schedule(Invocation)
    * 
    * Schedule implements a particular policy for scheduling the threads coming in. 
    * There is always the spec required "serialization" but we can add custom scheduling in here
    *
    * Synchronizing on lock: a failure to get scheduled must result in a wait() call and a 
    * release of the lock.  Schedulation must return with lock.
    * 
    */
   public void schedule(Invocation mi) 
      throws Exception
   {
      return;
   } 

   public void endTransaction(Transaction transaction)
   {
      // complete
   }

   public void wontSynchronize(Transaction trasaction)
   {
      // complete
   }

   public void endInvocation(Invocation mi) 
   { 
      // complete
   }
   
}

