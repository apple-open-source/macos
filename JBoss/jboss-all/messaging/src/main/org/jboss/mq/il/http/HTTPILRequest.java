/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.http;

import java.io.Serializable;

/**
 * This is the data type that HTTPServerILServlet expects.
 *
 * @author    Nathan Phelps (nathan@jboss.org)
 * @version   $Revision: 1.1.2.1 $
 * @created   January 15, 2003
 */
public class HTTPILRequest implements Serializable
{
    
    private String methodName;
    private Object[] arguments;
    private Class[] argumentTypes;
    
    public HTTPILRequest()
    {
    }
    
    public HTTPILRequest(String methodName, Object[] arguments, Class[] argumentTypes)
    {
        this.methodName = methodName;
        this.arguments = arguments;
        this.argumentTypes = argumentTypes;
    }
    
    public void setMethodName(String methodName)
    {
        this.methodName = methodName;
    }
    
    public String getMethodName()
    {
        return this.methodName;
    }
    
    public void setArguments(Object[] arguments, Class[] argumentTypes)
    {
        this.arguments = arguments;
        this.argumentTypes = argumentTypes;
    }
    
    public Object[] getArguments()
    {
        return this.arguments;
    }
    
    public Class[] getArgumentTypes()
    {
        return this.argumentTypes;
    }
    
    public String toString()
    {
        String argumentString = "(";
        if (this.arguments != null)
        {
            for (int i = 0; i < this.arguments.length; i++ )
            {
                if (i > 0)
                {
                    argumentString = argumentString + ", ";
                }
                if (this.arguments[i] != null)
                {
                    argumentString = argumentString + this.argumentTypes[i].toString() + " " + this.arguments[i].toString();
                }
                else
                {
                    argumentString = argumentString + this.argumentTypes[i].toString() + " null";
                }
            }
        }
        argumentString = argumentString + ")";
        return this.methodName + argumentString;
    }
}