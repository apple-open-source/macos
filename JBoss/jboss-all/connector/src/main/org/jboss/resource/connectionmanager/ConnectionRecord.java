/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.resource.connectionmanager;

import javax.resource.spi.ConnectionRequestInfo;

/**
 * Information about a connection
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 * @version $Revision: 1.1.4.1 $
 */
public class ConnectionRecord 
{
   ConnectionListener cl;
   final Object connection;
   final ConnectionRequestInfo cri;

   public ConnectionRecord (final ConnectionListener cl, 
                            final Object connection, 
                            final ConnectionRequestInfo cri)
   {
      this.cl = cl;
      this.connection = connection;
      this.cri = cri;
   }

   void setConnectionListener(final ConnectionListener cl)
   {
      this.cl = cl;
   }  
}
