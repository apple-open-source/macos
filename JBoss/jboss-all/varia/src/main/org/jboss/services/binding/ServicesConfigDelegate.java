/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.services.binding;

import javax.management.MBeanServer;

/** Interface for delegates capabable of taking a ServiceConfig and mapping
 * it onto an mbean service via JMX
 *
 * @version $Revision: 1.1 $
 * @author Scott.Stark@jboss.org
 */
public interface ServicesConfigDelegate 
{
   /** Take the given config and map it onto the service specified in the
    config using JMX via the given server.
    @param config, the service name and its config bindings
    @param server, the JMX server to use to apply the config
    */
   public void applyConfig(ServiceConfig config, MBeanServer server) throws Exception;

}
