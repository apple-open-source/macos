/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.jmx.connector.rmi;

import java.io.ObjectInputStream;

import java.rmi.server.UnicastRemoteObject;

import java.rmi.RemoteException;
import java.rmi.ServerException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Hashtable;
import java.util.Iterator;
import java.util.Random;
import java.util.Set;
import java.util.Vector;

import javax.jms.JMSException;
import javax.jms.Message;
import javax.jms.MessageListener;
import javax.jms.ObjectMessage;
import javax.jms.Queue;
import javax.jms.QueueConnection;
import javax.jms.QueueConnectionFactory;
import javax.jms.QueueReceiver;
import javax.jms.QueueSender;
import javax.jms.QueueSession;
import javax.jms.Session;

import javax.management.Attribute;
import javax.management.AttributeList;
import javax.management.ObjectName;
import javax.management.QueryExp;
import javax.management.ObjectInstance;
import javax.management.Notification;
import javax.management.NotificationFilter;
import javax.management.NotificationListener;
import javax.management.MBeanServer;
import javax.management.MBeanInfo;
import javax.management.loading.ClassLoaderRepository;

import javax.management.AttributeNotFoundException;
import javax.management.InstanceAlreadyExistsException;
import javax.management.InstanceNotFoundException;
import javax.management.IntrospectionException;
import javax.management.InvalidAttributeValueException;
import javax.management.ListenerNotFoundException;
import javax.management.MBeanException;
import javax.management.RuntimeMBeanException;
import javax.management.MBeanRegistrationException;
import javax.management.NotCompliantMBeanException;
import javax.management.OperationsException;
import javax.management.ReflectionException;

import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.rmi.PortableRemoteObject;

import org.jboss.system.ServiceMBeanSupport;

import org.jboss.jmx.adaptor.rmi.RMIAdaptor;
import org.jboss.jmx.connector.RemoteMBeanServer;
import org.jboss.jmx.connector.notification.ClientNotificationListener;
import org.jboss.jmx.connector.notification.JMSClientNotificationListener;
import org.jboss.jmx.connector.notification.PollingClientNotificationListener;
import org.jboss.jmx.connector.notification.RMIClientNotificationListener;
import org.jboss.jmx.connector.notification.SearchClientNotificationListener;

import org.jboss.logging.Logger;

import org.jboss.util.NestedRuntimeException;

/**
 * Implementation of the JMX Connector over the RMI protocol
 *
 * @jmx:mbean extends="org.jboss.jmx.connector.RemoteMBeanServer"
 *
 * @version <tt>$Revision: 1.9.2.1 $</tt>
 * @author  <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author  <A href="mailto:andreas@jboss.org">Andreas &quot;Mad&quot; Schaefer</A>
 */
