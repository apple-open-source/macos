/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.ha;

import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.Properties;

import javax.management.Attribute;
import javax.management.AttributeNotFoundException;
import javax.management.InstanceNotFoundException;
import javax.management.InvalidAttributeValueException;
import javax.management.MBeanException;
import javax.management.MBeanServer;
import javax.management.Notification;
import javax.management.ObjectName;
import javax.management.ReflectionException;

import org.jboss.logging.Logger;
import org.jboss.mq.il.ServerIL;
import org.jboss.mq.il.ServerILJMXService;
import org.jboss.mq.il.uil2.UILClientILService;
import org.jboss.mq.il.uil2.UILServerILFactory;

/** 
 * 
 * This is the server side MBean for the HAIL transport layer.
 * It builts upon UIL2.
 *
 * @author ivelin@apache.org
 * @version   $Revision: 1.1.2.1 $
 *
 * @jmx:mbean extends="org.jboss.mq.il.ServerILJMXServiceMBean"
 */
public class HAILServerILService extends ServerILJMXService
      implements HAILServerILServiceMBean
{

  public static final String NOTIFICATION_SINGLETON_MOVED = "jboss.mq.hail.singleton.moved";

   /**
    * Used to construct the GenericConnectionFactory (bindJNDIReferences()
    * builds it) Sets up the connection properties need by a client to use this
    * IL
    *
    * @return   The ClientConnectionProperties value
    */
   public Properties getClientConnectionProperties()
   {
    return connectionProperties_;
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
      return serverIL_;
   }

   /**
    * Starts this IL, and binds it to JNDI
    *
    * @exception Exception  Description of Exception
    */
   public void startService() throws Exception
   {
     
     serverIL_ = new HAILServerIL( getClientSocketFactory(), getEnableTcpNoDelay(), getBufferSize(), getChunkSize());
     
     // Initialize the connection poperties using the base class.
     connectionProperties_ = super.getClientConnectionProperties();
     connectionProperties_.setProperty(UILServerILFactory.CLIENT_IL_SERVICE_KEY, UILClientILService.class.getName());
     connectionProperties_.setProperty(UILServerILFactory.UIL_TCPNODELAY_KEY, getEnableTcpNoDelay() ? "yes" : "no");
     connectionProperties_.setProperty(UILServerILFactory.UIL_BUFFERSIZE_KEY, "" + getBufferSize());
     connectionProperties_.setProperty(UILServerILFactory.UIL_CHUNKSIZE_KEY, "" + getChunkSize());

     bindJNDIReferences();
   }

   /**
    * Stops this IL, and unbinds it from JNDI
    */
   public void stopService()
   {
      try
      {
         unbindJNDIReferences();
      }
      catch (Exception e)
      {
         log.error("Exception occured when trying to stop HAIL Service: ", e);
      }
   }

  /**
   * Get the value of UIL2Service.
   *
   * @return value of UILServerILService.
   */
  public ObjectName getUILServerILService()
  {
     return uilService_;
  }

  /**
   * Set the value of UILServerILService.
   *
   * @param v  Value to assign to UILServerILService.
   */
  public void setUILServerILService(ObjectName uilService)
  {
    uilService_ = uilService;
  }   


  /**
   * Broadcasts a notification to the cluster partition.
   * 
   */
  public void broadcastSingletonChangeNotification()
    throws InstanceNotFoundException, MBeanException, ReflectionException
  {
    long now = System.currentTimeMillis();
    Notification notification =
      new Notification(
        NOTIFICATION_SINGLETON_MOVED,
        getServiceName(),
        now, // seq #
        now, // time
        NOTIFICATION_SINGLETON_MOVED);
    server.invoke(
        sharedStateService_,
        "sendNotification",
        new Object[] { notification },
        new String[] { Notification.class.getName() }
        );
  }

  public void startSingleton()
  {
    log.info("Notified to become singleton");
    isMasterNode_ = true;
    
    
    try
    {
      updateSharedState();
    }
    catch (Exception e)
    {
      log.info("Exception while trying HAILServerILService.updateSharedState() : ", e);
    }
  }

  public boolean isMasterNode()
  {
    return isMasterNode_;
  }

  public void stopSingleton()
  {
    isMasterNode_ = false;
    log.info( "Notified to stop acting as singleton.");
  }

  public ObjectName getUILService()
  {
    return uilService_;
  }

  public void setUILService(ObjectName uilService)
  {
    uilService_ = uilService;
  }

  public ObjectName getSharedStateService()
  {
    return sharedStateService_;
  }

  public void setSharedStateService(ObjectName sharedStateService)
  {
    sharedStateService_ = sharedStateService;
  }

  /**
   * 
   * Share the coordinates of the new UIL singleton server and notify world
   * 
   */
  public void updateSharedState() throws InstanceNotFoundException, AttributeNotFoundException, InvalidAttributeValueException, MBeanException, ReflectionException, UnknownHostException
  {
    MBeanServer mbeanServer = getServer();
    // first update the shared HAIL state, then broadcast notification.
    Attribute attrAddr= new Attribute("ServerAddress", getServerAddress());
    mbeanServer.setAttribute( sharedStateService_, attrAddr);
    Attribute attrPort = new Attribute("ServerPort", getServerPort()); 
    mbeanServer.setAttribute( sharedStateService_, attrPort);

    broadcastSingletonChangeNotification();
  }


  public InetAddress getServerAddress() throws UnknownHostException, AttributeNotFoundException, InstanceNotFoundException, MBeanException, ReflectionException
  {
    MBeanServer mbeanServer = getServer();
    String host =  (String)mbeanServer.getAttribute( uilService_, "BindAddress");
    // if blank address, convert to a real IP which will make sense to remote clients.
    InetAddress addr = null;
    if (host.equals("0.0.0.0"))
    {
      addr = InetAddress.getLocalHost();
    }
    else
    {
      addr = InetAddress.getByName(host);
    }
    return addr;
  }

  public Integer getServerPort() throws AttributeNotFoundException, InstanceNotFoundException, MBeanException, ReflectionException
  {
    MBeanServer mbeanServer = getServer();
    Integer port =  (Integer)mbeanServer.getAttribute( uilService_, "ServerBindPort");
    return port;
  }
  
  /** Get the javax.net.SocketFactory implementation class to use on the
   *client.
   * @jmx:managed-attribute
   */
  public String getClientSocketFactory() throws AttributeNotFoundException, InstanceNotFoundException, MBeanException, ReflectionException
  {
    MBeanServer mbeanServer = getServer();
    String fname =  (String)mbeanServer.getAttribute( uilService_, "ClientSocketFactory");
    return fname;
  }


  /**
   * Gets the enableTcpNoDelay.
   * @return Returns a boolean
   *
   * @jmx:managed-attribute
   */
  public boolean getEnableTcpNoDelay() throws AttributeNotFoundException, InstanceNotFoundException, MBeanException, ReflectionException
  {
    MBeanServer mbeanServer = getServer();
    Boolean delay =  (Boolean)mbeanServer.getAttribute( uilService_, "EnableTcpNoDelay");
    return delay.booleanValue();
  }

  /**
   * Gets the chunk size.
   * @return Returns an int
   *
   * @jmx:managed-attribute
   */
  public int getChunkSize() throws AttributeNotFoundException, InstanceNotFoundException, MBeanException, ReflectionException
  {
    MBeanServer mbeanServer = getServer();
    Integer chunk =  (Integer)mbeanServer.getAttribute( uilService_, "ChunkSize");
    return chunk.intValue();
  }

  /**
   * Gets the buffer size.
   * @return Returns an int
   *
   * @jmx:managed-attribute
   */
  public int getBufferSize() throws AttributeNotFoundException, InstanceNotFoundException, MBeanException, ReflectionException
  {
    MBeanServer mbeanServer = getServer();
    Integer buffer =  (Integer)mbeanServer.getAttribute( uilService_, "BufferSize");
    return buffer.intValue();
  }
  
  /**
   * Gives this JMX service a name.
   *
   * @return   The Name value
   */
  public String getName()
  {
     return "JBossMQ-HAILServerIL";
  }

  // Attributes ----------------------------------------------

  private static Logger log = Logger.getLogger(HAILServerILService.class);
  private boolean isMasterNode_ = false;

  /**
   * The shared state MBean that HAIL Servers use to coordinate the current singleton
   */
  ObjectName sharedStateService_ = null;
  
  /**
   * The UILServerILService that the HAIL builts upon
   */
  private ObjectName uilService_;


  /**
   * The HAILServerIL which is cloned and given to the client so it 
   * can talk to the server.
   */
  private HAILServerIL serverIL_;
  
  /**
   * Connection Properties used by the client stub to talk to the server IL 
   */
  Properties connectionProperties_;
  
}
