/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.auth.callback;

import java.lang.reflect.Method;
import java.security.Principal;
import javax.security.auth.callback.Callback;
import javax.security.auth.callback.CallbackHandler;
import javax.security.auth.callback.NameCallback;
import javax.security.auth.callback.PasswordCallback;
import javax.security.auth.callback.UnsupportedCallbackException;

/** An implementation of CallbackHandler is assigned a Principal, and
 opaque Object credential as values passed to the constructor. This is suitable
 for environments that need non-interactive JAAS logins and is used by the
 JaasSecurityManager as its default CallbackHandler.

@see javax.security.auth.callback.CallbackHandler
@see #handle(Callback[])

@author  Scott.Stark@jboss.org
@version $Revision: 1.5.4.1 $
*/
public class SecurityAssociationHandler implements CallbackHandler
{
    private transient Principal principal;
    private transient Object credential;

    public SecurityAssociationHandler()
    {
    }
    /** Initialize the UsernamePasswordHandler with the principal
        and credentials to use.
    */
    public SecurityAssociationHandler(Principal principal, Object credential)
    {
        this.principal = principal;
        this.credential = credential;
    }

    public void setSecurityInfo(Principal principal, Object credential)
    {
        this.principal = principal;
        this.credential = credential;
    }

    /** Handles SecurityAssociationCallback, ObjectCallback, NameCallback and
        PasswordCallback types. A NameCallback name property is set to
     the Prinicpal.getName() value. A PasswordCallback password property is
     set to the getPassword() value. The preferred SecurityAssociationCallback
     has its principal and credential properties set to the instance principal
     and credential. An ObjectCallback has its credential set to the credential
     value.

    @see #getPassword()
    @exception UnsupportedCallbackException, thrown if any callback of
     type other than SecurityAssociationCallback, ObjectCallback, NameCallback
     or PasswordCallback are seen.
    */
    public void handle(Callback[] callbacks) throws
        UnsupportedCallbackException
    {
        for(int i = 0; i < callbacks.length; i++)
        {
            Callback c = callbacks[i];
            if( c instanceof SecurityAssociationCallback )
            {
                SecurityAssociationCallback sac = (SecurityAssociationCallback) c;
                sac.setPrincipal(principal);
                sac.setCredential(credential);
            }
            else if( c instanceof ObjectCallback )
            {
               ObjectCallback oc = (ObjectCallback) c;
               oc.setCredential(credential);
            }
            else if ( c instanceof NameCallback)
            {
                NameCallback nc = (NameCallback) c;
                if( principal != null )
                    nc.setName(principal.getName());
            }
            else if( c instanceof PasswordCallback )
            {
                PasswordCallback pc = (PasswordCallback) c;
                char[] password = getPassword();
                if( password != null )
                    pc.setPassword(password);
            }
            else
            {
                throw new UnsupportedCallbackException(c, "Unrecognized Callback");
            }
        }
    }

    /** Try to convert the credential value into a char[] using the
        first of the following attempts which succeeds:

        1. Check for instanceof char[]
        2. Check for instanceof String and then use toCharArray()
        3. See if credential has a toCharArray() method and use it
        4. Use toString() followed by toCharArray().
    @return a char[] representation of the credential.
     */
    private char[] getPassword()
    {
        char[] password = null;
        if( credential instanceof char[] )
        {
            password = (char[]) credential;
        }
        else if( credential instanceof String )
        {
            String s = (String) credential;
            password = s.toCharArray();
        }
        else
        {
            try
            {
                Class[] types = {};
                Method m = credential.getClass().getMethod("toCharArray", types);
                Object[] args = {};
                m.invoke(credential, args);
            }
            catch(Exception e)
            {
                if( credential != null )
                {
                    String s = credential.toString();
                    password = s.toCharArray();
                }
            }
        }
        return password;
    }
}
