/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.jmx.adaptor.rmi;

import java.net.InetAddress;
import java.rmi.server.UnicastRemoteObject;
import java.rmi.RemoteException;
import java.util.HashMap;
import java.util.Set;
import java.util.Vector;
import java.util.Iterator;
import java.io.Serializable;

import javax.management.Attribute;
import javax.management.AttributeList;
import javax.management.ObjectName;
import javax.management.QueryExp;
import javax.management.ObjectInstance;
import javax.management.NotificationFilter;
import javax.management.MBeanServer;
import javax.management.MBeanInfo;

import javax.management.AttributeNotFoundException;
import javax.management.InstanceAlreadyExistsException;
import javax.management.InstanceNotFoundException;
import javax.management.IntrospectionException;
import javax.management.InvalidAttributeValueException;
import javax.management.ListenerNotFoundException;
import javax.management.MBeanException;
import javax.management.MBeanRegistrationException;
import javax.management.NotCompliantMBeanException;
import javax.management.ReflectionException;

import org.jboss.logging.Logger;
import org.jboss.net.sockets.DefaultSocketFactory;

/**
 * RMI Interface for the server side Connector which
 * is nearly the same as the MBeanServer Interface but
 * has an additional RemoteException.
 *
 * @version <tt>$Revision: 1.3.2.3 $</tt>
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <A href="mailto:andreas.schaefer@madplanet.com">Andreas &quot;Mad&quot; Schaefer</A>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author Scott.Stark@jboss.org
 */
