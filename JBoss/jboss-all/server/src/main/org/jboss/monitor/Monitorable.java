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
 * @author <a href="mailto:simone.bordet@compaq.com">Simone Bordet</a>
 * @version $Revision: 1.4 $
 */
public interface Monitorable
{
	// Constants ----------------------------------------------------
	
	// Static -------------------------------------------------------

	// Public -------------------------------------------------------
	/**
	 * Samples the status of the implementor object and register the status
	 * into the snapshot argument.
	 */
	public void sample(Object snapshot);
  
	// Inner classes -------------------------------------------------
}
