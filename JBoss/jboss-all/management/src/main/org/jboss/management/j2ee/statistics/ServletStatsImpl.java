package org.jboss.management.j2ee.statistics;

import javax.management.j2ee.statistics.ServletStats;
import javax.management.j2ee.statistics.TimeStatistic;

/** The implementation of the JSR77 ServletStats model
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public class ServletStatsImpl extends StatsBase
   implements ServletStats
{
   public ServletStatsImpl()
   {
      this(new TimeStatisticImpl("ServiceTime", TimeStatistic.MILLISECOND,
         "The execution time of the servlet")
      );
   }
   public ServletStatsImpl(TimeStatistic serviceTime)
   {
      addStatistic("ServiceTime", serviceTime);
   }

   public TimeStatistic getServiceTime()
   {
      return (TimeStatistic) getStatistic("ServiceTime");
   }
}
