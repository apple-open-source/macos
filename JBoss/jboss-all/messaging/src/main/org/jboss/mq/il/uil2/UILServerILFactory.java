/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.uil2;

import java.util.Properties;

import org.jboss.mq.il.ServerIL;
import org.jboss.mq.il.ServerILFactory;
import java.net.InetAddress;

/**
 * Factory class to produce UILServerIL objects.
 *
 * @author    <a href="mailto:hiram.chirino@jboss.org">Hiram Chirino</a>
 * @version   $Revision: 1.1.4.2 $
 */
public class UILServerILFactory implements ServerILFactory {

   final public static String SERVER_IL_FACTORY = UILServerILFactory.class.getName();
   final public static String CLIENT_IL_SERVICE = UILClientILService.class.getName();  
   final public static String UIL_ADDRESS_KEY = "UIL_ADDRESS_KEY";
   final public static String UIL_PORT_KEY = "UIL_PORT_KEY";
   final public static String UIL_TCPNODELAY_KEY = "UIL_TCPNODELAY_KEY";
   final public static String UIL_BUFFERSIZE_KEY = "UIL_BUFFERSIZE_KEY";
   final public static String UIL_CHUNKSIZE_KEY = "UIL_CHUNKSIZE_KEY";
   
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

      int bufferSize = 1; // 1 byte == no buffering
      t = props.getProperty(UIL_BUFFERSIZE_KEY);
      if (t != null)
         bufferSize = Integer.parseInt(t);

      int chunkSize = 0x40000000; // 2^30 bytes
      t = props.getProperty(UIL_CHUNKSIZE_KEY);
      if (t != null)
         chunkSize = Integer.parseInt(t);
      
      String clientSocketFactoryName = null;

      serverIL = new UILServerIL(address, port, clientSocketFactoryName, enableTcpNoDelay, bufferSize, chunkSize);

   }

   /**
    * @see ServerILFactory#getServerIL()
    */
   public ServerIL getServerIL() throws Exception {
      return serverIL;
   }

}