/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.j2ee.statistics;

import javax.management.j2ee.statistics.Statistic;

/** Represents a standard Count Measurement as defined by JSR77.6.5
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.2 $
 */
public interface CountStatistic
   extends Statistic
{

   /**
    * @return The Count since the last reset
    */
   public long getCount();
}
