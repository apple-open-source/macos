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
import org.jboss.management.j2ee.statistics.TimeStatisticImpl;
import org.jboss.management.j2ee.statistics.CountStatisticImpl;
import org.jboss.management.j2ee.statistics.BoundedRangeStatisticImpl;
import org.jboss.management.j2ee.statistics.RangeStatisticImpl;
import org.jboss.management.j2ee.statistics.JCAConnectionPoolStatsImpl;

/**
 * Root class of the JBoss JSR-77 implementation of
 * {@link javax.management.j2ee.JCAConnectionFactory JCAConnectionFactory}.
 *
 * @author  <a href="mailto:mclaugs@comcast.net">Scott McLaughlin</a>.
 * @version $Revision: 1.7.2.4 $
 *
 * @jmx:mbean extends="org.jboss.management.j2ee.J2EEManagedObjectMBean"
 */
public class JCAConnectionFactory
      extends J2EEManagedObject
      implements JCAConnectionFactoryMBean
{
   // Constants -----------------------------------------------------
   private static Logger log = Logger.getLogger(JCAConnectionFactory.class);

   public static final String J2EE_TYPE = "JCAConnectionFactory";

   // Attributes ----------------------------------------------------

   /** The JBoss connection manager service name */
   private ObjectName cmServiceName;
   /** The JBoss managed connection service name */
   private ObjectName mcfServiceName;
   private ObjectName jsr77MCFName;
   private JCAConnectionPoolStatsImpl poolStats;

   // Static --------------------------------------------------------

   public static ObjectName create(MBeanServer mbeanServer, String resName,
      ObjectName jsr77ParentName, ObjectName ccmServiceNameName,
      ObjectName mcfServiceName)
   {
      ObjectName jsr77Name = null;
      try
      {
         JCAConnectionFactory jcaFactory = new JCAConnectionFactory(resName,
            jsr77ParentName, ccmServiceNameName, mcfServiceName);
         jsr77Name = jcaFactory.getObjectName();
         mbeanServer.registerMBean(jcaFactory, jsr77Name);
         log.debug("Created JSR-77 JCAConnectionFactory: " + resName);
         ObjectName jsr77MCFName = JCAManagedConnectionFactory.create(mbeanServer,
               resName, jsr77Name);
         jcaFactory.setManagedConnectionFactory(jsr77MCFName);
      }
      catch (Exception e)
      {
         log.debug("Could not create JSR-77 JCAConnectionFactory: " + resName, e);
      }
      return jsr77Name;
   }

   public static void destroy(MBeanServer mbeanServer, String resName)
   {
      try
      {
         J2EEManagedObject.removeObject(
               mbeanServer,
               J2EEDomain.getDomainName() + ":" +
               J2EEManagedObject.TYPE + "=" + JCAConnectionFactory.J2EE_TYPE + "," +
               "name=" + resName + "," +
               "*"
         );
      }
      catch (javax.management.InstanceNotFoundException infe)
      {
      }
      catch (Exception e)
      {
         log.debug("Could not destroy JSR-77 JCAConnectionFactory: " + resName, e);
      }
   }

   // Constructors --------------------------------------------------


   public JCAConnectionFactory(String resName, ObjectName jsr77ParentName,
      ObjectName ccmServiceNameName, ObjectName mcfServiceName)
      throws MalformedObjectNameException, InvalidParentException
   {
      super(J2EE_TYPE, resName, jsr77ParentName);
      this.cmServiceName = ccmServiceNameName;
      this.mcfServiceName = mcfServiceName;
   }

   // Public --------------------------------------------------------

   // javax.management.j2ee.JCAConnectionFactory implementation -----------------

   /**
    * @jmx:managed-attribute
    */
   public ObjectName getManagedConnectionFactory()
   {
      return jsr77MCFName;
   }
   void setManagedConnectionFactory(ObjectName jsr77MCFName)
   {
      this.jsr77MCFName = jsr77MCFName;
   }

   /**
    * @jmx:managed-operation
    */
   public JCAConnectionPoolStatsImpl getPoolStats(ObjectName poolServiceName)
   {
      TimeStatisticImpl waitTime = null;
      TimeStatisticImpl useTime = null;
      CountStatisticImpl closeCount = null;
      CountStatisticImpl createCount = null;
      BoundedRangeStatisticImpl freePoolSize = null;
      BoundedRangeStatisticImpl poolSize = null;
      RangeStatisticImpl waitingThreadCount = null;
      try
      {
         if( poolStats == null )
         {
            Integer max = (Integer) server.getAttribute(poolServiceName, "MaxSize");
            freePoolSize = new BoundedRangeStatisticImpl("FreePoolSize", "1",
                  "The free connection count", 0, max.longValue());
            poolSize = new BoundedRangeStatisticImpl("PoolSize", "1",
                  "The connection count", 0, max.longValue());
            poolStats = new JCAConnectionPoolStatsImpl(getObjectName(), jsr77MCFName,
               waitTime, useTime, closeCount, createCount, freePoolSize, poolSize,
               waitingThreadCount);
         }
         createCount = (CountStatisticImpl) poolStats.getCreateCount();
         closeCount = (CountStatisticImpl) poolStats.getCloseCount();
         freePoolSize = (BoundedRangeStatisticImpl) poolStats.getFreePoolSize();
         poolSize = (BoundedRangeStatisticImpl) poolStats.getPoolSize();

         // Update the stats
         Integer isize = (Integer) server.getAttribute(poolServiceName, "ConnectionCreatedCount");
         createCount.set(isize.longValue());
         isize = (Integer) server.getAttribute(poolServiceName, "ConnectionDestroyedCount");
         closeCount.set(isize.longValue());
         isize = (Integer) server.getAttribute(poolServiceName, "ConnectionCount");
         poolSize.set(isize.longValue());
         Long lsize = (Long) server.getAttribute(poolServiceName, "AvailableConnectionCount");
         freePoolSize.set(lsize.longValue());
      }
      catch (Exception e)
      {
         log.debug("Failed to update JCAConnectionPoolStats", e);
      }

      return poolStats;
   }

   // java.lang.Object overrides ------------------------------------

   public String toString()
   {
      return "JCAConnectionFactory { " + super.toString() + " } [ " +
            " ]";
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   /**
    * @return A hashtable with the JCAResource and J2EEServer
    */
   protected Hashtable getParentKeys(ObjectName parentName)
   {
      Hashtable keys = new Hashtable();
      Hashtable nameProps = parentName.getKeyPropertyList();
      String factoryName = (String) nameProps.get( "name");
      String serverName = (String) nameProps.get(J2EEServer.J2EE_TYPE);
      keys.put(J2EEServer.J2EE_TYPE, serverName);
      keys.put(JCAResource.J2EE_TYPE, factoryName);
      return keys;
   }

}
