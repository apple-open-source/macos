/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee;

import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import javax.management.j2ee.statistics.Stats;

import org.jboss.logging.Logger;
import org.jboss.management.j2ee.statistics.EntityBeanStatsImpl;
import org.jboss.management.j2ee.statistics.RangeStatisticImpl;

/** The JBoss JSR-77.3.12 implementation of the StatelessSessionBean model
 *
 * @author  <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @version $Revision: 1.2.2.4 $
 *
 * @jmx:mbean extends="org.jboss.management.j2ee.EJBMBean"
 **/
public class EntityBean
   extends EJB
   implements EntityBeanMBean
{
   // Constants -----------------------------------------------------
   public static final String J2EE_TYPE = "EntityBean";

   // Attributes ----------------------------------------------------
   private static Logger log = Logger.getLogger(EntityBean.class);

   private EntityBeanStatsImpl stats;
   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /** Create an EntityBean model
    *
    * @param name the ejb name, currently the JNDI name
    * @param ejbModuleName the JSR-77 EJBModule name for this bean
    * @param ejbContainerName the JMX name of the JBoss ejb container MBean
    * @throws MalformedObjectNameException
    * @throws InvalidParentException
    */
   public EntityBean( String name, ObjectName ejbModuleName,
      ObjectName ejbContainerName)
      throws MalformedObjectNameException,
         InvalidParentException
   {
      super( J2EE_TYPE, name, ejbModuleName, ejbContainerName );
      stats = new EntityBeanStatsImpl();
   }

   // Begin StatisticsProvider interface methods
   public Stats getStats()
   {
      try
      {
         updateCommonStats(stats);

         ObjectName poolName = getContainerPoolName();
         RangeStatisticImpl pooledCount = (RangeStatisticImpl) stats.getReadyCount();
         Integer poolSize = (Integer) server.getAttribute(poolName, "CurrentSize");
         pooledCount.set(poolSize.longValue());

         ObjectName cacheName = getContainerCacheName();
         RangeStatisticImpl readyCount = (RangeStatisticImpl) stats.getReadyCount();
         Long count = (Long) server.getAttribute(cacheName, "CacheSize");
         readyCount.set(count.longValue());
      }
      catch(Exception e)
      {
         log.debug("Failed to retrieve stats", e);
      }
      return stats;
   }
   public void resetStats()
   {
      stats.reset();
   }
   // End StatisticsProvider interface methods

   // Object overrides ---------------------------------------------------

   public String toString()
   {
      return "EntityBean { " + super.toString() + " } []";
   }
}
