/***************************************
*                                     *
*  JBoss: The OpenSource J2EE WebOS   *
*                                     *
*  Distributable under LGPL license.  *
*  See terms of license at gnu.org.   *
*                                     *
***************************************/
package org.jboss.ha.singleton;

import org.jboss.ha.jmx.HAServiceMBean;

/**
 * <p>
 * HA-Singleton interface.
 * Only one mbean is active at any point of time cluster-wide.
 * </p>
 *
 * <p> 
 * The abstract service provides a simple way for a concrete mbeans to detect whether
 * or not it is the active one in the cluster.
 * </p>
 * 
 * <p>
 * Concrete mbeans would usually do activities like regular clean up of database tables
 * or saving statistics about cluster usage.         
 * </p>
 * 
 * @author <a href="mailto:ivelin@apache.org">Ivelin Ivanov</a>
 * @version $Revision: 1.1.2.2 $
 *
 * <p><b>Revisions:</b><br>
 */

public interface HASingletonMBean extends HAServiceMBean
{

	/**
	 * 
	 * @return true if this cluster node has the active mbean singleton.
	 * false otherwise
	 */
	public boolean isMasterNode();

}
