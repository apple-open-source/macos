/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.resource.connectionmanager;

import javax.resource.ResourceException;
import javax.resource.spi.ConnectionEventListener;
import javax.resource.spi.ManagedConnection;
import javax.transaction.SystemException;

/**
 * A jboss connection listener
 *
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */

public interface ConnectionListener
   extends ConnectionEventListener
{
   /** Normal state */
   public static final int NORMAL = 0;
   
   /** Destroy this connection */
   public static final int DESTROY = 1;
   
   /** This connection has been destroyed */
   public static final int DESTROYED = 2;
   
   /**
    * Retrieve the managed connection for this listener
    * 
    * @return the managed connection
    */
   ManagedConnection getManagedConnection();
   /**
    * Retrieve the managed connection pool for this listener
    * 
    * @return the managed connection pool
    */
   ManagedConnectionPool getManagedConnectionPool();
   /**
    * Retrieve the context used by the pool
    * 
    * @return the context
    */
   Object getContext();
   /**
    * Retrieve the state of this connection
    * 
    * @return the state
    */
   int getState();
   /**
    * Set the state of this connection
    */
   void setState(int newState);
   /**
    * Has the connection timed out?
    * 
    * @param timeout the timeout
    * @return true for timed out, false otherwise
    */
   boolean isTimedOut(long timeout);
   /**
    * Mark the connection as used
    */
   void used();

   /**
    * Register a new connection
    * 
    * @param handle the connection handle
    */
   void registerConnection(Object handle);

   /**
    * Unregister a connection
    *
    * @param handle the connection handle
    */
   void unregisterConnection(Object handle);

   /**
    * Is the managed connection free?
    * 
    * @return true when it is free
    */
   boolean isManagedConnectionFree();

   /**
    * Enlist the managed connection
    */
   void enlist() throws SystemException;

   /**
    * Delist the managed connection
    */
   void delist() throws ResourceException;
}
