package javax.management.j2ee.statistics;

import javax.management.j2ee.statistics.TimeStatistic;

/**
 * Represents a statistics provided by a JMS message producer or a
 * JMS message consumer
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
public interface JMSEndpointStats
   extends Stats
{
   // Constants -----------------------------------------------------
   
   // Public --------------------------------------------------------
   
   /**
   * @return The number of messages sent or received
   **/
   public CountStatistic getMessageCount();
   
   /**
   * @return The number of pending messages
   **/
   public CountStatistic getPendingMessageCount();
   
   /**
   * @return The number of messages that expired before delivery
   **/
   public CountStatistic getExpiredMessageCount();
   
   /**
   * @return The time spent by a message before being delivered
   **/
   public TimeStatistic getMessageWaitTime();
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Static inner classes -------------------------------------------------
}
