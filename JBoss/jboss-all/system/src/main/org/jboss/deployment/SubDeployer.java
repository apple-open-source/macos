/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.deployment;

import javax.management.ObjectName;

/**
 * The common interface for sub-deployer components which
 * perform the actual deployment services for application
 * components.
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean"
 *
 * @version <tt>$Revision: 1.2.4.2 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author  <a href="mailto:toby.allsopp@peace.com">Toby Allsopp</a>
 * @author  <a href="mailto:marc.fleury@Jboss.org">Marc Fleury</a>
 */
public interface SubDeployer
{
   /** The notification type send when a SubDeployer completes init */
   public static final String INIT_NOTIFICATION = "org.jboss.deployment.SubDeployer.init";
   /** The notification type send when a SubDeployer completes create */
   public static final String CREATE_NOTIFICATION = "org.jboss.deployment.SubDeployer.create";
   /** The notification type send when a SubDeployer completes start */
   public static final String START_NOTIFICATION = "org.jboss.deployment.SubDeployer.start";
   /** The notification type send when a SubDeployer completes stop */
   public static final String STOP_NOTIFICATION = "org.jboss.deployment.SubDeployer.stop";
   /** The notification type send when a SubDeployer completes destroy */
   public static final String DESTROY_NOTIFICATION = "org.jboss.deployment.SubDeployer.destroy";

   /** Get the JMX ObjectName of the service that provides the SubDeployer
    * @return JMX ObjectName of the service
    */
   public ObjectName getServiceName();

   /**
    * The <code>accepts</code> method is called by MainDeployer to
    * determine which deployer is suitable for a DeploymentInfo.
    *
    * @param sdi a <code>DeploymentInfo</code> value
    * @return a <code>boolean</code> value
    *
    * @jmx:managed-operation
    */
   boolean accepts(DeploymentInfo sdi);

   /**
    * The <code>init</code> method lets the deployer set a few properties
    * of the DeploymentInfo, such as the watch url.
    *
    * @param sdi a <code>DeploymentInfo</code> value
    * @throws DeploymentException if an error occurs
    *
    * @jmx:managed-operation
    */
   void init(DeploymentInfo sdi) throws DeploymentException;

   /**
    * Set up the components of the deployment that do not
    * refer to other components
    *
    * @param sdi a <code>DeploymentInfo</code> value
    * @throws DeploymentException      Failed to deploy
    *
    * @jmx:managed-operation
    */
   void create(DeploymentInfo sdi) throws DeploymentException;

   /**
    * The <code>start</code> method sets up relationships with other components.
    *
    * @param sdi a <code>DeploymentInfo</code> value
    * @throws DeploymentException if an error occurs
    *
    * @jmx:managed-operation
    */
   void start(DeploymentInfo sdi) throws DeploymentException;

   /**
    * The <code>stop</code> method removes relationships between components.
    *
    * @param sdi a <code>DeploymentInfo</code> value
    * @throws DeploymentException if an error occurs
    *
    * @jmx:managed-operation
    */
   void stop(DeploymentInfo sdi) throws DeploymentException;

   /**
    * The <code>destroy</code> method removes individual components
    *
    * @param sdi a <code>DeploymentInfo</code> value
    * @throws DeploymentException if an error occurs
    *
    * @jmx:managed-operation
    */
   void destroy(DeploymentInfo sdi) throws DeploymentException;
}
