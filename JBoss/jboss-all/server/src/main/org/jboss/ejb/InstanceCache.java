/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb;

import java.rmi.RemoteException;
import java.rmi.NoSuchObjectException;

/**
 * The plugin that gives a container a cache for bean instances.
 *      
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:simone.bordet@compaq.com">Simone Bordet</a>
 * @version $Revision: 1.11.4.2 $
 */
public interface InstanceCache
   extends ContainerPlugin
{
   /**
    * Gets a bean instance from this cache given the identity.  This method
    * may involve activation if the instance is not in the cache.
    * 
    * <p>Implementation should have O(1) complexity.
    * 
    * <p>This method is never called for stateless session beans.
    *
    * @param id    The primary key of the bean .
    * @return      The EnterpriseContext related to the given id.
    * 
    * @throws RemoteException          In case of illegal calls (concurrent /
    *                                  reentrant)
    * @throws NoSuchObjectException    if the bean cannot be found.
    *                            
    * @see #release
    */
   EnterpriseContext get(Object id)
      throws RemoteException, NoSuchObjectException;

   /**
    * Inserts an active bean instance after creation or activation.
    * 
    * <p>Implementation should guarantee proper locking and O(1) complexity.
    *
    * @param ctx    The EnterpriseContext to insert in the cache
    * 
    * @see #remove
    */
   void insert(EnterpriseContext ctx);

   /**
    * Releases the given bean instance from this cache.
    * This method may passivate the bean to get it out of the cache.
    * Implementation should return almost immediately leaving the
    * passivation to be executed by another thread.
    *
    * @param ctx    The EnterpriseContext to release
    * 
    * @see #get
    */
   void release(EnterpriseContext ctx);

   /**
    * Removes a bean instance from this cache given the identity.
    * Implementation should have O(1) complexity and guarantee proper locking.
    *
    * @param id    The pimary key of the bean.
    * 
    * @see #insert
    */
   void remove(Object id);

   /**
    * Checks whether an instance corresponding to a particular id is active.
    *
    * @param id    The pimary key of the bean.
    * 
    * @see #insert
    */
   boolean isActive(Object id);

   /** Get the current cache size
    *
    * @return the size of the cache
    */
   long getCacheSize();
   /** Flush the cache.
    *
    */
   void flush();
}
