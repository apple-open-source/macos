/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.statistics;

import javax.management.j2ee.statistics.CountStatistic;
import javax.management.j2ee.statistics.JMSProducerStats;
import javax.management.j2ee.statistics.TimeStatistic;
import org.jboss.management.j2ee.statistics.JMSEndpointStatsImpl;

/**
 * Represents a statistics provided by a JMS message producer
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.1 $
 */
public final class JMSProducerStatsImpl
      extends JMSEndpointStatsImpl
      implements JMSProducerStats
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   private String mDestination;

   // Constructors --------------------------------------------------

   public JMSProducerStatsImpl(
         String pDestination,
         CountStatistic pMessageCount,
         CountStatistic pPendingMessageCount,
         CountStatistic pExpiredMessageCount,
         TimeStatistic pMessageWaitTime
         )
   {
      super(pMessageCount, pPendingMessageCount, pExpiredMessageCount, pMessageWaitTime);
      mDestination = pDestination;
   }

   // Public --------------------------------------------------------

   // javax.management.j2ee.JMSProducerStats implementation ---------

   public String getDestination()
   {
      return mDestination;
   }
}
