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
import java.util.HashMap;
import java.util.Hashtable;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.management.MalformedObjectNameException;
import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.jboss.logging.Logger;

/**
 * Root class of the JBoss JSR-77 implementation of
 * {@link javax.management.j2ee.ResourceAdapterModule ResourceAdapterModule}.
 *
 * @author  <a href="mailto:mclaugs@comcast.net">Scott McLaughlin</a>.
 * @version $Revision: 1.9.2.6 $
 *
 * @jmx:mbean extends="org.jboss.management.j2ee.J2EEModuleMBean"
 **/
public class ResourceAdapterModule
      extends J2EEModule
      implements ResourceAdapterModuleMBean
{

   // Constants -----------------------------------------------------

   public static final String J2EE_TYPE = "ResourceAdapterModule";

   // Attributes ----------------------------------------------------
   private static Logger log = Logger.getLogger(ResourceAdapterModule.class);

   private List resourceAdapters = new ArrayList();
   /** The JSR77 ObjectNames of fake J2EEApplications created by standalone jars */
   private static final Map fakeJ2EEApps = new HashMap();

   // Static --------------------------------------------------------

   /**
    * Creates the JSR-77 EJBModule
    *
    * @param mbeanServer MBeanServer the EJBModule is created on
    * @param earName the ear name unless null which indicates a standalone module (no EAR)
    * @param rarName the RAR name
    * @param pURL URL path to the local deployment of the module (where to find the DD file)
    * @return the JSR77 ObjectName of the RARModule
    */
   public static ObjectName create(MBeanServer mbeanServer, String earName,
         String rarName, URL pURL)
   {
      String lDD = null;
      ObjectName lParent = null;
      ObjectName lCreated = null;
      ObjectName jsr77Name = null;
      // Get the J2EEServer name
      ObjectName j2eeServerName = J2EEDomain.getDomainServerName(mbeanServer);

      try
      {
         Hashtable props = j2eeServerName.getKeyPropertyList();
         String j2eeServer = props.get(J2EEManagedObject.TYPE) + "=" +
               props.get("name");

         // if pName is equal to pApplicationName then we have
         // a stand alone Module so do not create a J2EEApplication
         if (earName == null)
         {
            // If there is no ear use the J2EEServer as the parent
            lParent = j2eeServerName;
         }
         else
         {
            ObjectName parentAppQuery = new ObjectName(
                  J2EEDomain.getDomainName() + ":" +
                  J2EEManagedObject.TYPE + "=" + J2EEApplication.J2EE_TYPE + "," +
                  "name=" + earName + "," +
                  j2eeServer + "," +
                  "*"
            );
            Set parentApps = mbeanServer.queryNames(parentAppQuery, null);

            if (parentApps.size() == 0)
            {
               lCreated = J2EEApplication.create(
                     mbeanServer,
                     earName,
                     null
               );
               lParent = lCreated;
            } // end of if ()
            else if (parentApps.size() == 1)
            {
               lParent = (ObjectName) parentApps.iterator().next();
            } // end of if ()
         }

         // Get the J2EE deployement descriptor
         lDD = J2EEDeployedObject.getDeploymentDescriptor(pURL, J2EEDeployedObject.RAR);
      }
      catch (Exception e)
      {
         log.debug("Could not create JSR-77 ResourceAdapterModule: " + rarName, e);
         return null;
      }

      try
      {
         // Get JVM of the j2eeServer
         ObjectName[] jvms = (ObjectName[]) mbeanServer.getAttribute(
               j2eeServerName,
               "JavaVMs"
         );

         // Now create the ResourceAdapterModule
         ResourceAdapterModule rarModule = new ResourceAdapterModule(rarName,
               lParent, jvms, lDD);
         jsr77Name = rarModule.getObjectName();
         mbeanServer.registerMBean(rarModule, jsr77Name);

         if (lCreated != null)
         {
            fakeJ2EEApps.put(jsr77Name, lCreated);
         }
         log.debug("Created JSR-77 EJBModule: " + jsr77Name);
      }
      catch (Exception e)
      {
         log.debug("Could not create JSR-77 ResourceAdapterModule: " + rarName, e);
      }
      return jsr77Name;
   }

   /**
    * Destroyes the given JSR-77 RARModule
    *
    * @param mbeanServer The JMX MBeanServer the desired RARModule is registered on
    * @param jsr77Name the JSR77 RARModule component ObjectName
    */
   public static void destroy(MBeanServer mbeanServer, ObjectName jsr77Name)
   {
      try
      {
         log.info("destroy(), remove RARModule: " + jsr77Name);
         mbeanServer.unregisterMBean(jsr77Name);

         ObjectName jsr77ParentName = (ObjectName) fakeJ2EEApps.get(jsr77Name);
         if (jsr77ParentName != null)
         {
            log.debug("Remove fake JSR-77 parent Application: " + jsr77ParentName);
            J2EEApplication.destroy(mbeanServer, jsr77ParentName);
         }
      }
      catch (Exception e)
      {
         log.debug("Could not destroy JSR-77 RARModule: " + jsr77Name, e);
      }
   }

   // Constructors --------------------------------------------------

   /**
    * Constructor taking the Name of this Object
    *
    * @param rarName Name to be set which must not be null
    * @param jsr77ParentName ObjectName of the Parent this Module belongs
    *                too. Either it is a J2EEApplication or J2EEServer
    *                if a standalone module.
    * @param pDeploymentDescriptor
    *
    * @throws InvalidParameterException If the given Name is null
    */
   public ResourceAdapterModule(String rarName, ObjectName jsr77ParentName,
      ObjectName[] pJVMs, String pDeploymentDescriptor)
      throws MalformedObjectNameException,
         InvalidParentException
   {
      super(J2EE_TYPE, rarName, jsr77ParentName, pJVMs, pDeploymentDescriptor);
   }

   // Public --------------------------------------------------------

   // ResourceAdapterodule implementation --------------------------------------

   /**
    * @jmx:managed-attribute
    **/
   public ObjectName[] getResourceAdapters()
   {
      ObjectName[] names = new ObjectName[resourceAdapters.size()];
      resourceAdapters.toArray(names);
      return names;
   }

   /**
    * @jmx:managed-operation
    **/
   public ObjectName getResourceAdapter(int pIndex)
   {
      if (pIndex >= 0 && pIndex < resourceAdapters.size())
      {
         return (ObjectName) resourceAdapters.get(pIndex);
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
      if (ResourceAdapter.J2EE_TYPE.equals(lType))
      {
         resourceAdapters.add(pChild);
      }
   }

   public void removeChild(ObjectName pChild)
   {
      String lType = J2EEManagedObject.getType(pChild);
      if (ResourceAdapter.J2EE_TYPE.equals(lType))
      {
         resourceAdapters.remove(pChild);
      }
   }

   // Object overrides ---------------------------------------------------

   public String toString()
   {
      return "ResourceAdapterModule[ " + super.toString() +
            "ResourceAdapters: " + resourceAdapters +
            " ]";
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   /**
    * @param jsr77ParentName the WebModule parent's JSR77 ObjectName
    * @return A hashtable with the J2EE-Application and J2EE-Server as parent
    */
   protected Hashtable getParentKeys(ObjectName jsr77ParentName)
   {
      Hashtable parentKeys = new Hashtable();
      Hashtable parentProps = jsr77ParentName.getKeyPropertyList();
      String parentName = (String) parentProps.get("name");
      String j2eeType = (String) parentProps.get(J2EEManagedObject.TYPE);

      // Check if parent is a J2EEServer or J2EEApplication
      if ( j2eeType.equals(J2EEApplication.J2EE_TYPE) == false )
      {
         // J2EEServer
         parentKeys.put(J2EEServer.J2EE_TYPE, parentName);
         parentKeys.put(J2EEApplication.J2EE_TYPE, "null");
      }
      else
      {
         // J2EEApplication
         parentKeys.put(J2EEApplication.J2EE_TYPE, parentName);
         String j2eeServerName = (String) parentProps.get(J2EEServer.J2EE_TYPE);
         parentKeys.put(J2EEServer.J2EE_TYPE, j2eeServerName);
      }

      return parentKeys;
   }

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
}
