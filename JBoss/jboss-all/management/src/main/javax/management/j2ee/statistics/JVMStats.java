package javax.management.j2ee.statistics;

/**
 * Represents the statistics provided by a Java VM
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.2 $
 */
public interface JVMStats
   extends Stats
{
   /**
   * @return The amount of time this JVM has been running
   **/
   public CountStatistic getUpTime();

   /**
   * @return The size of the JVM's heap
   **/
   public BoundedRangeStatistic getHeapSize();
}
