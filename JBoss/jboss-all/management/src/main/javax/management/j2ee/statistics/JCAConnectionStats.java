package javax.management.j2ee.statistics;

import javax.management.ObjectName;
import javax.management.j2ee.statistics.TimeStatistic;

/**
 * Represents the statistics provided by a JCA connection
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.2 $
 */
public interface JCAConnectionStats
   extends Stats
{
   /**
    * @return ObjectName of the Connection Factory
    */
   ObjectName getConnectionFactory();

   /**
    * @return ObjectName of the Managed Connection Factory
    */
   ObjectName getManagedConnectionFactory();

   /**
   * @return The time spent waiting for a conection to be available
   */
   public TimeStatistic getWaitTime();

   /**
   * @return The time spent using a connection
   */
   public TimeStatistic getUseTime();
}
