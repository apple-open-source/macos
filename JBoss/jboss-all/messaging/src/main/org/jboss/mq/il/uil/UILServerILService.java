/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.uil;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.EOFException;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.lang.reflect.Method;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.rmi.RemoteException;
import java.util.Properties;

import javax.jms.Destination;
import javax.jms.JMSException;
import javax.jms.Queue;
import javax.jms.TemporaryQueue;
import javax.jms.TemporaryTopic;
import javax.jms.Topic;
import javax.naming.InitialContext;
import javax.net.ServerSocketFactory;

import org.jboss.mq.AcknowledgementRequest;
import org.jboss.mq.ConnectionToken;
import org.jboss.mq.DurableSubscriptionID;
import org.jboss.mq.SpyDestination;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.Subscription;
import org.jboss.mq.TransactionRequest;
import org.jboss.mq.il.Invoker;
import org.jboss.mq.il.ServerIL;
import org.jboss.mq.il.uil.multiplexor.SocketMultiplexor;
import org.jboss.security.SecurityDomain;
import org.jboss.system.server.ServerConfigUtil;

/**
 * Implements the ServerILJMXService which is used to manage the JVM IL.
 *
 * @author    Hiram Chirino (Cojonudo14@hotmail.com)
 * @author    <a href="pra@tim.se">Peter Antman</a>
 * @version   $Revision: 1.21.2.6 $
 *
 * @jmx:mbean extends="org.jboss.mq.il.ServerILJMXServiceMBean"
 */
public class UILServerILService extends org.jboss.mq.il.ServerILJMXService implements Runnable, UILServerILServiceMBean
{
   //The server implementation
   /**
    * Description of the Field
    */
   // protected static JMSServerInvoker server;
   protected Invoker server;

   final static int m_acknowledge = 1;
   final static int m_addMessage = 2;
   final static int m_browse = 3;
   final static int m_checkID = 4;
   final static int m_connectionClosing = 5;
   final static int m_createQueue = 6;
   final static int m_createTopic = 7;
   final static int m_deleteTemporaryDestination = 8;
   final static int m_getID = 9;
   final static int m_getTemporaryQueue = 10;
   final static int m_getTemporaryTopic = 11;
   final static int m_listenerChange = 12;
   final static int m_receive = 13;
   final static int m_setEnabled = 14;
   final static int m_setSpyDistributedConnection = 15;
   final static int m_subscribe = 16;
   final static int m_transact = 17;
   final static int m_unsubscribe = 18;
   final static int m_destroySubscription = 19;
   final static int m_checkUser = 20;
   final static int m_ping = 21;
   final static int m_authenticate = 22;
   
   final static int SO_TIMEOUT = 5000;
   /** The security domain name to use with SSL aware socket factories.
    */
   private String securityDomain;

   /* The javax.net.SocketFactory implementation class to use on the client.
    */
   private String clientSocketFactoryName;
   /** The socket factory used to obtain the server socket.
    */
   private ServerSocketFactory serverSocketFactory;

   /**
    * Description of the Field
    */
   protected ServerSocket serverSocket;
   UILServerIL serverIL;

   boolean running;
   int serverBindPort = 0;
   InetAddress bindAddress = null;
   Thread worker;
   
   /**
    * If the TcpNoDelay option should be used on the socket.
    */
   private boolean enableTcpNoDelay=false;   
   
   /**
    * Number of OIL Worker threads started.
    */
   private int threadNumber = 0;
   
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
   public java.util.Properties getClientConnectionProperties()
   {
      return connectionProperties;
   }

