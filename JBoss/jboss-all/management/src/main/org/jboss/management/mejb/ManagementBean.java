/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.mejb;

import java.util.ArrayList;
import java.util.Set;
import java.rmi.RemoteException;
import java.io.ObjectInputStream;

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
import javax.management.JMException;
import javax.management.MBeanException;
import javax.management.MBeanInfo;
import javax.management.MBeanRegistrationException;
import javax.management.MBeanServer;
import javax.management.IntrospectionException;
import javax.management.ListenerNotFoundException;
import javax.management.NotCompliantMBeanException;
import javax.management.NotificationFilter;
import javax.management.NotificationListener;
import javax.management.ObjectName;
import javax.management.ObjectInstance;
import javax.management.QueryExp;
import javax.management.ReflectionException;
import javax.management.OperationsException;
import javax.management.j2ee.ManagementHome;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;

import org.jboss.jmx.connector.RemoteMBeanServer;
import org.jboss.logging.Logger;
import org.jboss.management.j2ee.J2EEManagedObject;
import org.jboss.management.j2ee.J2EEDomain;
import org.jboss.mx.util.MBeanServerLocator;

/**
 * Management Session Bean to enable the client to manage the
 * server its is deployed on. This is the implementation of the
 * JSR-77 specification MEJB.
 *
 * @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.6.2.4 $
 *
 * @ejb:bean name="MEJB"
 *           display-name="JBoss Management EJB (MEJB)"
 *           type="Stateless"
 *           jndi-name="ejb/mgmt/MEJB"
 * @ejb:interface extends="javax.management.j2ee.Management"
 * @ejb:home generate="none"
 *           remote-class="javax.management.j2ee.ManagementHome"
 * @ejb:env-entry description="JNDI-Name of the MBeanServer to be used to look it up. If 'null' the first of all listed local MBeanServer is taken"
 *                name="Server-Name"
 *                value="null"
 * @ejb:transaction type="Supports"
 *
 **/