public class RMIAdaptorImpl
   extends UnicastRemoteObject
   implements RMIAdaptor
{
   protected static Logger log = Logger.getLogger(RMIAdaptorImpl.class);

   /**
    * Reference to the MBeanServer all the methods of this Connector are
    * forwarded to
    **/
   protected MBeanServer mbeanServer;

   /** A list of the registered listener object names */
   protected Vector listenerNames = new Vector();
   /** A HashSet<RMINotificationListener, NotificationListenerDelegate> for the
    registered listeners */
   protected HashMap remoteListeners = new HashMap();

   public RMIAdaptorImpl(MBeanServer pServer) throws RemoteException
   {
      super();
      mbeanServer = pServer;
   }
   public RMIAdaptorImpl(MBeanServer server, int port, InetAddress bindAddress,
      int backlog)
      throws RemoteException
   {
      super(port, null, new DefaultSocketFactory(bindAddress, backlog));
      this.mbeanServer = server;
   }

   // RMIAdaptor implementation -------------------------------------

   public Object instantiate(String className)
      throws ReflectionException, MBeanException, RemoteException
   {
      return mbeanServer.instantiate(className);
   }
   
   public Object instantiate(String className, ObjectName loaderName) 
      throws ReflectionException, MBeanException, InstanceNotFoundException, RemoteException
   {
      return mbeanServer.instantiate(className, loaderName);
   }
   
   public Object instantiate(String className, Object[] params, String[] signature)
      throws ReflectionException, MBeanException, RemoteException
   {
      return mbeanServer.instantiate(className, params, signature);      
   }

   public Object instantiate(String className,
                             ObjectName loaderName,
                             Object[] params,
                             String[] signature)
      throws ReflectionException, MBeanException, InstanceNotFoundException, RemoteException
   {
      return mbeanServer.instantiate(className, loaderName, params, signature);
   }
   
   public ObjectInstance createMBean(String pClassName, ObjectName pName)
      throws ReflectionException,
             InstanceAlreadyExistsException,
             MBeanRegistrationException,
             MBeanException,
             NotCompliantMBeanException,
             RemoteException
   {
      return mbeanServer.createMBean( pClassName, pName );
   }

   public ObjectInstance createMBean(String pClassName,
                                     ObjectName pName,
                                     ObjectName pLoaderName)
      throws ReflectionException,
             InstanceAlreadyExistsException,
             MBeanRegistrationException,
             MBeanException,
             NotCompliantMBeanException,
             InstanceNotFoundException,
             RemoteException
   {
      return mbeanServer.createMBean( pClassName, pName, pLoaderName );
   }

   public ObjectInstance createMBean(String pClassName,
                                     ObjectName pName,
                                     Object[] pParams,
                                     String[] pSignature)
      throws ReflectionException,
             InstanceAlreadyExistsException,
             MBeanRegistrationException,
             MBeanException,
             NotCompliantMBeanException,
             RemoteException
   {
      return mbeanServer.createMBean( pClassName, pName, pParams, pSignature );
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
             InstanceNotFoundException,
             RemoteException
   {
      return mbeanServer.createMBean( pClassName, pName, pLoaderName, pParams, pSignature );
   }

   public ObjectInstance registerMBean(Object object, ObjectName name) 
      throws InstanceAlreadyExistsException,
             MBeanRegistrationException,
             NotCompliantMBeanException,
             RemoteException
   {
      return mbeanServer.registerMBean(object, name);
   }
   
   public void unregisterMBean(ObjectName pName)
      throws InstanceNotFoundException,
             MBeanRegistrationException,
             RemoteException
   {
      mbeanServer.unregisterMBean( pName );
   }

   public ObjectInstance getObjectInstance(ObjectName pName)
      throws InstanceNotFoundException,
             RemoteException
   {
      return mbeanServer.getObjectInstance( pName );
   }

   public Set queryMBeans(ObjectName pName, QueryExp pQuery)
      throws RemoteException
   {
      return mbeanServer.queryMBeans( pName, pQuery );
   }

   public Set queryNames(ObjectName pName, QueryExp pQuery)
      throws RemoteException
   {
      return mbeanServer.queryNames( pName, pQuery );
   }

   public boolean isRegistered(ObjectName pName)
      throws RemoteException
   {
      return mbeanServer.isRegistered( pName );
   }

   public boolean isInstanceOf(ObjectName pName, String pClassName)
      throws InstanceNotFoundException,
             RemoteException
   {
      return mbeanServer.isInstanceOf( pName, pClassName );
   }

   public Integer getMBeanCount() throws RemoteException
   {
      return mbeanServer.getMBeanCount();
   }

   public Object getAttribute(ObjectName pName, String pAttribute)
      throws MBeanException,
             AttributeNotFoundException,
             InstanceNotFoundException,
             ReflectionException,
             RemoteException
   {
      return mbeanServer.getAttribute( pName, pAttribute );
   }

   public AttributeList getAttributes(ObjectName name, String[] attributes)
      throws InstanceNotFoundException,
             ReflectionException,
             RemoteException
   {
      // Filter out any non-Serializable attributes
      AttributeList attrs = mbeanServer.getAttributes(name, attributes);
      Iterator iter = attrs.iterator();
      while( iter.hasNext() )
      {
         Attribute attr = (Attribute) iter.next();
         Object value = attr.getValue();
         if( (value instanceof Serializable) == false )
            iter.remove();
      }
      return attrs;
   }

   public void setAttribute(ObjectName pName, Attribute pAttribute) 
      throws InstanceNotFoundException,
             AttributeNotFoundException,
             InvalidAttributeValueException,
             MBeanException,
             ReflectionException,
             RemoteException
   {
      mbeanServer.setAttribute( pName, pAttribute );
   }

   public AttributeList setAttributes(ObjectName pName, AttributeList pAttributes)
      throws InstanceNotFoundException,
             ReflectionException,
             RemoteException
   {
      return mbeanServer.setAttributes( pName, pAttributes );
   }

   public Object invoke(ObjectName pName,
                        String pActionName,
                        Object[] pParams,
                        String[] pSignature)
      throws InstanceNotFoundException,
             MBeanException,
             ReflectionException,
             RemoteException
   {
      return mbeanServer.invoke( pName, pActionName, pParams, pSignature );
   }

   public String getDefaultDomain() throws RemoteException
   {
      return mbeanServer.getDefaultDomain();
   }

   public MBeanInfo getMBeanInfo(ObjectName pName)
      throws InstanceNotFoundException,
             IntrospectionException,
             ReflectionException,
             RemoteException
   {
      return mbeanServer.getMBeanInfo( pName );
   }

   public void addNotificationListener(ObjectName pName,
                                       ObjectName pListener,
                                       NotificationFilter pFilter,
                                       Object pHandback)
      throws InstanceNotFoundException,
             RemoteException
   {
      mbeanServer.addNotificationListener(
         pName,
         pListener,
         pFilter,
         pHandback
         );
      listenerNames.addElement( pListener );
   }

   public void removeNotificationListener(ObjectName pName,
                                          ObjectName pListener)
      throws InstanceNotFoundException,
             ListenerNotFoundException,
             RemoteException
   {
      mbeanServer.removeNotificationListener(pName, pListener);
      listenerNames.removeElement( pListener );
   }


   public void addNotificationListener(ObjectName name,
      RMINotificationListener listener, NotificationFilter filter,
      Object handback)
      throws InstanceNotFoundException, RemoteException
   {
      NotificationListenerDelegate delegate = new NotificationListenerDelegate(listener);
      remoteListeners.put(listener, delegate);
      delegate.handleNotification(null, null);
      mbeanServer.addNotificationListener(name, delegate, filter, handback);
   }

   public void removeNotificationListener(ObjectName name,
      RMINotificationListener listener)
      throws InstanceNotFoundException, ListenerNotFoundException,
         RemoteException
   {
      NotificationListenerDelegate delegate = (NotificationListenerDelegate)
         remoteListeners.remove(listener);
      if( delegate == null )
         throw new ListenerNotFoundException("No listener matches: "+listener);
      mbeanServer.removeNotificationListener(name, delegate);
   }
}
