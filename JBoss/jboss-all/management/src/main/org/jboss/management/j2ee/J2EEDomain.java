/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import javax.management.MBeanServer;

/**
 * Root class of the JBoss JSR-77 implementation of
 *
 * @author  <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @author  Scott.Stark@jboss.org
 * @version $Revision: 1.2.2.1 $
 *
 * @jmx:mbean extends="org.jboss.management.j2ee.J2EEManagedObjectMBean"
 **/
public class J2EEDomain
      extends J2EEManagedObject
      implements J2EEDomainMBean
{
   // Constants -----------------------------------------------------
   public static final String J2EE_TYPE = "J2EEDomain";

   // Attributes ----------------------------------------------------
   /** The local server J2EEDomain implementation name */
   private static String domainName = null;

   private List mServers = new ArrayList();

   // Static --------------------------------------------------------
   /** Get the local J2EEDomain instance name
    * @return the J2EEDomain object name for the local server.
    */
   public static String getDomainName()
   {
      return domainName;
   }

   /** Query for the J2EEServer MBean in the given domain.
    * @param mbeanServer the local MBeanServer
    * @return the J2EEServer name if found, null otherwise
    */
   public static ObjectName getDomainServerName(MBeanServer mbeanServer)
   {
      ObjectName domainServer = null;
      try
      {
         // Query for all MBeans matching the J2EEServer naming convention
         ObjectName serverQuery = new ObjectName(domainName + ":" +
            J2EEManagedObject.TYPE + "=" + J2EEServer.J2EE_TYPE + "," + "*");

         Set servers = mbeanServer.queryNames(serverQuery, null);
         if( servers.isEmpty() == false )
         {
            domainServer = (ObjectName) servers.iterator().next();
         }
      }
      catch(Exception ignore)
      {
      }
      return domainServer;
   }

   // Constructors --------------------------------------------------

   public J2EEDomain(String domainName)
         throws MalformedObjectNameException,
         InvalidParentException
   {
      super(domainName, J2EE_TYPE, "Manager");
      J2EEDomain.domainName = domainName;
   }

   // Public --------------------------------------------------------

   /**
    * @jmx:managed-attribute
    **/
   public ObjectName[] getServers()
   {
      return (ObjectName[]) mServers.toArray(new ObjectName[0]);
   }

   /**
    * @jmx:managed-operation
    **/
   public ObjectName getServer(int pIndex)
   {
      if (pIndex >= 0 && pIndex < mServers.size())
      {
         return (ObjectName) mServers.get(pIndex);
      }
      return null;
   }

   // J2EEManagedObject implementation ----------------------------------------------

   public void addChild(ObjectName pChild)
   {
      String lType = J2EEManagedObject.getType(pChild);
      if (J2EEServer.J2EE_TYPE.equals(lType))
      {
         mServers.add(pChild);
      }
   }

   public void removeChild(ObjectName pChild)
   {
      String lType = J2EEManagedObject.getType(pChild);
      if (J2EEServer.J2EE_TYPE.equals(lType))
      {
         mServers.remove(pChild);
      }
   }
   // Object overrides ---------------------------------------------------

   public String toString()
   {
      return "J2EEDomain { " + super.toString() + " } [ " +
            ", servers: " + mServers +
            " ]";
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
}
