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
 * Root class of the JBoss JSR-77 implementation of EJBModule.
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.16.2.8 $
 * @jmx:mbean extends="org.jboss.management.j2ee.EventProvider, org.jboss.management.j2ee.J2EEModuleMBean"
 */
public class EJBModule
      extends J2EEModule
      implements EJBModuleMBean
{
   // Constants -----------------------------------------------------
   public static final String J2EE_TYPE = "EJBModule";
   private static final String[] eventTypes = {StateManagement.CREATED_EVENT,
      StateManagement.DESTROYED_EVENT};

   // Attributes ----------------------------------------------------
   private static Logger log = Logger.getLogger(EJBModule.class);

   private List mEJBs = new ArrayList();
   private ObjectName moduleServiceName;
   private String mJBossDD;
   private String mJAWSDD;
   private String mCMPDD;

   /** The JSR77 ObjectNames of fake J2EEApplications created by standalone jars */
   private static final Map fakeJ2EEApps = new HashMap();

   // Static --------------------------------------------------------

   /**
    * Creates the JSR-77 EJBModule
    *
    * @param mbeanServer MBeanServer the EJBModule is created on
    * @param earName the ear name unless null which indicates a standalone module (no EAR)
    * @param jarName the ejb.jar name
    * @param pURL URL path to the local deployment of the module (where to find the DD file)
    * @param moduleServiceName ObjectName of the EjbModule service to start and stop the module
    * @return the JSR77 ObjectName of the EJBModule
    */
   public static ObjectName create(MBeanServer mbeanServer,
         String earName,
         String jarName,
         URL pURL,
         ObjectName moduleServiceName)
   {
      String lDD = null;
      String lJBossDD = null;
      String lJAWSDD = null;
      String lCMPDD = null;
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

         if (earName == null)
         {
            // If there is no ear use the J2EEServer as the parent
            lParent = j2eeServerName;
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
            Set parentApps = mbeanServer.queryNames(lApplicationQuery, null);

            if (parentApps.isEmpty())
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
         lDD = J2EEDeployedObject.getDeploymentDescriptor(pURL, J2EEDeployedObject.EJB);
         // Get the JBoss deployement descriptor
         lJBossDD = J2EEDeployedObject.getDeploymentDescriptor(pURL, J2EEDeployedObject.JBOSS);
         // Get the JAWS deployement descriptor
         lJAWSDD = J2EEDeployedObject.getDeploymentDescriptor(pURL, J2EEDeployedObject.JAWS);
         // Get the CMP 2.0 deployement descriptor
         lCMPDD = J2EEDeployedObject.getDeploymentDescriptor(pURL, J2EEDeployedObject.CMP);
      }
      catch (Exception e)
      {
         log.debug("Could not create JSR-77 EJBModule: " + jarName, e);
         return null;
      }

      try
      {
         // Get JVM of the j2eeServer
         ObjectName[] jvms = (ObjectName[]) mbeanServer.getAttribute(
               j2eeServerName,
               "JavaVMs"
         );

         EJBModule ejbModule = new EJBModule(jarName, lParent,
                  jvms,
                  lDD,
                  moduleServiceName,
                  lJBossDD,
                  lJAWSDD,
                  lCMPDD);
         jsr77Name = ejbModule.getObjectName();
         mbeanServer.registerMBean(ejbModule, jsr77Name);
         // If we created our parent, if we have to delete it in destroy.
         if (lCreated != null)
         {
            fakeJ2EEApps.put(jsr77Name, lCreated);
         }
         log.debug("Created JSR-77 EJBModule: " + jsr77Name);
      }
      catch (Exception e)
      {
         log.error("Could not create JSR-77 EJBModule: " + jarName, e);
      }

      return jsr77Name;
   }

   /**
    * Destroyes the given JSR-77 EJB-Module
    *
    * @param mbeanServer The JMX MBeanServer the desired EJB-Module is registered on
    * @param jsr77Name the JSR77 EJBModule component ObjectName
    */
   public static void destroy(MBeanServer mbeanServer, ObjectName jsr77Name)
   {
      try
      {
         log.info("destroy(), remove EJB-Module: " + jsr77Name);
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
         log.debug("Could not destroy JSR-77 EJBModule: " + jsr77Name, e);
      }
   }

   // Constructors --------------------------------------------------

   /**
    * Constructor taking the Name of this Object
    *
    * @param jarName the ejb jar name which must not be null
    * @param jsr77ParentName ObjectName of the Parent this Module belongs
    *                too. Either it is a J2EEApplication or J2EEServer
    *                if a standalone module.
    * @param pJVMs Array of ObjectNames of the JVM this module is deployed on
    * @param pDeploymentDescriptor Content of the module deployment descriptor
    * @param moduleServiceName ObjectName of the service this Managed Object represent
    *                 used for state management (start and stop)
    *
    * @throws MalformedObjectNameException If name or application name is incorrect
    * @throws InvalidParameterException If the given Name is null
    */
   public EJBModule(String jarName,
         ObjectName jsr77ParentName,
         ObjectName[] pJVMs,
         String pDeploymentDescriptor,
         ObjectName moduleServiceName,
         String pJBossDD,
         String pJAWSDD,
         String pCMPDD
         )
         throws
         MalformedObjectNameException,
         InvalidParentException
   {
      super(J2EE_TYPE, jarName, jsr77ParentName, pJVMs, pDeploymentDescriptor);
      this.moduleServiceName = moduleServiceName;
      mJBossDD = (pJBossDD == null ? "" : pJBossDD);
      mJAWSDD = (pJAWSDD == null ? "" : pJAWSDD);
      mCMPDD = (pCMPDD == null ? "" : pCMPDD);
   }

   // Public --------------------------------------------------------

   /**
    * @jmx:managed-attribute
    */
   public ObjectName[] getEjbs()
   {
      return (ObjectName[]) mEJBs.toArray(new ObjectName[0]);
   }

   /**
    * @jmx:managed-operation
    */
   public ObjectName getEJB(int pIndex)
   {
      if (pIndex >= 0 && pIndex < mEJBs.size())
      {
         return (ObjectName) mEJBs.get(pIndex);
      }
      else
      {
         return null;
      }
   }

   /**
    * @return JBoss Deployment Descriptor
    *
    * @jmx:managed-attribute
    */
   public String getJBossDeploymentDescriptor()
   {
      return mJBossDD;
   }

   /**
    * @return JAWS Deployment Descriptor
    *
    * @jmx:managed-attribute
    */
   public String getJAWSDeploymentDescriptor()
   {
      return mJAWSDD;
   }

   /**
    * @return CMP 2.0 Deployment Descriptor
    *
    * @jmx:managed-attribute
    */
   public String getCMPDeploymentDescriptor()
   {
      return mCMPDD;
   }

   // J2EEManagedObjectMBean implementation -------------------------

   public void addChild(ObjectName pChild)
   {
      String lType = J2EEManagedObject.getType(pChild);
      if (EntityBean.J2EE_TYPE.equals(lType) ||
            StatelessSessionBean.J2EE_TYPE.equals(lType) ||
            StatefulSessionBean.J2EE_TYPE.equals(lType) ||
            MessageDrivenBean.J2EE_TYPE.equals(lType)
      )
      {
         mEJBs.add(pChild);
      }
   }

   public void removeChild(ObjectName pChild)
   {
      String lType = J2EEManagedObject.getType(pChild);
      if (EntityBean.J2EE_TYPE.equals(lType) ||
            StatelessSessionBean.J2EE_TYPE.equals(lType) ||
            StatefulSessionBean.J2EE_TYPE.equals(lType) ||
            MessageDrivenBean.J2EE_TYPE.equals(lType)
      )
      {
         mEJBs.remove(pChild);
      }
   }

   public void postCreation()
   {
      sendNotification(StateManagement.CREATED_EVENT, "EJB Module created");
   }

   public void preDestruction()
   {
      sendNotification(StateManagement.DESTROYED_EVENT, "EJB Module destroyed");
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

   // Object overrides ---------------------------------------------------

   public String toString()
   {
      return "EJBModule[ " + super.toString() +
            ", EJBs: " + mEJBs +
            ", JBoss-DD: " + mJBossDD +
            ", JAWS-DD: " + mJAWSDD +
            ", CMP-2.0-DD: " + mCMPDD +
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

/*
vim:ts=3:sw=3:et
*/
