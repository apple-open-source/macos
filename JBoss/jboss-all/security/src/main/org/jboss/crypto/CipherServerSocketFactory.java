/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.crypto;

import java.io.IOException;
import java.net.ServerSocket;
import java.rmi.server.RMIServerSocketFactory;


/** An implementation of RMIServerSocketFactory that uses a
 DomainServerSocketFactory for its implementation. This class is just an
 adaptor from the RMIServerSocketFactory to the DomainServerSocketFactory.

 This class is not suitable for RMI object that require a Serializable socket
 factory like activatable services. The reason for this limitation is that
 a SecurityDomain is not serializable due to its association with a local
 KeyStore.

@author Scott.Stark@jboss.org
@version $Revision: 1.1.6.1 $
*/
public class CipherServerSocketFactory implements RMIServerSocketFactory
{

   /** Creates new RMISSLServerSocketFactory */
   public CipherServerSocketFactory()
   {
   }


   /**
    * Create a server socket on the specified port (port 0 indicates
    * an anonymous port).
    * @param  port the port number
    * @return the server socket on the specified port
    * @exception IOException if an I/O error occurs during server socket
    * creation
    */
   public ServerSocket createServerSocket(int port)
      throws IOException
   {
      CipherServerSocket socket = null;
      return socket;
   }

   public boolean equals(Object obj)
   {
      return obj instanceof CipherServerSocketFactory;
   }
   public int hashCode()
   {
      return getClass().getName().hashCode();
   }
}
