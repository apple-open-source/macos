/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.j2ee.statistics;

/** Represents a standard Time Measurement as defined by JSR77.6.6
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.2 $
 */
public interface TimeStatistic
      extends Statistic
{
   // Constants -----------------------------------------------------

   public static final String HOUR = "HOUR";
   public static final String MINUTE = "MINUTE";
   public static final String SECOND = "SECOND";
   public static final String MILLISECOND = "MILLISECOND";
   public static final String MICROSECOND = "MICROSECOND";
   public static final String NANOSECOND = "NANOSECOND";

   /**
    * @return The number of times a time measurements was added
    */
   public long getCount();

   /**
    * @return The maximum time added since start of the measurements
    */
   public long getMaxTime();

   /**
    * @return The minimum time added since start of the measurements
    */
   public long getMinTime();

   /**
    * @return The sum of all the time added to the measurements since
    *         it started
    */
   public long getTotalTime();
}
