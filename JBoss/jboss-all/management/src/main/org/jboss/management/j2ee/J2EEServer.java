/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee;

import java.util.ArrayList;
import java.util.List;

import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;

/**
 * JBoss implementation of the JSR-77 {@link javax.management.j2ee.J2EEServer
 * J2EEServer}.
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.5.2.1 $
 *
 * @jmx:mbean extends="org.jboss.management.j2ee.J2EEManagedObjectMBean"
 **/
public class J2EEServer
      extends J2EEManagedObject
      implements J2EEServerMBean
{
   // Constants -----------------------------------------------------

   public static final String J2EE_TYPE = "J2EEServer";

   // Attributes ----------------------------------------------------

   private List mDeployedObjects = new ArrayList();

   private List mResources = new ArrayList();

   private List mJVMs = new ArrayList();

   private String mServerVendor = null;

   private String mServerVersion = null;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   public J2EEServer(String pName, ObjectName pDomain, String pServerVendor, String pServerVersion)
         throws
         MalformedObjectNameException,
         InvalidParentException
   {
      super(J2EE_TYPE, pName, pDomain);
      mServerVendor = pServerVendor;
      mServerVersion = pServerVersion;
   }

   // Public --------------------------------------------------------

   /**
    * @jmx:managed-attribute
    **/
   public ObjectName[] getDeployedObjects()
   {
      return (ObjectName[]) mDeployedObjects.toArray(new ObjectName[0]);
   }

   /**
    * @jmx:managed-operation
    **/
   public ObjectName getDeployedObject(int pIndex)
   {
      if (pIndex >= 0 && pIndex < mDeployedObjects.size())
      {
         return (ObjectName) mDeployedObjects.get(pIndex);
      }
      return null;
   }

   /**
    * @jmx:managed-attribute
    **/
   public ObjectName[] getResources()
   {
      ObjectName[] names = new ObjectName[mResources.size()];
      mResources.toArray(names);
      return names;
   }

   /**
    * @jmx:managed-operation
    **/
   public ObjectName getResource(int pIndex)
   {
      if (pIndex >= 0 && pIndex < mResources.size())
      {
         return (ObjectName) mResources.get(pIndex);
      }
      return null;
   }

   /**
    * @jmx:managed-attribute
    **/
   public ObjectName[] getJavaVMs()
   {
      return (ObjectName[]) mJVMs.toArray(new ObjectName[0]);
   }

   /**
    * @jmx:managed-operation
    **/
   public ObjectName getJavaVM(int pIndex)
   {
      if (pIndex >= 0 && pIndex < mJVMs.size())
      {
         return (ObjectName) mJVMs.get(pIndex);
      }
      return null;
   }

   /**
    * @jmx:managed-attribute
    **/
   public String getServerVendor()
   {
      return mServerVendor;
   }

   /**
    * @jmx:managed-attribute
    **/
   public String getServerVersion()
   {
      return mServerVersion;
   }

   public void addChild(ObjectName pChild)
   {
      String lType = J2EEManagedObject.getType(pChild);
      if ( J2EEApplication.J2EE_TYPE.equals(lType) ||
            EJBModule.J2EE_TYPE.equals(lType) ||
            ResourceAdapterModule.J2EE_TYPE.equals(lType) ||
            WebModule.J2EE_TYPE.equals(lType) ||
            ServiceModule.J2EE_TYPE.equals(lType) )
      {
         mDeployedObjects.add(pChild);
      }
      else if (JVM.J2EE_TYPE.equals(lType))
      {
         mJVMs.add(pChild);
      }
      else if (JNDIResource.J2EE_TYPE.equals(lType) ||
            JMSResource.J2EE_TYPE.equals(lType) ||
            URLResource.J2EE_TYPE.equals(lType) ||
            JTAResource.J2EE_TYPE.equals(lType) ||
            JavaMailResource.J2EE_TYPE.equals(lType) ||
            JDBCResource.J2EE_TYPE.equals(lType) ||
            RMI_IIOPResource.J2EE_TYPE.equals(lType) ||
            JCAResource.J2EE_TYPE.equals(lType) )
      {
         mResources.add(pChild);
      }
   }

   public void removeChild(ObjectName pChild)
   {
      String lType = J2EEManagedObject.getType(pChild);
      if ( J2EEApplication.J2EE_TYPE.equals(lType) ||
            EJBModule.J2EE_TYPE.equals(lType) ||
            ResourceAdapterModule.J2EE_TYPE.equals(lType) ||
            WebModule.J2EE_TYPE.equals(lType) ||
            ServiceModule.J2EE_TYPE.equals(lType) )
      {
         mDeployedObjects.remove(pChild);
      }
      else if (JVM.J2EE_TYPE.equals(lType))
      {
         mJVMs.remove(pChild);
      }
      else if (JNDIResource.J2EE_TYPE.equals(lType) ||
            JMSResource.J2EE_TYPE.equals(lType) ||
            URLResource.J2EE_TYPE.equals(lType) ||
            JTAResource.J2EE_TYPE.equals(lType) ||
            JavaMailResource.J2EE_TYPE.equals(lType) ||
            JDBCResource.J2EE_TYPE.equals(lType) ||
            RMI_IIOPResource.J2EE_TYPE.equals(lType) ||
            JCAResource.J2EE_TYPE.equals(lType) )
      {
         mResources.remove(pChild);
      }
   }

   public String toString()
   {
      return "J2EEServer { " + super.toString() + " } [ " +
            "depoyed objects: " + mDeployedObjects +
            ", resources: " + mResources +
            ", JVMs: " + mJVMs +
            ", J2EE vendor: " + mServerVendor +
            " ]";
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
}
