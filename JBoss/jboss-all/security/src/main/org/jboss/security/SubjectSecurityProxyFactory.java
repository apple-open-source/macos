/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security;

import java.io.Serializable;

/** An implementation of SecurityProxyFactory that creates SubjectSecurityProxy
objects to wrap the raw security proxy objects.

@author Scott.Stark@jboss.org
@version $Revision: 1.2 $
*/
public class SubjectSecurityProxyFactory implements SecurityProxyFactory, Serializable
{
    public SecurityProxy create(Object proxyDelegate)
    {
        SecurityProxy proxy = new SubjectSecurityProxy(proxyDelegate);
        return proxy;
    }

}
