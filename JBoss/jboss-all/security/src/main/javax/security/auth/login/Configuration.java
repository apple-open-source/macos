/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.security.auth.login;

import java.lang.SecurityManager;
import javax.security.auth.AuthPermission;

import org.jboss.security.auth.login.XMLLoginConfigImpl;

/** An alternate implementation of the JAAS 1.0 Configuration class that deals
 * with ClassLoader shortcomings that were fixed in the JAAS included with
 * JDK1.4 and latter. This version allows LoginModules to be loaded from the
 * Thread context ClassLoader and uses an XML based configuration by default.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1 $
 */
public abstract class Configuration
{
   public static final AuthPermission GET_CONFIG_PERM = new AuthPermission("getLoginConfiguration");
   public static final AuthPermission SET_CONFIG_PERM = new AuthPermission("setLoginConfiguration");
   private static Configuration theConfiguration;

   public static synchronized Configuration getConfiguration()
   {
      SecurityManager sm = System.getSecurityManager();
      if( sm != null )
      {
         sm.checkPermission(GET_CONFIG_PERM);
      }

      if( theConfiguration == null )
      {
         // Create the default configuration
         theConfiguration = new XMLLoginConfigImpl();
      }
      return theConfiguration;
   }

   public static synchronized void setConfiguration(Configuration configuration)
   {
      SecurityManager sm = System.getSecurityManager();
      if( sm != null )
      {
         sm.checkPermission(SET_CONFIG_PERM);
      }
      theConfiguration = configuration;
   }

   protected Configuration()
   {
   }

   /**
    *
    * @param name The name of the login module configuration.
    * @return The array of AppConfigurationEntrys associated with the given
    *    name if such a configuration entry exists, null otherwise.
    */
   public abstract AppConfigurationEntry[] getAppConfigurationEntry(String name);
   /**
    *
    */
   public abstract void refresh();
}
