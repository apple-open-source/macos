/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.oil2;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.EOFException;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.net.InetAddress;
import java.net.Socket;

import javax.jms.Destination;
import javax.jms.JMSException;
import javax.jms.Queue;
import javax.jms.TemporaryQueue;
import javax.jms.TemporaryTopic;
import javax.jms.Topic;
import javax.net.SocketFactory;

import org.jboss.logging.Logger;
import org.jboss.mq.AcknowledgementRequest;
import org.jboss.mq.Connection;
import org.jboss.mq.ConnectionToken;
import org.jboss.mq.DurableSubscriptionID;
import org.jboss.mq.SpyDestination;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.TransactionRequest;
import org.jboss.mq.il.ServerIL;

import EDU.oswego.cs.dl.util.concurrent.Slot;

/**
 * The JVM implementation of the ServerIL object
 *
 * @author    <a href="mailto:hiram.chirino@jboss.org">Hiram Chirino</a>
 * @version   $Revision: $
 * @created   August 16, 2001
 */
public final class OIL2ServerIL
   implements java.io.Serializable, java.lang.Cloneable, org.jboss.mq.il.ServerIL, OIL2Constants
{
   private final static Logger log = Logger.getLogger(OIL2ServerIL.class);
   /** The org.jboss.mq.il.oil2.localAddr system property allows a client to
    *define the local interface to which its sockets should be bound
    */
   private final static String LOCAL_ADDR = "org.jboss.mq.il.oil2.localAddr";
   /** The org.jboss.mq.il.oil2.localPort system property allows a client to
    *define the local port to which its sockets should be bound
    */
   private final static String LOCAL_PORT = "org.jboss.mq.il.oil2.localPort";

   /** The server host name/IP to connect to
    */
   private String addr;
   /** The server port to connect to.
    */
   private int port;
   /** The name of the class implementing the javax.net.SocketFactory to
    * use for creating the client socket.
    */
   private String socketFactoryName;

   /**
    * If the TcpNoDelay option should be used on the socket.
    */
   private boolean enableTcpNoDelay = false;
   /** The local interface name/IP to use for the client
    */
   private transient InetAddress localAddr;
   /** The local port to use for the client
    */
   private transient int localPort;
   /**
    * Description of the Field
    */
   private transient ObjectInputStream in;

   /**
    * Description of the Field
    */
   private transient ObjectOutputStream out;

   /**
    * Description of the Field
    */
   private transient Socket socket;

   OIL2SocketHandler socketHandler;

   class RequestListner implements OIL2RequestListner
   {
      public void handleConnectionException(Exception e)
      {
      }

      public void handleRequest(OIL2Request request)
      {
      }

   }

   /**
    * Constructor for the OILServerIL object
    *
    * @param addr, the server host or ip
    * @param port, the server port
    * @param socketFactoryName, the name of the javax.net.SocketFactory to use
    * @param enableTcpNoDelay, 
    */
   public OIL2ServerIL(String addr, int port,
      String socketFactoryName, boolean enableTcpNoDelay)
   {
      this.addr = addr;
      this.port = port;
      this.socketFactoryName = socketFactoryName;
      this.enableTcpNoDelay = enableTcpNoDelay;
   }

   synchronized public void connect() throws IOException
   {
      if (socket == null)
      {
         boolean tracing = log.isTraceEnabled();
         if( tracing )
            log.trace("Connecting to : "+addr+":"+port);

         /** Attempt to load the socket factory and if this fails, use the
          * default socket factory impl.
          */
         SocketFactory socketFactory = null;
         if( socketFactoryName != null )
         {
            try
            {
               ClassLoader loader = Thread.currentThread().getContextClassLoader();
               Class factoryClass = loader.loadClass(socketFactoryName);
               socketFactory = (SocketFactory) factoryClass.newInstance();
            }
            catch(Exception e)
            {
               log.debug("Failed to load socket factory: "+socketFactoryName, e);
            }
         }
         // Use the default socket factory
         if( socketFactory == null )
         {
            socketFactory = SocketFactory.getDefault();
         }

         // Look for a local address and port as properties
         String tmp = System.getProperty(LOCAL_ADDR);
         if( tmp != null )
            this.localAddr = InetAddress.getByName(tmp);
         tmp = System.getProperty(LOCAL_PORT);
         if( tmp != null )
            this.localPort = Integer.parseInt(tmp);
         if( tracing )
         {
            log.trace("Connecting with addr="+addr+", port="+port
               + ", localAddr="+localAddr+", localPort="+localPort
               + ", socketFactory="+socketFactory);
         }

         if( localAddr != null )
            socket = socketFactory.createSocket(addr, port, localAddr, localPort);
         else
            socket = socketFactory.createSocket(addr, port);

         if( tracing )
            log.trace("Connection established.");
         
         socket.setTcpNoDelay(enableTcpNoDelay);
         out = new ObjectOutputStream(new BufferedOutputStream(socket.getOutputStream()));
         out.flush();
         in = new ObjectInputStream(new BufferedInputStream(socket.getInputStream()));

         if( tracing )
            log.trace("Streams initialized.");

         socketHandler = new OIL2SocketHandler(in, out, Connection.threadGroup);
         socketHandler.setRequestListner(new RequestListner());
         socketHandler.start();
      }
   }

   /**
    * Sets the ConnectionToken attribute of the OILServerIL object
    *
    * @param dest           The new ConnectionToken value
    * @exception Exception  Description of Exception
    */
   public void setConnectionToken(ConnectionToken dest) throws Exception
   {
      connect();
      OIL2Request request = new OIL2Request(OIL2Constants.SERVER_SET_SPY_DISTRIBUTED_CONNECTION, new Object[] { dest });
      OIL2Response response = socketHandler.synchRequest(request);
      response.evalThrowsJMSException();
   }

   /**
    * Sets the Enabled attribute of the OILServerIL object
    *
    * @param dc                The new Enabled value
    * @param enabled           The new Enabled value
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public void setEnabled(ConnectionToken dc, boolean enabled) throws JMSException, Exception
   {
      connect();
      OIL2Request request = new OIL2Request(OIL2Constants.SERVER_SET_ENABLED, new Object[] { new Boolean(enabled)});
      OIL2Response response = socketHandler.synchRequest(request);
      response.evalThrowsJMSException();
   }

   /**
    * Gets the ID attribute of the OILServerIL object
    *
    * @return               The ID value
    * @exception Exception  Description of Exception
    */
   public String getID() throws Exception
   {
      connect();
      OIL2Request request = new OIL2Request(OIL2Constants.SERVER_GET_ID, null);
      OIL2Response response = socketHandler.synchRequest(request);
      return (String) response.evalThrowsJMSException();
   }

   /**
    * Gets the TemporaryQueue attribute of the OILServerIL object
    *
    * @param dc                Description of Parameter
    * @return                  The TemporaryQueue value
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public TemporaryQueue getTemporaryQueue(ConnectionToken dc) throws JMSException, Exception
   {
      connect();
      OIL2Request request = new OIL2Request(OIL2Constants.SERVER_GET_TEMPORARY_QUEUE, null);
      OIL2Response response = socketHandler.synchRequest(request);
      return (TemporaryQueue) response.evalThrowsJMSException();
   }

   /**
    * Gets the TemporaryTopic attribute of the OILServerIL object
    *
    * @param dc                Description of Parameter
    * @return                  The TemporaryTopic value
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public TemporaryTopic getTemporaryTopic(ConnectionToken dc) throws JMSException, Exception
   {
      connect();
      OIL2Request request = new OIL2Request(OIL2Constants.SERVER_GET_TEMPORARY_TOPIC, null);
      OIL2Response response = socketHandler.synchRequest(request);
      return (TemporaryTopic) response.evalThrowsJMSException();
   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param item              Description of Parameter
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public void acknowledge(ConnectionToken dc, AcknowledgementRequest item) throws JMSException, Exception
   {
      connect();
      OIL2Request request = new OIL2Request(OIL2Constants.SERVER_ACKNOWLEDGE, new Object[] { item });
      OIL2Response response = socketHandler.synchRequest(request);
      response.evalThrowsJMSException();

   }

   /**
    * Adds a feature to the Message attribute of the OILServerIL object
    *
    * @param dc             The feature to be added to the Message attribute
    * @param val            The feature to be added to the Message attribute
    * @exception Exception  Description of Exception
    */
   public void addMessage(ConnectionToken dc, SpyMessage val) throws Exception
   {

      connect();
      OIL2Request request = new OIL2Request(OIL2Constants.SERVER_ADD_MESSAGE, new Object[] { val });
      OIL2Response response = socketHandler.synchRequest(request);
      response.evalThrowsJMSException();
   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param dest              Description of Parameter
    * @param selector          Description of Parameter
    * @return                  Description of the Returned Value
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public SpyMessage[] browse(ConnectionToken dc, Destination dest, String selector)
      throws JMSException, Exception
   {
      connect();
      OIL2Request request = new OIL2Request(OIL2Constants.SERVER_BROWSE, new Object[] { dest, selector });      
      OIL2Response response = socketHandler.synchRequest(request);
      return (SpyMessage[]) response.evalThrowsJMSException();
   }

   /**
    * #Description of the Method
    *
    * @param ID                Description of Parameter
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public void checkID(String ID) throws JMSException, Exception
   {
      connect();
      OIL2Request request = new OIL2Request(OIL2Constants.SERVER_CHECK_ID, new Object[] { ID });
      OIL2Response response = socketHandler.synchRequest(request);
      response.evalThrowsJMSException();
   }

   /**
    * #Description of the Method
    *
    * @param userName          Description of Parameter
    * @param password          Description of Parameter
    * @return                  Description of the Returned Value
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public String checkUser(String userName, String password) throws JMSException, Exception
   {
      connect();
      OIL2Request request = new OIL2Request(OIL2Constants.SERVER_CHECK_USER, new Object[] { userName, password });
      OIL2Response response = socketHandler.synchRequest(request);
      return (String) response.evalThrowsJMSException();
   }

   /**
    * #Description of the Method
    *
    * @param userName          Description of Parameter
    * @param password          Description of Parameter
    * @return                  Description of the Returned Value
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public String authenticate(String userName, String password) throws JMSException, Exception
   {
      connect();
      OIL2Request request = new OIL2Request(OIL2Constants.SERVER_AUTHENTICATE, new Object[] { userName, password });
      OIL2Response response = socketHandler.synchRequest(request);
      return (String) response.evalThrowsJMSException();

   }
   /**
    * #Description of the Method
    *
    * @return                                Description of the Returned Value
    * @exception CloneNotSupportedException  Description of Exception
    */
   public Object clone() throws CloneNotSupportedException
   {
      return super.clone();
   }

   /**
    * Need to clone because there are instance variables tha can get clobbered.
    * All Multiple connections can NOT share the same JVMServerIL object
    *
    * @return               Description of the Returned Value
    * @exception Exception  Description of Exception
    */
   public ServerIL cloneServerIL() throws Exception
   {
      return (ServerIL) clone();
   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public void connectionClosing(ConnectionToken dc) throws JMSException, Exception
   {
      try
      {
         connect();
         OIL2Request request = new OIL2Request(OIL2Constants.SERVER_CONNECTION_CLOSING, null);
         OIL2Response response = socketHandler.synchRequest(request);
         response.evalThrowsJMSException();
      }
      finally
      {
         close();
      }
   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param dest              Description of Parameter
    * @return                  Description of the Returned Value
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public Queue createQueue(ConnectionToken dc, String dest) throws JMSException, Exception
   {
      connect();
      OIL2Request request = new OIL2Request(OIL2Constants.SERVER_CREATE_QUEUE, new Object[] { dest });
      OIL2Response response = socketHandler.synchRequest(request);
      return (Queue) response.evalThrowsJMSException();

   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param dest              Description of Parameter
    * @return                  Description of the Returned Value
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public Topic createTopic(ConnectionToken dc, String dest) throws JMSException, Exception
   {
      connect();
      OIL2Request request = new OIL2Request(OIL2Constants.SERVER_CREATE_TOPIC, new Object[] { dest });
      OIL2Response response = socketHandler.synchRequest(request);
      return (Topic) response.evalThrowsJMSException();

   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param dest              Description of Parameter
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public void deleteTemporaryDestination(ConnectionToken dc, SpyDestination dest)
      throws JMSException, Exception
   {
      connect();
      OIL2Request request = new OIL2Request(OIL2Constants.SERVER_DELETE_TEMPORARY_DESTINATION, new Object[] { dest });
      OIL2Response response = socketHandler.synchRequest(request);
      response.evalThrowsJMSException();

   }

   /**
    * #Description of the Method
    *
    * @param id                Description of Parameter
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public void destroySubscription(ConnectionToken dc, DurableSubscriptionID id)
      throws JMSException, Exception
   {
      connect();
      OIL2Request request = new OIL2Request(OIL2Constants.SERVER_DESTROY_SUBSCRIPTION, new Object[] { id });
      OIL2Response response = socketHandler.synchRequest(request);
      response.evalThrowsJMSException();

   }

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @param clientTime     Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void ping(ConnectionToken dc, long clientTime) throws Exception
   {
      connect();
      OIL2Request request = new OIL2Request(OIL2Constants.SERVER_PING, new Object[] { new Long(clientTime)});
      OIL2Response response = socketHandler.synchRequest(request);
      response.evalThrowsJMSException();
   }

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @param subscriberId   Description of Parameter
    * @param wait           Description of Parameter
    * @return               Description of the Returned Value
    * @exception Exception  Description of Exception
    */
   public SpyMessage receive(ConnectionToken dc, int subscriberId, long wait) throws Exception, Exception
   {
      connect();
      OIL2Request request =
         new OIL2Request(OIL2Constants.SERVER_RECEIVE, new Object[] { new Integer(subscriberId), new Long(wait)});
      OIL2Response response = socketHandler.synchRequest(request);
      return (SpyMessage) response.evalThrowsJMSException();

   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param s                 Description of Parameter
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public void subscribe(ConnectionToken dc, org.jboss.mq.Subscription s) throws JMSException, Exception
   {
      connect();
      OIL2Request request = new OIL2Request(OIL2Constants.SERVER_SUBSCRIBE, new Object[] { s });
      OIL2Response response = socketHandler.synchRequest(request);
      response.evalThrowsJMSException();

   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param t                 Description of Parameter
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public void transact(org.jboss.mq.ConnectionToken dc, TransactionRequest t)
      throws JMSException, Exception
   {
      connect();
      OIL2Request request = new OIL2Request(OIL2Constants.SERVER_TRANSACT, new Object[] { t });
      OIL2Response response = socketHandler.synchRequest(request);
      response.evalThrowsJMSException();
   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param subscriptionId    Description of Parameter
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public void unsubscribe(ConnectionToken dc, int subscriptionId) throws JMSException, Exception
   {
      connect();
      OIL2Request request =
         new OIL2Request(OIL2Constants.SERVER_UNSUBSCRIBE, new Object[] { new Integer(subscriptionId)});
      OIL2Response response = socketHandler.synchRequest(request);
      response.evalThrowsJMSException();

   }

   /**
    * Used to close the current connection with the server
    *
    * @exception Exception  Description of Exception
    */
   synchronized public void close()
   {
      try
      {
         if (socket != null)
         {
            socketHandler.stop();
            in.close();
            out.close();
            socket.close();
            socket = null;
         }
      }
      catch (IOException e)
      {
         log.debug("Exception occured while closing opened resources: ", e);
      }
   }
}
