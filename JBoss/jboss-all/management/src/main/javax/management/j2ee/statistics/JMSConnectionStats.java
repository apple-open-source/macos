package javax.management.j2ee.statistics;

import javax.management.j2ee.statistics.JMSSessionStats;
import javax.management.j2ee.statistics.Stats;

/**
 * Represents the statistics provided by a JMS Connection.
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
public interface JMSConnectionStats
   extends Stats
{
   // Constants -----------------------------------------------------
   
   // Public --------------------------------------------------------
   
   /**
   * @return The list of JMSSessionStats that provide statistics about the sessions
   *         associated with the referencing JMSConnectionStats.
   **/
   public JMSSessionStats[] getSessions();
   
   /**
   * @return The transactional state of this JMS connection. If true, indicates that
   *         this JMS connection is transactional.
   **/
   public boolean isTransactional();
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Static inner classes -------------------------------------------------
}
