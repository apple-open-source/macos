/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security;

/** An interface for factories of SecurityProxy objects. It is used
to create a SecurityProxy from a security delegate object that is
not a SecurityProxy instance.

@author Scott.Stark@jboss.org
@version $Revision: 1.4 $
*/
public interface SecurityProxyFactory
{
    public SecurityProxy create(Object proxyDelegate);
}
