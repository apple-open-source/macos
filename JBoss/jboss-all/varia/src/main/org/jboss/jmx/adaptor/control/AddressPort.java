package org.jboss.jmx.adaptor.control;

import java.net.InetAddress;
import java.lang.reflect.Method;
import java.io.IOException;

/** A utility class for parsing cluster member addresses
 * 
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class AddressPort
{
   InetAddress addr;
   Integer port;

   /** Use reflection to access the address InetAddress and port if they exist
    * in the Address implementation
    */
   public static AddressPort getMemberAddress(Object addr)
      throws IOException
   {
      AddressPort info = null;
      try
      {
         Class[] parameterTypes = {};
         Object[] args = {};
         Method getIpAddress = addr.getClass().getMethod("getIpAddress", parameterTypes);
         InetAddress inetAddr = (InetAddress) getIpAddress.invoke(addr, args);
         Method getPort = addr.getClass().getMethod("getPort", parameterTypes);
         Integer port = (Integer) getPort.invoke(addr, args);
         info = new AddressPort(inetAddr, port);
      }
      catch(Exception e)
      {
         if( addr instanceof String )
         {
            // Parse as a host:port string
            String hostAddr = (String) addr;
            int colon = hostAddr.indexOf(':');
            String host = hostAddr;
            Integer port = new Integer(0);
            if( colon > 0 )
            {
               host = hostAddr.substring(0, colon);
               port = Integer.valueOf(hostAddr.substring(colon+1));
            }
            info = new AddressPort(InetAddress.getByName(host), port);
         }
         else
         {
            throw new IOException("Failed to parse addrType="+addr.getClass()
               +", msg="+e.getMessage());
         }
      }
      return info;
   }

   AddressPort(InetAddress addr, Integer port)
   {
      this.addr = addr;
      this.port = port;
   }

   public Integer getPort()
   {
      return port;
   }
   public InetAddress getInetAddress()
   {
      return addr;
   }
   public String getHostAddress()
   {
      return addr.getHostAddress();
   }
   public String getHostName()
   {
      return addr.getHostName();
   }
   public String toString()
   {
      return "{host("+addr+"), port("+port+")}";
   }
}