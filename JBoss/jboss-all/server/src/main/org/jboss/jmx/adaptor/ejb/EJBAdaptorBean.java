/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.jmx.adaptor.ejb;

import java.rmi.RemoteException;
import java.util.Set;
import java.util.Vector;
import java.util.HashMap;
import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;

import javax.management.Attribute;
import javax.management.AttributeList;
import javax.management.AttributeNotFoundException;
import javax.management.InvalidAttributeValueException;
import javax.management.InstanceNotFoundException;
import javax.management.InstanceAlreadyExistsException;
import javax.management.MBeanException;
import javax.management.MBeanInfo;
import javax.management.MBeanRegistrationException;
import javax.management.MBeanServer;
import javax.management.IntrospectionException;
import javax.management.ListenerNotFoundException;
import javax.management.NotCompliantMBeanException;
import javax.management.NotificationFilter;
import javax.management.ObjectName;
import javax.management.ObjectInstance;
import javax.management.QueryExp;
import javax.management.ReflectionException;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;

import org.jboss.jmx.connector.RemoteMBeanServer;
import org.jboss.jmx.adaptor.rmi.RMINotificationListener;
import org.jboss.jmx.adaptor.rmi.NotificationListenerDelegate;
import org.jboss.logging.Logger;
import org.jboss.mx.util.MBeanServerLocator;

/**
 * JMX EJB-Adaptor allowing a EJB client to work on a remote
 * MBean Server.
 *
 * @ejb:bean type="Stateless"
 *           name="jmx/ejb/Adaptor"
 *           jndi-name="ejb/jmx/ejb/Adaptor"
 *           remote-business-interface="org.jboss.jmx.adaptor.rmi.RMIAdaptor"
 * @ejb:env-entry description="JNDI-Name of the MBeanServer to be used to look it up. If 'null' the first of all listed local MBeanServer is taken"
 *                name="Server-Name"
 *                value="null"
 * @ejb:transaction type="Supports"
 *
 * @version <tt>$Revision: 1.9.2.3 $</tt>
 * @author  Andreas Schaefer
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class EJBAdaptorBean
   implements SessionBean
{
   private Logger log = Logger.getLogger(this.getClass());
   
   /** The EJB session context. */
   private SessionContext mContext;

   /** The mbean server which we will delegate the work too. */
   private MBeanServer mbeanServer;

   /** A list of the registered listener object names */
   private Vector listenerNames = new Vector();
   /** A HashSet<RMINotificationListener, NotificationListenerDelegate> for the
    registered listeners */
   protected HashMap remoteListeners = new HashMap();

   /**
    * Create the Session Bean which takes the first available
    * MBeanServer as target server
    *
    * @throws CreateException 
    *
    * @ejb:create-method
    **/
   public void ejbCreate() throws CreateException
   {
      if (mbeanServer != null ) {
         return;
      }
      
      try {
         Context ctx = new InitialContext();
         
         String lServerName = ((String)ctx.lookup("java:comp/env/Server-Name")).trim();
         
         if( lServerName == null || lServerName.length() == 0 || lServerName.equals( "null" ) ) {
            mbeanServer = MBeanServerLocator.locateJBoss();
            if (mbeanServer == null) {
               throw new CreateException("No local JMX MBeanServer available");
            }
         }
         else {
            Object lServer = ctx.lookup( lServerName );
            
            if( lServer != null ) {
               if( lServer instanceof MBeanServer ) {
                  mbeanServer = (MBeanServer)lServer;
               }
               else {
                  if( lServer instanceof RemoteMBeanServer ) {
                     mbeanServer = (RemoteMBeanServer) lServer;
                  }
                  else {
                     throw new CreateException(
                        "Server: " + lServer + " reference by Server-Name: " + lServerName +
                        " is not of type MBeanServer or RemoteMBeanServer: "
                     );
                  }
               }
            }
            else {
               throw new CreateException(
                  "Server-Name " + lServerName + " does not reference an Object in JNDI"
                  );
            }
         }
         
         ctx.close();
      }
      catch( NamingException ne ) {
         throw new EJBException( ne );
      }
   }
   
   // -------------------------------------------------------------------------
   // EJB Framework Callbacks
   // -------------------------------------------------------------------------  
   
   public void setSessionContext( SessionContext aContext )
      throws EJBException
   {
      mContext = aContext;
   }
   
   public void ejbActivate() throws EJBException
   {
      // empty
   }

   public void ejbPassivate() throws EJBException
   {
      // empty
   }
   
   public void ejbRemove() throws EJBException
   {
      // empty
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

   public AttributeList getAttributes(ObjectName pName, String[] pAttributes)
      throws InstanceNotFoundException,
             ReflectionException,
             RemoteException
   {
      return mbeanServer.getAttributes( pName, pAttributes );
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

   public MBeanInfo getMBeanInfo(ObjectName pName)
      throws InstanceNotFoundException,
             IntrospectionException,
             ReflectionException,
             RemoteException
   {
      return mbeanServer.getMBeanInfo( pName );
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
