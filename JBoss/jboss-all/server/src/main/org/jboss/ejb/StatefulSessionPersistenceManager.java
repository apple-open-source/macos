/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb;

import java.rmi.RemoteException;

import javax.ejb.RemoveException;

/**
 * The interface for persisting stateful session beans.
 *
 * @version <tt>$Revision: 1.9 $</tt>
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public interface StatefulSessionPersistenceManager
   extends ContainerPlugin
{
   /**
    * Create a unique identifier for the given SFSB context.
    *
    * @param ctx    The context of the SFSB to create an unique identifier for.
    * @return       A unique identifier.
    *
    * @throws Exception    Failed to create unique identifier.
    */
   Object createId(StatefulSessionEnterpriseContext ctx)
      throws Exception;

   /**
    * Called after the SFSB's ejbCreate method has been successfully
    * invoked to allow the PM to perform an post creation setup.
    *
    * @param ctx    The context of the SFSB which was created.
    */
   void createdSession(StatefulSessionEnterpriseContext ctx)
      throws Exception;
   
   /**
    * Activate the SFSB for the given context.
    *
    * <p>
    * Implementation is responsible for invoking the bean's
    * {@link javax.ejb.SessionBean#ejbActivate} method.
    *
    * @param ctx    The context of the SFSB to activate.
    *
    * @throws RemoteException
    */
   void activateSession(StatefulSessionEnterpriseContext ctx)
      throws RemoteException;
   
   /**
    * Passivate the SFSB for the given context.
    *
    * <p>
    * Implementation is responsible for invoking the bean's
    * {@link javax.ejb.SessionBean#ejbPassivate} method.
    * 
    * @param ctx    The context of the SFSB to passivate.
    *
    * @throws RemoteException
    */
   void passivateSession(StatefulSessionEnterpriseContext ctx)
      throws RemoteException;

   /**
    * Remove the SFSB for the given context.
    *
    * <p>
    * Implementation is responsible for invoking the bean's
    * {@link javax.ejb.SessionBean#ejbRemove} method.
    * 
    * @param ctx    The context of the SFSB to remove.
    *
    * @throws RemoteException
    */
   void removeSession(StatefulSessionEnterpriseContext ctx)
      throws RemoteException, RemoveException;

   /**
    * Remove any passivated state for the given SFSB identifier.
    *
    * <p>
    * This is called by the instance cache impl to clean up
    * the state for an old session.
    *
    * @param id    The identifier of the SFSB to remove passivate state for.
    */
   void removePassivated(Object id);
}
