/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.ha;

import org.jboss.mq.Connection;
import org.jboss.mq.il.ClientIL;
import org.jboss.mq.il.rmi.RMIClientIL;

/**
 * The HA implementation of the ClientIL object.
 *
 * @author    Hiram Chirino (Cojonudo14@hotmail.com)
 * @version   $Revision: 1.1 $
 */
public class HAClientIL extends RMIClientIL
{
   HAClientIL(Connection c) throws java.rmi.RemoteException
   {
      super(c);
   }

}
