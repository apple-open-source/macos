/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.statistics;

import javax.management.j2ee.statistics.RangeStatistic;
import javax.management.j2ee.statistics.StatefulSessionBeanStats;

import org.jboss.management.j2ee.statistics.RangeStatisticImpl;

/** The JSR77.6.11 EJBStats implementation
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class StatefulSessionBeanStatsImpl extends EJBStatsImpl
   implements StatefulSessionBeanStats
{
   private RangeStatisticImpl methodReadyCount;
   private RangeStatisticImpl passiveCount;

   public StatefulSessionBeanStatsImpl()
   {
      methodReadyCount = new RangeStatisticImpl("MethodReadyCount", "1",
         "The count of beans in the method-ready state");
      passiveCount = new RangeStatisticImpl("PassiveCount", "1",
         "The count of beans in the passivated state");
      addStatistic("MethodReadyCount", methodReadyCount);
      addStatistic("PassiveCount", passiveCount);
   }

// Begin javax.management.j2ee.statistics.StatefulSessionBeanStats interface methods

   public RangeStatistic getMethodReadyCount()
   {
      return methodReadyCount;
   }
   public RangeStatistic getPassiveCount()
   {
      return passiveCount;
   }

// End javax.management.j2ee.statistics.StatefulSessionBeanStats interface methods
}
