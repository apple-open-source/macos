/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee;

import java.util.Hashtable;

import javax.management.MalformedObjectNameException;
import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.jboss.logging.Logger;

/**
 * Root class of the JBoss JSR-77 implementation of
 * {@link javax.management.j2ee.ResourceAdapter ResourceAdapter}.
 *
 * @author  <a href="mailto:mclaugs@comcast.net">Scott McLaughlin</a>.
 * @version $Revision: 1.4.2.4 $
 *
 * @jmx:mbean extends="org.jboss.management.j2ee.J2EEManagedObjectMBean"
 */
public class ResourceAdapter
      extends J2EEManagedObject
      implements ResourceAdapterMBean
{
   // Constants -----------------------------------------------------

   public static final String J2EE_TYPE = "ResourceAdapter";

   // Attributes ----------------------------------------------------
   private static Logger log = Logger.getLogger(ResourceAdapter.class);

   /** The JBoss RAR service MBean name */
   private ObjectName rarServiceName;
   /** The JSR77 JCAResource associated with this adapter */
   private ObjectName jcaResourceName;

   // Static --------------------------------------------------------

   public static ObjectName create(MBeanServer mbeanServer, String displayName,
         ObjectName jsr77ParentName, ObjectName rarServiceName)
   {
      ObjectName jsr77Name = null;
      try
      {
         ResourceAdapter adapter = new ResourceAdapter(displayName, jsr77ParentName,
               rarServiceName);
         jsr77Name = adapter.getObjectName();
         mbeanServer.registerMBean(adapter, jsr77Name);
         log.debug("Created JSR-77 ResourceAdapter: " + displayName);
      }
      catch (Exception e)
      {
         log.debug("Could not create JSR-77 ResourceAdapter: " + displayName, e);
      }
      return jsr77Name;
   }

   public static void destroy(MBeanServer mbeanServer, String displayName)
   {
      try
      {
         J2EEManagedObject.removeObject(mbeanServer,
               J2EEDomain.getDomainName() + ":" +
               J2EEManagedObject.TYPE + "=" + ResourceAdapter.J2EE_TYPE + "," +
               "name=" + displayName + "," +
               "*"
         );
      }
      catch (Exception e)
      {
         log.error("Could not destroy JSR-77 ResourceAdapter: " + displayName, e);
      }
   }

   // Constructors --------------------------------------------------

   /**
    * @param displayName The ra.xml/connector/display-name value
    * @param jsr77ParentName ObjectName of the ResourceAdaptorModule
    * @throws InvalidParameterException
    */
   public ResourceAdapter(String displayName, ObjectName jsr77ParentName,
      ObjectName rarServiceName)
      throws MalformedObjectNameException,
         InvalidParentException
   {
      super(J2EE_TYPE, displayName, jsr77ParentName);
      this.rarServiceName = rarServiceName;
   }

   /**
    * @jmx:managed-attribute
    */
   public ObjectName getJBossServiceName()
   {
      return rarServiceName;
   }

   /**
    * @jmx:managed-attribute
    */
   public ObjectName getJcaResource()
   {
      return jcaResourceName;
   }

   // java.lang.Object overrides --------------------------------------

   public String toString()
   {
      return "ResourceAdapter { " + super.toString() + " } []";
   }

   public void addChild(ObjectName j2eeName)
   {
      String j2eeType = J2EEManagedObject.getType(j2eeName);
      if (JCAResource.J2EE_TYPE.equals(j2eeType))
      {
         jcaResourceName = j2eeName;
      }
   }

   public void removeChild(ObjectName j2eeName)
   {
      String j2eeType = J2EEManagedObject.getType(j2eeName);
      if (JCAResource.J2EE_TYPE.equals(j2eeType))
      {
         jcaResourceName = null;
      }
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   /**
    * @return A hashtable with the Resource-Adapter-Module, J2EE-Application and J2EE-Server as parent
    **/
   protected Hashtable getParentKeys(ObjectName pParent)
   {
      Hashtable lReturn = new Hashtable();
      Hashtable lProperties = pParent.getKeyPropertyList();
      lReturn.put(ResourceAdapterModule.J2EE_TYPE, lProperties.get("name"));
      // J2EE-Application and J2EE-Server is already parent of J2EE-Application therefore lookup
      // the name by the J2EE-Server type
      lReturn.put(J2EEApplication.J2EE_TYPE, lProperties.get(J2EEApplication.J2EE_TYPE));
      lReturn.put(J2EEServer.J2EE_TYPE, lProperties.get(J2EEServer.J2EE_TYPE));

      return lReturn;
   }

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------

}
