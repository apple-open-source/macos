/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.statistics;

import javax.management.j2ee.statistics.Stats;

/** Indicates that this Managed Object implements the JSR-77.6
 * StatisticsProvider model
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.2 $
 */
public interface StatisticsProvider
{
   /**
    * @return The statistics container containing all the statistics available
    */
   public Stats getStats();

   /** Called to resetStats all statistics in the provider
    *
    */
   public void resetStats();
}
