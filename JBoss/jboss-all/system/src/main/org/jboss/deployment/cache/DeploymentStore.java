/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.deployment.cache;

import java.net.URL;

import java.io.IOException;

import java.util.Collection;

/**
 * Provides the interface for abstracting the actual storage
 * of cached deployments.
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean"
 *
 * @todo Expose urls for cleaning
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public interface DeploymentStore
{
   /**
    * Get the stored URL for the given deployment URL.
    *
    * @param url    The original deployment URL.
    * @return       The stored URL or null if not stored.
    *
    * @throws Exception    Failed to get deployment URL from the store.
    *
    * @jmx:managed-operation
    */
   URL get(URL url) throws Exception;

   /**
    * Put a deployment URL into storage.  This will cause
    * the data associated with the given URL to be downloaded.
    * 
    * <p>If there is already a stored URL it will be overwritten.
    *
    * @param url    The original deployment URL.
    * @return       The stored URL.
    *
    * @throws Exception    Failed to put deployment URL into the store.
    *
    * @jmx:managed-operation
    */
   URL put(URL url) throws Exception;
}
