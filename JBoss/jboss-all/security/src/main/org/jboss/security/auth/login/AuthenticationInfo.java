/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.auth.login;

import java.util.Iterator;
import java.util.Map;
import java.util.Map.Entry;
import javax.security.auth.AuthPermission;
import javax.security.auth.callback.CallbackHandler;
import javax.security.auth.login.AppConfigurationEntry;

/** The login module configuration information.
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.2 $
 */
public class AuthenticationInfo
{
   public static final AuthPermission GET_CONFIG_ENTRY_PERM = new AuthPermission("getLoginConfiguration");
   public static final AuthPermission SET_CONFIG_ENTRY_PERM = new AuthPermission("setLoginConfiguration");
   private AppConfigurationEntry[] loginModules;
   private CallbackHandler callbackHandler;

   /** Get a copy of the  application authentication configuration. This requires
    an AuthPermission("getLoginConfiguration") access.
    */
   public AppConfigurationEntry[] copyAppConfigurationEntry()
   {
      SecurityManager sm = System.getSecurityManager();
      if( sm != null )
         sm.checkPermission(GET_CONFIG_ENTRY_PERM);
      AppConfigurationEntry[] copy = new AppConfigurationEntry[loginModules.length];
      for(int i = 0; i < loginModules.length; i ++)
      {
         AppConfigurationEntry entry = loginModules[i];
         copy[i] = new AppConfigurationEntry(entry.getLoginModuleName(),
                                entry.getControlFlag(), entry.getOptions());
      }
      return copy;
   }

   /** Get an application authentication configuration. This requires an
    AuthPermission("getLoginConfiguration") access.
    */
   public AppConfigurationEntry[] getAppConfigurationEntry()
   {
      SecurityManager sm = System.getSecurityManager();
      if( sm != null )
         sm.checkPermission(GET_CONFIG_ENTRY_PERM);
      return loginModules;
   }
   /** Set an application authentication configuration. This requires an
    AuthPermission("setLoginConfiguration") access.
    */
   public void setAppConfigurationEntry(AppConfigurationEntry[] loginModules)
   {
      SecurityManager sm = System.getSecurityManager();
      if( sm != null )
         sm.checkPermission(SET_CONFIG_ENTRY_PERM);
      this.loginModules = loginModules;
   }

   /**
    */
   public CallbackHandler getAppCallbackHandler()
   {
      return callbackHandler;
   }
   public void setAppCallbackHandler(CallbackHandler handler)
   {
      this.callbackHandler = handler;
   }

   public String toString()
   {
      StringBuffer buffer = new StringBuffer("AppConfigurationEntry[]:\n");
      for(int i = 0; i < loginModules.length; i ++)
      {
         AppConfigurationEntry entry = loginModules[i];
         buffer.append("["+i+"]");
         buffer.append("\nLoginModule Class: "+entry.getLoginModuleName());
         buffer.append("\nControlFlag: "+entry.getControlFlag());
         buffer.append("\nOptions:");
         Map options = entry.getOptions();
         Iterator iter = options.entrySet().iterator();
         while( iter.hasNext() )
         {
            Entry e = (Entry) iter.next();
            buffer.append("name="+e.getKey());
            buffer.append(", value="+e.getValue());
            buffer.append("\n");
         }
      }
      return buffer.toString();
   }
}
