/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.monitor;

import java.util.Map;

/**
 *   
 * @author <a href="mailto:andreas.schaefer@madplanet.com">Andreas Schaefer</a>
 * @version $Revision: 1.2 $
 */
public interface StatisticsProvider
{
	// Constants ----------------------------------------------------
	
	// Public -------------------------------------------------------
   /**
   * @return Map of Statistic Instances where the key is the
   *         name of the Statistic instance. The valid values are
   *         either any subclass of Statistic or an array of Statisc
   *         instances. Returns null if no statistics is provided.
   **/
   public Map retrieveStatistic();
   /**
   * Resets all the statistic values kept in this monitorable instance
   **/
   public void resetStatistic();

}
