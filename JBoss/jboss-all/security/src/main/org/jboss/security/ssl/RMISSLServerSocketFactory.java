/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.ssl;

import java.io.IOException;
import java.net.ServerSocket;
import java.net.UnknownHostException;
import java.rmi.server.RMIServerSocketFactory;

import org.jboss.security.SecurityDomain;

/** An implementation of RMIServerSocketFactory that uses a
 DomainServerSocketFactory for its implementation. This class is just an
 adaptor from the RMIServerSocketFactory to the DomainServerSocketFactory.

 This class is not suitable for RMI object that require a Serializable socket
 factory like activatable services. The reason for this limitation is that
 a SecurityDomain is not serializable due to its association with a local
 KeyStore.

@author Scott.Stark@jboss.org
@version $Revision: 1.5.2.1 $
*/
public class RMISSLServerSocketFactory implements RMIServerSocketFactory
{
   private DomainServerSocketFactory domainFactory;

   /** Creates new RMISSLServerSocketFactory initialized with a
    DomainServerSocketFactory with not security domain. The setSecurityDomain
    method must be invoked to establish the correct non-default value.
    */
   public RMISSLServerSocketFactory()
   {
      domainFactory = new DomainServerSocketFactory();
   }

   public String getBindAddress()
   {
      return domainFactory.getBindAddress();
   }
   public void setBindAddress(String host) throws UnknownHostException
   {
      domainFactory.setBindAddress(host);
   }

   public SecurityDomain getSecurityDomain()
   {
      return domainFactory.getSecurityDomain();
   }
   public void setSecurityDomain(SecurityDomain securityDomain)
   {
      domainFactory.setSecurityDomain(securityDomain);
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
      return domainFactory.createServerSocket(port);
   }

   public boolean equals(Object obj)
   {
      return obj instanceof RMISSLServerSocketFactory;
   }
   public int hashCode()
   {
      return getClass().getName().hashCode();
   }
}
