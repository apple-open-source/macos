/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.statistics;

import javax.management.j2ee.statistics.RangeStatistic;
import javax.management.j2ee.statistics.EntityBeanStats;

import org.jboss.management.j2ee.statistics.RangeStatisticImpl;

/** The JSR77.6.12 EntityBeanStats implementation
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class EntityBeanStatsImpl extends EJBStatsImpl
   implements EntityBeanStats
{
   private RangeStatisticImpl methodReadyCount;
   private RangeStatisticImpl pooledCount;

   public EntityBeanStatsImpl()
   {
      methodReadyCount = new RangeStatisticImpl("ReadyCount", "1",
         "The count of beans in the ready state");
      pooledCount = new RangeStatisticImpl("PooledCount", "1",
         "The count of beans in the pooled state");
      addStatistic("ReadyCount", methodReadyCount);
      addStatistic("PooledCount", pooledCount);
   }

// Begin javax.management.j2ee.statistics.EntityBeanStats interface methods

   public RangeStatistic getReadyCount()
   {
      return methodReadyCount;
   }
   public RangeStatistic getPooledCount()
   {
      return pooledCount;
   }

// End javax.management.j2ee.statistics.EntityBeanStats interface methods
}
