package javax.management.j2ee.statistics;

/**
 * Represents the statistics provided by a JCA resource.
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.1 $
 *   
 * <p><b>Revisions:</b>
 *
 * <p><b>20020307 Andreas Schaefer:</b>
 * <ul>
 * <li> Creation
 * </ul>
 **/
public interface JCAStats
   extends Stats
{
   // Constants -----------------------------------------------------
   
   // Public --------------------------------------------------------
   
   /**
   * @return The list of JCAConnectionStats that provide statistics about the nonpooled
   *         connections associated with the referencing JCA resource statistics.
   **/
   public JCAConnectionStats[] getConnections();
   
   /**
   * @return The list of JCAConnectionPoolStats that provide statistics about the
   *         connection pools associated with the referencing JCA resource statistics.
   **/
   public JCAConnectionPoolStats[] getConnectionPools();
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Static inner classes -------------------------------------------------
}
