/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.plugins;

import java.util.Stack;
import javax.management.JMException;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.security.auth.login.Configuration;

import org.jboss.system.ServiceMBeanSupport;

/** The SecurityConfigMBean implementation. This class needs the
 javax.security.auth.AuthPermission("setLoginConfiguration") to install
 the javax.security.auth.login.Configuration when running with a security
 manager.
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.4.4.1 $
 */
public class SecurityConfig extends ServiceMBeanSupport
   implements SecurityConfigMBean
{
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   /** The default Configuration mbean name */
   private String loginConfigName;
   /** The stack of Configuration mbeans that are active */
   private Stack loginConfigStack = new Stack();

   static class ConfigInfo
   {
      ObjectName name;
      Configuration config;
      ConfigInfo(ObjectName name, Configuration config)
      {
         this.name = name;
         this.config = config;
      }
   }

   public SecurityConfig()
   {
   }
   
   public String getName()
   {
      return "SecurityIntialization";
   }

   /** Get the name of the mbean that provides the default JAAS login configuration 
    */
   public String getLoginConfig()
   {
      return loginConfigName;
   }

   /** Set the name of the mbean that provides the default JAAS login configuration 
    */
   public void setLoginConfig(String name) throws MalformedObjectNameException
   {
      this.loginConfigName = name;
   }

   /** Start the configuration service by pushing the mbean given by the
    LoginConfig onto the configuration stack.
    */
   public void startService() throws Exception
   {
      pushLoginConfig(loginConfigName);
   }

   /** Start the configuration service by poping the top of the
    configuration stack.
    */
   public void stopService() throws Exception
   {
      if( loginConfigStack.empty() == false )
         popLoginConfig();
   }

   /** Push an mbean onto the login configuration stack and install its
    Configuration as the current instance.
    @see javax.security.auth.login.Configuration
    */
   public synchronized void pushLoginConfig(String objectName)
      throws JMException, MalformedObjectNameException
   {
      ObjectName name = new ObjectName(objectName);
      Configuration prevConfig = null;
      if( loginConfigStack.empty() == false )
      {
         ConfigInfo prevInfo = (ConfigInfo) loginConfigStack.peek();
         prevConfig = prevInfo.config;
      }

      ConfigInfo info = installConfig(name, prevConfig);
      loginConfigStack.push(info);
   }
   /** Pop the current mbean from the login configuration stack and install
    the previous Configuration as the current instance.
    @see javax.security.auth.login.Configuration
    */
   public synchronized void popLoginConfig()
      throws JMException
   {
      ConfigInfo info = (ConfigInfo) loginConfigStack.pop();
      Configuration prevConfig = null;
      if( loginConfigStack.empty() == false )
      {
         ConfigInfo prevInfo = (ConfigInfo) loginConfigStack.peek();
         prevConfig = prevInfo.config;
      }

      installConfig(info.name, prevConfig);
   }

   /** Obtain the Configuration from the named mbean using its getConfiguration
    operation and install it as the current Configuration.

    @see Configuration.setConfiguration(javax.security.auth.login.Configuration)
    */
   private ConfigInfo installConfig(ObjectName name, Configuration prevConfig)
      throws JMException
   {
      MBeanServer server = super.getServer();
      Object[] args = {prevConfig};
      String[] signature = {"javax.security.auth.login.Configuration"};
      Configuration config = (Configuration) server.invoke(name,
         "getConfiguration", args, signature);
      Configuration.setConfiguration(config);
      ConfigInfo info = new ConfigInfo(name, config);
      log.debug("Installed JAAS Configuration service="+name+", config="+config);
      return info;
   }
}
