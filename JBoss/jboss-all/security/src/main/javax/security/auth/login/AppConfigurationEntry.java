/**
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.security.auth.login;

import java.util.Map;

/** An alternate implementation of the JAAS 1.0 Configuration class that deals
 * with ClassLoader shortcomings that were fixed in the JAAS included with
 * JDK1.4 and latter. This version allows LoginModules to be loaded from the
 * Thread context ClassLoader and uses an XML based configuration by default.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public class AppConfigurationEntry
{
   /** Enum class for the login module control flags
    */
   public static class LoginModuleControlFlag
   {
      public static final LoginModuleControlFlag REQUIRED = new LoginModuleControlFlag("required");
      public static final LoginModuleControlFlag REQUISITE = new LoginModuleControlFlag("requisite");
      public static final LoginModuleControlFlag SUFFICIENT = new LoginModuleControlFlag("sufficient");
      public static final LoginModuleControlFlag OPTIONAL =  new LoginModuleControlFlag("optional");

      private String flagName;

      private LoginModuleControlFlag(String flagName)
      {
         this.flagName = flagName;
      }
      public String toString()
      {
         return "LoginModuleControlFlag: " + flagName;
      }
   }

   private String loginModuleName;
   private LoginModuleControlFlag controlFlag;
   private Map options;

   public AppConfigurationEntry(String loginModuleName,
      AppConfigurationEntry.LoginModuleControlFlag controlFlag, Map options)
   {
      this.loginModuleName = loginModuleName;
      this.controlFlag = controlFlag;
      this.options = options;
   }

   public String getLoginModuleName()
   {
      return loginModuleName;
   }

   public LoginModuleControlFlag getControlFlag()
   {
      return controlFlag;
   }

   public Map getOptions()
   {
      return options;
   }
}
