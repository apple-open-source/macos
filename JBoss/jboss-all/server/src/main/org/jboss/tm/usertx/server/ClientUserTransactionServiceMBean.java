/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.tm.usertx.server;

import javax.management.ObjectName;
import org.jboss.mx.util.ObjectNameFactory;
import org.jboss.invocation.Invocation;

/**
 * MBean for ClientUserTransaction service.
 *
 * @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.6.4.2 $
 */
public interface ClientUserTransactionServiceMBean
   extends org.jboss.system.ServiceMBean
{
   /** Set the proxy factory service for the UserTransactionSessions
    * @param proxyName the proxy factory MBean name
    */ 
   public void setTxProxyName(ObjectName proxyName);

   /** The detached invoker callback operation.
    * 
    * @param invocation
    * @return the invocation result
    * @throws Exception thrown on invocation failure
    */ 
   public Object invoke(Invocation invocation) throws Exception;
}

