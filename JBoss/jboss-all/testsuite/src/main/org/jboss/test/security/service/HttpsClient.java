/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.security.service;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.net.JarURLConnection;
import java.net.Socket;
import java.net.URL;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.security.Security;
import java.util.StringTokenizer;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import javax.net.ssl.SSLSocketFactory;

import com.sun.net.ssl.internal.ssl.Provider;

import org.jboss.logging.Logger;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.invocation.http.interfaces.Util;

/** A test mbean service that reads input from an https url passed in
 to its readURL method.

 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.4.4 $
 */
public class HttpsClient extends ServiceMBeanSupport
   implements HttpsClientMBean
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------
   private boolean addedHttpsHandler;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   public HttpsClient()
   {
   }

   public String getName()
   {
      return "HttpsClient";
   }

   /** Read the contents of the given URL and return it. */
   public String readURL(String urlString) throws IOException
   {
      try
      {
         String reply = internalReadURL(urlString);
         log.debug("readURL -> "+reply);
         return reply;
      }
      catch(Throwable e)
      {
         log.error("Failed to readURL", e);
         throw new IOException("Failed to readURL, ex="+e.getMessage());
      }
   }
   private String internalReadURL(String urlString) throws Exception
   {
      log.debug("Creating URL from string: "+urlString);
      URL url = new URL(urlString);
      log.debug("Created URL object from string, protocol="+url.getProtocol());
      HttpURLConnection conn = (HttpURLConnection) url.openConnection();
      /* Override the host verifier so we can use a test server cert with
       a hostname that may not match the https url hostname.
      */
      System.setProperty("org.jboss.security.ignoreHttpsHost", "true");
      Util.configureHttpsHostVerifier(conn);

      log.debug("Connecting to URL: "+url);
      byte[] buffer = new byte[1024];
      int length = conn.getContentLength();
      log.debug("ContentLength: "+length);
      InputStream is = conn.getInputStream();
      StringBuffer reply = new StringBuffer();
      while( (length = is.read(buffer)) > 0 )
         reply.append(new String(buffer, 0, length));
      log.debug("Done, closing streams");
      is.close();
      return reply.toString();
   }

   // Public --------------------------------------------------------
   protected void startService() throws Exception
   {
      try
      {
         new URL("https://www.https.test");
      }
      catch(MalformedURLException e)
      {
         // Install the default JSSE security provider
         log.debug("Adding com.sun.net.ssl.internal.ssl.Provider");
         Security.addProvider(new Provider());
         addedHttpsHandler = false;
         // Install the JSSE https handler if it has not already been added
         String handlers = System.getProperty("java.protocol.handler.pkgs");
         if( handlers == null || handlers.indexOf("com.sun.net.ssl.internal.www.protocol") < 0 )
         {
            handlers += "|com.sun.net.ssl.internal.www.protocol";
            log.debug("Adding https handler to java.protocol.handler.pkgs");
            System.setProperty("java.protocol.handler.pkgs", handlers);
            addedHttpsHandler = true;
         }
      }

      // Install the trust store
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      URL keyStoreURL = loader.getResource("META-INF/tst.keystore");
      if( keyStoreURL == null )
         throw new IOException("Failed to find resource tst.keystore");
      if( keyStoreURL.getProtocol().equals("jar") )
      {
         JarURLConnection conn = (JarURLConnection) keyStoreURL.openConnection();
         JarFile jar = conn.getJarFile();
         JarEntry entry = jar.getJarEntry("META-INF/tst.keystore");
         InputStream is = jar.getInputStream(entry);
         File tmp = File.createTempFile("tst-", ".keystore");
         tmp.deleteOnExit();
         FileOutputStream fos = new FileOutputStream(tmp);
         byte[] buffer = new byte[1024];
         int bytes;
         while( (bytes = is.read(buffer)) > 0 )
            fos.write(buffer, 0, bytes);
         fos.close();
         is.close();
         keyStoreURL = tmp.toURL();
      }
      log.debug("Setting javax.net.ssl.trustStore to: "+keyStoreURL.getPath());
      System.setProperty("javax.net.ssl.trustStore", keyStoreURL.getPath());
   }
   protected void stopService() throws Exception
   {
      String name = (new Provider()).getName();
      log.debug("Removing com.sun.net.ssl.internal.ssl.Provider");
      Security.removeProvider(name);
      if( addedHttpsHandler == true )
      {
         log.debug("Removing https handler from java.protocol.handler.pkgs");
         String handlers = System.getProperty("java.protocol.handler.pkgs");
         StringTokenizer tokenizer = new StringTokenizer(handlers, "|");
         StringBuffer buffer = new StringBuffer();
         while( tokenizer.hasMoreTokens() )
         {
            String handler = tokenizer.nextToken();
            if( handler.equals("com.sun.net.ssl.internal.www.protocol") == false )
            {
               buffer.append('|');
               buffer.append(handler);
            }
         }
         System.setProperty("java.protocol.handler.pkgs", buffer.toString());
      }
   }

   /** A SSLSocketFactory that logs the createSocket calls.
    */
   class DebugSSLSocketFactory extends SSLSocketFactory
   {
      SSLSocketFactory factoryDelegate;
      Logger theLog;
      DebugSSLSocketFactory(SSLSocketFactory factoryDelegate, Logger theLog)
      {
         this.factoryDelegate = factoryDelegate;
         this.theLog = theLog;
      }

      public Socket createSocket(java.net.InetAddress host, int port) throws java.io.IOException
      {
         theLog.debug("createSocket, host="+host+", port="+port);
         Socket s = factoryDelegate.createSocket(host, port);
         theLog.debug("created socket="+s);
         return s;
      }

      public Socket createSocket(String host, int port)
         throws java.io.IOException, java.net.UnknownHostException
      {
         theLog.debug("createSocket, host="+host+", port="+port);
         Socket s = factoryDelegate.createSocket(host, port);
         theLog.debug("created socket="+s);
         return s;
      }

      public Socket createSocket(Socket socket, String host, int port, boolean autoClose)
         throws java.io.IOException
      {
         theLog.debug("createSocket, socket="+socket+", host="+host+", port="+port);
         Socket s = factoryDelegate.createSocket(socket, host, port, autoClose);
         theLog.debug("created socket="+s);
         return s;
      }

      public Socket createSocket(java.net.InetAddress host, int port, java.net.InetAddress clientAddress, int clientPort)
         throws java.io.IOException
      {
         theLog.debug("createSocket, host="+host+", port="+port+", clientAddress="+clientAddress+", clientPort="+clientPort);
         Socket s = factoryDelegate.createSocket(host, port, clientAddress, clientPort);
         theLog.debug("created socket="+s);
         return s;
      }

      public Socket createSocket(String host, int port, java.net.InetAddress clientAddress, int clientPort)
         throws java.io.IOException, java.net.UnknownHostException
      {
         theLog.debug("createSocket, host="+host+", port="+port+", addr="+clientAddress);
         Socket s = factoryDelegate.createSocket(host, port, clientAddress, clientPort);
         theLog.debug("created socket="+s);
         return s;
      }

      public String[] getDefaultCipherSuites()
      {
         return factoryDelegate.getDefaultCipherSuites();      
      }

      public String[] getSupportedCipherSuites()
      {
         return factoryDelegate.getSupportedCipherSuites();
      }
   }

}
