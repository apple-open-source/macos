/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.security;

import java.security.Principal;
import java.util.Set;

/** The interface for Principal mapping. It defines the mapping from the
operational environment Principal to the application domain principal via
the {@link #getPrincipal(Principal) getPrincipal} method. It also defines
the method for validating the application domain roles to which the operational
environment Principal belongs via the {@link #getPrincipal(Principal) getPrincipal}
method.

@author Scott.Stark@jboss.org
@version $Revision: 1.7 $
*/
public interface RealmMapping
{
    /** Map from the operational environment Principal to the application
     domain principal. This is used by the EJBContext.getCallerPrincipal implentation
     to map from the authenticated principal to a principal in the application
     domain.
    @param principal, the caller principal as known in the operation environment.
    @return the principal 
    */
    public Principal getPrincipal(Principal principal);

    /** Validates the application domain roles to which the operational
    environment Principal belongs.
    @param principal, the caller principal as known in the operation environment.
    @param The Set<Principal> for the application domain roles that the
     principal is to be validated against.
    @return true if the principal has at least one of the roles in the roles set,
        false otherwise.
     */
    public boolean doesUserHaveRole(Principal principal, Set roles);

    /** Return the set of domain roles the principal has been assigned.
    @return The Set<Principal> for the application domain roles that the
     principal has been assigned.
     */
    public Set getUserRoles(Principal principal);
}
