/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il;

import java.util.Properties;

/**
 *  This interface is used to define a factory to produce ServerIL objects. This
 *  is used by the client in the GenericConnectionFactory class. Implementations
 *  should provide a default constructor.
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.3 $
 */
public interface ServerILFactory {

   /**
    * Constant used to identify the property that holds the ServerILFactor class name
    */
   public final static String SERVER_IL_FACTORY_KEY = "ServerILFactory";
   /**
    * Constant used to identify the property that holds the ClientILService class name
    */
   public final static String CLIENT_IL_SERVICE_KEY = "ClientILService";
   /**
    * Constant used to identify the property that holds time period between server pings.
    */
   public final static String PING_PERIOD_KEY = "PingPeriod";
	
   // init is called before any calls are made to getServerIL()
   public void init( Properties props )
      throws Exception;

   // must return a instance of ServerIL or else throw an Exception.
   public ServerIL getServerIL()
      throws Exception;
}
