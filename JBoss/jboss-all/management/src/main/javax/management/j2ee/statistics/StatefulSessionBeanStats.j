/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.j2ee.statistics;

import javax.management.j2ee.statistics.SessionBeanStats;
import javax.management.j2ee.statistics.RangeStatistic;

/**
 * Represents the statistics provided by a stateful session bean as defined
 * by JSR77.6.15
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.2 $
 */
public interface StatefulSessionBeanStats
      extends SessionBeanStats
{
   /**
    * @return The number of beans that are in the passivated state
    */
   public RangeStatistic getPassiveCount();
}
