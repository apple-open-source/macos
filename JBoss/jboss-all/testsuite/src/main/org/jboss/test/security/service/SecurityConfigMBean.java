/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.security.service;

import org.jboss.system.ServiceMBean;

/** An mbean interface for a config service that pushes an xml based
 javax.security.auth.login.Configuration onto the config stack managed by
 the mbean whose name is given by the SecurityConfigName attribute.

 @see org.jboss.security.plugins.SecurityConfigMBean

 @author Scott.Stark@jboss.org
 @version $Revision: 1.1 $
 */
public interface SecurityConfigMBean extends ServiceMBean
{
   /** Get the classpath resource name of the security configuration file */
   public String getAuthConfig();
   /** Set the classpath resource name of the security configuration file */
   public void setAuthConfig(String configURL);
   /** Get the name of the SecurityConfig mbean whose pushLoginConfig and
    popLoginConfig ops will be used to install and remove the xml login config*/
   public String getSecurityConfigName();
   /** Set the name of the SecurityConfig mbean whose pushLoginConfig and
    popLoginConfig ops will be used to install and remove the xml login config*/
   public void setSecurityConfigName(String objectName);
}
