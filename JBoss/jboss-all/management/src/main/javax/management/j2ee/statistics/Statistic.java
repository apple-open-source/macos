/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.j2ee.statistics;

/**
 * Base interface for a Statistic Information.
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.2 $
 */
public interface Statistic
{
   /**
    * @return Name of the Statistic
    */
   public String getName();

   /**
    * @return Unit of Measurement. For TimeStatistics valid values are "HOUR",
    *         "MINUTE", "SECOND", "MILLISECOND", "MICROSECOND", "NANOSECOND"
    */
   public String getUnit();

   /**
    * @return A human-readable description
    */
   public String getDescription();

   /**
    * @return The time the first measurment was taken represented as a long, whose
    *         value is the number of milliseconds since January 1, 1970, 00:00:00.
    */
   public long getStartTime();

   /**
    * @return The time the most recent measurment was taken represented as a long,
    *         whose value is the number of milliseconds since January 1, 1970, 00:00:00.
    */
   public long getLastSampleTime();
}
