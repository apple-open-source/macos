/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.auth.callback;

import javax.security.auth.callback.Callback;

/** An implementation of Callback that simply obtains an Object to be used
as the authentication credential. Interpretation of the Object is up to
the LoginModules that validate the credential.

@author  Scott.Stark@jboss.org
@version $Revision: 1.2.4.1 $
*/
public class ObjectCallback implements Callback
{
    private transient String prompt;
    private transient Object credential;

    /** Initialize the SecurityAssociationCallback
    */
    public ObjectCallback(String prompt)
    {
        this.prompt = prompt;
    }

    public String getPrompt()
    {
        return prompt;
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

