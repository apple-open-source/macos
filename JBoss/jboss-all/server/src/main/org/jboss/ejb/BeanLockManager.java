/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
*/

package org.jboss.ejb;

import java.util.HashMap;

import org.jboss.ejb.Container;
import org.jboss.monitor.EntityLockMonitor;
import org.jboss.monitor.LockMonitor;
import org.jboss.logging.Logger;
import javax.naming.InitialContext;

/**
 * Manages BeanLocks.  All BeanLocks have a reference count.
 * When the reference count goes to 0, the lock is released from the
 * id -> lock mapping.
 *
 * @author <a href="bill@burkecentral.com">Bill Burke</a>
 * @author <a href="marc.fleury@jboss.org">Marc Fleury</a>
 * @author Scott.Stark@jboss.org
 */
public class BeanLockManager
{
   private static Logger log = Logger.getLogger(BeanLockManager.class);
   private HashMap map = new HashMap();

   /** The container this manager reports to */
   private Container container;

   /** Reentrancy of calls */
   private boolean reentrant = false;
   private int txTimeout = 5000;
   /** The logging trace flag, only set in ctor */
   private boolean trace;
   public Class lockClass;
   protected LockMonitor monitor = null;

   public BeanLockManager(Container container)
   {
      this.container = container;
      trace = log.isTraceEnabled();
      try
      {
         InitialContext ctx = new InitialContext();
         EntityLockMonitor elm = (EntityLockMonitor) ctx.lookup(EntityLockMonitor.JNDI_NAME);
         String ejbName = container.getBeanMetaData().getEjbName();
         monitor = elm.getEntityLockMonitor(ejbName);
      }
      catch (Exception ignored)
      {
         // Ignore the lack of an EntityLockMonitor
      }
   }

   public LockMonitor getLockMonitor()
   {
      return monitor;
   }

   /**
    * returns the lock associated with the key passed.  If there is
    * no lock one is created this call also increments the number of 
    * references interested in Lock.
    * 
    * WARNING: All access to this method MUST have an equivalent 
    * removeLockRef cleanup call, or this will create a leak in the map,  
    */
   public synchronized BeanLock getLock(Object id)
   {
      if (id == null)
         throw new IllegalArgumentException("Attempt to get lock ref with a null object");
      BeanLock lock = (BeanLock) map.get(id);
      if (lock == null)
      {
         try
         {
            lock = (BeanLock) lockClass.newInstance();
            lock.setId(id);
            lock.setTimeout(txTimeout);
            lock.setContainer(container);
            if( trace )
               log.trace("Created lock: "+lock);
         }
         catch (Exception e)
         {
            // schrouf: should we really proceed with lock object
            // in case of exception ?? 
            log.warn("Failed to initialize lock:"+lock, e);
         }

         map.put(id, lock);
      }
      lock.addRef();
      if( trace )
         log.trace("Added ref to lock: "+lock);

      return lock;
   }

   public synchronized void removeLockRef(Object id)
   {
      if (id == null)
         throw new IllegalArgumentException("Attempt to remove lock ref with a null object");

      BeanLock lock = (BeanLock) map.get(id);

      if (lock != null)
      {
         try
         {
            lock.removeRef();
            if( trace )
               log.trace("Remove ref lock:"+lock);
         }
         finally
         {
            // schrouf: ALLWAYS ensure proper map lock removal even in case
            // of exception within lock.removeRef ! There seems to be a bug
            // in the ref counting of QueuedPessimisticEJBLock under certain
            // conditions ( lock.ref < 0 should never happen !!! )
            if (lock.getRefs() <= 0)
            {
               Object mapLock = map.remove(lock.getId());
               if( trace )
                  log.trace("Lock no longer referenced, lock: "+lock);
            }
         }
      }
   }

   public synchronized boolean canPassivate(Object id)
   {
      if (id == null)
         throw new IllegalArgumentException("Attempt to passivate with a null object");
      BeanLock lock = (BeanLock) map.get(id);
      if (lock == null)
         throw new IllegalStateException("Attempt to passivate without a lock");

      return (lock.getRefs() <= 1);
   }

   public void setLockCLass(Class lockClass)
   {
      this.lockClass = lockClass;
   }

   public void setReentrant(boolean reentrant)
   {
      this.reentrant = reentrant;
   }

   public void setContainer(Container container)
   {
      this.container = container;
   }
}
