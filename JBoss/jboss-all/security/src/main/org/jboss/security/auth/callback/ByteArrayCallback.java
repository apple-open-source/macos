/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.auth.callback;

import javax.security.auth.callback.Callback;

/** An implementation of Callback that obtains a binary parameter as a byte[].
 Interpretation of the array is up to the LoginModule.

@author  Scott.Stark@jboss.org
@version $Revision: 1.1.4.2 $
*/
public class ByteArrayCallback implements Callback
{
    private transient String prompt;
    private transient byte[] data;

    /** Initialize the SecurityAssociationCallback
    */
    public ByteArrayCallback(String prompt)
    {
        this.prompt = prompt;
    }

    public String getPrompt()
    {
        return prompt;
    }
    public byte[] getByteArray()
    {
        return data;
    }
    public void setByteArray(byte[] data)
    {
        this.data = data;
    }
    public void clearByteArray()
    {
        this.data = null;
    }
}

