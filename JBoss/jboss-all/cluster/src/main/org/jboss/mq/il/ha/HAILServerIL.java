/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.ha;

import java.io.Serializable;
import java.net.ConnectException;
import java.net.InetAddress;

import javax.management.AttributeNotFoundException;
import javax.management.InstanceNotFoundException;
import javax.management.ListenerNotFoundException;
import javax.management.MBeanException;
import javax.management.MBeanServer;
import javax.management.Notification;
import javax.management.NotificationListener;
import javax.management.ObjectName;
import javax.management.ReflectionException;
import javax.net.SocketFactory;

import org.jboss.logging.Logger;
import org.jboss.mq.Connection;
import org.jboss.mq.ConnectionToken;
import org.jboss.mq.il.ServerIL;
import org.jboss.mq.il.uil2.SocketManager;
import org.jboss.mq.il.uil2.UILServerIL;
import org.jboss.mq.il.uil2.msgs.MsgTypes;
import org.jboss.mx.util.MBeanServerLocator;

/** The HAILServerIL is created on the server and copied to the client during
 * connection factory lookups. It represents the transport interface to the
 * JMS server.
 * 
 * @author Scott.Stark@jboss.org
 * @author ivelin@apache.org
 * @version $Revision: 1.1.2.1 $
 */
public class HAILServerIL
  extends UILServerIL
  implements Cloneable, MsgTypes, Serializable, ServerIL, NotificationListener
{

  public static final String HAIL_SHARED_STATE_SERVICE = "jboss.mq:service=HAILSharedState";

  /**
   * Constructor for the UILServerIL object
   *
   * @param a     Description of Parameter
   * @param port  Description of Parameter
   */
  public HAILServerIL(
    String socketFactoryName,
    boolean enableTcpNoDelay,
    int bufferSize,
    int chunkSize)
    throws Exception
  {
    super(
      null, // server addr is not applicable in constructor  
      0, // server port is not applicable
      socketFactoryName,
      enableTcpNoDelay,
      bufferSize,
      chunkSize);
      
    // @TODO some of these private vars won't be necessary 
    // when the UILServerIL is refactored to allow 
    // subclasses to override values
    this.socketFactoryName = socketFactoryName;
    this.enableTcpNoDelay = enableTcpNoDelay;
    this.bufferSize = bufferSize;
    this.chunkSize = chunkSize;
          
    sharedStateService_ = new ObjectName(HAIL_SHARED_STATE_SERVICE);
    
  }


  /**
   * Used to establish a new connection to the server.
   * 
   * Almost identical to the UIL2 implementation, except for the fact that
   * the server address and port are obtained from the HAILSharedState MBean
   * instead of deployment-time properties.
   *
   * @exception Exception  Description of Exception
   */
  protected void createConnection() throws Exception
  {
    // first time connect to the current singleton address
    // if Singleton changes address, make sure this call fails, so that the client is forced to reconnect.
    if (singletonServerMoved_)
    {
      throw new IllegalStateException("Cannot reconnect, because singletonServerMoved_");
    }
    

    // will use this to talk to HAIL MBeans
    mbeanServer_ = MBeanServerLocator.locateJBoss();
    
    addHAILNotificationListener();


    //@TODO the rest should be moved to UILServerIL

    // lookup the address and port via method call, to allow subclasses to provide value.
    InetAddress addr = getServerAddress();
    int port = getServerPort(); 
  
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
  

  protected InetAddress getServerAddress() throws AttributeNotFoundException, InstanceNotFoundException, MBeanException, ReflectionException
  {
    return (InetAddress)mbeanServer_.getAttribute(sharedStateService_, "ServerAddress");
  }

  protected int getServerPort() throws AttributeNotFoundException, InstanceNotFoundException, MBeanException, ReflectionException
  {
    Integer port =  (Integer)mbeanServer_.getAttribute(sharedStateService_, "ServerPort");
    return port.intValue();
  }

  /**
   * Overrides the UIL ping to take into account 
   * occasional migration of the UIL server singleton 
   *
   * @param dc             Description of Parameter
   * @param clientTime     Description of Parameter
   * @exception Exception  Description of Exception
   */
  public void ping(ConnectionToken dc, long clientTime)
         throws Exception
  {
    // if the singleton server moved, disable ping.
    // This will cause an async failure of the connection, which will in turn notify
    // the client if it registered ExceptionListener.
    if (singletonServerMoved_) 
    {
      return;
    } 
    else
    {
      super.ping(dc, clientTime);
    }
  }
  
  protected void addHAILNotificationListener() throws InstanceNotFoundException
  {
    mbeanServer_.addNotificationListener(sharedStateService_, this, 
      /* no need for filter */ null, 
      /* no handback object */ null);
  }

  protected void removeHAILNotificationListener() throws InstanceNotFoundException, ListenerNotFoundException
  {
    mbeanServer_.removeNotificationListener(sharedStateService_, this);
  }

  /**
   * 
   * Notification that the singleton HAIL server moved.
   * Close connection and unsubscribe.
   * 
   */
  public void handleNotification(
    Notification notification,
    java.lang.Object handback)
  {
    // this will cause the connection to fail and notify the client's Connection ExceptionListener
    singletonServerMoved_ = true;
    
    // close previous connection
    try
    {
      connectionClosing(null);
    }
    catch (Exception e)
    {
      log.error("Exception occured when trying HAILServerIL.connectionClosing(): ", e);
    }

    // unsubscribe from HAIL notifications
    try
    {
      removeHAILNotificationListener();
    }
    catch (Exception e)
    {
      log.error("Exception occured when trying HAILServerIL.removeHAILNotificationListener(): ", e);
    }

    // the previous connection is no longer valid    
    socketMgr = null;
    
  }


  private static Logger log = Logger.getLogger(HAILServerIL.class);

  /**
   * The default JBoss MBean server
   */
  private transient MBeanServer mbeanServer_;

  /**
   * the name of the HAILSharedState MBean
   */
  private ObjectName sharedStateService_;
  
  /**
   * When the singleton server moves, this flag marks the connection as unrecoverable
   */
  private boolean singletonServerMoved_ = false;

  //@TODO All attributes below can be removed, once the UILServerIL is refactored

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

  private final static String LOCAL_ADDR = "org.jboss.mq.il.uil2.localAddr";
  /** The org.jboss.mq.il.uil2.localPort system property allows a client to
   *define the local port to which its sockets should be bound
   */
  private final static String LOCAL_PORT = "org.jboss.mq.il.uil2.localPort";

}
