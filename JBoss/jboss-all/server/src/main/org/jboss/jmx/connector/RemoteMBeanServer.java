/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.jmx.connector;

import javax.management.MBeanServer;

import org.jboss.jmx.adaptor.rmi.RMIAdaptor;

/**
 * Client-Side JMX Connector Interface.
 *
 * @version <tt>$Revision: 1.2 $</tt>
 * @author <A href="mailto:andreas.schaefer@madplanet.com">Andreas &quot;Mad&quot; Schaefer</A>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 **/
public interface RemoteMBeanServer
   extends MBeanServer
{
   /**
    * If this type is used and you specify a valid QueueConnectorFactory
    * then this connector will use JMS to transfer the events asynchronous
    * back from the server to the client.
    */
   int NOTIFICATION_TYPE_JMS = 0;
   
   /**
    * If this type is used the Connector will use RMI Callback Objects to
    * transfer the events back from the server synchronously.
    */
   int NOTIFICATION_TYPE_RMI = 1;

   /**
    * If this type is used the Connector will use Notification Polling to
    * transfer the events back from the server synchronously.
    */
   int NOTIFICATION_TYPE_POLLING = 2;
}
