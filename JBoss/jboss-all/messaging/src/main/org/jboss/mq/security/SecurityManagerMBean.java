/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.mq.security;

import javax.management.ObjectName;

import org.jboss.mx.util.ObjectNameFactory;
import org.jboss.system.ServiceMBean;
import org.jboss.mq.server.JMSServerInterceptor;
import org.jboss.mq.server.jmx.InterceptorMBean;
/**
 * MBean interface for security manager.
 *
 * @author     <a href="pra@tim.se">Peter Antman</a>
 * @version $Revision: 1.2.2.1 $
 */

public interface SecurityManagerMBean extends InterceptorMBean
{
   ObjectName OBJECT_NAME =
      ObjectNameFactory.create("jboss.mq:service=SecurityManager");
 
   // REMOVE
   String printAuthCache();

   public void addDestination(String jndi, String conf) throws Exception;

   public void addDestination(String jndi, org.w3c.dom.Element conf) throws Exception;

   public void removeDestination(String jndi) throws Exception;
} // SecurityManagerMBean
