package org.jboss.security.auth.login;

import java.io.InputStream;
import java.io.IOException;
import java.io.File;
import java.io.InputStreamReader;
import java.security.KeyException;
import java.security.PrivilegedAction;
import java.security.AccessController;
import java.security.PrivilegedExceptionAction;
import java.security.PrivilegedActionException;
import java.net.URL;
import java.net.MalformedURLException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import javax.security.auth.login.Configuration;
import javax.security.auth.login.AppConfigurationEntry;
import javax.security.auth.AuthPermission;
import javax.xml.parsers.FactoryConfigurationError;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.ParserConfigurationException;

import org.xml.sax.EntityResolver;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;
import org.jboss.logging.Logger;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.NodeList;

/** An concrete implementation of the javax.security.auth.login.Configuration
 class that parses an xml configuration of the form:

 <policy>
 <application-policy name = "test-domain">
 <authentication>
 <login-module code = "org.jboss.security.plugins.samples.IdentityLoginModule"
 flag = "required">
 <module-option name = "principal">starksm</module-option>
 </login-module>
 </authentication>
 </application-policy>
 </policy>

 @see javax.security.auth.login.Configuration

 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.4.5 $
 */
public class XMLLoginConfigImpl extends Configuration
{
   private static final String DEFAULT_APP_CONFIG_NAME = "other";
   private static final AuthPermission REFRESH_PERM = new AuthPermission("refreshLoginConfiguration");
   private static Logger log = Logger.getLogger(XMLLoginConfigImpl.class);
   /** A mapping of application name to AppConfigurationEntry[] */
   protected Map appConfigs = Collections.synchronizedMap(new HashMap());
   /** The URL to the XML or Sun login configuration */
   protected URL loginConfigURL;
   /** The inherited configuration we delegate to */
   protected Configuration parentConfig;
   /** A flag indicating if XML configs should be validated */
   private boolean validateDTD = true;

   // --- Begin Configuration method overrrides
   public void refresh()
   {
      SecurityManager sm = System.getSecurityManager();
      if (sm != null)
         sm.checkPermission(REFRESH_PERM);
      appConfigs.clear();
      loadConfig();
   }

   public AppConfigurationEntry[] getAppConfigurationEntry(String appName)
   {
      // If the config has not been loaded try to do so
      if (loginConfigURL == null)
      {
         loadConfig();
      }

      AppConfigurationEntry[] entry = null;
      AuthenticationInfo authInfo = (AuthenticationInfo) appConfigs.get(appName);
      if (authInfo == null)
      {
         if (parentConfig != null)
            entry = parentConfig.getAppConfigurationEntry(appName);
         if (entry == null)
            authInfo = (AuthenticationInfo) appConfigs.get(DEFAULT_APP_CONFIG_NAME);
      }

      if (authInfo != null)
      {
         if (log.isTraceEnabled())
            log.trace("getAppConfigurationEntry("+appName+"), authInfo=" + authInfo);
         // Make a copy of the authInfo object
         final AuthenticationInfo theAuthInfo = authInfo;
         PrivilegedAction action = new PrivilegedAction()
         {
            public Object run()
            {
               return theAuthInfo.copyAppConfigurationEntry();
            }
         };
         entry = (AppConfigurationEntry[]) AccessController.doPrivileged(action);
      }

      return entry;
   }
   // --- End Configuration method overrrides

   /** Set the URL of the XML login configuration file that should
    be loaded by this mbean on startup.
    */
   public URL getConfigURL()
   {
      return loginConfigURL;
   }

   /** Set the URL of the XML login configuration file that should
    be loaded by this mbean on startup.
    */
   public void setConfigURL(URL loginConfigURL)
   {
      this.loginConfigURL = loginConfigURL;
   }

   public void setConfigResource(String resourceName)
      throws IOException
   {
      ClassLoader tcl = Thread.currentThread().getContextClassLoader();
      loginConfigURL = tcl.getResource(resourceName);
      if (loginConfigURL == null)
         throw new IOException("Failed to find resource: " + resourceName);
   }

   public void setParentConfig(Configuration parentConfig)
   {
      this.parentConfig = parentConfig;
   }

   /** Get whether the login config xml document is validated againsts its DTD
    */
   public boolean getValidateDTD()
   {
      return this.validateDTD;
   }

   /** Set whether the login config xml document is validated againsts its DTD
    */
   public void setValidateDTD(boolean flag)
   {
      this.validateDTD = flag;
   }

   /** Add an application configuration
    */
   public void addAppConfig(String appName, AppConfigurationEntry[] entries)
   {
      SecurityManager sm = System.getSecurityManager();
      if (sm != null)
         sm.checkPermission(REFRESH_PERM);
      AuthenticationInfo authInfo = new AuthenticationInfo();
      authInfo.setAppConfigurationEntry(entries);
      appConfigs.put(appName, authInfo);
   }

   public void removeAppConfig(String appName)
   {
      SecurityManager sm = System.getSecurityManager();
      if (sm != null)
         sm.checkPermission(REFRESH_PERM);
      appConfigs.remove(appName);
   }

   public void clear()
   {

   }

