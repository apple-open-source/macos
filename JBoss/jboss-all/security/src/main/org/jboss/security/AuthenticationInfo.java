/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security;

import java.security.AccessController;
import javax.security.auth.AuthPermission;
import javax.security.auth.callback.CallbackHandler;
import javax.security.auth.login.AppConfigurationEntry;

/** The login module configuration information.

@author Scott.Stark@jboss.org
@version $Revision: 1.3 $
*/
public class AuthenticationInfo
{
    public static final AuthPermission GET_CONFIG_ENTRY_PERM = new AuthPermission("getLoginConfiguration");
    public static final AuthPermission SET_CONFIG_ENTRY_PERM = new AuthPermission("setLoginConfiguration");
    private AppConfigurationEntry[] loginModules;
    private CallbackHandler callbackHandler;

    /** Get an application authentication configuration. This requires an
    AuthPermission("getLoginConfiguration") access.
    */
    public AppConfigurationEntry[] getAppConfigurationEntry()
    {
        AccessController.checkPermission(GET_CONFIG_ENTRY_PERM);
        return loginModules;
    }
    /** Set an application authentication configuration. This requires an
    AuthPermission("setLoginConfiguration") access.
    */
    public void setAppConfigurationEntry(AppConfigurationEntry[] loginModules)
    {
        AccessController.checkPermission(SET_CONFIG_ENTRY_PERM);
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
}
