package javax.management.j2ee.statistics;

import javax.management.j2ee.statistics.TimeStatistic;

/**
 * Represents the statistics provided by a JMS Session
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.1 $
 *   
 * <p><b>Revisions:</b>
 *
 * <p><b>200112009 Andreas Schaefer:</b>
 * <ul>
 * <li> Adjustment to the new JBoss guide lines and also adjustments
 *      to the latest JSR-77 specification
 * </ul>
 **/
public interface JMSSessionStats
   extends Stats
{
   // Constants -----------------------------------------------------
   
   // Public --------------------------------------------------------
   
   /**
   * @return The list of JMSProducerStats that provide statistics about the message
   *         producers associated with the referencing JMS session statistics
   **/
   public JMSProducerStats[] getProducers();
   
   /**
   * @return The list of JMSConsumerStats that provide statistics about the message
   *         consumers associated with the referencing JMS session statistics.
   **/
   public JMSConsumerStats[] getConsumers();
   
   /**
   * @return The number of messages exchanged
   **/
   public CountStatistic getMessageCount();
   
   /**
   * @return The number of pending messages
   **/
   public CountStatistic getPendingMessageCount();
   
   /**
   * @return The number of expired messages
   **/
   public CountStatistic getExpiredMessageCount();
   
   /**
   * @return The time spent by a message before being delivered
   **/
   public TimeStatistic getMessageWaitTime();
   
   /**
   * @return The number of durable subscriptions
   **/
   public CountStatistic getDurableSubscriptionCount();
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Static inner classes -------------------------------------------------
}
