/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.invocation.iiop;

import javax.management.MBeanServer;
import org.omg.PortableServer.Servant;

/**
 * Abstract class of a CORBA servant that has a reference to an MBean server.
 *
 * @author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.1 $
 */
public abstract class ServantWithMBeanServer
        extends Servant
{
   public abstract void setMBeanServer(MBeanServer mbeanServer);
}
