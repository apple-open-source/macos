/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.uil;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;

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
import org.jboss.mq.ConnectionToken;
import org.jboss.mq.DurableSubscriptionID;
import org.jboss.mq.SpyDestination;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.TransactionRequest;

import org.jboss.mq.il.ServerIL;
import org.jboss.mq.il.uil.multiplexor.SocketMultiplexor;

/**
 * The JVM implementation of the ServerIL object
 *
 * @author    Hiram Chirino (Cojonudo14@hotmail.com)
 * @author    Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author    <a href="pra@tim.se">Peter Antman</a>
 * @version   $Revision: 1.8.4.5 $
 * @created   August 16, 2001
 */
public class UILServerIL implements java.io.Serializable, Cloneable, org.jboss.mq.il.ServerIL
{
   private static Logger log = Logger.getLogger(UILServerIL.class);

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

   /** The org.jboss.mq.il.oil2.localAddr system property allows a client to
    *define the local interface to which its sockets should be bound
    */
   private final static String LOCAL_ADDR = "org.jboss.mq.il.oil.localAddr";
   /** The org.jboss.mq.il.oil2.localPort system property allows a client to
    *define the local port to which its sockets should be bound
    */
   private final static String LOCAL_PORT = "org.jboss.mq.il.oil.localPort";

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
   /** The local interface name/IP to use for the client
    */
   private transient InetAddress localAddr;
   /** The local port to use for the client
    */
   private transient int localPort;

   /**
    * Description of the Field
    */
   protected transient ObjectInputStream in;

   /**
    * Description of the Field
    */
   protected transient Socket socket;
   /**
    * Description of the Field
    */
   protected transient SocketMultiplexor mSocket;
   /**
    * Description of the Field
    */
   protected transient ObjectOutputStream out;

   /**
    * Constructor for the UILServerIL object
    *
    * @param a     Description of Parameter
    * @param port  Description of Parameter
    */
   public UILServerIL(InetAddress addr, int port, 
      String socketFactoryName, boolean enableTcpNoDelay)
   {
      this.addr = addr;
      this.port = port;
      this.socketFactoryName = socketFactoryName;
      this.enableTcpNoDelay = enableTcpNoDelay;
   }

   /**
    * Sets the ConnectionToken attribute of the UILServerIL object
    *
    * @param dest           The new ConnectionToken value
    * @exception Exception  Description of Exception
    */
   public synchronized void setConnectionToken(ConnectionToken dest)
          throws Exception
   {
      checkConnection();
      out.writeByte(m_setSpyDistributedConnection);
      out.writeObject(dest);
      waitAnswer();
   }

   /**
    * Sets the Enabled attribute of the UILServerIL object
    *
    * @param dc                The new Enabled value
    * @param enabled           The new Enabled value
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public synchronized void setEnabled(ConnectionToken dc, boolean enabled)
          throws JMSException, Exception
   {
      checkConnection();
      out.writeByte(m_setEnabled);
      out.writeBoolean(enabled);
      waitAnswer();
   }

   /**
    * Gets the ID attribute of the UILServerIL object
    *
    * @return               The ID value
    * @exception Exception  Description of Exception
    */
   public synchronized String getID()
          throws Exception
   {
      checkConnection();
      out.writeByte(m_getID);
      return (String)waitAnswer();
   }

