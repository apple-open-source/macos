/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.uil2;

import java.io.Serializable;
import java.io.IOException;
import java.net.InetAddress;
import java.net.ConnectException;
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
import org.jboss.mq.il.uil2.msgs.MsgTypes;
import org.jboss.mq.il.uil2.msgs.ConnectionTokenMsg;
import org.jboss.mq.il.uil2.msgs.EnableConnectionMsg;
import org.jboss.mq.il.uil2.msgs.GetIDMsg;
import org.jboss.mq.il.uil2.msgs.TemporaryDestMsg;
import org.jboss.mq.il.uil2.msgs.AcknowledgementRequestMsg;
import org.jboss.mq.il.uil2.msgs.AddMsg;
import org.jboss.mq.il.uil2.msgs.BrowseMsg;
import org.jboss.mq.il.uil2.msgs.CheckIDMsg;
import org.jboss.mq.il.uil2.msgs.CheckUserMsg;
import org.jboss.mq.il.uil2.msgs.CloseMsg;
import org.jboss.mq.il.uil2.msgs.CreateDestMsg;
import org.jboss.mq.il.uil2.msgs.DeleteTemporaryDestMsg;
import org.jboss.mq.il.uil2.msgs.DeleteSubscriptionMsg;
import org.jboss.mq.il.uil2.msgs.PingMsg;
import org.jboss.mq.il.uil2.msgs.ReceiveMsg;
import org.jboss.mq.il.uil2.msgs.SubscribeMsg;
import org.jboss.mq.il.uil2.msgs.TransactMsg;
import org.jboss.mq.il.uil2.msgs.UnsubscribeMsg;

/** The UILServerIL is created on the server and copied to the client during
 * connection factory lookups. It represents the transport interface to the
 * JMS server.
 * 
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.4 $
 */
