/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.deployment;

import java.net.URL;

/**
 * The interface which a deployer must implement.
 *
 * <p>Clients should use the MainDeployer to deploy URLs.
 *
 * @jmx:mbean
 *
 * @version <tt>$Revision: 1.3 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public interface Deployer
{
   /**
    * Deploys a package identified by a URL
    *
    * @param url an <code>URL</code> value
    *
    * @throws DeploymentException    Failed to deploy URL.
    *
    * @jmx:managed-operation
    */
   void deploy(URL url) throws DeploymentException;

   /**
    * Undeploys a package identified by a URL
    *
    * @param url an <code>URL</code> value
    *
    * @throws DeploymentException    Failed to undeploy URL.
    *
    * @jmx:managed-operation
    */
   void undeploy(URL url) throws DeploymentException;

   /**
    * Tells you if a packaged identified by a URL is deployed.
    *
    * @param url an <code>URL</code> value
    * @return a <code>boolean</code> value
    *
    * @jmx:managed-operation
    */
   boolean isDeployed(URL url);
}
