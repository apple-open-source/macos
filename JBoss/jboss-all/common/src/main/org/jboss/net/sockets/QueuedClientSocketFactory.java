package org.jboss.net.sockets;

import java.io.IOException;
import java.io.Serializable;
import java.rmi.server.RMIClientSocketFactory;
import java.net.Socket;
import EDU.oswego.cs.dl.util.concurrent.FIFOSemaphore;
/**
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class QueuedClientSocketFactory
   implements RMIClientSocketFactory, java.io.Externalizable
{
   private transient FIFOSemaphore permits;
   private long numPermits;
   public QueuedClientSocketFactory()
   {
   }

   public QueuedClientSocketFactory(long nPermits)
   {
      permits = new FIFOSemaphore(nPermits);
      numPermits = nPermits;
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
   public Socket createSocket(String host, int port) throws IOException
   {
      try
      {
         permits.acquire();
         return new Socket(host, port);
      }
      catch (InterruptedException ex)
      {
         throw new IOException("Failed to acquire FIFOSemaphore for ClientSocketFactory");
      }
      finally
      {
         permits.release();
      }
   }
   
   public boolean equals(Object obj)
   {
      return obj instanceof QueuedClientSocketFactory;
   }
   public int hashCode()
   {
      return getClass().getName().hashCode();
   }
   
   public void writeExternal(java.io.ObjectOutput out)
      throws IOException
   {
      out.writeLong(numPermits);
   }
   public void readExternal(java.io.ObjectInput in)
      throws IOException, ClassNotFoundException
   {
      numPermits = in.readLong();
      permits = new FIFOSemaphore(numPermits);
   }
}
