/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.statistics;

import javax.management.j2ee.statistics.RangeStatistic;
import javax.management.j2ee.statistics.StatelessSessionBeanStats;

import org.jboss.management.j2ee.statistics.RangeStatisticImpl;

/** The JSR77.6.14 StatlessBeanStats implementation
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class StatelessSessionBeanStatsImpl extends EJBStatsImpl
   implements StatelessSessionBeanStats
{
   private RangeStatisticImpl methodReadyCount;

   public StatelessSessionBeanStatsImpl()
   {
      methodReadyCount = new RangeStatisticImpl("MethodReadyCount", "1",
         "The count of beans in the method-ready state");
      addStatistic("MethodReadyCount", methodReadyCount);
   }

// Begin javax.management.j2ee.statistics.StatelessSessionBeanStats interface methods

   public RangeStatistic getMethodReadyCount()
   {
      return methodReadyCount;
   }

// End javax.management.j2ee.statistics.StatelessSessionBeanStats interface methods
}
