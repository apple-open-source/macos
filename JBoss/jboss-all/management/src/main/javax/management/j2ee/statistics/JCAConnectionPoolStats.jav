package javax.management.j2ee.statistics;

import javax.management.j2ee.statistics.JCAConnectionStats;

/**
 * Represents the statistics provided by a JCA connection pool
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
public interface JCAConnectionPoolStats
   extends JCAConnectionStats
{
   // Constants -----------------------------------------------------
   
   // Public --------------------------------------------------------
   
   /**
   * @return The number of connections closed
   **/
   public CountStatistic getCloseCount();

   /**
   * @return The number of connections created
   **/
   public CountStatistic getCreateCount();

   /**
   * @return The number of free connections in the pool
   **/
   public BoundedRangeStatistic getFreePoolSize();

   /**
   * @return The size of the connection pool
   **/
   public BoundedRangeStatistic getPoolSize();

   /**
   * @return The number of threats waiting for a connection
   **/
   public RangeStatistic getWaitingThreadCount();
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Static inner classes -------------------------------------------------
}