public class ManagementBean
      implements SessionBean
{
   // -------------------------------------------------------------------------
   // Static
   // -------------------------------------------------------------------------
   /** Logger to log. Can be used because this EJB is specific to JBoss */
   private static Logger log = Logger.getLogger(ManagementBean.class);

   // -------------------------------------------------------------------------
   // Members
   // -------------------------------------------------------------------------

   private SessionContext mContext;

   /**
    * Reference to the MBeanServer all the methods of this Connector are
    * forwarded to
    **/
   private RemoteMBeanServer mConnector;
   /** JMX Name of the HA Management Service used in a Cluster */
   private ObjectName mManagementService;

   // -------------------------------------------------------------------------
   // Methods
   // -------------------------------------------------------------------------

   /**
    * @throws RemoteException Necessary for a EJB
    *
    * @ejb:interface-method view-type="remote"
    */
   public Object getAttribute(ObjectName pName, String pAttribute)
         throws
         MBeanException,
         AttributeNotFoundException,
         InstanceNotFoundException,
         ReflectionException,
         RemoteException
   {
      if (mManagementService == null)
      {
         return mConnector.getAttribute(pName, pAttribute);
      }
      else
      {
         return mConnector.invoke(
               mManagementService,
               "getAttribute",
               new Object[]{
                  pName,
                  pAttribute
               },
               new String[]{
                  ObjectName.class.getName(),
                  String.class.getName()
               }
         );
      }
   }

   /**
    * @throws RemoteException Necessary for a EJB
    *
    * @ejb:interface-method view-type="remote"
    */
   public AttributeList getAttributes(ObjectName pName, String[] pAttributes)
         throws
         InstanceNotFoundException,
         ReflectionException,
         RemoteException
   {
      if (mManagementService == null)
      {
         return mConnector.getAttributes(pName, pAttributes);
      }
      else
      {
         try
         {
            return (AttributeList) mConnector.invoke(
                  mManagementService,
                  "getAttributes",
                  new Object[]{
                     pName,
                     pAttributes
                  },
                  new String[]{
                     ObjectName.class.getName(),
                     String[].class.getName()
                  }
            );
         }
         catch (MBeanException me)
         {
            log.error("getAttributes() got exception from cluster service", me);
            // Should never happens
            return null;
         }
      }
   }

   /**
    * @throws RemoteException Necessary for a EJB
    *
    * @ejb:interface-method view-type="remote"
    */
   public String getDefaultDomain()
         throws RemoteException
   {
      if (mManagementService == null)
      {
         return J2EEDomain.getDomainName();
      }
      else
      {
         try
         {
            return (String) mConnector.getAttribute(
                  mManagementService,
                  "DefaultDomain"
            );
         }
         catch (JMException jme)
         {
            // Should never happen
            log.error("getDefaultDomain() got exception from cluster service", jme);
            return null;
         }
      }
   }

   /**
    * @throws RemoteException Necessary for a EJB
    *
    * @ejb:interface-method view-type="remote"
    */
   public Integer getMBeanCount()
         throws RemoteException
   {
      if (mManagementService == null)
      {
         try
         {
            Set mbeans = this.queryNames(new ObjectName(getDefaultDomain() + ":*"), null);
            return new Integer(mbeans.size());
         }
         catch (Exception e)
         {
         }
         return new Integer(0);
      }
      else
      {
         try
         {
            return (Integer) mConnector.invoke(
                  mManagementService,
                  "getMBeanCount",
                  new Object[]{},
                  new String[]{}
            );
         }
         catch (JMException jme)
         {
            log.error("getMBeanCount() got exception from cluster service", jme);
            // Should never happen
            return null;
         }
      }
   }

   /**
    * @throws RemoteException Necessary for a EJB
    *
    * @ejb:interface-method view-type="remote"
    */
   public MBeanInfo getMBeanInfo(ObjectName pName)
         throws
         IntrospectionException,
         InstanceNotFoundException,
         ReflectionException,
         RemoteException
   {
      if (mManagementService == null)
      {
         return mConnector.getMBeanInfo(pName);
      }
      else
      {
         try
         {
            return (MBeanInfo) mConnector.invoke(
                  mManagementService,
                  "getMBeanInfo",
                  new Object[]{
                     pName
                  },
                  new String[]{
                     ObjectName.class.getName()
                  }
            );
         }
         catch (MBeanException me)
         {
            log.error("getMBeanInfo() got exception from cluster service", me);
            // Should never happen
            return null;
         }
      }
   }

   /**
    * @throws RemoteException Necessary for a EJB
    *
    * @ejb:interface-method view-type="remote"
    */
   public javax.management.j2ee.ListenerRegistration getListenerRegistry()
         throws RemoteException
   {
      return new ListenerRegistration(
            (ManagementHome) mContext.getEJBObject().getEJBHome(),
            new String[]{}
      );
   }

   /**
    * @throws RemoteException Necessary for a EJB
    *
    * @ejb:interface-method view-type="remote"
    */
   public Object invoke(ObjectName pName, String pOperationName, Object[] pParams, String[] pSignature)
         throws
         InstanceNotFoundException,
         MBeanException,
         ReflectionException,
         RemoteException
   {
      // Convert start(), startRecursive() and stop() to the
      // internal methods: mejbStart(), mejbStartRecursive() and mejbStop
      if (pOperationName.equals("start"))
      {
         pOperationName = "mejbStart";
      }
      else if (pOperationName.equals("startRecursive"))
      {
         pOperationName = "mejbStartRecursive";
      }
      else if (pOperationName.equals("stop"))
      {
         pOperationName = "mejbStop";
      }
      if (mManagementService == null)
      {
         return mConnector.invoke(
               pName,
               pOperationName,
               pParams,
               pSignature
         );
      }
      else
      {
         return mConnector.invoke(
               mManagementService,
               "invoke",
               new Object[]{
                  pName,
                  pOperationName,
                  pParams,
                  pSignature
               },
               new String[]{
                  ObjectName.class.getName(),
                  String.class.getName(),
                  Object[].class.getName(),
                  String[].class.getName()
               }
         );
      }
   }

   /**
    * @throws RemoteException Necessary for a EJB
    *
    * @ejb:interface-method view-type="remote"
    */
   public boolean isRegistered(ObjectName pName)
         throws RemoteException
   {
      if (mManagementService == null)
      {
         return mConnector.isRegistered(pName);
      }
      else
      {
         try
         {
            Boolean lCheck = (Boolean) mConnector.invoke(
                  mManagementService,
                  "isRegistered",
                  new Object[]{
                     pName
                  },
                  new String[]{
                     ObjectName.class.getName()
                  }
            );
            if (lCheck != null)
            {
               return lCheck.booleanValue();
            }
         }
         catch (JMException jme)
         {
            log.error("isRegistered() got exception from cluster service", jme);
            // Should never happen
         }
         return false;
      }
   }

   /**
    * @throws RemoteException Necessary for a EJB
    *
    * @ejb:interface-method view-type="remote"
    **/
   public Set queryNames(ObjectName pName, QueryExp pQuery)
         throws RemoteException
   {
      if (mManagementService == null)
      {
         return mConnector.queryNames(pName, pQuery);
      }
      else
      {
         try
         {
            return (Set) mConnector.invoke(
                  mManagementService,
                  "queryNames",
                  new Object[]{
                     pName,
                     pQuery
                  },
                  new String[]{
                     ObjectName.class.getName(),
                     QueryExp.class.getName()
                  }
            );
         }
         catch (JMException jme)
         {
            log.error("queryNames() got exception from cluster service", jme);
            // Should never happen
            return null;
         }
      }
   }

   /**
    * @throws RemoteException Necessary for a EJB
    *
    * @ejb:interface-method view-type="remote"
    */
   public void setAttribute(ObjectName pName, Attribute pAttribute)
         throws
         AttributeNotFoundException,
         InstanceNotFoundException,
         InvalidAttributeValueException,
         MBeanException,
         ReflectionException,
         RemoteException
   {
      if (mManagementService == null)
      {
         mConnector.setAttribute(pName, pAttribute);
      }
      else
      {
         mConnector.invoke(
               mManagementService,
               "setAttribute",
               new Object[]{
                  pName,
                  pAttribute
               },
               new String[]{
                  ObjectName.class.getName(),
                  String.class.getName()
               }
         );
      }
   }

   /**
    * @throws RemoteException Necessary for a EJB
    *
    * @ejb:interface-method view-type="remote"
    */
   public AttributeList setAttributes(ObjectName pName, AttributeList pAttributes)
         throws
         InstanceNotFoundException,
         ReflectionException,
         RemoteException
   {
      if (mManagementService == null)
      {
         return setAttributes(pName, pAttributes);
      }
      else
      {
         try
         {
            return (AttributeList) mConnector.invoke(
                  mManagementService,
                  "setAttributes",
                  new Object[]{
                     pName,
                     pAttributes
                  },
                  new String[]{
                     ObjectName.class.getName(),
                     AttributeList.class.getName()
                  }
            );
         }
         catch (MBeanException me)
         {
            log.error("setAttributes() got exception from cluster service", me);
            // Should never happen
            return null;
         }
      }
   }

   /**
    * @throws RemoteException Necessary for a EJB
    *
    * @ejb:interface-method view-type="remote"
    */
   public ObjectInstance createMBean(
         String pClass,
         ObjectName pName,
         Object[] pParameters,
         String[] pSignature
         )
         throws
         InstanceAlreadyExistsException,
         MBeanException,
         MBeanRegistrationException,
         NotCompliantMBeanException,
         ReflectionException,
         RemoteException
   {
      if (mManagementService == null)
      {
         return mConnector.createMBean(pClass, pName, pParameters, pSignature);
      }
      else
      {
         try
         {
            return (ObjectInstance) mConnector.invoke(
                  mManagementService,
                  "createMBean",
                  new Object[]{
                     pClass,
                     pName,
                     pParameters,
                     pSignature
                  },
                  new String[]{
                     String.class.getName(),
                     ObjectName.class.getName(),
                     Object[].class.getName(),
                     String[].class.getName()
                  }
            );
         }
         catch (InstanceNotFoundException infe)
         {
            log.error("createMBean() got exception from cluster service", infe);
            // Should never happen
            return null;
         }
      }
   }

   /**
    * @throws RemoteException Necessary for a EJB
    *
    * @ejb:interface-method view-type="remote"
    */
   public void unregisterMBean(
         ObjectName pName
         )
         throws
         InstanceNotFoundException,
         MBeanRegistrationException,
         RemoteException
   {
      if (mManagementService == null)
      {
         mConnector.unregisterMBean(pName);
      }
      else
      {
         try
         {
            mConnector.invoke(
                  mManagementService,
                  "unregisterMBean",
                  new Object[]{
                     pName
                  },
                  new String[]{
                     ObjectName.class.getName()
                  }
            );
         }
               // Should never happen
         catch (MBeanException me)
         {
            log.error("unregisterMBean() got exception from cluster service", me);
         }
         catch (ReflectionException re)
         {
            log.error("unregisterMBean() got exception from cluster service", re);
         }
      }
   }

   /**
    * @throws RemoteException Necessary for a EJB
    *
    * @ejb:interface-method view-type="remote"
    */
   public void addNotificationListener(
         ObjectName pBroadcaster,
         ObjectName pListener,
         NotificationFilter pFilter,
         Object pHandback
         )
         throws
         InstanceNotFoundException,
         RemoteException
   {
      if (mManagementService == null)
      {
         mConnector.addNotificationListener(pBroadcaster, pListener, pFilter, pHandback);
      }
      else
      {
         try
         {
            mConnector.invoke(
                  mManagementService,
                  "addNotificationListener",
                  new Object[]{
                     pBroadcaster,
                     pListener,
                     pFilter,
                     pHandback
                  },
                  new String[]{
                     ObjectName.class.getName(),
                     ObjectName.class.getName(),
                     NotificationFilter.class.getName(),
                     Object.class.getName()
                  }
            );
         }
               // Should never happen
         catch (MBeanException me)
         {
            log.error("addNotificationListener() got exception from cluster service", me);
         }
         catch (ReflectionException re)
         {
            log.error("addNotificationListener() got exception from cluster service", re);
         }
      }
   }

   /**
    * @throws RemoteException Necessary for a EJB
    *
    * @ejb:interface-method view-type="remote"
    */
   public void removeNotificationListener(
         ObjectName pBroadcaster,
         ObjectName pListener
         )
         throws
         InstanceNotFoundException,
         ListenerNotFoundException,
         RemoteException
   {
      if (mManagementService == null)
      {
         mConnector.removeNotificationListener(pBroadcaster, pListener);
      }
      else
      {
         try
         {
            mConnector.invoke(
                  mManagementService,
                  "removeNotificationListener",
                  new Object[]{
                     pBroadcaster,
                     pListener
                  },
                  new String[]{
                     ObjectName.class.getName(),
                     ObjectName.class.getName()
                  }
            );
         }
               // Should never happen
         catch (MBeanException me)
         {
            log.error("removeNotificationListener() got exception from cluster service", me);
         }
         catch (ReflectionException re)
         {
            log.error("removeNotificationListener() got exception from cluster service", re);
         }
      }
   }

   /**
    * Create the Session Bean which takes the first available
    * MBeanServer as target server
    *
    * @throws CreateException
    *
    * @ejb:create-method
    */
   public void ejbCreate()
         throws
         CreateException
   {
      if (mConnector == null)
      {
         try
         {
            Context jndiCtx = new InitialContext();
            String serverName = (String) jndiCtx.lookup("java:comp/env/Server-Name");
            serverName = serverName.trim();
            if (serverName == null || serverName.length() == 0 || serverName.equals("null"))
            {
               try
               {
                  mConnector = new LocalConnector(MBeanServerLocator.locateJBoss());
               }
               catch (IllegalStateException e)
               {
                  throw new CreateException("No local JMX MBeanServer available");
               }
            }
            else
            {
               Object lServer = jndiCtx.lookup(serverName);
               if (lServer != null)
               {
                  if (lServer instanceof MBeanServer)
                  {
                     mConnector = new LocalConnector((MBeanServer) lServer);
                  }
                  else if (lServer instanceof RemoteMBeanServer)
                  {
                     mConnector = (RemoteMBeanServer) lServer;
                  }
                  else
                  {
                     throw new CreateException(
                           "Server: " + lServer + " reference by Server-Name: " + serverName +
                           " is not of type MBeanServer or RemoteMBeanServer: "
                     );
                  }
               }
               else
               {
                  throw new CreateException("Server-Name " + serverName
                        + " does not reference an Object in JNDI");
               }
            }
         }
         catch (NamingException ne)
         {
            throw new EJBException(ne);
         }
      }

      // Check to see if a HA-Management Service is available to be used
      try
      {
         ObjectName haManagement = new ObjectName("jboss:service=HAManagement");
         ObjectInstance oi = mConnector.getObjectInstance(haManagement);
         mManagementService = oi.getObjectName();
      }
      catch (Exception e)
      {
         log.debug("ejbCreate() failed to locate jboss:service=HAManagement", e);
      }
   }

   /**
    * Describes the instance and its content for debugging purpose
    *
    * @return Debugging information about the instance and its content
    */
   public String toString()
   {
      return "Management [ " + " ]";
   }

   // -------------------------------------------------------------------------
   // Framework Callbacks
   // -------------------------------------------------------------------------

   /**
    * Set the associated session context. The container invokes this method on
    * an instance after the instance has been created.
    * <p>This method is called with no transaction context.
    *
    * @param aContext A SessionContext interface for the instance. The instance
    *  should store the reference to the context in an instance variable.
    * @throws EJBException Should something go wrong while seting the context,
    *  an EJBException will be thrown.
    */
   public void setSessionContext(SessionContext aContext)
         throws
         EJBException
   {
      mContext = aContext;
   }


   /**
    * The activate method is called when the instance is activated from its
    * "passive" state. The instance should acquire any resource that it has
    * released earlier in the ejbPassivate() method.
    * <p>This method is called with no transaction context.
    *
    * @throws EJBException Thrown by the method to indicate a failure caused
    *  by a system-level error
    **/
   public void ejbActivate()
         throws
         EJBException
   {
   }


   /**
    * The passivate method is called before the instance enters the "passive"
    * state. The instance should release any resources that it can re-acquire
    * later in the ejbActivate() method.
    * <p>After the passivate method completes, the instance must be in a state
    * that allows the container to use the Java Serialization protocol to
    * externalize and store away the instance's state.
    * <p>This method is called with no transaction context.
    *
    * @throws EJBException Thrown by the method to indicate a failure caused
    *  by a system-level error
    */
   public void ejbPassivate()
         throws
         EJBException
   {
   }


   /**
    * A container invokes this method before it ends the life of the session
    * object. This happens as a result of a client's invoking a remove
    * operation, or when a container decides to terminate the session object
    * after a timeout.
    * <p>This method is called with no transaction context.
    *
    * @throws EJBException Thrown by the method to indicate a failure caused
    *  by a system-level error
    */
   public void ejbRemove()
         throws
         EJBException
   {
   }

   private class LocalConnector implements RemoteMBeanServer
   {
      private MBeanServer mServer = null;

      public LocalConnector(MBeanServer pServer)
      {
         mServer = pServer;
      }

      public ObjectInstance createMBean(
            String pClassName,
            ObjectName pName
            ) throws
            ReflectionException,
            InstanceAlreadyExistsException,
            MBeanRegistrationException,
            MBeanException,
            NotCompliantMBeanException
      {
         return mServer.createMBean(pClassName, pName);
      }

      public ObjectInstance createMBean(
            String pClassName,
            ObjectName pName,
            ObjectName pLoaderName
            ) throws
            ReflectionException,
            InstanceAlreadyExistsException,
            MBeanRegistrationException,
            MBeanException,
            NotCompliantMBeanException,
            InstanceNotFoundException
      {
         return mServer.createMBean(pClassName, pName, pLoaderName);
      }

      public ObjectInstance createMBean(
            String pClassName,
            ObjectName pName,
            Object[] pParams,
            String[] pSignature
            ) throws
            ReflectionException,
            InstanceAlreadyExistsException,
            MBeanRegistrationException,
            MBeanException,
            NotCompliantMBeanException
      {
         return mServer.createMBean(pClassName, pName, pParams, pSignature);
      }

      public ObjectInstance createMBean(
            String pClassName,
            ObjectName pName,
            ObjectName pLoaderName,
            Object[] pParams,
            String[] pSignature
            ) throws
            ReflectionException,
            InstanceAlreadyExistsException,
            MBeanRegistrationException,
            MBeanException,
            NotCompliantMBeanException,
            InstanceNotFoundException
      {
         return mServer.createMBean(pClassName, pName, pLoaderName, pParams, pSignature);
      }

      public void unregisterMBean(
            ObjectName pName
            ) throws
            InstanceNotFoundException,
            MBeanRegistrationException
      {
         mServer.unregisterMBean(pName);
      }

      public ObjectInstance getObjectInstance(
            ObjectName pName
            ) throws
            InstanceNotFoundException
      {
         return mServer.getObjectInstance(pName);
      }

      public Set queryMBeans(
            ObjectName pName,
            QueryExp pQuery
            )
      {
         return mServer.queryMBeans(pName, pQuery);
      }

      public Set queryNames(
            ObjectName pName,
            QueryExp pQuery
            )
      {
         return mServer.queryNames(pName, pQuery);
      }

      public boolean isRegistered(
            ObjectName pName
            )
      {
         return mServer.isRegistered(pName);
      }

      public boolean isInstanceOf(
            ObjectName pName,
            String pClassName
            ) throws
            InstanceNotFoundException
      {
         return mServer.isInstanceOf(pName, pClassName);
      }

      public Integer getMBeanCount(
            )
      {
         return mServer.getMBeanCount();
      }

      public Object getAttribute(
            ObjectName pName,
            String pAttribute
            ) throws
            MBeanException,
            AttributeNotFoundException,
            InstanceNotFoundException,
            ReflectionException
      {
         return mServer.getAttribute(pName, pAttribute);
      }

      public AttributeList getAttributes(
            ObjectName pName,
            String[] pAttributes
            ) throws
            InstanceNotFoundException,
            ReflectionException
      {
         return mServer.getAttributes(pName, pAttributes);
      }

      public void setAttribute(
            ObjectName pName,
            Attribute pAttribute
            ) throws
            InstanceNotFoundException,
            AttributeNotFoundException,
            InvalidAttributeValueException,
            MBeanException,
            ReflectionException
      {
         mServer.setAttribute(pName, pAttribute);
      }

      public AttributeList setAttributes(
            ObjectName pName,
            AttributeList pAttributes
            ) throws
            InstanceNotFoundException,
            ReflectionException
      {
         return mServer.setAttributes(pName, pAttributes);
      }

      public Object invoke(
            ObjectName pName,
            String pActionName,
            Object[] pParams,
            String[] pSignature
            ) throws
            InstanceNotFoundException,
            MBeanException,
            ReflectionException
      {
         return mServer.invoke(pName, pActionName, pParams, pSignature);
      }

      public String getDefaultDomain(
            )
      {
         return mServer.getDefaultDomain();
      }

      public MBeanInfo getMBeanInfo(
            ObjectName pName
            ) throws
            InstanceNotFoundException,
            IntrospectionException,
            ReflectionException
      {
         return mServer.getMBeanInfo(pName);
      }

      public void addNotificationListener(
            ObjectName pName,
            NotificationListener pListener,
            NotificationFilter pFilter,
            Object pHandback
            ) throws
            InstanceNotFoundException
      {
         mServer.addNotificationListener(pName, pListener, pFilter, pHandback);
      }

      public void removeNotificationListener(
            ObjectName pName,
            NotificationListener pListener
            ) throws
            InstanceNotFoundException,
            ListenerNotFoundException
      {
         mServer.removeNotificationListener(pName, pListener);
      }

      public void addNotificationListener(
            ObjectName pName,
            ObjectName pListener,
            NotificationFilter pFilter,
            Object pHandback
            ) throws
            InstanceNotFoundException
      {
         mServer.addNotificationListener(pName, pListener, pFilter, pHandback);
      }

      public void removeNotificationListener(
            ObjectName pName,
            ObjectName pListener
            ) throws
            InstanceNotFoundException,
            ListenerNotFoundException,
            UnsupportedOperationException
      {
         mServer.removeNotificationListener(pName, pListener);
      }

      public ObjectInstance registerMBean(Object object, ObjectName name)
            throws InstanceAlreadyExistsException, MBeanRegistrationException,
            NotCompliantMBeanException
      {
         return null;
      }

      public ObjectInputStream deserialize(ObjectName name, byte[] data)
            throws InstanceNotFoundException, OperationsException
      {
         return null;
      }

      public ObjectInputStream deserialize(String className, byte[] data)
            throws OperationsException, ReflectionException
      {
         return null;
      }

      public ObjectInputStream deserialize(String className, ObjectName loaderName, byte[] data)
            throws InstanceNotFoundException, OperationsException, ReflectionException
      {
         return null;
      }

      public Object instantiate(String className)
            throws ReflectionException, MBeanException
      {
         return null;
      }

      public Object instantiate(String className, ObjectName loaderName)
            throws ReflectionException, MBeanException, InstanceNotFoundException
      {
         return null;
      }

      public Object instantiate(String className, Object[] params, String[] signature)
            throws ReflectionException, MBeanException
      {
         return null;
      }

      public Object instantiate(String className, ObjectName loaderName,
                                Object[] params, String[] signature)
            throws ReflectionException, MBeanException, InstanceNotFoundException
      {
         return null;
      }

   }
}
