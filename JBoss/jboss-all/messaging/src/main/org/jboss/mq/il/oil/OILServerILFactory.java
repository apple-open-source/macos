/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.oil;

import java.util.Properties;

import org.jboss.mq.il.ServerIL;
import org.jboss.mq.il.ServerILFactory;
import java.net.InetAddress;

/**
 * Factory class to produce OILServerIL objects.
 *
 * @author    <a href="mailto:hiram.chirino@jboss.org">Hiram Chirino</a>
 * @version   $Revision: 1.3.4.1 $
 */
public final class OILServerILFactory
   implements ServerILFactory
{

   final public static String SERVER_IL_FACTORY = OILServerILFactory.class.getName();
   final public static String CLIENT_IL_SERVICE = OILClientILService.class.getName();  
   final public static String OIL_ADDRESS_KEY = "OIL_ADDRESS_KEY";
   final public static String OIL_PORT_KEY = "OIL_PORT_KEY";
   final public static String OIL_TCPNODELAY_KEY = "OIL_TCPNODELAY_KEY";

   private ServerIL serverIL;

   /**
    * @see ServerILFactory#init(Properties)
    */
   public void init(Properties props)
      throws Exception
   {
   	
      String t = props.getProperty(OIL_ADDRESS_KEY);
      if (t == null)
         throw new javax.jms.JMSException("A required connection property was not set: " + OIL_ADDRESS_KEY);
      InetAddress address = InetAddress.getByName(t);

      t = props.getProperty(OIL_PORT_KEY);
      if (t == null)
         throw new javax.jms.JMSException("A required connection property was not set: " + OIL_PORT_KEY);
      int port = Integer.parseInt(t);
      
      boolean enableTcpNoDelay=false;
      t = props.getProperty(OIL_TCPNODELAY_KEY);
      if (t != null) 
         enableTcpNoDelay = t.equals("yes");

      String clientSocketFactoryName = null;
      serverIL = new OILServerIL(address, port, clientSocketFactoryName, enableTcpNoDelay);

   }

   /**
    * @see ServerILFactory#getServerIL()
    */
   public ServerIL getServerIL()
      throws Exception
   {
      return serverIL;
   }

}
