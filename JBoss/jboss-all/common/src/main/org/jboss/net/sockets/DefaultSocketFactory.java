package org.jboss.net.sockets;

import java.io.IOException;
import java.io.Serializable;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.UnknownHostException;
import java.rmi.server.RMIServerSocketFactory;

import java.io.IOException;
import java.io.Serializable;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.UnknownHostException;
import java.rmi.server.RMIServerSocketFactory;

/** An implementation of RMIServerSocketFactory that supports backlog and
 * bind address settings
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class DefaultSocketFactory
   implements RMIServerSocketFactory, Serializable
{
   private transient InetAddress bindAddress;
   private int backlog = 200;

   /** Create a socket factory that binds on any address with a default
    * backlog of 200
    */
   public DefaultSocketFactory()
   {
      this(null, 200);
   }
   /** Create a socket factory with the given bind address
    */
   public DefaultSocketFactory(InetAddress bindAddress)
   {
      this(bindAddress, 200);
   }
   /** Create a socket factory with the given backlog
    */
   public DefaultSocketFactory(int backlog)
   {
      this(null, backlog);
   }
   /** Create a socket factory with the given bind address and backlog
    */
   public DefaultSocketFactory(InetAddress bindAddress, int backlog)
   {
      this.bindAddress = bindAddress;
      this.backlog = backlog;
   }

   public String getBindAddress()
   {
      String address = null;
      if( bindAddress != null )
         address = bindAddress.getHostAddress();
      return address;
   }
   public void setBindAddress(String host) throws UnknownHostException
   {
      bindAddress = InetAddress.getByName(host);
   }

    /**
     * Create a server socket on the specified port (port 0 indicates
     * an anonymous port).
     * @param  port the port number
     * @return the server socket on the specified port
     * @exception IOException if an I/O error occurs during server socket
     * creation
     * @since 1.2
     */
    public ServerSocket createServerSocket(int port) throws IOException
    {
        ServerSocket activeSocket = new ServerSocket(port, backlog, bindAddress);
        return activeSocket;
    }

    public boolean equals(Object obj)
    {
        return obj instanceof DefaultSocketFactory;
    }
    public int hashCode()
    {
        return getClass().getName().hashCode();
    }
}
