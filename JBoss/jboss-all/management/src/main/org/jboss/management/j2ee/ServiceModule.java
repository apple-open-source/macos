/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee;

import java.net.URL;
import java.security.InvalidParameterException;
import java.util.ArrayList;
import java.util.Hashtable;
import java.util.List;

import javax.management.MalformedObjectNameException;
import javax.management.MBeanServer;
import javax.management.Notification;
import javax.management.ObjectName;

import org.jboss.logging.Logger;

/** Root class of the JBoss JSR-77 implementation of ServiceModule model.
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.4.2.5 $
 *
 * @jmx:mbean extends="org.jboss.management.j2ee.EventProvider, org.jboss.management.j2ee.J2EEModuleMBean"
 */
public class ServiceModule
      extends J2EEModule
      implements ServiceModuleMBean
{
   // Constants -----------------------------------------------------
   public static final String J2EE_TYPE = "ServiceModule";
   private static final String[] eventTypes = {StateManagement.CREATED_EVENT,
      StateManagement.DESTROYED_EVENT};

   // Attributes ----------------------------------------------------
   private static Logger log = Logger.getLogger(ServiceModule.class);

   private List mbeans = new ArrayList();

   // Static --------------------------------------------------------

   /**
    *
    * @param pServer
    * @param pName
    * @param pURL
    * @return
    */
   public static ObjectName create(MBeanServer mbeanServer, String moduleName, URL url)
   {
      String lDD = null;
      ObjectName jsr77Name = null;
      ObjectName j2eeServerName = J2EEDomain.getDomainServerName(mbeanServer);
      // First get the deployement descriptor
      lDD = J2EEDeployedObject.getDeploymentDescriptor(url, J2EEDeployedObject.APPLICATION);

      try
      {
         // Get JVM of the j2eeServer
         ObjectName[] jvms = (ObjectName[]) mbeanServer.getAttribute(
               j2eeServerName, "JavaVMs");
         // Now create the ServiceModule
         ServiceModule serviceModule = new ServiceModule(moduleName, j2eeServerName, jvms, lDD);
         jsr77Name = serviceModule.getObjectName();
         mbeanServer.registerMBean(serviceModule, jsr77Name);
         log.debug("Created JSR-77 ServiceModule, name: " + moduleName);
      }
      catch (Exception e)
      {
         log.debug("Could not create JSR-77 ServiceModule: " + moduleName, e);
      }
      return jsr77Name;
   }

   public static void destroy(MBeanServer mbeanServer, String pModuleName)
   {
      try
      {
         log.debug("destroy(), remove Service Module: " + pModuleName);
         // If Module Name already contains the JSR-77 Object Name String
         if (pModuleName.indexOf(J2EEManagedObject.TYPE + "=" + ServiceModule.J2EE_TYPE) >= 0)
         {
            J2EEManagedObject.removeObject(mbeanServer, pModuleName);
         }
         else
         {
            J2EEManagedObject.removeObject(
                  mbeanServer,
                  pModuleName,
                  J2EEDomain.getDomainName() + ":" +
                  J2EEManagedObject.TYPE + "=" + ServiceModule.J2EE_TYPE +
                  "," + "*"
            );
         }
      }
      catch (javax.management.InstanceNotFoundException infe)
      {
      }
      catch (Throwable e)
      {
         log.error("Could not destroy JSR-77 ServiceModule: " + pModuleName, e);
      }
   }

   // Constructors --------------------------------------------------

   /**
    * Constructor taking the Name of this Object
    *
    * @param moduleName the sar deployment module name
    * @param j2eeServerName the J2EEServer ObjectName parent
    * @param pDeploymentDescriptor
    *
    * @throws InvalidParameterException If the given Name is null
    **/
   public ServiceModule(String moduleName, ObjectName j2eeServerName,
      ObjectName[] jvmNames, String pDeploymentDescriptor)
      throws MalformedObjectNameException,
         InvalidParentException
   {
      super(J2EE_TYPE, moduleName, j2eeServerName, jvmNames, pDeploymentDescriptor);
   }

   // Public --------------------------------------------------------

   // ResourceAdapterodule implementation --------------------------------------

   /**
    * @jmx:managed-attribute
    **/
   public ObjectName[] getMBeans()
   {
      return (ObjectName[]) mbeans.toArray(new ObjectName[0]);
   }

   /**
    * @jmx:managed-operation
    **/
   public ObjectName getMBean(int pIndex)
   {
      if (pIndex >= 0 && pIndex < mbeans.size())
      {
         return (ObjectName) mbeans.get(pIndex);
      }
      else
      {
         return null;
      }
   }

   // J2EEManagedObjectMBean implementation -------------------------

   public void addChild(ObjectName pChild)
   {
      String lType = J2EEManagedObject.getType(pChild);
      if (MBean.J2EE_TYPE.equals(lType))
      {
         mbeans.add(pChild);
      }
   }

   public void removeChild(ObjectName pChild)
   {
      String lType = J2EEManagedObject.getType(pChild);
      if (MBean.J2EE_TYPE.equals(lType))
      {
         mbeans.remove(pChild);
      }
   }

   // javax.managment.j2ee.EventProvider implementation -------------

   public String[] getEventTypes()
   {
      return eventTypes;
   }

   public String getEventType(int index)
   {
      String type = null;
      if (index >= 0 && index < eventTypes.length)
      {
         type = eventTypes[index];
      }
      return type;
   }


   // org.jboss.ServiceMBean overrides ------------------------------------

   public void postCreation()
   {
      sendNotification(StateManagement.CREATED_EVENT, "SAR module created");
   }

   public void preDestruction()
   {
      sendNotification(StateManagement.DESTROYED_EVENT, "SAR module destroyed");
   }

   // Object overrides ---------------------------------------------------

   public String toString()
   {
      return "ServiceModule[ " + super.toString() +
            "MBeans: " + mbeans +
            " ]";
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   /**
    * @return A hashtable with the J2EE-Application and J2EE-Server as parent
    **/
   protected Hashtable getParentKeys(ObjectName pParent)
   {
      Hashtable lReturn = new Hashtable();
      Hashtable lProperties = pParent.getKeyPropertyList();
      lReturn.put(J2EEServer.J2EE_TYPE, lProperties.get("name"));

      return lReturn;
   }
}
