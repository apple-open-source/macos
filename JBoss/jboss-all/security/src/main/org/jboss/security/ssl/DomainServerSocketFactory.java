
package org.jboss.security.ssl;

import java.io.IOException;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.UnknownHostException;
import java.security.KeyManagementException;
import java.security.NoSuchAlgorithmException;
import javax.naming.InitialContext;
import javax.net.ServerSocketFactory;
import javax.net.ssl.SSLServerSocketFactory;

import com.sun.net.ssl.KeyManagerFactory;
import com.sun.net.ssl.SSLContext;
import com.sun.net.ssl.TrustManager;
import com.sun.net.ssl.TrustManagerFactory;

import org.jboss.logging.Logger;
import org.jboss.security.SecurityDomain;

/** An implementation of ServerSocketFactory that creates SSL server sockets
 using the JSSE SSLContext and a JBossSX SecurityDomain for the KeyManagerFactory
 and TrustManagerFactory objects.

 @see com.sun.net.ssl.SSLContext
 @see com.sun.net.ssl.KeyManagerFactory
 @see com.sun.net.ssl.TrustManagerFactory
 @see org.jboss.security.SecurityDomain

@author  Scott.Stark@jboss.org
@version $Revision: 1.3.2.3 $
*/
public class DomainServerSocketFactory extends SSLServerSocketFactory
{
   private static Logger log = Logger.getLogger(DomainServerSocketFactory.class);
   private transient SecurityDomain securityDomain;
   private transient InetAddress bindAddress;
   private transient SSLContext sslCtx = null;

   /** A default constructor for use when created by Class.newInstance. The
    factory is not usable until its SecurityDomain has been established.
    */
   public DomainServerSocketFactory()
   {
   }
   /** Create a sockate factory instance that uses the given SecurityDomain
    as the source for the SSL KeyManagerFactory and TrustManagerFactory.
    */
   public DomainServerSocketFactory(SecurityDomain securityDomain) throws IOException
   {
      if( securityDomain == null )
         throw new IOException("The securityDomain may not be null");
      this.securityDomain = securityDomain;
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

   public SecurityDomain getSecurityDomain()
   {
      return securityDomain;
   }
   public void setSecurityDomain(SecurityDomain securityDomain)
   {
      this.securityDomain = securityDomain;
   }

// --- Begin SSLServerSocketFactory interface methods
   public ServerSocket createServerSocket(int port) throws IOException
   {
      return createServerSocket(port, 50, bindAddress);
   }
   public ServerSocket createServerSocket(int port, int backlog)
      throws IOException
   {
      return createServerSocket(port, backlog, bindAddress);
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
   public ServerSocket createServerSocket(int port, int backlog, InetAddress ifAddress)
      throws IOException
   {
      initSSLContext();
      ServerSocketFactory factory = sslCtx.getServerSocketFactory();
      ServerSocket socket = factory.createServerSocket(port, backlog, ifAddress);
      return socket;
   }

   /** The default ServerSocketFactory which looks to the java:/jaas/other
    security domain configuration.
    */
   public static ServerSocketFactory getDefault()
   {
      DomainServerSocketFactory ssf = null;
      try
      {
         InitialContext iniCtx = new InitialContext();
         SecurityDomain sd = (SecurityDomain) iniCtx.lookup("java:/jaas/other");
         ssf = new DomainServerSocketFactory(sd);
      }
      catch(Exception e)
      {
         log.error("Failed to create default ServerSocketFactory", e);
      }
      return ssf;
   }
   
   public String[] getDefaultCipherSuites()
   {
      String[] cipherSuites = {};
      try
      {
         initSSLContext();
         SSLServerSocketFactory factory = (SSLServerSocketFactory) sslCtx.getServerSocketFactory();
         cipherSuites = factory.getDefaultCipherSuites();
      }
      catch(IOException e)
      {
         log.error("Failed to get default SSLServerSocketFactory", e);
      }      
      return cipherSuites;
   }
   
   public String[] getSupportedCipherSuites()
   {
      String[] cipherSuites = {};
      try
      {
         initSSLContext();
         SSLServerSocketFactory factory = (SSLServerSocketFactory) sslCtx.getServerSocketFactory();
         cipherSuites = factory.getSupportedCipherSuites();
      }
      catch(IOException e)
      {
         log.error("Failed to get default SSLServerSocketFactory", e);
      }      
      return cipherSuites;
   }
   
// --- End SSLServerSocketFactory interface methods

   private void initSSLContext()
      throws IOException
   {
      if( sslCtx != null )
         return;

      try
      {
         sslCtx = SSLContext.getInstance("TLS");
         KeyManagerFactory keyMgr = securityDomain.getKeyManagerFactory();
         if( keyMgr == null )
            throw new IOException("KeyManagerFactory is null for security domain: "+securityDomain.getSecurityDomain());
         TrustManagerFactory trustMgr = securityDomain.getTrustManagerFactory();
         TrustManager[] trustMgrs = null;
         if( trustMgr != null )
            trustMgrs = trustMgr.getTrustManagers();
         sslCtx.init(keyMgr.getKeyManagers(), trustMgrs, null);
      }
      catch(NoSuchAlgorithmException e)
      {
         log.error("Failed to get SSLContext for TLS algorithm", e);
         throw new IOException("Failed to get SSLContext for TLS algorithm");
      }
      catch(KeyManagementException e)
      {
         log.error("Failed to init SSLContext", e);
         throw new IOException("Failed to init SSLContext");
      }
      catch(SecurityException e)
      {
         log.error("Failed to init SSLContext", e);
         throw new IOException("Failed to init SSLContext");
      }
   }
}
