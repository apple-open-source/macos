/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.resource.connectionmanager;

import javax.resource.ResourceException;
import javax.resource.spi.ConnectionRequestInfo;
import javax.resource.spi.ManagedConnectionFactory;
import javax.security.auth.Subject;

/**
 * A managed connection pool
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version $Revision: 1.2.2.6 $
 */
public interface ManagedConnectionPool
{
   /**
    * Retrieve the managed connection factory for this pool
    * 
    * @return the managed connection factory
    */ 
   ManagedConnectionFactory getManagedConnectionFactory();

   /**
    * Set the connection listener factory
    * 
    * @param clf the connection event listener factory
    */
   void setConnectionListenerFactory(ConnectionListenerFactory clf);

   /**
    * Get a connection
    * 
    * @param subject the subject for connection
    * @param cri the connection request information
    * @return a connection event listener wrapping the connection
    * @throws ResourceException for any error
    */
   ConnectionListener getConnection(Subject subject, ConnectionRequestInfo cri)
      throws ResourceException;

   /**
    * Return a connection
    * 
    * @param cl the connection event listener wrapping the connection
    * @param kill whether to destroy the managed connection
    * @throws ResourceException for any error
    */
   void returnConnection(ConnectionListener cl, boolean kill) 
      throws ResourceException;

   /**
    * @return the connection count
    */
   int getConnectionCount ();

   /**
    * @return the connections in use count
    */
   int getInUseConnectionCount();

   /**
    * @return the connections created count
    */
   int getConnectionCreatedCount();

   /**
    * @return the connections destroyed count
    */
   int getConnectionDestroyedCount();

   /**
    * shutdown the pool
    */
   void shutdown();

   /**
    * @return the available connections
    */
   long getAvailableConnectionCount();

   /**
    * @return the available connections
    */
   int getMaxConnectionsInUseCount();

   /**
    * flush the pool
    */
   void flush();

}
