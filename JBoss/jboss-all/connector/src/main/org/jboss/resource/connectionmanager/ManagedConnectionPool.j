
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
import javax.resource.spi.ManagedConnection;
import javax.resource.spi.ManagedConnectionFactory;
import javax.security.auth.Subject;




/**
* ManagedConnectionPool.java
*
*
* Created: Fri Feb  8 00:07:09 2002
*
* @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
* @version
*/

public interface ManagedConnectionPool
{

   ManagedConnectionFactory getManagedConnectionFactory();

   ManagedConnection getConnection(Subject subject, ConnectionRequestInfo cri)
      throws ResourceException;

   void returnConnection(ManagedConnection mc, boolean kill) throws ResourceException;

   int getConnectionCount();

   int getConnectionCreatedCount();

   int getConnectionDestroyedCount();

   void shutdown();

   long getAvailableConnectionCount();

   void flush();

}// ManagedConnectionPool
