
/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.auth.login;

import java.util.Iterator;
import java.util.Map;
import java.util.Map.Entry;
import java.io.IOException;
import java.net.URL;
import javax.security.auth.login.Configuration;
import javax.security.auth.login.AppConfigurationEntry;

import org.jboss.system.ServiceMBeanSupport;

/** An MBean for managing a XMLLoginConfigImpl instance.

 @author Scott.Stark@jboss.org
 @version $Revision: 1.4.2.4 $
 */
public class XMLLoginConfig extends ServiceMBeanSupport
      implements XMLLoginConfigMBean
{
   XMLLoginConfigImpl config;

   public XMLLoginConfig()
   {
      config = new XMLLoginConfigImpl();
   }

// --- Begin XMLLoginConfigMBean interface methods

   /** Set the URL of the XML login configuration file that should
    be loaded by this mbean on startup.
    */
   public URL getConfigURL()
   {
      return config.getConfigURL();
   }
   /** Set the URL of the XML login configuration file that should
    be loaded by this mbean on startup.
    */
   public void setConfigURL(URL configURL)
   {
      config.setConfigURL(configURL);
   }

   /** Set the resource name of the XML login configuration file that should
    be loaded by this mbean on startup.
    */
   public void setConfigResource(String resourceName)
      throws IOException
   {
      config.setConfigResource(resourceName);
   }

   /** Get whether the login config xml document is validated againsts its DTD
    */
   public boolean getValidateDTD()
   {
      return config.getValidateDTD();
   }
   /** Set whether the login config xml document is validated againsts its DTD
    */
   public void setValidateDTD(boolean flag)
   {
      config.setValidateDTD(flag);
   }

   /** Add an application login configuration. Any existing configuration for
    the given appName will be replaced.
    */
   public void addAppConfig(String appName, AppConfigurationEntry[] entries)
   {
      config.addAppConfig(appName, entries);
   }
   /** Remove an application login configuration.
    */
   public void removeAppConfig(String appName)
   {
      config.removeAppConfig(appName);
   }

   /** Get the XML based configuration given the Configuration it should
    delegate to when an application cannot be found.
    */
   public Configuration getConfiguration(Configuration prevConfig)
   {
      config.setParentConfig(prevConfig);
      return config;
   }

   /** Load the login configuration information from the given config URL.
    * @param configURL A URL to an XML or Sun login config file.
    * @throws Exception on failure to load the configuration
    */ 
   public String[] loadConfig(URL configURL) throws Exception
   {
      return config.loadConfig(configURL);
   }

   public void removeConfigs(String[] appNames)
   {
      int count = appNames == null ? 0 : appNames.length;
      for(int a = 0; a < count; a ++)
         removeAppConfig(appNames[a]);
   }

   /** Display the login configuration for the given application.
    */
   public String displayAppConfig(String appName)
   {
      StringBuffer buffer = new StringBuffer("<h2>"+appName+" LoginConfiguration</h2>\n");
      AppConfigurationEntry[] appEntry = config.getAppConfigurationEntry(appName);
      if( appEntry == null )
         buffer.append("No Entry\n");
      else
      {
         for(int c = 0; c < appEntry.length; c ++)
         {
            AppConfigurationEntry entry = appEntry[c];
            buffer.append("LoginModule Class: "+entry.getLoginModuleName());
            buffer.append("\n<br>ControlFlag: "+entry.getControlFlag());
            buffer.append("\n<br>Options:<ul>");
            Map options = entry.getOptions();
            Iterator iter = options.entrySet().iterator();
            while( iter.hasNext() )
            {
               Entry e = (Entry) iter.next();
               buffer.append("<li>");
               buffer.append("name="+e.getKey());
               buffer.append(", value="+e.getValue());
               buffer.append("</li>\n");
            }
            buffer.append("</ul>\n");
         }
      }
      return buffer.toString();
   }
// --- End XMLLoginConfigMBean interface methods

// --- Begin ServiceMBeanSupport overriden methods

   /** Load the configuration
    */
   protected void startService() throws Exception
   {
      config.loadConfig();
   }

   /** Clear all configuration entries
    */
   protected void destroyService()
   {
      config.clear();
   }

// --- End ServiceMBeanSupport overriden methods

}
