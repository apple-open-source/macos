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

import org.jboss.management.j2ee.statistics.RangeStatisticImpl;
import org.jboss.management.j2ee.statistics.StatefulSessionBeanStatsImpl;
import org.jboss.logging.Logger;

/** The JBoss JSR-77.3.14 implementation of the StatefulSessionBean model
 *
 * @author  <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @version $Revision: 1.2.2.4 $
 *
 * @jmx:mbean extends="org.jboss.management.j2ee.SessionBeanMBean"
 */
public class StatefulSessionBean
   extends SessionBean
   implements StatefulSessionBeanMBean
{
   private static Logger log = Logger.getLogger(StatefulSessionBean.class);

   private StatefulSessionBeanStatsImpl stats;

   // Constants -----------------------------------------------------

   public static final String J2EE_TYPE = "StatefulSessionBean";

   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /** Create a StatelessSessionBean model
    *
    * @param name the ejb name, currently the JNDI name
    * @param ejbModuleName the JSR-77 EJBModule name for this bean
    * @param ejbContainerName the JMX name of the JBoss ejb container MBean
    * @throws MalformedObjectNameException
    * @throws InvalidParentException
    */
   public StatefulSessionBean(String name, ObjectName ejbModuleName,
      ObjectName ejbContainerName)
      throws MalformedObjectNameException,
         InvalidParentException
   {
      super( J2EE_TYPE, name, ejbModuleName, ejbContainerName );
      stats = new StatefulSessionBeanStatsImpl();
   }

// Begin StatisticsProvider interface methods

   /** Query the ejb container and its mbeans for the bean stats:
    * CreateCount,
    * RemoveCount,
    * MethodReadyCount,
    * PassiveCount
    *
    * @return the StatefulSessionBeanStats for this bean
    */
   public Stats getStats()
   {
      try
      {
         updateCommonStats(stats);

         ObjectName poolName = getContainerPoolName();
         RangeStatisticImpl readyCount = (RangeStatisticImpl) stats.getMethodReadyCount();
         Integer poolSize = (Integer) server.getAttribute(poolName, "CurrentSize");
         readyCount.set(poolSize.longValue());

         ObjectName cacheName = getContainerCacheName();
         RangeStatisticImpl passiveCount = (RangeStatisticImpl) stats.getPassiveCount();
         Long passive = (Long) server.getAttribute(cacheName, "PassivatedCount");
         passiveCount.set(passive.longValue());
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
      return "StatefulSessionBean { " + super.toString() + " } []";
   }

}
