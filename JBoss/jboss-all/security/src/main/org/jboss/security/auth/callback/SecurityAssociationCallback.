/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.auth.callback;

import java.security.Principal;
import javax.security.auth.callback.Callback;


/** An implementation of Callback useful on the server side for
propagating the request Principal and credentials to LoginModules.

@author  Scott.Stark@jboss.org
@version $Revision: 1.2 $
*/
public class SecurityAssociationCallback implements Callback
{
    private transient Principal principal;
    private transient Object credential;

    /** Initialize the SecurityAssociationCallback
    */
    public SecurityAssociationCallback()
    {
    }

    public Principal getPrincipal()
    {
        return principal;
    }
    public void setPrincipal(Principal principal)
    {
        this.principal = principal;
    }

    public Object getCredential()
    {
        return credential;
    }
    public void setCredential(Object credential)
    {
        this.credential = credential;
    }
    public void clearCredential()
    {
        this.credential = null;
    }
}

