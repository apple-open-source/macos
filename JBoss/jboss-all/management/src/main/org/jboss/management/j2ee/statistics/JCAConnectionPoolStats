package org.jboss.management.j2ee.statistics;

import javax.management.j2ee.statistics.JCAConnectionPoolStats;
import javax.management.j2ee.statistics.CountStatistic;
import javax.management.j2ee.statistics.BoundedRangeStatistic;
import javax.management.j2ee.statistics.RangeStatistic;
import javax.management.j2ee.statistics.TimeStatistic;
import javax.management.ObjectName;

/** The JSR77.6.20 JCAConnectionPoolStats implementation
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class JCAConnectionPoolStatsImpl extends JCAConnectionStatsImpl
   implements JCAConnectionPoolStats
{
   public JCAConnectionPoolStatsImpl(ObjectName cfName, ObjectName mcfName,
      BoundedRangeStatistic freePoolSize, BoundedRangeStatistic poolSize)
   {
      this(cfName, mcfName, null, null, null, null, freePoolSize, poolSize, null);
   }
   public JCAConnectionPoolStatsImpl(ObjectName cfName, ObjectName mcfName,
      TimeStatistic waitTime, TimeStatistic useTime, CountStatistic closeCount,
      CountStatistic createCount, BoundedRangeStatistic freePoolSize,
      BoundedRangeStatistic poolSize, RangeStatistic waitingThreadCount)
   {
      super(cfName, mcfName, waitTime, useTime);
      if( closeCount == null )
      {
         closeCount = new CountStatisticImpl("CloseCount", "1",
            "The number of connection closes");
      }
      if( createCount == null )
      {
         createCount = new CountStatisticImpl("CreateCount", "1",
            "The number of connection creates");
      }
      if( waitingThreadCount == null )
      {
         waitingThreadCount = new RangeStatisticImpl("WaitingThreadCount",
               "1", "The number of threads waiting for a connection");
      }
      super.addStatistic("CloseCount", closeCount);
      super.addStatistic("CreateCount", createCount);
      super.addStatistic("FreePoolSize", freePoolSize);
      super.addStatistic("PoolSize", poolSize);
      super.addStatistic("WaitingThreadCount", waitingThreadCount);
   }

   public CountStatistic getCloseCount()
   {
      CountStatistic cs = (CountStatistic) getStatistic("CloseCount");
      return cs;
   }

   public CountStatistic getCreateCount()
   {
      CountStatistic cs = (CountStatistic) getStatistic("CreateCount");
      return cs;
   }

   public BoundedRangeStatistic getFreePoolSize()
   {
      BoundedRangeStatistic brs = (BoundedRangeStatistic) getStatistic("FreePoolSize");
      return brs;
   }

   public BoundedRangeStatistic getPoolSize()
   {
      BoundedRangeStatistic brs = (BoundedRangeStatistic) getStatistic("PoolSize");
      return brs;
   }

   public RangeStatistic getWaitingThreadCount()
   {
      RangeStatistic rs = (RangeStatistic) getStatistic("WaitingThreadCount");
      return rs;
   }
}
