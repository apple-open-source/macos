/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.management.j2ee;

import java.io.Serializable;
import java.security.InvalidParameterException;
import java.util.Hashtable;
import java.util.Set;

import javax.management.InstanceNotFoundException;
import javax.management.JMException;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.Notification;

import org.jboss.logging.Logger;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.mx.util.ObjectNameConverter;
import org.jboss.management.j2ee.statistics.StatisticsProvider;

/**
 * Root class of the JBoss JSR-77 implementation of
 * {@link javax.management.j2ee.J2EEManagedObject J2EEManagedObject}.
 *
 * @author  <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @author  Scott.Stark@jboss.org
 * @version $Revision: 1.17.2.6 $
 *
 * @jmx:mbean extends="org.jboss.system.Service"
 */
public abstract class J2EEManagedObject
      extends ServiceMBeanSupport
      implements J2EEManagedObjectMBean, Serializable
{
   // Constants -----------------------------------------------------
   public static final String TYPE = "j2eeType";
   public static final String NAME = "name";

   // Attributes ----------------------------------------------------
   private static Logger classLog = Logger.getLogger(J2EEManagedObject.class);

   private ObjectName parentName = null;
   private ObjectName name = null;

   // Static --------------------------------------------------------

   /**
    * Retrieves the type out of an JSR-77 object name
    *
    * @param pName Object Name to check if null then
    *              it will be treated like NO type found
    *
    * @return The type of the given Object Name or an EMPTY
    *         string if either Object Name null or type not found
    **/
   protected static String getType(ObjectName pName)
   {
      String lType = null;
      if (pName != null)
      {
         lType = (String) pName.getKeyPropertyList().get(TYPE);
      }
      // Return an empty string if type not found
      return lType == null ? "" : lType;
   }

   protected static ObjectName removeObject(MBeanServer pServer, String pSearchCriteria)
         throws JMException
   {
      ObjectName lSearch = ObjectNameConverter.convert(pSearchCriteria);
      classLog.debug("removeObject(), search for: " + pSearchCriteria +
            ", search criteria: " + lSearch);
      Set lNames = pServer.queryNames(lSearch, null);
      if (!lNames.isEmpty())
      {
         ObjectName lName = (ObjectName) lNames.iterator().next();
         pServer.unregisterMBean(lName);
         return lName;
      }
      return null;
   }

   protected static ObjectName removeObject(MBeanServer pServer, String pName, String pSearchCriteria)
         throws JMException
   {
      String lEncryptedName = ObjectNameConverter.convertCharacters(pName, true);
      ObjectName lSearch = new ObjectName(pSearchCriteria + "," + NAME + "=" + lEncryptedName);
      classLog.debug("removeObject(), name: " + pName +
            ", encrypted name: " + lEncryptedName +
            ", search criteria: " + lSearch);
      Set lNames = pServer.queryNames(lSearch, null);
      if (!lNames.isEmpty())
      {
         ObjectName lName = (ObjectName) lNames.iterator().next();
         pServer.unregisterMBean(lName);
         return lName;
      }
      return null;
   }

   // Constructors --------------------------------------------------

   /**
    * Constructor for the root J2EEDomain object
    *
    * @param domainName domain portion to use for the JMX ObjectName
    * @param j2eeType JSR77 j2ee-type of the resource being created
    * @param resName Name of the managed resource
    *
    * @throws InvalidParameterException If the given Domain Name, Type or Name is null
    **/
   public J2EEManagedObject(String domainName, String j2eeType, String resName)
         throws MalformedObjectNameException
   {
      if (domainName == null)
      {
         throw new InvalidParameterException("Domain Name must be set");
      }
      Hashtable lProperties = new Hashtable();
      lProperties.put(TYPE, j2eeType);
      lProperties.put(NAME, resName);
      name = ObjectNameConverter.convert(domainName, lProperties);
      log.debug("ctor, name: " + name);
   }

   /**
    * Constructor for any Managed Object except the root J2EEMangement.
    *
    * @param j2eeType JSR77 j2ee-type of the resource being created
    * @param resName name of the resource
    * @param jsr77ParentName Object Name of the parent of this Managed Object
    *                which must be defined
    *
    * @throws InvalidParameterException If the given Type, Name or Parent is null
    **/
   public J2EEManagedObject(String j2eeType, String resName, ObjectName jsr77ParentName)
      throws MalformedObjectNameException,
         InvalidParentException
   {
      Hashtable lProperties = getParentKeys(jsr77ParentName);
      lProperties.put(TYPE, j2eeType);
      lProperties.put(NAME, resName);
      name = ObjectNameConverter.convert(J2EEDomain.getDomainName(), lProperties);
      setParent(jsr77ParentName);
   }

   // Public --------------------------------------------------------

   // J2EEManagedObjectMBean implementation ----------------------------------------------

   /**
    * @jmx:managed-attribute
    **/
   public ObjectName getObjectName()
   {
      return name;
   }

   /**
    * @jmx:managed-attribute
    **/
   public ObjectName getParent()
   {
      return parentName;
   }

   /**
    * @jmx:managed-attribute
    **/
   public void setParent(ObjectName pParent)
         throws
         InvalidParentException
   {
      if (pParent == null)
      {
         throw new InvalidParameterException("Parent must be set");
      }
      parentName = pParent;
   }

   /**
    * @jmx:managed-operation
    **/
   public void addChild(ObjectName pChild)
   {
   }

   /**
    * @jmx:managed-operation
    **/
   public void removeChild(ObjectName pChild)
   {
   }

   // J2EEManagedObject implementation ----------------------------------------------

   /**
    * @jmx:managed-attribute
    **/
   public boolean isStateManageable()
   {
      return this instanceof StateManageable;
   }

   /**
    * @jmx:managed-attribute
    **/
   public boolean isStatisticsProvider()
   {
      return this instanceof StatisticsProvider;
   }

   /**
    * @jmx:managed-attribute
    **/
   public boolean isEventProvider()
   {
      return this instanceof EventProvider;
   }

   // ServiceMBeanSupport overrides ---------------------------------------------------

   public ObjectName getObjectName(MBeanServer pServer, ObjectName pName)
   {
      return getObjectName();
   }

   /**
    * Last steps to be done after MBean is registered on MBeanServer. This
    * method is made final because it contains vital steps mandatory to all
    * J2EEManagedObjects. To perform your own Post-Creation steps please
    * override {@link #postCreation postCreation()} method.
    **/
   public final void postRegister(Boolean pRegistrationDone)
   {
      // This try-catch block is here because of debugging purposes because
      // runtime exception in JMX client is a awful thing to figure out
      try
      {
         log.debug("postRegister(), parent: " + parentName);
         if (pRegistrationDone.booleanValue())
         {
            // Let the subclass handle post creation steps
            postCreation();
            if (parentName != null)
            {
               try
               {
                  // Notify the parent about its new child
                  if (parentName.getKeyProperty("name").compareTo("null") != 0)
                  {
                     getServer().invoke(
                           parentName,
                           "addChild",
                           new Object[]{name},
                           new String[]{ObjectName.class.getName()}
                     );
                  }
                  else
                  {
                     ObjectName j2eeServerName = J2EEDomain.getDomainServerName(server);
                     server.invoke(j2eeServerName,
                           "addChild",
                           new Object[]{name},
                           new String[]{ObjectName.class.getName()}
                     );
                  }
                  super.postRegister(pRegistrationDone);
               }
               catch (JMException e)
               {
                  log.debug("Failed to add child", e);

                  // Stop it because of the error
                  super.postRegister(new Boolean(false));
               }
            }
         }
      }
      catch (RuntimeException re)
      {
         log.debug("postRegister() caught this exception", re);
         throw re;
      }
   }

   /**
    * Last steps to be done before MBean is unregistered on MBeanServer. This
    * method is made final because it contains vital steps mandatory to all
    * J2EEManagedObjects. To perform your own Pre-Destruction steps please
    * override {@link #preDestruction preDestruction()} method.
    **/
   public final void preDeregister()
         throws Exception
   {
      log.debug("preDeregister(), parent: " + parentName);
      // Only remove child if it is a child (root has not parent)
      if (parentName != null)
      {
         try
         {
            // Notify the parent about the removal of its child
            server.invoke(
                  parentName,
                  "removeChild",
                  new Object[]{name},
                  new String[]{ObjectName.class.getName()}
            );
         }
         catch (InstanceNotFoundException infe)
         {
         }
         preDestruction();
      }
   }

   /** An overload of the super sendNotification that only takes the event
    * type and msg. The source will be set to the managed object name, the
    * sequence will be the getNextNotificationSequenceNumber() value, and the
    * timestamp System.currentTimeMillis().
    *
    * @param type the notification event type
    * @param info the notification event msg info
    */
   public void sendNotification(String type, String info)
   {
      Notification msg = new Notification(type, this.getObjectName(),
         this.getNextNotificationSequenceNumber(),
            System.currentTimeMillis(),
            info);
      super.sendNotification(msg);
   }

   // Object overrides ---------------------------------------------------

   public String toString()
   {
      return "J2EEManagedObject [ name: " + name + ", parent: " + parentName + " ];";
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   protected void postCreation()
   {
   }

   protected void preDestruction()
   {
   }

   /**
    * This method can be overwritten by any subclass which must
    * return &lt;parent-j2eeType&gt; indicating its parents. By
    * default it returns an empty hashtable instance.
    *
    * @param pParent The direct parent of this class
    *
    * @return An empty hashtable
    **/
   protected Hashtable getParentKeys(ObjectName pParent)
   {
      return new Hashtable();
   }

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
}
