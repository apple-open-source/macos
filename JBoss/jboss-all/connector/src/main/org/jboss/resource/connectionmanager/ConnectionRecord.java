
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.resource.connectionmanager;

import javax.resource.spi.ConnectionRequestInfo;


import javax.security.auth.Subject;


import javax.resource.spi.ManagedConnection;



/**
 * ConnectionRecord.java
 *
 *
 * Created: Sat Jan  5 19:55:24 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class ConnectionRecord 
{
   ManagedConnection mc;
   final Object connection;
   final ConnectionRequestInfo cri;

   public ConnectionRecord (final ManagedConnection mc, 
                            final Object connection, 
                            final ConnectionRequestInfo cri)
   {
      this.mc = mc;
      this.connection = connection;
      this.cri = cri;
   }

   void setManagedConnection(final ManagedConnection mc)
   {
      this.mc = mc;
   }
   
}// ConnectionRecord
