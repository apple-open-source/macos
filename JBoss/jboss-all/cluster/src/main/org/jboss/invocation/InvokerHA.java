/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.invocation;

import java.io.Serializable;
import javax.management.ObjectName;

import org.jboss.ha.framework.interfaces.LoadBalancePolicy;
import org.jboss.ha.framework.server.HATarget;

/** An administrative interface used by server side proxy factories during
 * the creation of HA capable invokers. Note that this does NOT extend the
 * Invoker interface because these methods are not for use by an invoker
 * client.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public interface InvokerHA
{
   public Invoker createProxy(ObjectName targetName, LoadBalancePolicy policy,
      String proxyFamilyName)
      throws Exception;
   public Serializable getStub();
   public void registerBean(ObjectName targetName, HATarget target)
      throws Exception;
   public void unregisterBean(ObjectName targetName)
      throws Exception;
}
