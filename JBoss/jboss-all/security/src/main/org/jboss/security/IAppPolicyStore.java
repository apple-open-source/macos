/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security;


/** An interface describing an AppPolicy security store. It is used by
the SecurityPolicy class to isolate the source of security information
from the SecurityPolicy.

@author Scott.Stark@jboss.org
@version $Revision: 1.3 $
*/
public interface IAppPolicyStore
{
    public AppPolicy getAppPolicy(String appName);
    public void refresh();

    /** @link aggregation 
     * @supplierCardinality 1..*
     * @clientCardinality 1*/
    /*#AppPolicy lnkAppPolicy;*/
}
