/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee;

import java.net.URL;
import java.util.ArrayList;
import java.util.Hashtable;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.management.MalformedObjectNameException;
import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.jboss.logging.Logger;

/** The JBoss JSR-77.3.16 implementation of the WebModule model
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.6.2.5 $
 *
 * @jmx:mbean extends="org.jboss.management.j2ee.EventProvider, org.jboss.management.j2ee.J2EEModuleMBean"
 */
public class WebModule
      extends J2EEModule
      implements WebModuleMBean
{

   // Constants -----------------------------------------------------
   public static final String J2EE_TYPE = "WebModule";
   private static final String[] eventTypes = {StateManagement.CREATED_EVENT,
      StateManagement.DESTROYED_EVENT};

   // Attributes ----------------------------------------------------
   private static Logger log = Logger.getLogger(WebModule.class);

   private List servlets = new ArrayList();
   private String jbossWebDD;

   /** used to see if we should remove our parent when we are destroyed. */
   private static final Map fakeJ2EEApps = new HashMap();

   // Static --------------------------------------------------------

   /**
    * Creates the JSR-77 WebModule
    *
    * @param pServer MBeanServer the WebModule is created on
    * @param earName Name of the Application but if null object then it
    *                         is a standalone module (no EAR wrapper around)
    * @param warName Name of the war
    * @param pURL URL path to the local deployment of the module (where to find the DD file)
    * @param webContainerName the JBoss web container mbean name
    */
   public static ObjectName create(MBeanServer mbeanServer,
         String earName,
         String warName,
         URL pURL,
         ObjectName webContainerName)
   {
      String lDD = null;
      String lJBossWebDD = null;
      ObjectName jsr77ParentName = null;
      ObjectName lCreated = null;
      ObjectName j2eeServerName = J2EEDomain.getDomainServerName(mbeanServer);
      ObjectName jsr77Name = null;
      try
      {
         // Get the J2EEServer name
         Hashtable props = j2eeServerName.getKeyPropertyList();
         String j2eeServer = props.get(J2EEManagedObject.TYPE) + "=" +
               props.get("name");


         if (earName == null)
         {
            // If there is no ear use the J2EEServer as the parent
            jsr77ParentName = j2eeServerName;
         }
         else
         {
            // Query for the J2EEApplication matching earName
            ObjectName lApplicationQuery = new ObjectName(
                  J2EEDomain.getDomainName() + ":" +
                  J2EEManagedObject.TYPE + "=" + J2EEApplication.J2EE_TYPE + "," +
                  "name=" + earName + "," +
                  j2eeServer + "," +
                  "*"
            );
            Set lApplications = mbeanServer.queryNames(lApplicationQuery, null);

            if (lApplications.isEmpty())
            {
               lCreated = J2EEApplication.create(
                     mbeanServer,
                     earName,
                     null
               );
               jsr77ParentName = lCreated;
            } // end of if ()
            else if (lApplications.size() == 1)
            {
               jsr77ParentName = (ObjectName) lApplications.iterator().next();
            } // end of if ()
         }

         // Get the J2EE deployement descriptor
         lDD = J2EEDeployedObject.getDeploymentDescriptor(pURL, J2EEDeployedObject.WEB);
         // Get the JBoss Web deployement descriptor
         lJBossWebDD = J2EEDeployedObject.getDeploymentDescriptor(pURL, J2EEDeployedObject.JBOSS);
      }
      catch (Exception e)
      {
         log.error("Could not create JSR-77 WebModule: " + warName, e);
         return null;
      }

      try
      {
         // Get JVM of the j2eeServer
         ObjectName[] jvms = (ObjectName[]) mbeanServer.getAttribute(
               j2eeServerName,
               "JavaVMs"
         );

         WebModule webModule = new WebModule(warName, jsr77ParentName, jvms, lDD,
               webContainerName, lJBossWebDD);
         jsr77Name = webModule.getObjectName();
         mbeanServer.registerMBean(webModule, jsr77Name);
         //remember if we created our parent, if we did we have to kill it on destroy.
         if (lCreated != null)
         {
            fakeJ2EEApps.put(jsr77Name, lCreated);
         } // end of if ()
         log.debug("Created JSR-77 WebModule: " + jsr77Name);
      }
      catch (Exception e)
      {
         log.debug("Could not create JSR-77 WebModule: " + warName, e);
         return null;
      }
      return jsr77Name;
   }

   /** Destroy a JSR-77 WebModule
    *
    * @param mbeanServer The JMX MBeanServer the desired WebModule is registered on
    * @param jsr77Name the JSR77 EJBModule component ObjectName
    */
   public static void destroy(MBeanServer mbeanServer, ObjectName jsr77Name)
   {
      try
      {
         mbeanServer.unregisterMBean(jsr77Name);
         log.debug("Remove JSR-77 WebModule: " + jsr77Name);
         ObjectName jsr77ParentName = (ObjectName) fakeJ2EEApps.get(jsr77Name);
         if (jsr77ParentName != null)
         {
            log.debug("Remove fake JSR-77 parent Application: " + jsr77ParentName);
            J2EEApplication.destroy(mbeanServer, jsr77ParentName);
         }
      }
      catch (Exception e)
      {
         log.debug("Could not destroy JSR-77 WebModule: " + jsr77Name, e);
      }
   }

   // Constructors --------------------------------------------------

   /**
    * Constructor taking the Name of this Object
    *
    * @param webModuleName Name to be set which must not be null
    * @param j2eeAppName the name of the parent JSR77 model component
    * @param jvms the names of the deployment env JVM JSR77 model components
    * @param webDD the web.xml descriptor text
    * @param webContainerName the JBoss web container service name for the war
    * @param jbossWebDD the jboss-web.xml descriptor text
    *
    * @throws InvalidParameterException If the given Name is null
    */
   public WebModule(String warName, ObjectName j2eeAppName, ObjectName[] jvms,
      String webDD, ObjectName webContainerName, String jbossWebDD)
      throws MalformedObjectNameException,
         InvalidParentException
   {
      super("WebModule", warName, j2eeAppName, jvms, webDD);
      jbossWebDD = (jbossWebDD == null ? "" : jbossWebDD);
   }

   // Public --------------------------------------------------------

   /**
    * @jmx:managed-attribute
    */
   public ObjectName[] getServlets()
   {
      ObjectName[] names = new ObjectName[servlets.size()];
      servlets.toArray(names);
      return names;
   }

   /**
    * @jmx:managed-operation
    */
   public ObjectName getServlet(int pIndex)
   {
      if (pIndex >= 0 && pIndex < servlets.size())
      {
         return (ObjectName) servlets.get(pIndex);
      }
      else
      {
         return null;
      }
   }

   /**
    * @jmx:managed-attribute
    */
   public String getJBossWebDeploymentDescriptor()
   {
      return jbossWebDD;
   }

   // J2EEManagedObjectMBean implementation -------------------------

   public void addChild(ObjectName pChild)
   {
      String lType = J2EEManagedObject.getType(pChild);
      if (Servlet.J2EE_TYPE.equals(lType)
      )
      {
         servlets.add(pChild);
      }
   }

   public void removeChild(ObjectName pChild)
   {
      String lType = J2EEManagedObject.getType(pChild);
      if ( Servlet.J2EE_TYPE.equals(lType) )
      {
         servlets.remove(pChild);
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

   public void postCreation()
   {
      sendNotification(StateManagement.CREATED_EVENT, "Web module created");
   }

   public void preDestruction()
   {
      sendNotification(StateManagement.DESTROYED_EVENT, "Web module destroyed");
   }

   // Object overrides ---------------------------------------------------

   public String toString()
   {
      return "WebModule[ " + super.toString() +
            ", Servlets: " + servlets +
            ", JBoss-Web-DD: " + jbossWebDD +
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
