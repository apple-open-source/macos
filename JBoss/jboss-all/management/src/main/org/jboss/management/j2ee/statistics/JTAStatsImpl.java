/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.statistics;

import javax.management.j2ee.statistics.CountStatistic;
import javax.management.j2ee.statistics.JTAStats;

/** The JSR77.6.30 JTAStats implementation
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class JTAStatsImpl extends StatsBase
   implements JTAStats
{
   public JTAStatsImpl()
   {
      this(new CountStatisticImpl("ActiveCount", "1", "The number of active transactions"),
           new CountStatisticImpl("CommitedCount", "1", "The number of transactions committed"),
           new CountStatisticImpl("RolledbackCount", "1", "The number of transactions rolled back")
      );
   }

   public JTAStatsImpl(CountStatistic activeCount, CountStatistic commitCount,
         CountStatistic rollbackCount)
   {
      addStatistic("ActiveCount", activeCount);
      addStatistic("CommitedCount", commitCount);
      addStatistic("RolledbackCount", rollbackCount);
   }

// Begin javax.management.j2ee.statistics.JTAStats interface methods
   public CountStatistic getActiveCount()
   {
      CountStatisticImpl active = (CountStatisticImpl) getStatistic("ActiveCount");
      return active;
   }
   public CountStatistic getCommitedCount()
   {
      CountStatisticImpl active = (CountStatisticImpl) getStatistic("CommitedCount");
      return active;
   }
   public CountStatistic getRolledbackCount()
   {
      CountStatisticImpl active = (CountStatisticImpl) getStatistic("RolledbackCount");
      return active;
   }
// End javax.management.j2ee.statistics.JTAStats interface methods
}
