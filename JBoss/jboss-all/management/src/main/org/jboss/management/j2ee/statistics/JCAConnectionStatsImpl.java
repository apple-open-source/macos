package org.jboss.management.j2ee.statistics;

import javax.management.ObjectName;
import javax.management.j2ee.statistics.JCAConnectionStats;
import javax.management.j2ee.statistics.TimeStatistic;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class JCAConnectionStatsImpl extends StatsBase
   implements JCAConnectionStats
{
   private ObjectName cfName;
   private ObjectName mcfName;

   public JCAConnectionStatsImpl(ObjectName cfName, ObjectName mcfName)
   {
      this(cfName, mcfName, null, null);
   }
   public JCAConnectionStatsImpl(ObjectName cfName, ObjectName mcfName,
      TimeStatistic waitTime, TimeStatistic useTime)
   {
      if( waitTime == null )
      {
         waitTime = new TimeStatisticImpl("WaitTime", TimeStatistic.MILLISECOND,
            "Time spent waiting for a connection to be available");
      }
      if( useTime == null )
      {
         useTime = new TimeStatisticImpl("UseTime", TimeStatistic.MILLISECOND,
            "Time spent using the connection");
      }
      super.addStatistic("WaitTime", waitTime);
      super.addStatistic("UseTime", useTime);
      this.cfName = cfName;
      this.mcfName = mcfName;
   }

   public ObjectName getConnectionFactory()
   {
      return cfName;
   }

   public ObjectName getManagedConnectionFactory()
   {
      return mcfName;
   }

   public TimeStatistic getWaitTime()
   {
      TimeStatistic ts = (TimeStatistic) getStatistic("WaitTime");
      return ts;
   }

   public TimeStatistic getUseTime()
   {
      TimeStatistic ts = (TimeStatistic) getStatistic("UseTime");
      return ts;
   }
}
