/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.security.test;

import com.sun.net.ssl.internal.ssl.Provider;
import com.sun.net.ssl.KeyManager;
import com.sun.net.ssl.KeyManagerFactory;
import com.sun.net.ssl.SSLContext;
import com.sun.net.ssl.TrustManager;
import com.sun.net.ssl.TrustManagerFactory;

import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.URL;
import java.security.KeyStore;
import java.security.Security;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.TimeZone;
import javax.management.ObjectName;
import javax.net.ServerSocketFactory;
import javax.net.ssl.SSLServerSocket;
import javax.net.ssl.SSLServerSocketFactory;

import junit.extensions.TestSetup;
import junit.framework.Test;
import junit.framework.TestSuite;

import org.apache.log4j.Category;
import org.jboss.test.JBossTestCase;
import org.jboss.test.JBossTestSetup;
import org.jboss.test.util.Debug;

/** Test of using https urls inside of the JBoss server. This testcase
 creates a simple https server and deploys a service that tries to
 connect to the server using the https url passed to the service.

 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.4.6 $
 */
public class HttpsUnitTestCase extends JBossTestCase
{
   static final String JAR = "https-service.sar";
   static final String KEYSTORE_PASSWORD = "unit-tests";

   public HttpsUnitTestCase(String name)
   {
      super(name);
   }

   /** Test the JSSE installation
    */
   public void testJSSE() throws Exception
   {
      log.debug("+++ testJSSE");
      ServerSocketFactory factory =
         SSLServerSocketFactory.getDefault();
      SSLServerSocket sslSocket = (SSLServerSocket)
         factory.createServerSocket(0);
      int port = sslSocket.getLocalPort();

      String [] cipherSuites = sslSocket.getEnabledCipherSuites();
      for(int i = 0; i < cipherSuites.length; i++)
      {
         getLog().debug("Cipher Suite " + i +
         " = " + cipherSuites[i]);
      }
      sslSocket.close();
   }

   /** Test a login against the SRP service using the SRPLoginModule
    */
   public void testHttpsURL() throws Exception
   {
      log.debug("+++ testHttpsURL");
      // Setup the SSL listening port
      String httpsURL = initServer();
      log.debug("Setup SSL socket, URL="+httpsURL);
      // Have the service in JBoss use the https url
      ObjectName name = new ObjectName("jboss.security.tests:service=HttpsClient");
      String method = "readURL";
      Object[] args = {httpsURL};
      String[] sig = {"java.lang.String"};
      String reply = (String) invoke(name, method, args, sig);
      log.debug("Reply for url="+httpsURL+" is: "+reply);
   }

   private String initServer() throws Exception
   {
      String httpsURL = null;
      SSLContext sslCtx = null;
      try
      {
         sslCtx = SSLContext.getInstance("TLS");
         ClassLoader loader = getClass().getClassLoader();
         URL keyStoreURL = loader.getResource("tst.keystore");
         if( keyStoreURL == null )
            throw new IOException("Failed to find resource tst.keystore");
         log.debug("Opening KeyStore: "+keyStoreURL);
         KeyStore keyStore = KeyStore.getInstance("JKS");
         InputStream is = keyStoreURL.openStream();
         keyStore.load(is, KEYSTORE_PASSWORD.toCharArray());
         String algorithm = KeyManagerFactory.getDefaultAlgorithm();
         KeyManagerFactory keyMgr = KeyManagerFactory.getInstance(algorithm);
         keyMgr.init(keyStore, KEYSTORE_PASSWORD.toCharArray());
         algorithm = TrustManagerFactory.getDefaultAlgorithm();
         TrustManagerFactory trustMgr = TrustManagerFactory.getInstance(algorithm);
         trustMgr.init(keyStore);
         TrustManager[] trustMgrs = trustMgr.getTrustManagers();
         sslCtx.init(keyMgr.getKeyManagers(), trustMgrs, null);
      }
      catch(Exception e)
      {
         log.error("Failed to init SSLContext", e);
         throw new IOException("Failed to get SSLContext for TLS algorithm");
      }

      ServerSocketFactory factory = sslCtx.getServerSocketFactory();
      ServerSocket serverSocket = factory.createServerSocket(0);
      getLog().debug("Created serverSocket: "+serverSocket);
      int port = serverSocket.getLocalPort();
      InetAddress addr = serverSocket.getInetAddress();
      httpsURL = "https://localhost:" + port + '/';
      AcceptThread thread = new AcceptThread(serverSocket, getLog(), httpsURL);
      synchronized( httpsURL )
      {
         log.debug("Starting server socket thread");
         thread.start();
         log.debug("Waiting for accept thread notify");
         httpsURL.wait();
      }
      return httpsURL;
   }

   /**
    * Setup the test suite.
    */
   public static Test suite() throws Exception
   {
      TestSuite suite = new TestSuite();
      suite.addTest(new TestSuite(HttpsUnitTestCase.class));

      // Create an initializer for the test suite
      TestSetup wrapper = new JBossTestSetup(suite)
      {
         protected void setUp() throws Exception
         {
            super.setUp();
            deploy(JAR);
            Security.addProvider(new com.sun.net.ssl.internal.ssl.Provider());
         }
         protected void tearDown() throws Exception
         {
            undeploy(JAR);
            super.tearDown();
         }
      };
      return wrapper;
   }

   /** A subclass of Thread that processes a single request sent to
    the serverSocket.
    */
   static class AcceptThread extends Thread
   {
      ServerSocket serverSocket;
      Category log;
      Object lock;
      AcceptThread(ServerSocket serverSocket, Category log, Object lock)
      {
         super("AcceptThread");
         super.setDaemon(true);
         this.serverSocket = serverSocket;
         this.log = log;
         this.lock = lock;
      }

      public void run()
      {
         SimpleDateFormat fmt = new SimpleDateFormat("E, dd MMM yyyy HH:mm:ss z");
         fmt.setTimeZone(TimeZone.getTimeZone("GMT"));
         Date now = new Date();
         String dateString = fmt.format(now);
         String content = "<html><head><title>HttpsUnitTestCase</title></head>"
            + "<body>"+dateString+"</body></html>\r\n";
         String reply = "HTTP/1.1 200 OK\r\n"
            + "Date: "+dateString+"\r\n"
            + "Server: HttpsUnitTestCase/JSSE SSL\r\n"
            + "Last-Modified: "+dateString+"\r\n"
            + "Content-Length: "+content.length()+"\r\n"
            + "Connection: close\r\n"
            + "Content-Type: text/html\r\n\r\n"
            + content;

         while( true )
         {
            try
            {
               log.debug("Waiting for client connection");
               synchronized( lock )
               {
                  lock.notify();
               }
               Socket client = serverSocket.accept();
               log.debug("Accepted client: "+client);
               InputStream is = client.getInputStream();
               OutputStream os = client.getOutputStream();
               byte[] buffer = new byte[4096];
               int bytes = is.read(buffer);
               log.debug("Read: "+bytes);
               os.write(reply.getBytes());
               os.flush();
               log.debug("Wrote: "+reply.length());
               log.debug("ReplyData: "+reply);
               os.close();
               is.close();
               client.close();
               log.debug("Closed client");
            }
            catch(Exception e)
            {
               log.error("Failed to process request", e);
               break;
            }
         }

         try
         {
            serverSocket.close();
         }
         catch(Exception e)
         {
            log.error("Failed to close server socket", e);
         }
      }
   }
}
