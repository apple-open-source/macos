/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security;

import java.security.AccessController;
import java.security.CodeSource;
import java.security.PermissionCollection;
import java.util.HashSet;
import java.util.Set;
import javax.security.auth.AuthPermission;
import javax.security.auth.Policy;
import javax.security.auth.Subject;
import javax.security.auth.login.Configuration;
import javax.security.auth.login.AppConfigurationEntry;


/** An concrete implementation of the javax.security.auth.Policy class that
categorizes authorization info by application.

@see javax.security.auth.Policy

@author Scott.Stark@jboss.org
@version $Revision: 1.3 $
*/
public class SecurityPolicy extends Policy
{
    private static final AuthPermission REFRESH_PERM = new AuthPermission("refreshPolicy");
    private static final Set emptySet = new HashSet();
    private static final ThreadLocal activeApp = new ThreadLocal();

    /**
     * @clientCardinality 1
     * @supplierCardinality 1
     * @clientRole Policy/Configuration Impl
     * @supplierRole Policy/Configuration data 
     */
    private IAppPolicyStore policyStore;
    private LoginConfiguration loginConfig = new LoginConfiguration();

    public class LoginConfiguration extends Configuration
    {
        public AppConfigurationEntry[] getAppConfigurationEntry(String appName)
        {
            AppConfigurationEntry[] entry = null;
            AppPolicy appPolicy = policyStore.getAppPolicy(appName);
            if( appPolicy != null )
            {
                entry = appPolicy.getAppConfigurationEntry();
            }
            return entry;
        }

        public void refresh()
        {
            SecurityPolicy.this.refresh();
        }
    }

    public static void setActiveApp(String appName)
    {
        activeApp.set(appName);
    }
    public static void unsetActiveApp()
    {
        activeApp.set(null);
    }

    public SecurityPolicy(IAppPolicyStore policyStore)
    {
        this.policyStore = policyStore;
    }

    public Configuration getLoginConfiguration()
    {
        return loginConfig;
    }

    public PermissionCollection getPermissions(Subject subject, CodeSource codesource)
    {
        String appName = (String) activeApp.get();
        if( appName == null )
            appName = "other";
        PermissionCollection perms = getPermissions(subject, codesource, appName);
        return perms;
    }

    public PermissionCollection getPermissions(Subject subject, CodeSource codesource, String appName)
    {
        AppPolicy policy = policyStore.getAppPolicy(appName);
        PermissionCollection perms = AppPolicy.NO_PERMISSIONS;
        if( policy != null )
            perms = policy.getPermissions(subject, codesource);
        return perms;
    }
    public AppPolicy getAppPolicy(String appName)
    {
        AppPolicy appPolicy = policyStore.getAppPolicy(appName);
        return appPolicy;
    }

    public void refresh()
    {
        AccessController.checkPermission(REFRESH_PERM);
        policyStore.refresh();
    }
}