   /**
    * Gets the TemporaryQueue attribute of the UILServerIL object
    *
    * @param dc                Description of Parameter
    * @return                  The TemporaryQueue value
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public synchronized TemporaryQueue getTemporaryQueue(ConnectionToken dc)
          throws JMSException, Exception
   {
      checkConnection();
      out.writeByte(m_getTemporaryQueue);
      return (TemporaryQueue)waitAnswer();
   }

   /**
    * Gets the TemporaryTopic attribute of the UILServerIL object
    *
    * @param dc                Description of Parameter
    * @return                  The TemporaryTopic value
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public synchronized TemporaryTopic getTemporaryTopic(ConnectionToken dc)
          throws JMSException, Exception
   {
      checkConnection();
      out.writeByte(m_getTemporaryTopic);
      return (TemporaryTopic)waitAnswer();
   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param item              Description of Parameter
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public synchronized void acknowledge(ConnectionToken dc, AcknowledgementRequest item)
          throws JMSException, Exception
   {
      checkConnection();
      out.writeByte(m_acknowledge);
      item.writeExternal(out);
      waitAnswer();
   }

   /**
    * Adds a feature to the Message attribute of the UILServerIL object
    *
    * @param dc             The feature to be added to the Message attribute
    * @param val            The feature to be added to the Message attribute
    * @exception Exception  Description of Exception
    */
   public synchronized void addMessage(ConnectionToken dc, SpyMessage val)
          throws Exception
   {
      checkConnection();
      out.writeByte(m_addMessage);
      SpyMessage.writeMessage(val, out);
      waitAnswer();
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
   public synchronized SpyMessage[] browse(ConnectionToken dc, Destination dest, String selector)
          throws JMSException, Exception
   {
      checkConnection();
      out.writeByte(m_browse);
      out.writeObject(dest);
      out.writeObject(selector);
      return (SpyMessage[])waitAnswer();
   }

   /**
    * #Description of the Method
    *
    * @param ID                Description of Parameter
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public synchronized void checkID(String ID)
          throws JMSException, Exception
   {
      checkConnection();
      out.writeByte(m_checkID);
      out.writeObject(ID);
      waitAnswer();
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
   public synchronized String checkUser(String userName, String password)
          throws JMSException, Exception
   {
      checkConnection();
      out.writeByte(m_checkUser);
      out.writeObject(userName);
      out.writeObject(password);
      return (String)waitAnswer();
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
   public synchronized String authenticate(String userName, String password)
          throws JMSException, Exception
   {
      checkConnection();
      out.writeByte(m_authenticate);
      out.writeObject(userName);
      out.writeObject(password);
      return (String)waitAnswer();
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
   public synchronized void connectionClosing(ConnectionToken dc)
          throws JMSException, Exception
   {
      try
      {
         checkConnection();
         out.writeByte(m_connectionClosing);
         waitAnswer();
      }
      finally
      {
         destroyConnection();
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
   public synchronized Queue createQueue(ConnectionToken dc, String dest)
          throws JMSException, Exception
   {
      checkConnection();
      out.writeByte(m_createQueue);
      out.writeObject(dest);
      return (Queue)waitAnswer();
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
   public synchronized Topic createTopic(ConnectionToken dc, String dest)
          throws JMSException, Exception
   {
      checkConnection();
      out.writeByte(m_createTopic);
      out.writeObject(dest);
      return (Topic)waitAnswer();
   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param dest              Description of Parameter
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public synchronized void deleteTemporaryDestination(ConnectionToken dc, SpyDestination dest)
          throws JMSException, Exception
   {
      checkConnection();
      out.writeByte(m_deleteTemporaryDestination);
      out.writeObject(dest);
      waitAnswer();
   }

   /**
    * #Description of the Method
    *
    * @param id                Description of Parameter
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public synchronized void destroySubscription(ConnectionToken dc,DurableSubscriptionID id)
          throws JMSException, Exception
   {
      checkConnection();
      out.writeByte(m_destroySubscription);
      out.writeObject(id);
      waitAnswer();
   }

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @param clientTime     Description of Parameter
    * @exception Exception  Description of Exception
    */
   public synchronized void ping(ConnectionToken dc, long clientTime)
          throws Exception
   {
      checkConnection();
      out.writeByte(m_ping);
      out.writeLong(clientTime);
      out.flush();
      // This can cause a deadlock
      //waitAnswer();
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
   public synchronized SpyMessage receive(ConnectionToken dc, int subscriberId, long wait)
          throws Exception, Exception
   {
      checkConnection();
      out.writeByte(m_receive);
      out.writeInt(subscriberId);
      out.writeLong(wait);
      return (SpyMessage)waitAnswer();
   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param s                 Description of Parameter
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public synchronized void subscribe(ConnectionToken dc, org.jboss.mq.Subscription s)
          throws JMSException, Exception
   {
      checkConnection();
      out.writeByte(m_subscribe);
      out.writeObject(s);
      waitAnswer();
   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param t                 Description of Parameter
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public synchronized void transact(org.jboss.mq.ConnectionToken dc, TransactionRequest t)
          throws JMSException, Exception
   {
      checkConnection();
      out.writeByte(m_transact);
      t.writeExternal(out);
      waitAnswer();
   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param subscriptionId    Description of Parameter
    * @exception JMSException  Description of Exception
    * @exception Exception     Description of Exception
    */
   public synchronized void unsubscribe(ConnectionToken dc, int subscriptionId)
          throws JMSException, Exception
   {
      checkConnection();
      out.writeByte(m_unsubscribe);
      out.writeInt(subscriptionId);
      waitAnswer();
   }

   /**
    * #Description of the Method
    *
    * @exception Exception  Description of Exception
    */
   protected void checkConnection()
          throws Exception
   {
      if (mSocket == null)
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
            );
      }

      if( localAddr != null )
         socket = socketFactory.createSocket(addr, port, localAddr, localPort);
      else
         socket = socketFactory.createSocket(addr, port);

      socket.setTcpNoDelay(enableTcpNoDelay);
      mSocket = new SocketMultiplexor(socket);
      out = new ObjectOutputStream(new BufferedOutputStream(mSocket.getOutputStream(1)));
      out.flush();
      in = new ObjectInputStream(new BufferedInputStream(mSocket.getInputStream(1)));
   }
   
   /**
    * Used to close the current connection with the server
    *
    * @exception Exception  Description of Exception
    */
   protected void destroyConnection()
          throws Exception
   {
   	  // This may fail as the other (muxed) stream may close the socket
   	  // before we get to close out.
   	  try {
	      in.close();
	      out.close();
	      // The socket is closed by the clientILService
   	  } catch ( java.io.IOException ignore ) {}
   }
   
   /**
    * #Description of the Method
    *
    * @return               Description of the Returned Value
    * @exception Exception  Description of Exception
    */
   protected Object waitAnswer()
          throws Exception
   {
      boolean trace = log.isTraceEnabled();
      if( trace )
         log.debug("Begin waitAnswer");
      out.reset();
      out.flush();
      int val = in.readByte();
      if( trace )
         log.debug("waitAnswer, val="+val);
      Object answer = null;
      if (val == 1)
      {
         answer = in.readObject();;
      }
      else if( val != 0 )
      {
         Exception e = (Exception)in.readObject();
         throw e;
      }
      if( trace )
         log.debug("End waitAnswer");
      return answer;
   }
}
