/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.statistics;

import javax.management.j2ee.statistics.CountStatistic;
import javax.management.j2ee.statistics.JMSEndpointStats;
import javax.management.j2ee.statistics.TimeStatistic;

/**
 * Represents a statistics provided by a JMS message producer or a
 * JMS message consumer
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.1 $
 */
public abstract class JMSEndpointStatsImpl extends StatsBase
      implements JMSEndpointStats
{
   // Attributes ----------------------------------------------------
   private CountStatistic mMessageCount;
   private CountStatistic mPendingMessageCount;
   private CountStatistic mExpiredMessageCount;
   private TimeStatistic mMessageWaitTime;

   // Constructors --------------------------------------------------

   public JMSEndpointStatsImpl(
         CountStatistic pMessageCount,
         CountStatistic pPendingMessageCount,
         CountStatistic pExpiredMessageCount,
         TimeStatistic pMessageWaitTime
         )
   {
      mMessageCount = pMessageCount;
      super.addStatistic("MessageCount", mMessageCount);
      mPendingMessageCount = pPendingMessageCount;
      super.addStatistic("PendingMessageCount", mPendingMessageCount);
      mExpiredMessageCount = pExpiredMessageCount;
      super.addStatistic("ExpiredMessageCoun", mExpiredMessageCount);
      mMessageWaitTime = pMessageWaitTime;
      super.addStatistic("MessageWaitTime", mMessageWaitTime);
   }

   // Public --------------------------------------------------------

   // javax.management.j2ee.JMSConnectionStats implementation -------

   public CountStatistic getMessageCount()
   {
      return mMessageCount;
   }

   public CountStatistic getPendingMessageCount()
   {
      return mPendingMessageCount;
   }

   public CountStatistic getExpiredMessageCount()
   {
      return mExpiredMessageCount;
   }

   public TimeStatistic getMessageWaitTime()
   {
      return mMessageWaitTime;
   }
}
