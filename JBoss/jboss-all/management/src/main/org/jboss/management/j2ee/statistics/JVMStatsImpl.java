/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.statistics;

import javax.management.j2ee.statistics.CountStatistic;
import javax.management.j2ee.statistics.JVMStats;
import javax.management.j2ee.statistics.BoundedRangeStatistic;

/** The JSR77.6.32 JMVStats implementation
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class JVMStatsImpl extends StatsBase
   implements JVMStats
{
   public JVMStatsImpl()
   {
      this(new CountStatisticImpl("UpTime", "MILLISECOND", "Time the VM has been running"),
           new BoundedRangeStatisticImpl("HeapSize", "Bytes", "Size of the VM's heap", 0, 0)
      );
   }

   public JVMStatsImpl(CountStatistic upTime, BoundedRangeStatistic heapSize)
   {
      addStatistic("UpTime", upTime);
      addStatistic("HeapSize", heapSize);
   }

// Begin javax.management.j2ee.statistics.JVMStats interface methods
   public CountStatistic getUpTime()
   {
      CountStatisticImpl upTime = (CountStatisticImpl) getStatistic("UpTime");
      long now = System.currentTimeMillis();
      long elapsed = now - upTime.getStartTime();
      upTime.set(elapsed);
      return upTime;
   }

   public BoundedRangeStatistic getHeapSize()
   {
      BoundedRangeStatisticImpl heapSize = (BoundedRangeStatisticImpl) getStatistic("HeapSize");
      long totalMemory = Runtime.getRuntime().totalMemory();
      heapSize.set(totalMemory);
      return heapSize;
   }
// End javax.management.j2ee.statistics.JVMStats interface methods
}