public class RMIConnectorImpl
   implements RMIConnectorImplMBean
{
   protected Logger log = Logger.getLogger(this.getClass());

   protected RMIAdaptor mRemoteAdaptor;
   protected Object mServer = "";
   protected Vector mListeners = new Vector();
   protected int mEventType = NOTIFICATION_TYPE_RMI;
   protected String[] mOptions = new String[ 0 ];
   protected Random mRandom = new Random();

   /**
    * For sub-class support.
    */
   protected RMIConnectorImpl()
   {
      super();
   }
   
   /**
    * AS For evaluation purposes
    * Creates a Connector based on an already found Adaptor
    *
    * @param pAdaptor RMI-Adaptor used to connect to the remote JMX Agent
    **/
   public RMIConnectorImpl(RMIAdaptor pAdaptor)
   {
      mRemoteAdaptor = pAdaptor;
      mServer = "Dummy";
   }
   
   public RMIConnectorImpl(int pNotificationType,
                           String[] pOptions,
                           String pServerName)
      throws Exception
   {
      mEventType = pNotificationType;
      
      if( pOptions == null ) {
         mOptions = new String[ 0 ];
      } else {
         mOptions = pOptions;
      }
      
      start( pServerName );
   }
   
   // RemoteMBeanServer implementation -------------------------------------

   public Object instantiate(String className) 
      throws ReflectionException, MBeanException
   {
      try {
         return mRemoteAdaptor.instantiate(className);
      }
      catch (RemoteException e) {
         throw new MBeanException(e);
      }
   }

   public Object instantiate(String className, ObjectName loaderName) 
      throws ReflectionException, MBeanException, InstanceNotFoundException
   {
      try {
         return mRemoteAdaptor.instantiate(className, loaderName);
      }
      catch (RemoteException e) {
         throw new MBeanException(e);
      }
   }
   
   public Object instantiate(String className, Object[] params, String[] signature)
      throws ReflectionException, MBeanException
   {
      try {
         return mRemoteAdaptor.instantiate(className, params, signature);
      }
      catch (RemoteException e) {
         throw new MBeanException(e);
      }      
   }
   
   public Object instantiate(String className,
                             ObjectName loaderName,
                             Object[] params,
                             String[] signature)
      throws ReflectionException, MBeanException, InstanceNotFoundException
   {
      try {
         return mRemoteAdaptor.instantiate(className, loaderName, params, signature);
      }
      catch (RemoteException e) {
         throw new MBeanException(e);
      }      
   }

   public ObjectInstance createMBean(String pClassName, ObjectName pName)
      throws ReflectionException,
             InstanceAlreadyExistsException,
             MBeanRegistrationException,
             MBeanException,
             NotCompliantMBeanException
   {
      try {
         return mRemoteAdaptor.createMBean( pClassName, pName );
      }
      catch( RemoteException re ) {
         throw new MBeanException(re);
      }
   }

   public ObjectInstance createMBean(String pClassName,
                                     ObjectName pName,
                                     ObjectName pLoaderName)
      throws ReflectionException,
             InstanceAlreadyExistsException,
             MBeanRegistrationException,
             MBeanException,
             NotCompliantMBeanException,
             InstanceNotFoundException
   {
      try {
         return mRemoteAdaptor.createMBean( pClassName, pName, pLoaderName );
      }
      catch( RemoteException re ) {
         throw new MBeanException( re );
      }
   }

   public ObjectInstance createMBean(String pClassName,
                                     ObjectName pName,
                                     Object[] pParams,
                                     String[] pSignature)
      throws ReflectionException,
             InstanceAlreadyExistsException,
             MBeanRegistrationException,
             MBeanException,
             NotCompliantMBeanException
   {
      try {
         return mRemoteAdaptor.createMBean( pClassName, pName, pParams, pSignature );
      }
      catch( RemoteException re ) {
         throw new MBeanException( re );
      }
   }

   public ObjectInstance createMBean(String pClassName,
                                     ObjectName pName,
                                     ObjectName pLoaderName,
                                     Object[] pParams,
                                     String[] pSignature)
      throws ReflectionException,
             InstanceAlreadyExistsException,
             MBeanRegistrationException,
             MBeanException,
             NotCompliantMBeanException,
             InstanceNotFoundException
   {
      try {
         return mRemoteAdaptor.createMBean( pClassName, pName, pLoaderName, pParams, pSignature );
      }
      catch( RemoteException re ) {
         throw new MBeanException( re );
      }
   }

   public ObjectInstance registerMBean(Object object, ObjectName name) 
      throws InstanceAlreadyExistsException, MBeanRegistrationException, NotCompliantMBeanException
   {
      try {
         return mRemoteAdaptor.registerMBean(object, name);
      }
      catch (RemoteException e) {
         throw new RuntimeMBeanException(new NestedRuntimeException(e));
      }
   }
   
   public void unregisterMBean(ObjectName pName)
      throws InstanceNotFoundException,
             MBeanRegistrationException
   {
      try {
         mRemoteAdaptor.unregisterMBean( pName );
      }
      catch( RemoteException e ) {
         throw new RuntimeMBeanException(new NestedRuntimeException(e));
      }
   }

   public ObjectInstance getObjectInstance(ObjectName pName)
      throws InstanceNotFoundException
   {
      try {
         return mRemoteAdaptor.getObjectInstance( pName );
      }
      catch( RemoteException e ) {
         throw new RuntimeMBeanException(new NestedRuntimeException(e));
      }
   }

   public Set queryMBeans(ObjectName pName, QueryExp pQuery)
   {
      try {
         return mRemoteAdaptor.queryMBeans( pName, pQuery );
      }
      catch( RemoteException e ) {
         throw new RuntimeMBeanException(new NestedRuntimeException(e));
      }
   }

   public Set queryNames(ObjectName pName, QueryExp pQuery)
   {
      try {
         return mRemoteAdaptor.queryNames( pName, pQuery );
      }
      catch( RemoteException e ) {
         throw new RuntimeMBeanException(new NestedRuntimeException(e));
      }
   }

   public boolean isRegistered(ObjectName pName)
   {
      try {
         return mRemoteAdaptor.isRegistered( pName );
      }
      catch( RemoteException e ) {
         throw new RuntimeMBeanException(new NestedRuntimeException(e));
      }
   }

   public boolean isInstanceOf(ObjectName pName, String pClassName)
      throws InstanceNotFoundException
   {
      try {
         return mRemoteAdaptor.isInstanceOf( pName, pClassName );
      }
      catch( RemoteException e ) {
         throw new RuntimeMBeanException(new NestedRuntimeException(e));
      }
   }

   public Integer getMBeanCount()
   {
      try {
         return mRemoteAdaptor.getMBeanCount();
      }
      catch( RemoteException e ) {
         throw new RuntimeMBeanException(new NestedRuntimeException(e));
      }
   }

   public Object getAttribute(ObjectName pName, String pAttribute)
      throws MBeanException,
             AttributeNotFoundException,
             InstanceNotFoundException,
             ReflectionException
   {
      try {
         return mRemoteAdaptor.getAttribute( pName, pAttribute );
      }
      catch( RemoteException e ) {
         throw new MBeanException(e);
      }
   }

   public AttributeList getAttributes(ObjectName pName, String[] pAttributes)
      throws InstanceNotFoundException,
             ReflectionException
   {
      try {
         return mRemoteAdaptor.getAttributes( pName, pAttributes );
      }
      catch( RemoteException e ) {
         throw new RuntimeMBeanException(new NestedRuntimeException(e));
      }
   }

   public void setAttribute(ObjectName pName, Attribute pAttribute)
      throws InstanceNotFoundException,
             AttributeNotFoundException,
             InvalidAttributeValueException,
             MBeanException,
             ReflectionException
   {
      try {
         mRemoteAdaptor.setAttribute( pName, pAttribute );
      }
      catch( RemoteException e ) {
         throw new MBeanException(e);
      }
   }

   public AttributeList setAttributes(ObjectName pName,
                                      AttributeList pAttributes)
      throws InstanceNotFoundException,
             ReflectionException
   {
      try {
         return mRemoteAdaptor.setAttributes( pName, pAttributes );
      }
      catch( RemoteException e ) {
         throw new RuntimeMBeanException(new NestedRuntimeException(e));
      }
   }

   public Object invoke(ObjectName pName,
                        String pActionName,
                        Object[] pParams,
                        String[] pSignature)
      throws InstanceNotFoundException,
             MBeanException,
             ReflectionException
   {
      try {
         return mRemoteAdaptor.invoke( pName, pActionName, pParams, pSignature );
      }
      catch( RemoteException e ) {
         throw new MBeanException(e);
      }
   }

   public String getDefaultDomain()
   {
      try {
         return mRemoteAdaptor.getDefaultDomain();
      }
      catch( RemoteException e ) {
         throw new RuntimeMBeanException(new NestedRuntimeException(e));
      }
   }

   public void addNotificationListener(ObjectName pName,
                                       ObjectName pListener,
                                       NotificationFilter pFilter,
                                       Object pHandback)
      throws InstanceNotFoundException
   {
      try {
         mRemoteAdaptor.addNotificationListener( pName, pListener, pFilter, pHandback );
      }
      catch( RemoteException e ) {
         throw new RuntimeMBeanException(new NestedRuntimeException(e));
      }
   }

   public void addNotificationListener(ObjectName pName,
                                       NotificationListener pListener,
                                       NotificationFilter pFilter,
                                       Object pHandback)
      throws InstanceNotFoundException
   {
      try {
         ClientNotificationListener lListener = null;
         switch( mEventType ) {
            case NOTIFICATION_TYPE_RMI:
               lListener = new RMIClientNotificationListener(
                  pName,
                  pListener,
                  pHandback,
                  pFilter,
                  this
               );
               break;
               
            case NOTIFICATION_TYPE_JMS:
               lListener = new JMSClientNotificationListener(
                  pName,
                  pListener,
                  pHandback,
                  pFilter,
                  mOptions[ 0 ],
                  (String) mServer,
                  this
               );
               break;
               
            case NOTIFICATION_TYPE_POLLING:
               lListener = new PollingClientNotificationListener(
                  pName,
                  pListener,
                  pHandback,
                  pFilter,
                  5000, // Sleeping Period
                  2500, // Maximum Pooled List Size
                  this
               );
         }
         
         // Add this listener on the client to remove it when the client goes down
         mListeners.addElement( lListener );
      }
      catch( Exception e ) {
         if( e instanceof RuntimeException ) {
            throw (RuntimeException) e;
         }
         if( e instanceof InstanceNotFoundException ) {
            throw (InstanceNotFoundException) e;
         }

         throw new RuntimeMBeanException(new NestedRuntimeException(e));
      }
   }

   public void removeNotificationListener(ObjectName pName,
                                          NotificationListener pListener)
      throws InstanceNotFoundException,
             ListenerNotFoundException
   {
      ClientNotificationListener lCheck = new SearchClientNotificationListener( pName, pListener );
      int i = mListeners.indexOf( lCheck );
      if( i >= 0 ) {
         ClientNotificationListener lListener = (ClientNotificationListener) mListeners.get( i );
         lListener.removeNotificationListener( this );
      }
   }

   public void removeNotificationListener(ObjectName pName, ObjectName pListener)
      throws InstanceNotFoundException,
             ListenerNotFoundException
   {
      try {
         mRemoteAdaptor.removeNotificationListener( pName, pListener );
      }
      catch( RemoteException e ) {
         throw new RuntimeMBeanException(new NestedRuntimeException(e));
      }
   }

   public MBeanInfo getMBeanInfo(ObjectName pName)
      throws InstanceNotFoundException,
             IntrospectionException,
             ReflectionException
   {
      try {
         return mRemoteAdaptor.getMBeanInfo( pName );
      }
      catch( RemoteException e ) {
         throw new RuntimeMBeanException(new NestedRuntimeException(e));
      }
   }

   /**
    * Always throws {@link java.lang.UnsupportedOperationException}.
    *
    * @throws UnsupportedOperationException
    */
   public ObjectInputStream deserialize(ObjectName name, byte[] data) 
      throws InstanceNotFoundException, OperationsException
   {
      throw new UnsupportedOperationException();
   }
 
   /**
    * Always throws {@link java.lang.UnsupportedOperationException}.
    *
    * @throws UnsupportedOperationException
    */
   public ObjectInputStream deserialize(String className, byte[] data) 
      throws OperationsException, ReflectionException
   {
      throw new UnsupportedOperationException();
   }

   /**
    * Always throws {@link java.lang.UnsupportedOperationException}.
    *
    * @throws UnsupportedOperationException
    */
   public ObjectInputStream deserialize(String className, ObjectName loaderName, byte[] data)
      throws InstanceNotFoundException, OperationsException, ReflectionException
   {
      throw new UnsupportedOperationException();
   }

   // MBeanServer JMX 1.2 implementation ------------------------------

   public String[] getDomains()
   {
      throw new UnsupportedOperationException();
   }

   public void removeNotificationListener(ObjectName target, ObjectName listener, NotificationFilter filter, Object handback)
   {
      throw new UnsupportedOperationException();
   }

   public void removeNotificationListener(ObjectName target, NotificationListener listener, NotificationFilter filter, Object handback)
   {
      throw new UnsupportedOperationException();
   }

   public ClassLoaderRepository getClassLoaderRepository()
   {
      throw new UnsupportedOperationException();
   }

   public ClassLoader getClassLoader(ObjectName name)
   {
      throw new UnsupportedOperationException();
   }

   public ClassLoader getClassLoaderFor(ObjectName name)
   {
      throw new UnsupportedOperationException();
   }
   // JMXClientConnector implementation -------------------------------

   /**
    * @jmx:managed-operation
    */
   public void start(Object pServer) throws Exception
   {
      log.debug( "Starting");
      
      if( pServer == null ) {
         throw new IllegalArgumentException( "Server cannot be null. "
                                             + "To close the connection use stop()" );
      }

      InitialContext ctx = new InitialContext();
      
      try {   
         log.debug("Using Naming Context: " + ctx +
                   ", environment: " + ctx.getEnvironment() +
                   ", name in namespace: " + ctx.getNameInNamespace());
         
         // This has to be adjusted later on to reflect the given parameter
         mRemoteAdaptor = (RMIAdaptor)ctx.lookup( "jmx:" + pServer + ":rmi" );
         log.error( "Using remote adaptor: " + mRemoteAdaptor );
         mServer = pServer;

         ctx.close();
      }
      finally {
         ctx.close();
      }
   }

   /**
    * @jmx:managed-operation
    */
   public void stop() {
      log.debug( "Stopping");
      
      // First go through all the reistered listeners and remove them
      if( mRemoteAdaptor != null ) {
         // Loop through all the listeners and remove them
         Iterator i = mListeners.iterator();
         while( i.hasNext() ) {
            ClientNotificationListener lListener = (ClientNotificationListener) i.next();
            try {
               lListener.removeNotificationListener( this );
            }
            catch( Exception ignore ) {}

            i.remove();
         }
      }
      
      mRemoteAdaptor = null;
      mServer = "";
   }
   
   public boolean isAlive() {
      return mRemoteAdaptor != null;
   }

   public String getServerDescription() {
      return String.valueOf(mServer);
   }   
}   

