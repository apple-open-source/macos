package org.jboss.security.auth.login;

import java.io.IOException;
import java.net.URL;
import javax.security.auth.login.AppConfigurationEntry;
import javax.security.auth.login.Configuration;

import org.jboss.system.ServiceMBean;

/** The managment bean interface for the XML based JAAS login configuration
 object.

@author  Scott.Stark@jboss.org
@version $Revision: 1.3.2.1 $
 */
public interface XMLLoginConfigMBean extends ServiceMBean
{
   /** Set the URL of the XML login configuration file that should
    be loaded by this mbean on startup.
    */
   public URL getConfigURL();
   /** Set the URL of the XML login configuration file that should
    be loaded by this mbean on startup.
    */
   public void setConfigURL(URL configURL);

   /** Set the resource name of the XML login configuration file that should
    be loaded by this mbean on startup.
    */
   public void setConfigResource(String resourceName) throws IOException;

   /** Get whether the login config xml document is validated againsts its DTD
    */
   public boolean getValidateDTD();
   /** Set whether the login config xml document is validated againsts its DTD
    */
   public void setValidateDTD(boolean flag);

   /** Get the XML based configuration given the Configuration it should
    delegate to when an application cannot be found.
    */
   public Configuration getConfiguration(Configuration prevConfig);

   /** Add an application login configuration. Any existing configuration for
    the given appName will be replaced.
    */
   public void addAppConfig(String appName, AppConfigurationEntry[] entries);
   /** Remove an application login configuration.
    */
   public void removeAppConfig(String appName);

   /** Load the login configuration information from the given config URL.
    * @param configURL A URL to an XML or Sun login config file.
    * @return An array of the application config names loaded
    * @throws Exception on failure to load the configuration
    */ 
   public String[] loadConfig(URL configURL) throws Exception;
   /** Remove the given login configurations. This invokes removeAppConfig
    * for each element of appNames.
    * 
    * @param appNames the names of the login configurations to remove. 
    */ 
   public void removeConfigs(String[] appNames);

   /** Display the login configuration for the given application.
    */
   public String displayAppConfig(String appName);
}

