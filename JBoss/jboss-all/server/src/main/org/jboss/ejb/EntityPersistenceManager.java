/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb;

import java.lang.reflect.Method;
import java.rmi.RemoteException;
import java.util.Collection;

import javax.ejb.RemoveException;

import org.jboss.monitor.StatisticsProvider;

/**
 *	This interface is implemented by any EntityBean persistence managers plugins.
 *
 * <p>Implementations of this interface are called by other plugins in the
 *    container.  If the persistence manager wants to, it may attach any
 *    instance specific metadata to the EntityEnterpriseContext that is
 *    passed in method calls.
 *
 * @see EntityContainer
 * 
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.15 $
 */
public interface EntityPersistenceManager extends ContainerPlugin
{
   /**
    * Returns a new instance of the bean class or a subclass of the bean class.
    * 
    * @return the new instance
    */
   Object createBeanClassInstance() throws Exception;

   /**
    * This method is called whenever an entity is to be created. The
    * persistence manager is responsible for calling the ejbCreate method
    * on the instance and to handle the results properly wrt the persistent
    * store.
    *
    * @param m           the create method in the home interface that was
    *                    called
    * @param args        any create parameters
    * @param instance    the instance being used for this create call
    */
   void createEntity(Method m,
                     Object[] args,
                     EntityEnterpriseContext instance)
      throws Exception;

   /**
    * This method is called whenever an entity is to be created. The
    * persistence manager is responsible for calling the ejbPostCreate method
    * on the instance and to handle the results properly wrt the persistent
    * store.
    *
    * @param m           the create method in the home interface that was
    *                    called
    * @param args        any create parameters
    * @param instance    the instance being used for this create call
    */
   void postCreateEntity(Method m,
                     Object[] args,
                     EntityEnterpriseContext instance)
      throws Exception;

   /**
    * This method is called when single entities are to be found. The
    * persistence manager must find out whether the wanted instance is
    * available in the persistence store, and if so it shall use the
    * EJBProxyFactory plugin to create an EJBObject to the instance, which
    * is to be returned as result.
    *
    * @param finderMethod    the find method in the home interface that was
    *                        called
    * @param args            any finder parameters
    * @param instance        the instance to use for the finder call
    * @return                an EJBObject representing the found entity
    */
   Object findEntity(Method finderMethod,
                     Object[] args,
                     EntityEnterpriseContext instance)
      throws Exception;

   /**
    * This method is called when collections of entities are to be found. The
    * persistence manager must find out whether the wanted instances are
    * available in the persistence store, and if so it shall use the
    * EJBProxyFactory plugin to create EJBObjects to the instances, which are
    * to be returned as result.
    *
    * @param finderMethod    the find method in the home interface that was
    *                        called
    * @param args            any finder parameters
    * @param instance        the instance to use for the finder call
    * @return                an EJBObject collection representing the found
    *                        entities
    */
   Collection findEntities(Method finderMethod,
                           Object[] args,
                           EntityEnterpriseContext instance)
      throws Exception;

   /**
    * This method is called when an entity shall be activated. The persistence
    * manager must call the ejbActivate method on the instance.
    *
    * @param instance    the instance to use for the activation
    * 
    * @throws RemoteException    thrown if some system exception occurs
    */
   void activateEntity(EntityEnterpriseContext instance)
      throws RemoteException;
   
   /**
    * This method is called whenever an entity shall be load from the
    * underlying storage. The persistence manager must load the state from
    * the underlying storage and then call ejbLoad on the supplied instance.
    *
    * @param instance    the instance to synchronize
    * 
    * @throws RemoteException    thrown if some system exception occurs
    */
   void loadEntity(EntityEnterpriseContext instance)
      throws RemoteException;
      
   /**
    * This method is used to determine if an entity should be stored.
    *
    * @param instance    the instance to check
    * @return true, if the entity has been modified
    * @throws Exception    thrown if some system exception occurs
    */
   boolean isModified(EntityEnterpriseContext instance) throws Exception;

   /**
    * This method is called whenever an entity shall be stored to the
    * underlying storage. The persistence manager must call ejbStore on the
    * supplied instance and then store the state to the underlying storage.
    *
    * @param instance    the instance to synchronize
    * 
    * @throws RemoteException    thrown if some system exception occurs
    */
   void storeEntity(EntityEnterpriseContext instance)
      throws RemoteException;

   /**
    * This method is called when an entity shall be passivate. The persistence
    * manager must call the ejbPassivate method on the instance.
    *
    * @param instance    the instance to passivate
    * 
    * @throws RemoteException    thrown if some system exception occurs
    */
   void passivateEntity(EntityEnterpriseContext instance)
      throws RemoteException;

   /**
    * This method is called when an entity shall be removed from the
    * underlying storage. The persistence manager must call ejbRemove on the
    * instance and then remove its state from the underlying storage.
    *
    * @param instance    the instance to remove
    * 
    * @throws RemoteException    thrown if some system exception occurs
    * @throws RemoveException    thrown if the instance could not be removed
    */
   void removeEntity(EntityEnterpriseContext instance)
      throws RemoteException, RemoveException;
}

