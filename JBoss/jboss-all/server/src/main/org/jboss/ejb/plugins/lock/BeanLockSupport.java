/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.lock;

import java.lang.reflect.Method;

import javax.transaction.Transaction;
import javax.ejb.EJBObject;

import org.jboss.ejb.BeanLock;
import org.jboss.ejb.Container;
import org.jboss.invocation.Invocation;
import org.jboss.logging.Logger;
import org.jboss.util.deadlock.ApplicationDeadlockException;
import org.jboss.util.deadlock.Resource;
import java.util.HashMap;
import java.util.HashSet;


/**
 * Support for the BeanLock
 *
 * @author <a href="bill@burkecentral.com">Bill Burke</a>
 * @author <a href="marc.fleury@jboss.org">Marc Fleury</a>
 * @version $Revision: 1.16.2.11 $
 */
public abstract class BeanLockSupport implements Resource, BeanLock
{
   protected Container container = null;
   
   /**
    * Number of threads that retrieved this lock from the manager
    * (0 means removing)
    */ 
   protected int refs = 0;
   
   /** The Cachekey corresponding to this Bean */
   protected Object id = null;
 
   /** Logger instance */
   static Logger log = Logger.getLogger(BeanLock.class);
 
   /** Transaction holding lock on bean */
   protected Transaction tx = null;
 
   protected Thread synched = null;
   protected int synchedDepth = 0;

   protected int txTimeout;

	
   public void setId(Object id) { this.id = id;}
   public Object getId() { return id;}
   public void setTimeout(int timeout) {txTimeout = timeout;}
   public void setContainer(Container container) { this.container = container; }
   public Object getResourceHolder() { return tx; }

   public void sync()
   {
      synchronized(this)
      {
         Thread thread = Thread.currentThread();
         while(synched != null && synched.equals(thread) == false)
         {
            try
            {
               this.wait();
            }
            catch (InterruptedException ex) { /* ignore */ }
         }
         synched = thread;
         ++synchedDepth;
      }
   }
 
   public void releaseSync()
   {
      synchronized(this)
      {
         if (--synchedDepth == 0)
            synched = null;
         this.notify();
      }
   }
 
   public abstract void schedule(Invocation mi) throws Exception;
	
   /**
    * The setTransaction associates a transaction with the lock.
    * The current transaction is associated by the schedule call.
    */
   public void setTransaction(Transaction tx){this.tx = tx;}
   public Transaction getTransaction(){return tx;}
   
   public abstract void endTransaction(Transaction tx);
   public abstract void wontSynchronize(Transaction tx);
	
   public abstract void endInvocation(Invocation mi);
   
   public void addRef() { refs++;}
   public void removeRef() { refs--;}
   public int getRefs() { return refs;}
   
   // Private --------------------------------------------------------
   
}
