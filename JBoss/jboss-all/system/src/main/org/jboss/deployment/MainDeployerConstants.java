package org.jboss.deployment;

/** Constants for MainDeployers
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public interface MainDeployerConstants
{
   /** The JMX notification type sent when a deployer is added */
   public static final String ADD_DEPLOYER = "org.jboss.deployment.MainDeployer.addDeployer";
   /** The JMX notification type sent when a deployer is added */
   public static final String REMOVE_DEPLOYER = "org.jboss.deployment.MainDeployer.removeDeployer";
}
