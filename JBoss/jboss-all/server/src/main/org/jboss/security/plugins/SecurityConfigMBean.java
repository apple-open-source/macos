/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.plugins;

import javax.management.JMException;
import javax.management.MalformedObjectNameException;

import org.jboss.system.ServiceMBean;

/** A security configuration MBean. This establishes the JAAS and Java2
 security properties and related configuration.

 @see DefaultLoginConfig
 @see javax.security.auth.login.Configuration

@author Scott.Stark@jboss.org
@version $Revision: 1.2 $
*/
public interface SecurityConfigMBean extends ServiceMBean
{
   /** Get the name of the mbean that provides the default JAAS login configuration */
   public String getLoginConfig();
   /** Set the name of the mbean that provides the default JAAS login configuration */
   public void setLoginConfig(String objectName) throws MalformedObjectNameException;
   /** Push an mbean onto the login configuration stack and install its
    Configuration as the current instance.
    @see javax.security.auth.login.Configuration
    */
   public void pushLoginConfig(String objectName) throws JMException, MalformedObjectNameException;
   /** Pop the current mbean from the login configuration stack and install
    the previous Configuration as the current instance.
    @see javax.security.auth.login.Configuration
    */
   public void popLoginConfig() throws JMException;

}
