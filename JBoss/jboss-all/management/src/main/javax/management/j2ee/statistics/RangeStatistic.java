/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.j2ee.statistics;

import javax.management.j2ee.statistics.Statistic;

/**
 * Represents a standard measurements of the lowest and highest
 * value an attribute has held as well as its current value as defined by
 * JSR77.6.7
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.2 $
 */
public interface RangeStatistic
      extends Statistic
{
   /**
    * @return The highest value this attribute has held since the beginning of
    *        the measurements
    */
   public long getHighWaterMark();

   /**
    * @return The lowest value this attribute has held since the beginning of
    *         the measurements
    */
   public long getLowWaterMark();

   /**
    * @return The current value of the attribute
    */
   public long getCurrent();
}