public class UILServerIL
   implements Cloneable, MsgTypes, Serializable, ServerIL
{
   private static Logger log = Logger.getLogger(UILServerIL.class);

   /** The org.jboss.mq.il.uil2.localAddr system property allows a client to
    *define the local interface to which its sockets should be bound
    */
   private final static String LOCAL_ADDR = "org.jboss.mq.il.uil2.localAddr";
   /** The org.jboss.mq.il.uil2.localPort system property allows a client to
    *define the local port to which its sockets should be bound
    */
   private final static String LOCAL_PORT = "org.jboss.mq.il.uil2.localPort";

   /** The server host name/IP to connect to
    */
   private InetAddress addr;
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

   /**
    * The buffer size.
    */
   private int bufferSize;

   /**
    * The chunk size.
    */
   private int chunkSize;

   /** The local interface name/IP to use for the client
    */
   private transient InetAddress localAddr;
   /** The local port to use for the client
    */
   private transient int localPort;

   /**
    * Description of the Field
    */
   protected transient Socket socket;
   /**
    * Description of the Field
    */
   protected transient SocketManager socketMgr;

   /**
    * Constructor for the UILServerIL object
    *
    * @param a     Description of Parameter
    * @param port  Description of Parameter
    */
   public UILServerIL(InetAddress addr, int port, String socketFactoryName,
      boolean enableTcpNoDelay, int bufferSize, int chunkSize)
      throws Exception
   {
      this.addr = addr;
      this.port = port;
      this.socketFactoryName = socketFactoryName;
      this.enableTcpNoDelay = enableTcpNoDelay;
      this.bufferSize = bufferSize;
      this.chunkSize = chunkSize;
   }

   /**
    * Sets the ConnectionToken attribute of the UILServerIL object
    *
    * @param dest           The new ConnectionToken value
    * @exception Exception  Description of Exception
    */
   public void setConnectionToken(ConnectionToken dest)
          throws Exception
   {
      ConnectionTokenMsg msg = new ConnectionTokenMsg(dest);
      getSocketMgr().sendMessage(msg);
   }

   /**
    * Sets the Enabled attribute of the UILServerIL object
    *
    * @param dc                The new Enabled value
    * @param enabled           The new Enabled value
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public void setEnabled(ConnectionToken dc, boolean enabled)
          throws JMSException, Exception
   {
      EnableConnectionMsg msg = new EnableConnectionMsg(enabled);
      getSocketMgr().sendMessage(msg);
   }

   /**
    * Gets the ID attribute of the UILServerIL object
    *
    * @return               The ID value
    * @exception Exception  Description of Exception
    */
   public String getID()
          throws Exception
   {
      GetIDMsg msg = new GetIDMsg();
      getSocketMgr().sendMessage(msg);
      String id = msg.getID();
      return id;
   }

   /**
    * Gets the TemporaryQueue attribute of the UILServerIL object
    *
    * @param dc                Description of Parameter
    * @return                  The TemporaryQueue value
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public TemporaryQueue getTemporaryQueue(ConnectionToken dc)
          throws JMSException, Exception
   {
      TemporaryDestMsg msg = new TemporaryDestMsg(true);
      getSocketMgr().sendMessage(msg);
      TemporaryQueue dest = msg.getQueue();
      return dest;
   }

   /**
    * Gets the TemporaryTopic attribute of the UILServerIL object
    *
    * @param dc                Description of Parameter
    * @return                  The TemporaryTopic value
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public TemporaryTopic getTemporaryTopic(ConnectionToken dc)
          throws JMSException, Exception
   {
      TemporaryDestMsg msg = new TemporaryDestMsg(false);
      getSocketMgr().sendMessage(msg);
      TemporaryTopic dest = msg.getTopic();
      return dest;
   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param item              Description of Parameter
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public void acknowledge(ConnectionToken dc, AcknowledgementRequest item)
          throws JMSException, Exception
   {
      AcknowledgementRequestMsg msg = new AcknowledgementRequestMsg(item);
      getSocketMgr().sendMessage(msg);
   }

   /**
    * Adds a feature to the Message attribute of the UILServerIL object
    *
    * @param dc             The feature to be added to the Message attribute
    * @param val            The feature to be added to the Message attribute
    * @exception Exception  Description of Exception
    */
   public void addMessage(ConnectionToken dc, SpyMessage val)
          throws Exception
   {
      AddMsg msg = new AddMsg(val);
      getSocketMgr().sendMessage(msg);
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
      BrowseMsg msg = new BrowseMsg(dest, selector);
      getSocketMgr().sendMessage(msg);
      SpyMessage[] msgs = msg.getMessages();
      return msgs;
   }

   /**
    * #Description of the Method
    *
    * @param ID                Description of Parameter
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public void checkID(String id)
          throws JMSException, Exception
   {
      CheckIDMsg msg = new CheckIDMsg(id);
      getSocketMgr().sendMessage(msg);
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
   public String checkUser(String username, String password)
          throws JMSException, Exception
   {
      CheckUserMsg msg = new CheckUserMsg(username, password, false);
      getSocketMgr().sendMessage(msg);
      String clientID = msg.getID();
      return clientID;
   }

   /**
    * Authenticate the user
    *
    * @param userName          Description of Parameter
    * @param password          Description of Parameter
    * @return                  a sessionID
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public String authenticate(String username, String password)
          throws JMSException, Exception
   {
      CheckUserMsg msg = new CheckUserMsg(username, password, true);
      getSocketMgr().sendMessage(msg);
      String sessionID = msg.getID();
      return sessionID;
   }

   /**
    * #Description of the Method
    *
    * @return                                Description of the Returned Value
    * @exception CloneNotSupportedException  Description of Exception
    */
   public Object clone()
          throws CloneNotSupportedException
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
   public ServerIL cloneServerIL()
          throws Exception
   {
      return (ServerIL)clone();
   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public void connectionClosing(ConnectionToken dc)
          throws JMSException, Exception
   {
      CloseMsg msg = new CloseMsg();
      try
      {
         getSocketMgr().sendMessage(msg);
      }
      catch (IOException ignored)
      {
      }
      destroyConnection();
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
   public Queue createQueue(ConnectionToken dc, String name)
          throws JMSException, Exception
   {
      CreateDestMsg msg = new CreateDestMsg(name, true);
      getSocketMgr().sendMessage(msg);
      Queue dest = msg.getQueue();
      return dest;
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
   public Topic createTopic(ConnectionToken dc, String name)
          throws JMSException, Exception
   {
      CreateDestMsg msg = new CreateDestMsg(name, true);
      getSocketMgr().sendMessage(msg);
      Topic dest = msg.getTopic();
      return dest;
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
      DeleteTemporaryDestMsg msg = new DeleteTemporaryDestMsg(dest);
      getSocketMgr().sendMessage(msg);
   }

   /**
    * #Description of the Method
    *
    * @param id                Description of Parameter
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public void destroySubscription(ConnectionToken dc,DurableSubscriptionID id)
          throws JMSException, Exception
   {
      DeleteSubscriptionMsg msg = new DeleteSubscriptionMsg(id);
      getSocketMgr().sendMessage(msg);
   }

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @param clientTime     Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void ping(ConnectionToken dc, long clientTime)
          throws Exception
   {
      PingMsg msg = new PingMsg(clientTime, true);
      msg.getMsgID();
      getSocketMgr().sendReply(msg);
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
   public SpyMessage receive(ConnectionToken dc, int subscriberId, long wait)
          throws Exception, Exception
   {
      ReceiveMsg msg = new ReceiveMsg(subscriberId, wait);
      getSocketMgr().sendMessage(msg);
      SpyMessage reply = msg.getMessage();
      return reply;
   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param s                 Description of Parameter
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public void subscribe(ConnectionToken dc, org.jboss.mq.Subscription s)
          throws JMSException, Exception
   {
      SubscribeMsg msg = new SubscribeMsg(s);
      getSocketMgr().sendMessage(msg);
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
      TransactMsg msg = new TransactMsg(t);
      getSocketMgr().sendMessage(msg);
   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param subscriptionId    Description of Parameter
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public void unsubscribe(ConnectionToken dc, int subscriptionID)
          throws JMSException, Exception
   {
      UnsubscribeMsg msg = new UnsubscribeMsg(subscriptionID);
      getSocketMgr().sendMessage(msg);
   }

   final SocketManager getSocketMgr()
      throws Exception
   {
      if( socketMgr == null )
         createConnection();
      return socketMgr;
   }

   /**
    * #Description of the Method
    *
    * @exception Exception  Description of Exception
    */
   protected void checkConnection()
          throws Exception
   {
      if (socketMgr == null)
      {
         createConnection();
      }
   }

   /**
    * Used to establish a new connection to the server
    *
    * @exception Exception  Description of Exception
    */
   protected void createConnection()
          throws Exception
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
            + ", socketFactory="+socketFactory
            + ", enableTcpNoDelay="+enableTcpNoDelay
            + ", bufferSize="+bufferSize
            + ", chunkSize="+chunkSize
            );
      }

      int retries = 0;
      while (true)
      {
         try
         {
            if( localAddr != null )
               socket = socketFactory.createSocket(addr, port, localAddr, localPort);
            else
               socket = socketFactory.createSocket(addr, port);
            break;
         }
         catch (ConnectException e)
         {
            if (++retries > 10)
               throw e;
         }
      }

      socket.setTcpNoDelay(enableTcpNoDelay);
      socketMgr = new SocketManager(socket);
      socketMgr.setBufferSize(bufferSize);
      socketMgr.setChunkSize(chunkSize);
      socketMgr.start(Connection.threadGroup);
   }

   /**
    * Used to close the current connection with the server
    *
    * @exception Exception  Description of Exception
    */
   protected void destroyConnection()
   {
      try
      {
        if( socket != null )
        {
           try
           {
              socketMgr.stop();
           }
           finally
           {
              socket.close();
           }
        }
      }
      catch(IOException ignore)
      {
      }
   }

}
