/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.oil;

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
 * Implements the ServerILJMXService which is used to manage the JVM IL.
 *
 * @author    Hiram Chirino (Cojonudo14@hotmail.com)
 * @version   $Revision: 1.23.2.4 $
 *
 * @jmx:mbean extends="org.jboss.mq.il.ServerILJMXServiceMBean"
 */
public final class OILServerILService
   extends org.jboss.mq.il.ServerILJMXService
   implements java.lang.Runnable,
      OILServerILServiceMBean
{
   /**
    * logger instance.
    */
   final static private Logger log = Logger.getLogger(OILServerILService.class);
      
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
   private boolean enableTcpNoDelay=false;   

   /**
    * The read timeout
    */
   private int readTimeout = 0;
   
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
   private OILServerIL serverIL;

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
    * Number of OIL Worker threads started.
    */
   private int threadNumber = 0;

   /**
    * The connection properties passed to the client to connect to this IL
    */
   private Properties connectionProperties;

   /**
    * This class is used to encapsulate the basic connection and
    * work for a connected client thread. The run() method of this
    * class processes requests and sends responses to and from a 
    * single client. All requests are forwarded to the outer class'
    * JMS server instance.
    *
    * @author Brian Weaver (weave@opennms.org)
    */
   private final class Client
      implements Runnable
   {
      /**
       * The TCP/IP socket for communications.
       */
      private Socket sock;

      /**
       * The object output stream running on top of the
       * socket's output stream.
       */
      private ObjectOutputStream out;

      /**
       * The objec5t input stream running on top of the
       * socket's input stream
       */
      private ObjectInputStream in;

      /**
       * Allocates a new runnable instance to process requests from
       * the server's client and pass them to the JMS server. The
       * passed socket is used for all communications between the 
       * service and the client.
       *
       * @param s The socket used for communications.
       *
       * @throws java.io.IOException Thrown if an I/O error occurs
       *    constructing the object streams.
       */
      Client(Socket s)
         throws IOException
      {
         this.sock = s;
         this.out= new ObjectOutputStream(new BufferedOutputStream(this.sock.getOutputStream()));
         this.out.flush();
         this.in = new ObjectInputStream(new BufferedInputStream(this.sock.getInputStream()));
         sock.setTcpNoDelay(enableTcpNoDelay);
         if( log.isTraceEnabled() )
            log.trace("Setting TcpNoDelay Option to:"+enableTcpNoDelay);
      }

      /**
       * The main threads processing routine. This loop processes
       * requests from the server and sends the appropriate responses
       * based upon the results.
       */
      public void run()
      {
         int code = 0;
         boolean closed = false;
         ConnectionToken connectionToken = null;

         while (!closed && running)
         {
            try
            {
               // read in the next request/directive
               // from the client
               //
               code = in.readByte();
            }
            catch (EOFException e)
            {
               // end of file, exit processing loop
               //
               break;
            }
            catch (IOException e)
            {
               if(closed || !running)
               {
                  // exit out of the loop if the connection
                  // is closed or the service is shutdown
                  //
                  break;
               }
               log.warn("Connection failure (1).", e);
               break;
            }

            // now based upon the input directive, preform the 
            // requested action. Any exceptions are processed
            // and potentially returned to the client.
            //
            try
            {
               Object result = null;

               switch (code)
               {
               case OILConstants.SET_SPY_DISTRIBUTED_CONNECTION:
                  // assert connectionToken == null
                  connectionToken = (ConnectionToken)in.readObject();
                  break;

               case OILConstants.ACKNOWLEDGE:
                  AcknowledgementRequest ack = new AcknowledgementRequest();
                  ack.readExternal(in);
                  server.acknowledge(connectionToken, ack);
                  break;

               case OILConstants.ADD_MESSAGE:
                  server.addMessage(connectionToken, SpyMessage.readMessage(in));
                  break;

               case OILConstants.BROWSE:
                  result = server.browse(connectionToken, (Destination)in.readObject(), (String)in.readObject());
                  break;

               case OILConstants.CHECK_ID:
                  String ID = (String)in.readObject();
                  server.checkID(ID);
                  if (connectionToken != null)
                     connectionToken.setClientID(ID);
                  break;

               case OILConstants.CONNECTION_CLOSING:
                  server.connectionClosing(connectionToken);
                  closed = true;
                  break;

               case OILConstants.CREATE_QUEUE:
                  result = (Queue)server.createQueue(connectionToken, (String)in.readObject());
                  break;

               case OILConstants.CREATE_TOPIC:
                  result = (Topic)server.createTopic(connectionToken, (String)in.readObject());
                  break;

               case OILConstants.DELETE_TEMPORARY_DESTINATION:
                  server.deleteTemporaryDestination(connectionToken, (SpyDestination)in.readObject());
                  break;

               case OILConstants.GET_ID:
                  result = server.getID();
                  if (connectionToken != null)
                     connectionToken.setClientID((String)result);
                  break;

               case OILConstants.GET_TEMPORARY_QUEUE:
                  result = (TemporaryQueue)server.getTemporaryQueue(connectionToken);
                  break;

               case OILConstants.GET_TEMPORARY_TOPIC:
                  result = (TemporaryTopic)server.getTemporaryTopic(connectionToken);
                  break;

               case OILConstants.RECEIVE:
                  result = server.receive(connectionToken, in.readInt(), in.readLong());
                  break;

               case OILConstants.SET_ENABLED:
                  server.setEnabled(connectionToken, in.readBoolean());
                  break;

               case OILConstants.SUBSCRIBE:
                  server.subscribe(connectionToken, (Subscription)in.readObject());
                  break;

               case OILConstants.TRANSACT:
                  TransactionRequest trans = new TransactionRequest();
                  trans.readExternal(in);
                  server.transact(connectionToken, trans);
                  break;

               case OILConstants.UNSUBSCRIBE:
                  server.unsubscribe(connectionToken, in.readInt());
                  break;

               case OILConstants.DESTROY_SUBSCRIPTION:
                  server.destroySubscription(connectionToken,(DurableSubscriptionID)in.readObject());
                  break;

               case OILConstants.CHECK_USER:
                  result = server.checkUser((String)in.readObject(), (String)in.readObject());
                  break;

               case OILConstants.PING:
                  server.ping(connectionToken, in.readLong());
                  break;

               case OILConstants.AUTHENTICATE:
                  result = server.authenticate((String)in.readObject(), (String)in.readObject());
                  break;

               default:
                  throw new RemoteException("Bad method code !");
               }

               //Everthing was OK, otherwise
               //an exception would have prevented us from getting
               //to this point in the code ;-)
               //
               try
               {
                  if (result == null)
                  {
                     out.writeByte(OILConstants.SUCCESS);
                  }
                  else
                  {
                     out.writeByte(OILConstants.SUCCESS_OBJECT);
                     out.writeObject(result);
                     out.reset();
                  }
                  out.flush();
               }
               catch (IOException e)
               {
                  if(closed)
                     break;

                  log.warn("Connection failure (2).", e);
                  break;
               }

            }
            catch (Exception e)
            {
               // this processes any exceptions from the actually switch
               // statement processing
               //
               if (closed)
                  break;

               log.warn("Client request resulted in a server exception: ", e);

               try
               {
                  out.writeByte(OILConstants.EXCEPTION);
                  out.writeObject(e);
                  out.reset();
                  out.flush();
               }
               catch (IOException e2)
               {
                  if (closed)
                     break;

                  log.warn("Connection failure (3).", e);
                  break;
               }
            } // end catch of try { switch(code) ... }
         } // end whlie loop

         try
         {
            if(!closed)
            {
               try
               {
                  server.connectionClosing(connectionToken);
               }
               catch (JMSException e)
               {
                  // do nothing
               }
            }
            in.close();
            out.close();
         }
         catch (IOException e)
         {
            log.warn("Connection failure during connection close.", e);
         }
         finally
         {
            try
            {
               sock.close();
            }
            catch(IOException e)
            {
               log.warn("Connection failure during connection close.", e);
            }
         }
      } // end run method
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
            }
            catch (java.io.InterruptedIOException e)
            {
               continue;
            }

            // it's possible that the service is no longer
            // running but it got a connection, no point in
            // starting up a thread!
            //
            if (!running)
            {
               if(socket != null)
               {
                  try
                  {
                     socket.close();
                  }
                  catch(Exception e)
                  {
                     // do nothing
                  }
               }
               return;
            }

            try
            {
               socket.setSoTimeout(readTimeout);
               new Thread(new Client(socket), "OIL Worker-" + threadNumber++).start();
            }
            catch(IOException ie)
            {
               if(log.isDebugEnabled())
               {
                  log.debug("IOException processing client connection", ie);
                  log.debug("Dropping client connection, server will not terminate");
               }
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
            log.warn("SocketException occured (Connection reset by peer?). Cannot initialize the OILServerILService.");
      }
      catch (IOException e)
      {
         if (running)
            log.warn("IOException occured. Cannot initialize the OILServerILService.");
      }
      finally
      {
         try
         {
            serverSocket.close();
         }
         catch(Exception e)
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
      if(log.isInfoEnabled())
         log.info("JBossMQ OIL service available at : " + socketAddress + ":" + serverSocket.getLocalPort());

      new Thread(server.getThreadGroup(), this, "OIL Worker Server").start();

      /* We need to check the socketAddress against "0.0.0.0/0.0.0.0"
         because this is not a valid address on Win32 while it is for
         *NIX. See BugParade bug #4343286.
         */
      socketAddress = ServerConfigUtil.fixRemoteAddress(socketAddress);

      serverIL = new OILServerIL(socketAddress, serverSocket.getLocalPort(),
         clientSocketFactoryName, enableTcpNoDelay);

      // Initialize the connection poperties using the base class.
      connectionProperties = super.getClientConnectionProperties();
      connectionProperties.setProperty(OILServerILFactory.CLIENT_IL_SERVICE_KEY, "org.jboss.mq.il.oil.OILClientILService");
      connectionProperties.setProperty(OILServerILFactory.OIL_PORT_KEY, ""+serverSocket.getLocalPort());
      connectionProperties.setProperty(OILServerILFactory.OIL_ADDRESS_KEY, ""+socketAddress.getHostAddress());
      connectionProperties.setProperty(OILServerILFactory.OIL_TCPNODELAY_KEY, enableTcpNoDelay?"yes":"no");

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
    * Get the OIL server listening port
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
    * Set the OIL server listening port
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
    * Get the interface address the OIL server bind its listening port on.
    *
    * @return The hostname or dotted decimal address that the service is
    *    bound to.
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
    * Set the interface address the OIL server bind its listening port on.
    *
    * @param host The host address to bind to, if any.
    *
    * @throws java.net.UnknownHostException Thrown if the hostname cannot
    *    be resolved to an InetAddress object.
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
