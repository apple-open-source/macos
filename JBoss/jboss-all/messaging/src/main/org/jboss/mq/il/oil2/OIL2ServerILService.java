/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.oil2;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
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
import javax.naming.InitialContext;
import javax.net.ServerSocketFactory;

import org.jboss.logging.Logger;
import org.jboss.mq.AcknowledgementRequest;
import org.jboss.mq.ConnectionToken;
import org.jboss.mq.DurableSubscriptionID;
import org.jboss.mq.SpyDestination;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.Subscription;
import org.jboss.mq.TransactionRequest;
import org.jboss.mq.il.Invoker;
import org.jboss.mq.il.ServerIL;
import org.jboss.security.SecurityDomain;
import org.jboss.system.server.ServerConfigUtil;

/**
 * Implements the ServerILJMXService which is used to manage the OIL2 IL.
 *
 * @author    <a href="mailto:hiram.chirino@jboss.org">Hiram Chirino</a>
 * @version   $Revision: 1.22 $
 * @jmx:mbean extends="org.jboss.mq.il.ServerILJMXServiceMBean"
 */
public final class OIL2ServerILService
   extends org.jboss.mq.il.ServerILJMXService
   implements java.lang.Runnable, OIL2ServerILServiceMBean
{
   /**
    * logger instance.
    */
   final static private Logger log = Logger.getLogger(OIL2ServerILService.class);
   

   /**
    * The default timeout for the server socket. This is
    * set so the socket will periodically return to check
    * the running flag.
    */
   private final static int SO_TIMEOUT = 5000;

   /**
    * The JMS server where requests are forwarded to.
    */
   //private static JMSServerInvoker server;
   private Invoker server;

   /**
    * If the TcpNoDelay option should be used on the socket.
    */
   private boolean enableTcpNoDelay = false;

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
    * The listening socket that receives incomming connections
    * for servicing.
    */
   private ServerSocket serverSocket;

   /**
    * The managed serverIL.
    */
   private OIL2ServerIL serverIL;

   /**
    * The running flag that all worker and server
    * threads check to determine if the service should
    * be stopped.
    */
   private volatile boolean running;

   /**
    * The server port to bind to.
    */
   private int serverBindPort = 0;

   /**
    * The internet address to bind to by
    * default.
    */
   private InetAddress bindAddress = null;

   /**
    * The connection properties passed to the client to connect to this IL
    */
   private Properties connectionProperties;
   

   public class RequestListner implements OIL2RequestListner
   {

      Socket socket;
      ObjectInputStream in;
      ObjectOutputStream out;
      OIL2SocketHandler socketHandler;
      ConnectionToken connectionToken;
      boolean closing = false;

      RequestListner(Socket socket) throws IOException
      {
         socket.setSoTimeout(0);
         socket.setTcpNoDelay(enableTcpNoDelay);
         out = new ObjectOutputStream(new BufferedOutputStream(socket.getOutputStream()));
         out.flush();
         in = new ObjectInputStream(new BufferedInputStream(socket.getInputStream()));
      }

      public void handleRequest(OIL2Request request)
      {
//         if( log.isTraceEnabled() )
//             log.trace("RequestListner handing request: "+request);

         if( closing ) {
            log.trace("A connection that is closing received another request.  Droping request.");
            return;
         }
         
         Object result = null;
         Exception resultException = null;

         // now based upon the input directive, preform the 
         // requested action. Any exceptions are processed
         // and potentially returned to the client.
         //
         try
         {
            switch (request.operation)
            {
               case OIL2Constants.SERVER_SET_SPY_DISTRIBUTED_CONNECTION :
                  connectionToken = (ConnectionToken) request.arguments[0];
                  // Make the client IL aware of us since he will be using our requestHander
                  // To make requests.
                  ((OIL2ClientIL)connectionToken.clientIL).setRequestListner(this);
                  break;

               case OIL2Constants.SERVER_ACKNOWLEDGE :
                  server.acknowledge(connectionToken, (AcknowledgementRequest) request.arguments[0]);
                  break;

               case OIL2Constants.SERVER_ADD_MESSAGE :
                  server.addMessage(connectionToken, (SpyMessage) request.arguments[0]);
                  break;

               case OIL2Constants.SERVER_BROWSE :
                  result =
                     server.browse(connectionToken, (Destination) request.arguments[0], (String) request.arguments[1]);
                  break;

               case OIL2Constants.SERVER_CHECK_ID :
                  server.checkID((String) request.arguments[0]);
                  if (connectionToken != null)
                     connectionToken.setClientID((String) request.arguments[0]);
                  break;

               case OIL2Constants.SERVER_CONNECTION_CLOSING :
                  beginClose();
                  break;

               case OIL2Constants.SERVER_CREATE_QUEUE :
                  result = server.createQueue(connectionToken, (String) request.arguments[0]);
                  break;

               case OIL2Constants.SERVER_CREATE_TOPIC :
                  result = server.createTopic(connectionToken, (String) request.arguments[0]);
                  break;

               case OIL2Constants.SERVER_DELETE_TEMPORARY_DESTINATION :
                  server.deleteTemporaryDestination(connectionToken, (SpyDestination) request.arguments[0]);
                  break;

               case OIL2Constants.SERVER_GET_ID :
                  result = server.getID();
                  if (connectionToken != null)
                     connectionToken.setClientID((String) result);
                  break;

               case OIL2Constants.SERVER_GET_TEMPORARY_QUEUE :
                  result = server.getTemporaryQueue(connectionToken);
                  break;

               case OIL2Constants.SERVER_GET_TEMPORARY_TOPIC :
                  result = server.getTemporaryTopic(connectionToken);
                  break;

               case OIL2Constants.SERVER_RECEIVE :
                  result =
                     server.receive(
                        connectionToken,
                        ((Integer) request.arguments[0]).intValue(),
                        ((Long) request.arguments[1]).longValue());
                  break;

               case OIL2Constants.SERVER_SET_ENABLED :
                  server.setEnabled(
                     connectionToken,
                     ((Boolean) request.arguments[0]).booleanValue());
                  break;

               case OIL2Constants.SERVER_SUBSCRIBE :
                  server.subscribe(connectionToken, (Subscription) request.arguments[0]);
                  break;

               case OIL2Constants.SERVER_TRANSACT :
                  server.transact(connectionToken, (TransactionRequest) request.arguments[0]);
                  break;

               case OIL2Constants.SERVER_UNSUBSCRIBE :
                  server.unsubscribe(connectionToken, ((Integer) request.arguments[0]).intValue());
                  break;

               case OIL2Constants.SERVER_DESTROY_SUBSCRIPTION :
                  server.destroySubscription(connectionToken, (DurableSubscriptionID) request.arguments[0]);
                  break;

               case OIL2Constants.SERVER_CHECK_USER :
                  result =
                     server.checkUser(
                        (String) request.arguments[0],
                        (String) request.arguments[1]);
                  break;

               case OIL2Constants.SERVER_PING :
                  server.ping(connectionToken, ((Long) request.arguments[0]).longValue());
                  break;

               case OIL2Constants.SERVER_AUTHENTICATE :
                  result = server.authenticate((String) request.arguments[0], (String) request.arguments[1]);
                  break;

               default :
                  throw new RemoteException("Bad method code !");
            } // switch
         } catch (Exception e ) {
           resultException = e; 
         } // try
         
         try {
               OIL2Response response = new OIL2Response(request);
               response.result = result;
               response.exception = resultException;
               socketHandler.sendResponse(response);
         } catch ( IOException e ) {
            handleConnectionException(e);
         }
      }
      
      public void handleConnectionException(Exception e)
      {
         if( !closing )
            log.info("Client Disconnected: "+e);
         beginClose();
      }
      
      void beginClose() {
         closing = true;
         try { 
            if( connectionToken != null ) 
               server.connectionClosing(connectionToken);
         } catch ( JMSException ignore ) {
         } finally {
            close();
         }
      }   
         
      void close() {
         try {
            if( socket != null ) {
               socketHandler.stop();
               in.close();
               out.close();
               socket.close();
               socket=null;
            }
         } catch (IOException e ) {
            log.debug("Exception occured while closing opened resources: ", e);
         }
      }
      
      public OIL2SocketHandler getSocketHandler()
      {
         return socketHandler;
      }

   }

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
      return "JBossMQ-OILServerIL";
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
    * Main processing method for the OILServerILService object
    */
   public void run()
   {
      try
      {
         while (running)
         {
            Socket socket = null;
            try
            {
               socket = serverSocket.accept();
               if( log.isTraceEnabled() )
                  log.trace("Accepted connection: "+socket);
            }
            catch (java.io.InterruptedIOException e)
            {
               // It's ok, this is due to the SO_TIME_OUT
               continue;
            }

            // it's possible that the service is no longer
            // running but it got a connection, no point in
            // starting up a thread!
            //
            if (!running)
            {
               if (socket != null)
               {
                  try
                  {
                     socket.close();
                  }
                  catch (Exception ignore)
                  {
                  }
               }
               return;
            }

            try
            {

               if( log.isTraceEnabled() )
                  log.trace("Initializing RequestListner for socket: "+socket);
               RequestListner requestListner = new RequestListner(socket);
               OIL2SocketHandler socketHandler =
                  new OIL2SocketHandler(
                     requestListner.in, 
                     requestListner.out, 
                     Thread.currentThread().getThreadGroup());
               requestListner.socketHandler = socketHandler;
               socketHandler.setRequestListner(requestListner);
               socketHandler.start();

            }
            catch (IOException ie)
            {
               log.debug("Client connection could not be accepted: ", ie);
            }
         }
      }
      catch (SocketException e)
      {
         // There is no easy way (other than string comparison) to
         // determine if the socket exception is caused by connection
         // reset by peer. In this case, it's okay to ignore both
         // SocketException and IOException.
         if (running)
            log.warn("SocketException occured (Connection reset by peer?). Cannot initialize the OIL2ServerILService.");
      }
      catch (IOException e)
      {
         if (running)
            log.warn("IOException occured. Cannot initialize the OIL2ServerILService.");
      }
      finally
      {
         try
         {
            serverSocket.close();
         }
         catch (Exception e)
         {
            log.debug("error closing server socket", e);
         }
         return;
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
      log.info("JBossMQ OIL2 service available at : " + socketAddress + ":" + serverSocket.getLocalPort());

      new Thread(server.getThreadGroup(), this, "OIL2 Worker Server").start();
      /* We need to check the socketAddress against "0.0.0.0/0.0.0.0"
         because this is not a valid address on Win32 while it is for
         *NIX. See BugParade bug #4343286.
         */
      socketAddress = ServerConfigUtil.fixRemoteAddress(socketAddress);

      serverIL = new OIL2ServerIL(socketAddress.getHostAddress(), serverSocket.getLocalPort(),
         clientSocketFactoryName, enableTcpNoDelay);

      // Initialize the connection poperties using the base class.
      connectionProperties = super.getClientConnectionProperties();
      connectionProperties.setProperty(OIL2ServerILFactory.CLIENT_IL_SERVICE_KEY, "org.jboss.mq.il.oil2.OIL2ClientILService");
      connectionProperties.setProperty(OIL2ServerILFactory.OIL2_PORT_KEY, "" + serverSocket.getLocalPort());
      connectionProperties.setProperty(OIL2ServerILFactory.OIL2_ADDRESS_KEY, "" + socketAddress.getHostAddress());
      connectionProperties.setProperty(OIL2ServerILFactory.OIL2_TCPNODELAY_KEY, enableTcpNoDelay ? "yes" : "no");

      bindJNDIReferences();

   }

   /**
    * Stops this IL, and unbinds it from JNDI.
    */
   public void stopService()
   {
      try
      {
         unbindJNDIReferences();
      }
      catch (Exception e)
      {
         log.error("Exception unbinding from JNDI", e);
      }
      try
      {
         running = false;
         if (serverSocket != null)
            serverSocket.close();
      }
      catch (Exception e)
      {
         log.debug("Exception stopping server thread", e);
      }
   }

   /**
    * Getter for property serverBindPort.
    *
    * @return Value of property serverBindPort.
    * @jmx:managed-attribute
    */
   public int getServerBindPort()
   {
      return serverBindPort;
   }

   /**
    * Setter for property serverBindPort.
    *
    * @param serverBindPort New value of property serverBindPort.
    * @jmx:managed-attribute
    */
   public void setServerBindPort(int serverBindPort)
   {
      this.serverBindPort = serverBindPort;
   }

   /**
    * Get the interface address the OIL server bind its listening port on.
    *
    * @return The hostname or dotted decimal address that the service is
    *    bound to.
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
    * Set the interface address the OIL server bind its listening port on.
    *
    * @param host The host address to bind to, if any.
    *
    * @throws java.net.UnknownHostException Thrown if the hostname cannot
    *    be resolved to an InetAddress object.
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
    * @jmx:managed-attribute
    */
   public boolean getEnableTcpNoDelay()
   {
      return enableTcpNoDelay;
   }

   /**
    * Sets the enableTcpNoDelay.
    * @param enableTcpNoDelay The enableTcpNoDelay to set
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
// vim:expandtab:tabstop=3:shiftwidth=3
