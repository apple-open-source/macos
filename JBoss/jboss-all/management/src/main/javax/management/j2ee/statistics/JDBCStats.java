package javax.management.j2ee.statistics;

import javax.management.j2ee.statistics.JDBCConnectionStats;

/**
 * Represents the statistics provided by a JDBC resource.
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
public interface JDBCStats
   extends Stats
{
   // Constants -----------------------------------------------------
   
   // Public --------------------------------------------------------
   
   /**
   * @return The list of JDBCConnectionStats that provide statistics about the nonpooled
   *         connections associated with the referencing JDBC resource statistics.
   **/
   public JDBCConnectionStats[] getConnections();
   
   /**
   * @return The list of JDBCConnectionPoolStats that provide statistics about the
   *         connection pools associated with the referencing JDBC resource statistics.
   **/
   public JDBCConnectionPoolStats[] getConnectionPools();
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Static inner classes -------------------------------------------------
}
