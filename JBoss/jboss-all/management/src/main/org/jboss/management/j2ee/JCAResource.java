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
import java.util.Hashtable;
import javax.management.MBeanServer;
import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import javax.management.j2ee.statistics.Stats;

import org.jboss.logging.Logger;
import org.jboss.management.j2ee.statistics.JCAConnectionPoolStatsImpl;
import org.jboss.management.j2ee.statistics.JCAStatsImpl;

/** The JBoss JSR-77.3.22 JCAResource model implementation
 *
 * @author <a href="mailto:mclaugs@comcast.com">Scott McLaughlin</a>.
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.6.2.5 $
 *
 * @jmx:mbean extends="org.jboss.management.j2ee.J2EEResourceMBean, org.jboss.management.j2ee.statistics.StatisticsProvider"
 */
public class JCAResource
      extends J2EEResource
      implements JCAResourceMBean
{
   // Constants -----------------------------------------------------
   private static Logger log = Logger.getLogger(JCAResource.class);

   public static final String J2EE_TYPE = "JCAResource";

   // Attributes ----------------------------------------------------

   private List connectionFactories = new ArrayList();
   private ObjectName cmServiceName;
   private ObjectName poolServiceName;
   private JCAStatsImpl stats;

   // Static --------------------------------------------------------

   /** Create a JCAResource
    *
    * @param mbeanServer
    * @param resName
    * @param jsr77RAParentName
    * @param cmServiceName
    * @param mcfServiceName
    * @param poolServiceName
    * @return
    */
   public static ObjectName create(MBeanServer mbeanServer, String resName,
      ObjectName jsr77RAParentName, ObjectName cmServiceName,
      ObjectName mcfServiceName, ObjectName poolServiceName)
   {
      ObjectName jsr77Name = null;
      try
      {
         JCAResource jcaRes = new JCAResource(resName, jsr77RAParentName,
            cmServiceName, poolServiceName);
         jsr77Name = jcaRes.getObjectName();
         mbeanServer.registerMBean(jcaRes, jsr77Name);
         log.debug("Created JSR-77 JCAResource: " + resName);

         // Create a JCAConnectionFactory and JCAManagedConnectionFactory
         ObjectName jcaFactoryName = JCAConnectionFactory.create(mbeanServer,
               resName, jsr77Name, cmServiceName, mcfServiceName);
      }
      catch (Exception e)
      {
         log.debug("Could not create JSR-77 JCAResource: " + resName, e);
      }
      return jsr77Name;
   }

   public static void destroy(MBeanServer mbeanServer, String resName)
   {
      try
      {
         // Find the Object to be destroyed
         ObjectName lSearch = new ObjectName(
               J2EEDomain.getDomainName() + ":" +
               J2EEManagedObject.TYPE + "=" + JCAResource.J2EE_TYPE + "," +
               "name=" + resName + "," +
               "*"
         );
         Set lNames = mbeanServer.queryNames(
               lSearch,
               null
         );
         if (!lNames.isEmpty())
         {
            ObjectName lJCAResource = (ObjectName) lNames.iterator().next();
            // Now check if the JCAResource does not contains another Connection Factory
            ObjectName[] lConnectionFactories = (ObjectName[]) mbeanServer.getAttribute(
                  lJCAResource,
                  "ConnectionFactories"
            );
            if (lConnectionFactories.length == 0)
            {
               // Remove it because it does not reference any JDBC DataSources
               mbeanServer.unregisterMBean(lJCAResource);
            }
         }
      }
      catch (Exception e)
      {
         log.debug("Could not destroy JSR-77 JCAResource", e);
      }
   }

   // Constructors --------------------------------------------------

   /**
    *
    * @param resName
    * @param jsr77ParentName
    * @param cmServiceName
    * @param poolServiceName
    * @throws MalformedObjectNameException
    * @throws InvalidParentException
    */
   public JCAResource(String resName, ObjectName jsr77ParentName,
      ObjectName cmServiceName, ObjectName poolServiceName)
      throws MalformedObjectNameException, InvalidParentException
   {
      super(J2EE_TYPE, resName, jsr77ParentName);
      this.cmServiceName = cmServiceName;
      this.poolServiceName = poolServiceName;
   }

   // Public --------------------------------------------------------

   // javax.management.j2ee.JCAResource implementation ---------------------

   /**
    * @jmx:managed-attribute
    */
   public ObjectName[] getConnectionFactories()
   {
      ObjectName[] names = new ObjectName[connectionFactories.size()];
      connectionFactories.toArray(names);
      return names;
   }

   /**
    * @jmx:managed-operation
    */
   public ObjectName getConnectionFactory(int n)
   {
      ObjectName name = null;
      if (n >= 0 && n < connectionFactories.size())
      {
         name = (ObjectName) connectionFactories.get(n);
      }
      return name;
   }

   // J2EEManagedObjectMBean implementation -------------------------

   public void addChild(ObjectName pChild)
   {
      String lType = J2EEManagedObject.getType(pChild);
      if (JCAConnectionFactory.J2EE_TYPE.equals(lType))
      {
         connectionFactories.add(pChild);
      }
   }

   public void removeChild(ObjectName pChild)
   {
      String lType = J2EEManagedObject.getType(pChild);
      if (JCAConnectionFactory.J2EE_TYPE.equals(lType))
      {
         connectionFactories.remove(pChild);
      }
   }

  // Begin StatisticsProvider interface methods

   /** Obtain the Stats from the StatisticsProvider.
    *
    * @jmx:managed-attribute
    * @return An JCAStats implementation
    */
   public Stats getStats()
   {
      try
      {
         ObjectName jsr77CFName = getConnectionFactory(0);
         Object[] params = {poolServiceName};
         String[] sig = {ObjectName.class.getName()};
         JCAConnectionPoolStatsImpl cfStats = (JCAConnectionPoolStatsImpl)
               server.invoke(jsr77CFName, "getPoolStats", params, sig);
         JCAConnectionPoolStatsImpl[] poolStats = {cfStats};
         stats = new JCAStatsImpl(null, poolStats);
      }
      catch(Exception e)
      {
         log.debug("Failed to create JCAStats", e);
      }
      return stats;
   }

   /** Reset all statistics in the StatisticsProvider
    * @jmx:managed-operation
    */
   public void resetStats()
   {
      if( stats != null )
         stats.reset();
   }
   // End StatisticsProvider interface methods

   // java.lang.Object overrides ------------------------------------

   public String toString()
   {
      return "JCAResource { " + super.toString() + " } [ " +
            "Connection Factories: " + connectionFactories +
            " ]";
   }

   /**
    * @return A hashtable with the J2EEServer and ResourceAdapter
    */
   protected Hashtable getParentKeys( ObjectName parentName )
   {
      Hashtable keys = new Hashtable();
      Hashtable nameProps = parentName.getKeyPropertyList();
      String adapterName = (String) nameProps.get( "name");
      String serverName = (String) nameProps.get(J2EEServer.J2EE_TYPE);
      keys.put( J2EEServer.J2EE_TYPE, serverName);
      keys.put(ResourceAdapter.J2EE_TYPE, adapterName);
      return keys;
   }
}