   /**
    * Gives this JMX service a name.
    *
    * @return   The Name value
    */
   public String getName()
   {
      return "JBossMQ-UILServerIL";
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

   /**
    * Main processing method for the UILServerILService object
    */
   public void run()
   {
      Socket socket = null;
      SocketMultiplexor mSocket = null;
      int code = 0;
      ObjectOutputStream out = null;
      ObjectInputStream in = null;
      ConnectionToken connectionToken = null;
      boolean closed = false;

      try
      {

         while (running && socket == null)
         {
            try
            {
               socket = serverSocket.accept();
            }
            catch (java.io.InterruptedIOException e)
            {
            }
         }

         if (!running)
         {
            return;
         }

         socket.setSoTimeout(0);
         socket.setTcpNoDelay(enableTcpNoDelay);
         new Thread(this, "UIL Worker-"+threadNumber++).start();

         mSocket = new SocketMultiplexor(socket);

         out = new ObjectOutputStream(new BufferedOutputStream(mSocket.getOutputStream(1)));
         out.flush();

         in = new ObjectInputStream(new BufferedInputStream(mSocket.getInputStream(1)));

      }
      catch (SocketException e)
      {
         // There is no easy way (other than string comparison) to
         // determine if the socket exception is caused by connection
         // reset by peer. In this case, it's okay to ignore both
         // SocketException and IOException.
         log.warn("SocketException occured (Connection reset by peer?). Cannot initialize the UILServerILService.");
         return;
      }
      catch (IOException e)
      {
         log.warn("IOException occured. Cannot initialize the UILServerILService.");
         return;
      }

      while (!closed && running)
      {

         try
         {
            code = in.readByte();
         }
         catch (EOFException e)
         {
            break;
         }
         catch (IOException e)
         {
            if (closed || !running)
            {
               break;
            }
            log.debug("Connection failure (1).", e);
            break;
         }

         try
         {
            boolean sendAck = true;
            Object result = null;

            switch (code)
            {
               case m_setSpyDistributedConnection:
                  log.debug("Setting up the UILClientIL Connection");
                  connectionToken = (ConnectionToken)in.readObject();
                  ((UILClientIL)connectionToken.clientIL).mSocket = mSocket;
                  ((UILClientIL)connectionToken.clientIL).createConnection();
                  log.debug("The UILClientIL Connection is set up");
                  break;
               case m_acknowledge:
                  AcknowledgementRequest ack = new AcknowledgementRequest();
                  ack.readExternal(in);
                  server.acknowledge(connectionToken, ack);
                  break;
               case m_addMessage:
                  server.addMessage(connectionToken, SpyMessage.readMessage(in));
                  break;
               case m_browse:
                  result = server.browse(connectionToken, (Destination)in.readObject(), (String)in.readObject());
                  break;
               case m_checkID:
                  String ID = (String)in.readObject();
                  server.checkID(ID);
                  if (connectionToken != null)
                     connectionToken.setClientID(ID);
                  break;
               case m_connectionClosing:
                  server.connectionClosing(connectionToken);
                  closed = true;
                  break;
               case m_createQueue:
                  result = (Queue)server.createQueue(connectionToken, (String)in.readObject());
                  break;
               case m_createTopic:
                  result = (Topic)server.createTopic(connectionToken, (String)in.readObject());
                  break;
               case m_deleteTemporaryDestination:
                  server.deleteTemporaryDestination(connectionToken, (SpyDestination)in.readObject());
                  break;
               case m_getID:
                  result = server.getID();
                  if (connectionToken != null)
                     connectionToken.setClientID((String)result);
                  break;
               case m_getTemporaryQueue:
                  result = (TemporaryQueue)server.getTemporaryQueue(connectionToken);
                  break;
               case m_getTemporaryTopic:
                  result = (TemporaryTopic)server.getTemporaryTopic(connectionToken);
                  break;
               case m_receive:
                  result = server.receive(connectionToken, in.readInt(), in.readLong());
                  break;
               case m_setEnabled:
                  server.setEnabled(connectionToken, in.readBoolean());
                  break;
               case m_subscribe:
                  server.subscribe(connectionToken, (Subscription)in.readObject());
                  break;
               case m_transact:
                  TransactionRequest trans = new TransactionRequest();
                  trans.readExternal(in);
                  server.transact(connectionToken, trans);
                  break;
               case m_unsubscribe:
                  server.unsubscribe(connectionToken, in.readInt());
                  break;
               case m_destroySubscription:
                  server.destroySubscription(connectionToken,(DurableSubscriptionID)in.readObject());
                  break;
               case m_checkUser:
                  result = server.checkUser((String)in.readObject(), (String)in.readObject());
                  break;
               case m_ping:
                  sendAck = false;
                  server.ping(connectionToken, in.readLong());
                  break;
               case m_authenticate:
                  result = server.authenticate((String)in.readObject(), (String)in.readObject());
                  break;
               default:
                  throw new RemoteException("Bad method code !");
            }

            // Do not send an ack for waitAnswer unless sendAck is true
            if( sendAck == false )
               continue;

            //Everthing was OK
            try
            {
               if (result == null)
               {
                  out.writeByte(0);
               }
               else
               {
                  out.writeByte(1);
                  out.writeObject(result);
                  out.reset();
               }
               out.flush();
            }
            catch (IOException e)
            {
               if (closed)
               {
                  break;
               }

               log.warn("Connection failure (2).", e);
               break;
            }

         }
         catch (Exception e)
         {
            if (closed)
            {
               break;
            }

            log.info("Client request resulted in a server exception: ", e);

            try
            {
               out.writeByte(2);
               out.writeObject(e);
               out.reset();
               out.flush();
            }
            catch (IOException e2)
            {
               if (closed)
               {
                  break;
               }

               log.warn("Connection failure (3).", e);
               break;
            }
         }
      }

      try
      {

         if (!closed)
         {
            try
            {
               server.connectionClosing(connectionToken);
            }
            catch (JMSException ignore)
            {
            }
         }
         mSocket.close();
      }
      catch (IOException e)
      {
         log.warn("Connection failure during connection close.", e);
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
      if( serverSocketFactory == null )
         serverSocketFactory = ServerSocketFactory.getDefault();

      /* See if the server socket supports setSecurityDomain(SecurityDomain)
      if an securityDomain was specified
      */
      if( securityDomain != null )
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
         catch(NoSuchMethodException e)
         {
            log.error("Socket factory does not support setSecurityDomain(SecurityDomain)");
         }
         catch(Exception e)
         {
            log.error("Failed to setSecurityDomain="+securityDomain+" on socket factory");
         }
      }

      // Create the server socket using the socket factory
      serverSocket = serverSocketFactory.createServerSocket(serverBindPort, 50, bindAddress);
      serverSocket.setSoTimeout(SO_TIMEOUT);

      InetAddress socketAddress = serverSocket.getInetAddress();
      log.info("JBossMQ UIL service available at : " + socketAddress + ":" + serverSocket.getLocalPort());
      worker = new Thread(server.getThreadGroup(), this, "UIL Worker");
      worker.start();

      /* We need to check the socketAddress against "0.0.0.0/0.0.0.0"
         because this is not a valid address on Win32 while it is for
         *NIX. See BugParade bug #4343286.
      */
      socketAddress = ServerConfigUtil.fixRemoteAddress(socketAddress);

      serverIL = new UILServerIL(socketAddress, serverSocket.getLocalPort(),
         clientSocketFactoryName, enableTcpNoDelay);

      // Initialize the connection poperties using the base class.
      connectionProperties = super.getClientConnectionProperties();
      connectionProperties.setProperty(UILServerILFactory.CLIENT_IL_SERVICE_KEY, "org.jboss.mq.il.uil.UILClientILService");
      connectionProperties.setProperty(UILServerILFactory.UIL_PORT_KEY, ""+serverSocket.getLocalPort());
      connectionProperties.setProperty(UILServerILFactory.UIL_ADDRESS_KEY, ""+socketAddress.getHostAddress());
      connectionProperties.setProperty(UILServerILFactory.UIL_TCPNODELAY_KEY, enableTcpNoDelay?"yes":"no");

      bindJNDIReferences();

   }

   /**
    * Stops this IL, and unbinds it from JNDI; also closes the server socket
    * if it is still bound.
    */
   public void stopService()
   {
      try
      {
         running = false;
         unbindJNDIReferences();

         // unbind the serverSocket if needed
         if( serverSocket != null )
         {
            serverSocket.close();
         }
      }
      catch (Exception e)
      {
         log.error( "Exception occured when trying to stop UIL Service: ", e );
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
      if( bindAddress != null )
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
      if( host == null || host.length() == 0 )
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
       if( serverSocketFactory != null )
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
