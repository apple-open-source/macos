/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.deployment.scanner;

import javax.management.ObjectName;

import org.jboss.system.Service;

/**
 * Provides the basic interface for a deployment scanner.
 *
 * <p>A deployment scanner scans for new, removed or changed
 *    deployments.
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean"
 *
 * @version <tt>$Revision: 1.4 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public interface DeploymentScanner
   extends Service
{
   /**
    * The ObjectName of the {@link Deployer} which we will use.
    *
    * @param deployerName    The object name of the deployer to use.
    *
    * @jmx:managed-attribute
    */
   void setDeployer(ObjectName deployerName);

   /**
    * Get the ObjectName of the {@link Deployer} which we are using.
    *
    * @return    The object name of the deployer we are using.
    *
    * @jmx:managed-attribute
    */
   ObjectName getDeployer();

   /**
    * Set the scan period for the scanner.
    *
    * @param period    This is the time in milliseconds between scans.
    *
    * @throws IllegalArgumentException    Period value out of range.
    *
    * @jmx:managed-attribute
    */
   void setScanPeriod(long period);

   /**
    * Get the scan period for the scanner.
    *
    * @return    This is the time in milliseconds between scans.
    */
   long getScanPeriod();

   /**
    * Disable or enable the period based deployment scanning.
    *
    * <p>Manual scanning can still be performed by calling
    *    {@link #scan}.
    *
    * @param flag    True to enable or false to disable period
    *                based scanning.
    *
    * @jmx:managed-attribute
    */
   void setScanEnabled(boolean flag);

   /**
    * Check if period based scanning is enabled.
    *
    * @return    True if enabled, false if disabled.
    *
    * @jmx:managed-attribute
    */
   boolean isScanEnabled();

   /**
    * Scan for deployment changes.
    *
    * @throws IllegalStateException    Not initialized.
    * @throws Exception                Scan failed.
    *
    * @jmx:managed-operation
    */
   void scan() throws Exception;
}
