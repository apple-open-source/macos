/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.http;

import java.io.Serializable;
/**
 * This is the date type the HTTPClient expects back from a HTTPILRequest.
 *
 * @author    Nathan Phelps (nathan@jboss.org)
 * @version   $Revision: 1.1.2.1 $
 * @created   January 15, 2003
 */
public class HTTPILResponse implements Serializable
{
    
    private Object value;
    
    public HTTPILResponse()
    {
    }
    
    public HTTPILResponse(Object value)
    {
        this.value = value;
    }
    
    public void setValue(Object value)
    {
        this.value = value;
    }
    
    public Object getValue()
    {
        return this.value;
    }
    
    public String toString()
    {
        if (this.value == null)
        {
            return null;
        }
        else
        {
            return this.value.toString();
        }
    }
}