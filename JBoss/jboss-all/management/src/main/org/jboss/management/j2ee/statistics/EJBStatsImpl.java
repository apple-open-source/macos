/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.statistics;

import javax.management.j2ee.statistics.CountStatistic;
import javax.management.j2ee.statistics.EJBStats;

/** The JSR77.6.11 EJBStats implementation
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public abstract class EJBStatsImpl extends StatsBase
   implements EJBStats
{
   public EJBStatsImpl()
   {
      this(new CountStatisticImpl("CreateCount", "1", "Number of creates"),
           new CountStatisticImpl("RemoveCount", "1", "Number of removes")
      );
   }

   public EJBStatsImpl(CountStatistic createCount, CountStatistic removeCount)
   {
      addStatistic("CreateCount", createCount);
      addStatistic("RemoveCount", removeCount);
   }

// Begin javax.management.j2ee.statistics.EJBStats interface methods
   public CountStatistic getCreateCount()
   {
      return (CountStatistic) getStatistic("CreateCount");
   }

   public CountStatistic getRemoveCount()
   {
      return (CountStatistic) getStatistic("RemoveCount");
   }
// End javax.management.j2ee.statistics.EJBStats interface methods
}
