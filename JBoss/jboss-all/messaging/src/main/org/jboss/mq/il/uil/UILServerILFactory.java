/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.uil;

import java.util.Properties;

import org.jboss.mq.il.ServerIL;
import org.jboss.mq.il.ServerILFactory;
import java.net.InetAddress;

/**
 * Factory class to produce UILServerIL objects.
 *
 * @author    <a href="mailto:hiram.chirino@jboss.org">Hiram Chirino</a>
 * @version   $Revision: 1.2.4.1 $
 */
public class UILServerILFactory implements ServerILFactory {

   final public static String SERVER_IL_FACTORY = UILServerILFactory.class.getName();
   final public static String CLIENT_IL_SERVICE = UILClientILService.class.getName();  
   final public static String UIL_ADDRESS_KEY = "UIL_ADDRESS_KEY";
   final public static String UIL_PORT_KEY = "UIL_PORT_KEY";
   final public static String UIL_TCPNODELAY_KEY = "UIL_TCPNODELAY_KEY";
   
   private ServerIL serverIL;

   /**
    * @see ServerILFactory#init(Properties)
    */
   public void init(Properties props) throws Exception {
   	
      String t = props.getProperty(UIL_ADDRESS_KEY);
      if (t == null)
         throw new javax.jms.JMSException("A required connection property was not set: " + UIL_ADDRESS_KEY);
      InetAddress address = InetAddress.getByName(t);

      t = props.getProperty(UIL_PORT_KEY);
      if (t == null)
         throw new javax.jms.JMSException("A required connection property was not set: " + UIL_PORT_KEY);
      int port = Integer.parseInt(t);
      
      boolean enableTcpNoDelay=false;
      t = props.getProperty(UIL_TCPNODELAY_KEY);
      if (t != null) 
         enableTcpNoDelay = t.equals("yes");
      
      String clientSocketFactoryName = null;

      serverIL = new UILServerIL(address, port, clientSocketFactoryName, enableTcpNoDelay);

   }

   /**
    * @see ServerILFactory#getServerIL()
    */
   public ServerIL getServerIL() throws Exception {
      return serverIL;
   }

}