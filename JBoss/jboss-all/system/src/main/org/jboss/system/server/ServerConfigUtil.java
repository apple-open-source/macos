/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.system.server;

import java.net.InetAddress;
import java.net.UnknownHostException;

/**
 * Utilities for accessing server configuration
 *
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 * @version <tt>$Revision: 1.1.2.1 $</tt>
 */
public class ServerConfigUtil
{
   private static final String ANY = "0.0.0.0";
   
   /**
    * Retrieve the default bind address for the server
    * 
    * @return the default bind adress
    */
   public static String getDefaultBindAddress()
   {
      return System.getProperty(ServerConfig.SERVER_BIND_ADDRESS);
   }

   /**
    * Retrieve the default bind address, but only if it is specific
    * 
    * @return the specific bind address
    */
   public static String getSpecificBindAddress()
   {
      String address = System.getProperty(ServerConfig.SERVER_BIND_ADDRESS);
      if (address == null || address.equals(ANY))
         return null;
      return address;
   }

   /**
    * Fix the remote inet address.
    * 
    * If we pass the address to the client we don't want to
    * tell it to connect to 0.0.0.0, use our host name instead
    * @param address the passed address
    * @return the fixed address
    */
   public static InetAddress fixRemoteAddress(InetAddress address)
   {
      try
      {
         if (address == null || InetAddress.getByName(ANY).equals(address))
            return InetAddress.getLocalHost();
      }
      catch (UnknownHostException ignored)
      {
      }
      return address;
   }

   /**
    * Fix the remote address.
    * 
    * If we pass the address to the client we don't want to
    * tell it to connect to 0.0.0.0, use our host name instead
    * @param address the passed address
    * @return the fixed address
    */
   public static String fixRemoteAddress(String address)
   {
      try
      {
         if (address == null || ANY.equals(address))
            return InetAddress.getLocalHost().getHostName();
      }
      catch (UnknownHostException ignored)
      {
      }
      return address;
   }
}
