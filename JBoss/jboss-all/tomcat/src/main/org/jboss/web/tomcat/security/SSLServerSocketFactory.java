/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.tomcat.security;

import java.io.IOException;
import java.net.InetAddress;
import java.net.ServerSocket;
import javax.naming.InitialContext;
import javax.naming.NamingException;

import org.apache.catalina.net.ServerSocketFactory;

import org.jboss.security.SecurityDomain;
import org.jboss.security.ssl.DomainServerSocketFactory;

/** An implementation of the catalina ServerSocketFactory for use with https
 secure transport.
 
 @see org.apache.catalina.net.ServerSocketFactory
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.1.1 $
 */
public class SSLServerSocketFactory
   implements ServerSocketFactory
{
   private DomainServerSocketFactory socketFactory;

   public SSLServerSocketFactory()
   {
   }
   public SSLServerSocketFactory(SecurityDomain securityDomain)
      throws IOException
   {
      socketFactory = new DomainServerSocketFactory(securityDomain);
   }

   public void setSecurityDomainName(String jndiName)
      throws NamingException, IOException
   {
      InitialContext iniCtx = new InitialContext();
      SecurityDomain securityDomain = (SecurityDomain) iniCtx.lookup(jndiName);
      socketFactory = new DomainServerSocketFactory(securityDomain);
   }

   public ServerSocket createSocket(int port) throws IOException
   {
      return createSocket(port, 50, null);
   }
   public ServerSocket createSocket(int port, int backlog)
      throws IOException
   {
      return createSocket(port, backlog, null);
   }
   /**
    * Returns a server socket which uses only the specified network
    * interface on the local host, is bound to a the specified port,
    * and uses the specified connection backlog.  The socket is configured
    * with the socket options (such as accept timeout) given to this factory.
    *
    * @param port the port to listen to
    * @param backlog how many connections are queued
    * @param ifAddress the network interface address to use
    *
    * @exception IOException for networking errors
    */
   public ServerSocket createSocket(int port, int backlog, InetAddress ifAddress)
      throws IOException
   {
      return socketFactory.createServerSocket(port, backlog, ifAddress);
   }
   
}
