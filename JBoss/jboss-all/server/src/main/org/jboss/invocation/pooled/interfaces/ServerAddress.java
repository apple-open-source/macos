/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/
package org.jboss.invocation.pooled.interfaces;

import java.io.Serializable;

/**
 * This class encapsulates all the required information for a client to 
 * establish a connection with the server.
 * 
 * It also attempts to provide a fast hash() function since this object
 * is used as a key in a hashmap mainted by the ConnectionManager. 
 *
 * @author <a href="mailto:hiram.chirino@jboss.org">Hiram Chirino</a>
 * @version $Revision: 1.1.4.2 $
 */
public class ServerAddress implements Serializable
{
   /** The serialVersionUID @since 1.1.4.1 */
   private static final long serialVersionUID = -7206359745950445445L;

   /**
    * Address of host ot connect to
    */
   public String address;

   /**
    * Port the service is listening on
    */
   public int port;

   /**
    * If the TcpNoDelay option should be used on the socket.
    */
   public boolean enableTcpNoDelay = false;

   /**
    * Timeout of setSoTimeout
    */
   public int timeout = 60000;

   /**
    * This object is used as a key in a hashmap,
    * so we precompute the hascode for faster lookups.
    */
   private transient int hashCode;

   public ServerAddress(String address, int port, boolean enableTcpNoDelay, int timeout)
   {
      this.address = address;
      this.port = port;
      this.enableTcpNoDelay = enableTcpNoDelay;
      this.hashCode = address.hashCode() + port;
      this.timeout = timeout;
   }

   public String toString()
   {
      return "[address:" + address + ",port:" + port + ",enableTcpNoDelay:" + enableTcpNoDelay + "]";
   }

   public boolean equals(Object obj)
   {
      try
      {
         ServerAddress o = (ServerAddress) obj;
         if (o.hashCode != hashCode)
            return false;
         if (port != port)
            return false;
         if (!o.address.equals(address))
            return false;
         if (o.enableTcpNoDelay != enableTcpNoDelay)
            return false;
         return true;
      }
      catch (Throwable e)
      {
         return false;
      }
   }

   public int hashCode()
   {
      return hashCode;
   }

}
