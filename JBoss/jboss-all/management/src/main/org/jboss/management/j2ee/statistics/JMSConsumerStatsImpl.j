/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.statistics;

import javax.management.j2ee.statistics.CountStatistic;
import javax.management.j2ee.statistics.JMSConsumerStats;
import javax.management.j2ee.statistics.TimeStatistic;

/**
 * Represents a statistics provided by a JMS message producer
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.1 $
 */
public final class JMSConsumerStatsImpl
      extends JMSEndpointStatsImpl
      implements JMSConsumerStats
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   private String mOrigin;

   // Constructors --------------------------------------------------

   public JMSConsumerStatsImpl(
         String pOrigin,
         CountStatistic pMessageCount,
         CountStatistic pPendingMessageCount,
         CountStatistic pExpiredMessageCount,
         TimeStatistic pMessageWaitTime
         )
   {
      super(pMessageCount, pPendingMessageCount, pExpiredMessageCount, pMessageWaitTime);
      mOrigin = pOrigin;
   }

   // javax.management.j2ee.JMSConsumerStats implementation ---------

   public String getOrigin()
   {
      return mOrigin;
   }
}