   /** Called to try to load the config from the java.security.auth.login.config
    * property value when there is no loginConfigURL.
    */
   public void loadConfig()
   {
      // Try to load the java.security.auth.login.config property
      String loginConfig = System.getProperty("java.security.auth.login.config");
      if (loginConfig == null)
         loginConfig = "login-config.xml";

      // If there is no loginConfigURL build it from the loginConfig
      if (loginConfigURL == null)
      {
         try
         {
            // Try as a URL
            loginConfigURL = new URL(loginConfig);
         }
         catch (MalformedURLException e)
         {
            // Try as a resource
            try
            {
               setConfigResource(loginConfig);
            }
            catch (IOException ignore)
            {
               // Try as a file
               File configFile = new File(loginConfig);
               try
               {
                  setConfigURL(configFile.toURL());
               }
               catch (MalformedURLException ignore2)
               {
               }
            }
         }
      }

      if (loginConfigURL == null)
      {
         log.warn("Failed to find config: " + loginConfig);
         return;
      }

      // Try to load the config if found
      try
      {
         loadConfig(loginConfigURL);
      }
      catch (Exception e)
      {
         log.warn("Failed to load config: " + loginConfigURL, e);
      }
   }

   protected String[] loadConfig(URL config) throws Exception
   {
      SecurityManager sm = System.getSecurityManager();
      if (sm != null)
         sm.checkPermission(REFRESH_PERM);

      ArrayList configNames = new ArrayList();
      log.debug("Try loading config as XML, url=" + config);
      try
      {
         loadXMLConfig(config, configNames);
      }
      catch(Throwable e)
      {
         log.debug("Failed to load config as XML", e);
         log.debug("Try loading config as Sun format, url=" + config);
         loadSunConfig(config, configNames);
      }
      String[] names = new String[configNames.size()];
      configNames.toArray(names);
      return names;
   }

   private void loadSunConfig(URL sunConfig, ArrayList configNames)
      throws Exception
   {
      InputStream is = sunConfig.openStream();
      if (is == null)
         throw new IOException("InputStream is null for: " + sunConfig);

      InputStreamReader configFile = new InputStreamReader(is);
      boolean trace = log.isTraceEnabled();
      SunConfigParser.doParse(configFile, this, trace);
   }

   private void loadXMLConfig(URL loginConfigURL, ArrayList configNames)
      throws IOException, ParserConfigurationException, SAXException
   {
      HashMap tmpAppConfigs = new HashMap();
      Document doc = loadURL(loginConfigURL);
      Element root = doc.getDocumentElement();
      NodeList apps = root.getElementsByTagName("application-policy");
      for (int n = 0; n < apps.getLength(); n++)
      {
         Element appPolicy = (Element) apps.item(n);
         String appName = appPolicy.getAttribute("name");
         log.trace("Parsing application-policy=" + appName);

         try
         {
            AuthenticationInfo authInfo = ConfigUtil.parseAuthentication(appPolicy);
            if (authInfo != null)
            {
               if (appConfigs.containsKey(appName) == true)
                  throw new KeyException("Config name: " + appName + "already exists");
               tmpAppConfigs.put(appName, authInfo);
            }
         }
         catch (Exception e)
         {
            log.warn("Failed to parse config for entry:"+appName, e);
         }
      }
      configNames.addAll(tmpAppConfigs.keySet());
      appConfigs.putAll(tmpAppConfigs);
   }

   private Document loadURL(URL configURL)
      throws IOException, ParserConfigurationException, SAXException
   {
      InputStream is = configURL.openStream();
      if (is == null)
         throw new IOException("Failed to obtain InputStream from url: " + configURL);

      // Get the xml DOM parser
      PrivilegedAction action = new PrivilegedAction()
      {
         public Object run() throws FactoryConfigurationError
         {
            return DocumentBuilderFactory.newInstance();
         }
      };

      DocumentBuilderFactory docBuilderFactory = null;
      try
      {
         docBuilderFactory = (DocumentBuilderFactory) AccessController.doPrivileged(action);
      }
      catch (FactoryConfigurationError e)
      {
         throw e;
      }

      docBuilderFactory.setValidating(validateDTD);
      DocumentBuilder docBuilder = docBuilderFactory.newDocumentBuilder();
      EntityResolver resolver = new LocalResolver(log);
      docBuilder.setEntityResolver(resolver);
      Document doc = docBuilder.parse(is);
      return doc;
   }

   /** Local entity resolver to handle the security-policy DTD public id.
    */
   private static class LocalResolver implements EntityResolver
   {
      private static final String LOGIN_CIONFIG_PUBLIC_ID = "-//JBoss//DTD JBOSS Security Config 3.0//EN";
      private static final String LOGIN_CIONFIG_DTD_NAME = "/org/jboss/metadata/security_config.dtd";
      private Logger log;

      LocalResolver(Logger log)
      {
         this.log = log;
      }

      public InputSource resolveEntity(String publicId, String systemId)
      {
         InputSource is = null;
         if (publicId.equals(LOGIN_CIONFIG_PUBLIC_ID))
         {
            try
            {
               InputStream dtdStream = getClass().getResourceAsStream(LOGIN_CIONFIG_DTD_NAME);
               is = new InputSource(dtdStream);
            }
            catch (Exception ex)
            {
               log.warn("Failed to resolve DTD publicId: " + publicId);
            }
         }
         return is;
      }
   }
}
