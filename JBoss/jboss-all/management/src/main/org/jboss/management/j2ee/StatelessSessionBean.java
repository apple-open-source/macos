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
import org.jboss.management.j2ee.statistics.StatelessSessionBeanStatsImpl;
import org.jboss.logging.Logger;

/** The JBoss JSR-77.3.15 implementation of the StatelessSessionBean model
 *
 * @author  <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @version $Revision: 1.2.2.4 $
 *
 * @jmx:mbean extends="org.jboss.management.j2ee.SessionBeanMBean"
 */
public class StatelessSessionBean
   extends SessionBean
   implements StatelessSessionBeanMBean
{
   // Constants -----------------------------------------------------
   public static final String J2EE_TYPE = "StatelessSessionBean";

   // Attributes ----------------------------------------------------
   private static Logger log = Logger.getLogger(StatelessSessionBean.class);
   private StatelessSessionBeanStatsImpl stats;

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
   public StatelessSessionBean( String name, ObjectName ejbModuleName,
      ObjectName ejbContainerName)
      throws MalformedObjectNameException,
         InvalidParentException
   {
      super( J2EE_TYPE, name, ejbModuleName, ejbContainerName);
      stats = new StatelessSessionBeanStatsImpl();
   }

   // Begin StatisticsProvider interface methods
   public Stats getStats()
   {
      try
      {
         updateCommonStats(stats);

         ObjectName poolName = getContainerPoolName();
         RangeStatisticImpl readyCount = (RangeStatisticImpl) stats.getMethodReadyCount();
         Integer poolSize = (Integer) server.getAttribute(poolName, "CurrentSize");
         readyCount.set(poolSize.longValue());
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
      return "StatelessSessionBean { " + super.toString() + " } []";
   }

}
