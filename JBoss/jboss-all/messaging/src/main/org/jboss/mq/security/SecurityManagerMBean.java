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
import org.w3c.dom.Element;

/**
 * MBean interface for security manager.
 *
 * @author     <a href="pra@tim.se">Peter Antman</a>
 * @version $Revision: 1.2.2.3 $
 */

public interface SecurityManagerMBean extends InterceptorMBean
{
   ObjectName OBJECT_NAME =
      ObjectNameFactory.create("jboss.mq:service=SecurityManager");

   Element getDefaultSecurityConfig();
   void setDefaultSecurityConfig(Element conf) throws Exception;

   String getSecurityDomain();
   void setSecurityDomain(String securityDomain);
 
   // REMOVE
   //String printAuthCache();

   public void addDestination(String jndi, String conf) throws Exception;

   public void addDestination(String jndi, Element conf) throws Exception;

   public void removeDestination(String jndi) throws Exception;
} // SecurityManagerMBean
