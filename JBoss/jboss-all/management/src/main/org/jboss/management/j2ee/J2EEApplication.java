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

import javax.management.JMException;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.jboss.logging.Logger;

/**
 * Root class of the JBoss JSR-77 implementation of
 * {@link javax.management.j2ee.J2EEApplication J2EEApplication}.
 *
 * @author  <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @version $Revision: 1.10.2.7 $

 * @todo When all components of a J2EEApplication is state manageable
 *       this have to be too !!
 *
 * @jmx:mbean extends="org.jboss.management.j2ee.J2EEDeployedObjectMBean"
 **/
public class J2EEApplication
      extends J2EEDeployedObject
      implements J2EEApplicationMBean
{
   // Constants -----------------------------------------------------
   public static final String J2EE_TYPE = "J2EEApplication";

   // Attributes ----------------------------------------------------
   private static Logger log = Logger.getLogger(J2EEApplication.class);

   private List mModules = new ArrayList();

   // Static --------------------------------------------------------

   /** Create a JSR77 ear model instnace
    * @param mbeanServer the MBeanServer to register with
    * @param earName the name of the j2ee ear deployment
    * @param url the ear URL, which may be null to represent a standalone
    *    jar/war/war without an ear
    * @return the JSR77 ObjectName for the J2EEApplication
    */
   public static ObjectName create(MBeanServer mbeanServer, String earName, URL url)
   {
      String lDD = null;
      ObjectName jsr77Name = null;
      ObjectName j2eeServerName = J2EEDomain.getDomainServerName(mbeanServer);
      // First get the deployement descriptor
      lDD = J2EEDeployedObject.getDeploymentDescriptor(url, J2EEDeployedObject.APPLICATION);
      try
      {
         // Now create the J2EEApplication
         J2EEApplication j2eeApp = new J2EEApplication(earName, j2eeServerName, lDD);
         jsr77Name = j2eeApp.getObjectName();
         /* Check to see if the ear is already registered. This will occur when
         an ear is deployed because we do not receive the ear module start
         notification until its contained modules have started. The content
         modules will have created a placeholder ear when they could not find
         an existing J2EEApplication registered.
         */
         if( mbeanServer.isRegistered(jsr77Name) == true )
         {
            // We take the modules from the EAR placeholder
            ObjectName[] tmpModules = (ObjectName[])mbeanServer.getAttribute(jsr77Name, "Modules");
            // Remove the placeholder and register the j2eeApp
            mbeanServer.unregisterMBean(jsr77Name);
            mbeanServer.registerMBean(j2eeApp, jsr77Name);
            // Add the 
            if (tmpModules != null)
            {
               for (int m=0; m<tmpModules.length; m++)
                  j2eeApp.addChild(tmpModules[m]);
            }
         }
         else
         {
            mbeanServer.registerMBean(j2eeApp, jsr77Name);
         }
         log.debug("Created JSR-77 J2EEApplication: " + earName);
      }
      catch (Exception e)
      {
         log.debug("Could not create JSR-77 J2EEApplication: " + jsr77Name, e);
      }
      return jsr77Name;
   }

   /** Destroy the J2EEApplication component
    * @param mbeanServer the MBeanServer used during create
    * @param jsr77Name the JSR77 J2EEApplication component name
    */
   public static void destroy(MBeanServer mbeanServer, ObjectName jsr77Name)
   {
      try
      {
         mbeanServer.unregisterMBean(jsr77Name);
         log.debug("Destroyed JSR-77 J2EEApplication: " + jsr77Name);
      }
      catch (javax.management.InstanceNotFoundException infe)
      {
      }
      catch (Exception e)
      {
         log.debug("Could not destroy JSR-77 J2EEApplication: " + jsr77Name, e);
      }
   }

   // Constructors --------------------------------------------------

   /**
    * Constructor taking the Name of this Object
    *
    * @param name Name to be set which must not be null
    * @param pDeploymentDescriptor
    *
    * @throws InvalidParameterException If the given Name is null
    **/
   public J2EEApplication(String name, ObjectName mbeanServer, String pDeploymentDescriptor)
      throws MalformedObjectNameException, InvalidParentException
   {
      super(J2EE_TYPE, name, mbeanServer, pDeploymentDescriptor);
   }

   // Public --------------------------------------------------------

   // J2EEApplication implementation --------------------------------

   /**
    * @jmx:managed-attribute
    **/
   public ObjectName[] getModules()
   {
      return (ObjectName[]) mModules.toArray(new ObjectName[mModules.size()]);
   }

   /**
    * @jmx:managed-operation
    **/
   public ObjectName getModule(int pIndex)
   {
      if (pIndex >= 0 && pIndex < mModules.size())
      {
         return (ObjectName) mModules.get(pIndex);
      }
      return null;
   }

   // J2EEManagedObjectMBean implementation -------------------------

   public void addChild(ObjectName pChild)
   {
      String lType = J2EEManagedObject.getType(pChild);
      if (
            EJBModule.J2EE_TYPE.equals(lType) ||
            WebModule.J2EE_TYPE.equals(lType) ||
            ResourceAdapterModule.J2EE_TYPE.equals(lType) ||
            ServiceModule.J2EE_TYPE.equals(lType)
      )
      {
         mModules.add(pChild);
         try
         {
            // Now it also have to added as child to its
            // parent
            server.invoke(
                  getParent(),
                  "addChild",
                  new Object[]{pChild},
                  new String[]{ObjectName.class.getName()}
            );
         }
         catch (JMException jme)
         {
            // Ignore it because parent has to be there
         }
      }
   }

   public void removeChild(ObjectName pChild)
   {
      String lType = J2EEManagedObject.getType(pChild);
      if (
            EJBModule.J2EE_TYPE.equals(lType) ||
            WebModule.J2EE_TYPE.equals(lType) ||
            ResourceAdapterModule.J2EE_TYPE.equals(lType) ||
            ServiceModule.J2EE_TYPE.equals(lType)
      )
      {
         mModules.remove(pChild);
         try
         {
            // Now it also have to added as child to its
            // parent
            server.invoke(
                  getParent(),
                  "removeChild",
                  new Object[]{pChild},
                  new String[]{ObjectName.class.getName()}
            );
         }
         catch (JMException jme)
         {
            // Ignore it because parent has to be there
         }
      }
   }

   // Object overrides ---------------------------------------------------

   public String toString()
   {
      return "J2EEApplication { " + super.toString() + " } [ " +
            "modules: " + mModules +
            " ]";
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   /**
    * @return A hashtable with the J2EE Server as parent
    **/
   protected Hashtable getParentKeys(ObjectName pParent)
   {
      Hashtable lReturn = new Hashtable();
      Hashtable lProperties = pParent.getKeyPropertyList();
      lReturn.put(J2EEServer.J2EE_TYPE, lProperties.get("name"));

      return lReturn;
   }

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
}
