/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.uil2;

import java.io.IOException;
import java.lang.reflect.Method;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.UnknownHostException;
import java.util.Properties;
import javax.naming.InitialContext;
import javax.net.ServerSocketFactory;

import org.jboss.mq.il.Invoker;
import org.jboss.mq.il.ServerIL;
import org.jboss.mq.il.ServerILJMXService;
import org.jboss.mq.il.uil2.msgs.MsgTypes;
import org.jboss.mq.il.uil2.msgs.BaseMsg;
import org.jboss.security.SecurityDomain;
import org.jboss.system.server.ServerConfigUtil;

/** This is the server side MBean for the UIL2 transport layer.
 *
 * @author Scott.Stark@jboss.org
 * @version   $Revision: 1.1.4.4 $
 *
 * @jmx:mbean extends="org.jboss.mq.il.ServerILJMXServiceMBean"
 */
public class UILServerILService extends ServerILJMXService
      implements MsgTypes, Runnable, UILServerILServiceMBean
{
   final static int SO_TIMEOUT = 5000;

   private Invoker server;
   /** The security domain name to use with SSL aware socket factories.
    */
   private String securityDomain;
   /* The javax.net.SocketFactory implementation class to use on the client.
    */
   private String clientSocketFactoryName;
   /** The socket factory used to obtain the server socket.
    */
   private ServerSocketFactory serverSocketFactory;
   /** The UIL2 server socket clients connect to
    */
   private ServerSocket serverSocket;
   private UILServerIL serverIL;
   private boolean running;
   private int serverBindPort = 0;
   private InetAddress bindAddress = null;
   /** The thread that manages the client connection attempts */
   private Thread acceptThread;

   /**
    * If the TcpNoDelay option should be used on the socket.
    */
   private boolean enableTcpNoDelay = false;

   /**
    * The socket read timeout.
    */
   private int readTimeout = 0;

   /**
    * The buffer size.
    */
   private int bufferSize = 1;

   /**
    * The chunk size.
    */
   private int chunkSize = 0x40000000;

   /**
    * The connection properties passed to the client to connect to this IL
    */
   private Properties connectionProperties;

   /**
    * Used to construct the GenericConnectionFactory (bindJNDIReferences()
    * builds it) Sets up the connection properties need by a client to use this
    * IL
    *
    * @return   The ClientConnectionProperties value
    */
   public Properties getClientConnectionProperties()
   {
      return connectionProperties;
   }

   /**
    * Used to construct the GenericConnectionFactory (bindJNDIReferences()
    * builds it)
    *
    * @return    The ServerIL value
    * @returns   ServerIL the instance of this IL
    */
   public ServerIL getServerIL()
   {
      return serverIL;
   }

   /** Client socket accept thread.
    */
   public void run()
   {
      while (running)
      {
         Socket socket = null;
         SocketManager socketMgr = null;
         try
         {
            socket = serverSocket.accept();
            socket.setSoTimeout(readTimeout);
            socket.setTcpNoDelay(enableTcpNoDelay);
            socketMgr = new SocketManager(socket);
            ServerSocketManagerHandler handler = new ServerSocketManagerHandler(server, socketMgr);
            socketMgr.setHandler(handler);
            socketMgr.setBufferSize(bufferSize);
            socketMgr.setChunkSize(chunkSize);
            socketMgr.start(server.getThreadGroup());
         }
         catch (IOException e)
         {
            if (running)
               log.warn("Failed to setup client connection", e);
         }
      }
   }

   /**
    * Starts this IL, and binds it to JNDI
    *
    * @exception Exception  Description of Exception
    */
   public void startService() throws Exception
   {
      super.startService();
      running = true;
      this.server = lookupJMSServer();

      // Use the default javax.net.ServerSocketFactory if none was set
      if (serverSocketFactory == null)
         serverSocketFactory = ServerSocketFactory.getDefault();

      /* See if the server socket supports setSecurityDomain(SecurityDomain)
      if an securityDomain was specified
      */
      if (securityDomain != null)
      {
         try
         {
            InitialContext ctx = new InitialContext();
            Class ssfClass = serverSocketFactory.getClass();
            SecurityDomain domain = (SecurityDomain) ctx.lookup(securityDomain);
            Class[] parameterTypes = {SecurityDomain.class};
            Method m = ssfClass.getMethod("setSecurityDomain", parameterTypes);
            Object[] args = {domain};
            m.invoke(serverSocketFactory, args);
         }
         catch (NoSuchMethodException e)
         {
            log.error("Socket factory does not support setSecurityDomain(SecurityDomain)");
         }
         catch (Exception e)
         {
            log.error("Failed to setSecurityDomain=" + securityDomain + " on socket factory");
         }
      }

      // Create the server socket using the socket factory
      serverSocket = serverSocketFactory.createServerSocket(serverBindPort, 50, bindAddress);

      InetAddress socketAddress = serverSocket.getInetAddress();
      log.info("JBossMQ UIL service available at : " + socketAddress + ":" + serverSocket.getLocalPort());
      acceptThread = new Thread(server.getThreadGroup(), this, "UILServerILService Accept Thread");
      acceptThread.start();

      /* We need to check the socketAddress against "0.0.0.0/0.0.0.0"
         because this is not a valid address on Win32 while it is for
         *NIX. See BugParade bug #4343286.
      */
      socketAddress = ServerConfigUtil.fixRemoteAddress(socketAddress);

      serverIL = new UILServerIL(socketAddress, serverSocket.getLocalPort(),
            clientSocketFactoryName, enableTcpNoDelay, bufferSize, chunkSize);

      // Initialize the connection poperties using the base class.
      connectionProperties = super.getClientConnectionProperties();
      connectionProperties.setProperty(UILServerILFactory.CLIENT_IL_SERVICE_KEY, UILClientILService.class.getName());
      connectionProperties.setProperty(UILServerILFactory.UIL_PORT_KEY, "" + serverSocket.getLocalPort());
      connectionProperties.setProperty(UILServerILFactory.UIL_ADDRESS_KEY, "" + socketAddress.getHostAddress());
      connectionProperties.setProperty(UILServerILFactory.UIL_TCPNODELAY_KEY, enableTcpNoDelay ? "yes" : "no");
      connectionProperties.setProperty(UILServerILFactory.UIL_BUFFERSIZE_KEY, "" + bufferSize);
      connectionProperties.setProperty(UILServerILFactory.UIL_CHUNKSIZE_KEY, "" + chunkSize);

      bindJNDIReferences();
      BaseMsg.setUseJMSServerMsgIDs(true);
   }

   /**
    * Stops this IL, and unbinds it from JNDI
    */
   public void stopService()
   {
      try
      {
         running = false;
         unbindJNDIReferences();

         // unbind Server Socket if needed
         if (serverSocket != null)
         {
            serverSocket.close();
         }
      }
      catch (Exception e)
      {
         log.error("Exception occured when trying to stop UIL Service: ", e);
      }
   }

   /**
    * Get the UIL server listening port
    *
    * @return Value of property serverBindPort.
    *
    * @jmx:managed-attribute
    */
   public int getServerBindPort()
   {
      return serverBindPort;
   }

   /**
    * Set the UIL server listening port
    *
    * @param serverBindPort New value of property serverBindPort.
    *
    * @jmx:managed-attribute
    */
   public void setServerBindPort(int serverBindPort)
   {
      this.serverBindPort = serverBindPort;
   }

   /**
    * Get the interface address the OIL server bind its listening port on
    *
    * @jmx:managed-attribute
    */
   public String getBindAddress()
   {
      String addr = "0.0.0.0";
      if (bindAddress != null)
         addr = bindAddress.getHostName();
      return addr;
   }

   /**
    * Set the interface address the OIL server bind its listening port on
    *
    * @jmx:managed-attribute
    */
   public void setBindAddress(String host) throws UnknownHostException
   {
      // If host is null or empty use any address
      if (host == null || host.length() == 0)
         bindAddress = null;
      else
         bindAddress = InetAddress.getByName(host);
   }

   /**
    * Gets the enableTcpNoDelay.
    * @return Returns a boolean
    *
    * @jmx:managed-attribute
    */
   public boolean getEnableTcpNoDelay()
   {
      return enableTcpNoDelay;
   }

   /**
    * Sets the enableTcpNoDelay.
    * @param enableTcpNoDelay The enableTcpNoDelay to set
    *
    * @jmx:managed-attribute
    */
   public void setEnableTcpNoDelay(boolean enableTcpNoDelay)
   {
      this.enableTcpNoDelay = enableTcpNoDelay;
   }

   /**
    * Gets the buffer size.
    * @return Returns an int
    *
    * @jmx:managed-attribute
    */
   public int getBufferSize()
   {
      return bufferSize;
   }

   /**
    * Sets the buffer size.
    * @param size the buffer size
    *
    * @jmx:managed-attribute
    */
   public void setBufferSize(int size)
   {
      this.bufferSize = size;
   }

   /**
    * Gets the chunk size.
    * @return Returns an int
    *
    * @jmx:managed-attribute
    */
   public int getChunkSize()
   {
      return chunkSize;
   }

   /**
    * Sets the chunk size.
    * @param size the chunk size
    *
    * @jmx:managed-attribute
    */
   public void setChunkSize(int size)
   {
      this.chunkSize = size;
   }

   /**
    * Gets the socket read timeout.
    * @return Returns the read timeout in milli-seconds
    *
    * @jmx:managed-attribute
    */
   public int getReadTimeout()
   {
      return readTimeout;
   }

   /**
    * Sets the read time out.
    * @param timeout The read time out in milli seconds
    *
    * @jmx:managed-attribute
    */
   public void setReadTimeout(int timeout)
   {
      this.readTimeout = timeout;
   }

   /** Get the javax.net.SocketFactory implementation class to use on the
    *client.
    * @jmx:managed-attribute
    */
   public String getClientSocketFactory()
   {
      return clientSocketFactoryName;
   }

   /** Set the javax.net.SocketFactory implementation class to use on the
    *client.
    * @jmx:managed-attribute
    */
   public void setClientSocketFactory(String name)
   {
      this.clientSocketFactoryName = name;
   }

   /** Set the javax.net.ServerSocketFactory implementation class to use to
    *create the service SocketFactory.
    *@jmx:managed-attribute
    */
   public void setServerSocketFactory(String name) throws Exception
   {
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      Class ssfClass = loader.loadClass(name);
      serverSocketFactory = (ServerSocketFactory) ssfClass.newInstance();
   }

   /** Get the javax.net.ServerSocketFactory implementation class to use to
    *create the service SocketFactory.
    *@jmx:managed-attribute
    */
   public String getServerSocketFactory()
   {
      String name = null;
      if (serverSocketFactory != null)
         name = serverSocketFactory.getClass().getName();
      return name;
   }

   /** Set the security domain name to use with SSL aware socket factories
    *@jmx:managed-attribute
    */
   public void setSecurityDomain(String domainName)
   {
      this.securityDomain = domainName;
   }

   /** Get the security domain name to use with SSL aware socket factories
    *@jmx:managed-attribute
    */
   public String getSecurityDomain()
   {
      return this.securityDomain;
   }
}
