/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.statistics;

import javax.management.j2ee.statistics.CountStatistic;
import javax.management.j2ee.statistics.JMSProducerStats;
import javax.management.j2ee.statistics.JMSConsumerStats;
import javax.management.j2ee.statistics.JMSSessionStats;
import javax.management.j2ee.statistics.TimeStatistic;


/**
 * Represents the statistics provided by a JMS Session
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.1 $
 */
public final class JMSSessionStatsImpl extends StatsBase
      implements JMSSessionStats
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------
   private JMSProducerStats[] mProducers;
   private JMSConsumerStats[] mConsumers;
   private CountStatistic mMessageCount;
   private CountStatistic mPendingMessageCount;
   private CountStatistic mExpiredMessageCount;
   private TimeStatistic mMessageWaitTime;
   private CountStatistic mDurableSubscriptionCount;

   // Constructors --------------------------------------------------

   public JMSSessionStatsImpl(
         JMSProducerStats[] pProducers,
         JMSConsumerStats[] pConsumers,
         CountStatistic pMessageCount,
         CountStatistic pPendingMessageCount,
         CountStatistic pExpiredMessageCount,
         TimeStatistic pMessageWaitTime,
         CountStatistic pDurableSubscriptionCount
         )
   {
      mProducers = (pProducers != null ? pProducers : new JMSProducerStats[0]);
      mConsumers = (pConsumers != null ? pConsumers : new JMSConsumerStats[0]);
      mMessageCount = pMessageCount;
      super.addStatistic("MessageCount", mMessageCount);
      mPendingMessageCount = pPendingMessageCount;
      super.addStatistic("PendingMessageCount", mPendingMessageCount);
      mExpiredMessageCount = pExpiredMessageCount;
      super.addStatistic("ExpiredMessageCount", mExpiredMessageCount);
      mMessageWaitTime = pMessageWaitTime;
      super.addStatistic("MessageWaitTime", mMessageWaitTime);
      mDurableSubscriptionCount = pDurableSubscriptionCount;
      super.addStatistic("DurableSubscriptionCount", mDurableSubscriptionCount);
   }

   // Public --------------------------------------------------------

   // javax.management.j2ee.JMSConnectionStats implementation -------

   public JMSProducerStats[] getProducers()
   {
      return mProducers;
   }

   public JMSConsumerStats[] getConsumers()
   {
      return mConsumers;
   }

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

   public CountStatistic getDurableSubscriptionCount()
   {
      return mDurableSubscriptionCount;
   }
}
