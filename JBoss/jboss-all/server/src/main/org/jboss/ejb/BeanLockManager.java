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
import javax.naming.InitialContext;

/**
 * Manages BeanLocks.  All BeanLocks have a reference count.
 * When the reference count goes to 0, the lock is released from the
 * id -> lock mapping.
 *
 * @author <a href="bill@burkecentral.com">Bill Burke</a>
 * @author <a href="marc.fleury@jboss.org">Marc Fleury</a>
 *
 * @version $Revision: 1.7.4.5 $
 * <p><b>Revisions:</b><br>
 * <p><b>20010802: marcf</b>
 * <ol>
 *   <li>Made bean lock pluggable, container factory passes in lockClass
 *   <li>Removed un-used constructor, added getters and setters
 * </ol>
 */
public class BeanLockManager
{
   private HashMap map = new HashMap();

   /** The container this manager reports to */
   private Container container;

   /** Reentrancy of calls */
   private boolean reentrant = false;
   private int txTimeout = 5000;
	
   public Class lockClass;
   protected LockMonitor monitor = null;

   public BeanLockManager(Container container)
   {
      this.container = container;
      try
      {
         InitialContext ctx = new InitialContext();
         EntityLockMonitor elm = (EntityLockMonitor)ctx.lookup(EntityLockMonitor.JNDI_NAME);
         String ejbName = container.getBeanMetaData().getEjbName();
         monitor = elm.getEntityLockMonitor(ejbName);
         //         if (monitor == null) System.out.println("----- monitor is null ------");
      }
      catch (Exception ignored)
      {
         //         ignored.printStackTrace();
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
      BeanLock lock = (BeanLock)map.get(id);
      if (lock == null)
      {
         try
         {
            lock = (BeanLock) lockClass.newInstance();
            
            lock.setId(id);
            lock.setTimeout(txTimeout);
	        lock.setContainer(container);
         }
         catch (Exception e )
         {
         	// schrouf: should we really proceed with lock object
         	// in case of exception ?? 
         	e.printStackTrace();
         }
         
         map.put(id, lock);
      }
      lock.addRef();
	
      return lock;
   }

   public synchronized void removeLockRef(Object id)
   {
      if (id == null)
         throw new IllegalArgumentException("Attempt to remove lock ref with a null object");
      
      BeanLock lock = (BeanLock)map.get(id);

      if (lock != null)
      {
	     try
	     {
		    lock.removeRef();
	     }
	     finally
	     {
		    // schrouf: ALLWAYS ensure proper map lock removal even in case
		    // of exception within lock.removeRef ! There seems to be a bug
		    // in the ref counting of QueuedPessimisticEJBLock under certain
		    // conditions ( lock.ref < 0 should never happen !!! )
		    if (lock.getRefs() <= 0)
		    {
			   map.remove(lock.getId());
		    }
	     }
      }
   }

   public synchronized boolean canPassivate(Object id)
   {
      if (id == null)
         throw new IllegalArgumentException("Attempt to passivate with a null object");
      BeanLock lock = (BeanLock)map.get(id);
      if (lock == null)
         throw new IllegalStateException("Attempt to passivate without a lock");

      return (lock.getRefs() <= 1);
   }
	
   public void setLockCLass(Class lockClass) {this.lockClass=lockClass;}
   public void setReentrant(boolean reentrant) {this.reentrant = reentrant;}
   public void setContainer(Container container) {this.container = container;}
}

