/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.factory;

import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.jboss.deployment.DeploymentInfo;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.3 $
 */
public interface ManagedObjectFactory
{
   public ObjectName create(MBeanServer server, Object data);
   public void destroy(MBeanServer server, Object data);
}
