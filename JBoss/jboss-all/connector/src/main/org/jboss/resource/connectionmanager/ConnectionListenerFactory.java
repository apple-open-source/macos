/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.resource.connectionmanager;

import javax.resource.ResourceException;
import javax.resource.spi.ManagedConnection;

/**
 * A factory for connection event listeners
 *
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 * @version $Revision: 1.1.2.2 $
 */
public interface ConnectionListenerFactory
{
   /**
    * Create a managed connection listener for the managed connection
    * 
    * @param mc the managed connection
    * @param context a context object used by the pool
    * @return a new connection event listener
    * @throws ResourceException for any error
    */
   ConnectionListener createConnectionListener(ManagedConnection mc, Object context)
      throws ResourceException;

   /**
    * Determine whether is a transaction
    *
    * @return whether there is a transaction
    */
   boolean isTransactional();
}
