/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb;

import java.rmi.RemoteException;
import java.util.Collection;

import javax.ejb.EJBMetaData;
import javax.ejb.EJBHome;
import javax.ejb.EJBObject;

import org.jboss.metadata.InvokerProxyBindingMetaData;

/**
 * This is an interface for Container plugins. Implementations of this
 * interface are responsible for receiving remote invocations of EJB's
 * and to forward these requests to the Container it is being used with.
 *
 * <p>It is responsible for providing any EJBObject and EJBHome 
 *    implementations (which may be statically or dynamically created). 
 *
 * <p>Before forwarding a call to the container it must call
 *    Thread.setContextClassLoader() with the classloader of the container.
 *    It must also handle any propagated transaction and security contexts
 *    properly. It may acquire the TransactionManager from JNDI.
 *
 * @see Container
 * 
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @version $Revision: 1.1 $
 *
 *<p><b>20011219 marc fleury:</b>
* <ul>
*  <li>Moved from typed return to Object to allow for optimizations in creation
*  <li>Per Dain Sundstrom requests removed the Remote Exception.
*  <li>The Container Invoker is going to be replaced by the ProxyFactory 
*</ul>
 */
public interface EJBProxyFactory
extends ContainerPlugin
{
   /**
    * Set the invoker meta data so that the ProxyFactory can initialize properly
    */
   void setInvokerMetaData(InvokerProxyBindingMetaData imd);
   /**
    * Set the invoker jndi binding
    */
   void setInvokerBinding(String binding);
   /**
    * This method is called whenever the metadata for this container is
    * needed.
    *
    * @return    An implementation of the EJBMetaData interface.
    */
   EJBMetaData getEJBMetaData();

   /**
    * This method is called whenever the EJBHome implementation for this
    * container is needed.
    *
    * @return    An implementation of the home interface for this container.
    */
   Object getEJBHome();

   /**
    * This method is called whenever an EJBObject implementation for a
    * stateless session bean is needed.
    *
    * @return    An implementation of the remote interface for this container.
    * 
    * @throws RemoteException    Thrown if the EJBObject could not be created.
    */
   Object getStatelessSessionEJBObject();

   /**
    * This method is called whenever an EJBObject implementation for a stateful
    * session bean is needed.
    *
    * @param id    The id of the session.
    * @return      An implementation of the remote interface for this
    *              container.
    *              
    * @throws RemoteException    Thrown if the EJBObject could not be created.
    */
   Object getStatefulSessionEJBObject(Object id);

   /**
    * This method is called whenever an EJBObject implementation for an 
    * entitybean is needed.
    *
    * @param id    The primary key of the entity.
    * @return      An implementation of the remote interface for this
    *              container.
    *              
    * @throws RemoteException    Thrown if the EJBObject could not be created.
    */
   Object getEntityEJBObject(Object id);

   /**
    * This method is called whenever a collection of EJBObjects for a
    * collection of primary keys is needed.
    *
    * @param enum    Enumeration of primary keys.
    * @return        A collection of EJBObjects implementing the remote
    *                interface for this container.
    *                
    * @throws RemoteException    Thrown if the EJBObjects could not be created.
    */
   Collection getEntityCollection(Collection enum);
}

