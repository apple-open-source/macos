package org.jboss.security.plugins;

import java.security.Principal;
import java.util.Set;

/** An MBean interface that unifies the AuthenticationManager and RealmMapping
 * security interfaces implemented by a security manager for a given domain
 * and provides access to this functionaliy across all domains by including
 * the security domain name as a method argument.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public interface SecurityManagerMBean
{

   /** The isValid method is invoked to see if a user identity and associated
    credentials as known in the operational environment are valid proof of the
    user identity.
    @param securityDomain,
    @param principal, the user identity in the operation environment
    @param credential, the proof of user identity as known in the
    operation environment
    @return true if the principal, credential pair is valid, false otherwise.
   */
   public boolean isValid(String securityDomain, Principal principal, Object credential);

    /** Map from the operational environment Principal to the application
     domain principal. This is used by the EJBContext.getCallerPrincipal implentation
     to map from the authenticated principal to a principal in the application
     domain.
    @param principal, the caller principal as known in the operation environment.
    @return the principal
    */
    public Principal getPrincipal(String securityDomain, Principal principal);

    /** Validates the application domain roles to which the operational
    environment Principal belongs.
    @param principal, the caller principal as known in the operation environment.
    @param The Set<Principal> for the application domain roles that the
     principal is to be validated against.
    @return true if the principal has at least one of the roles in the roles set,
        false otherwise.
     */
    public boolean doesUserHaveRole(String securityDomain, Principal principal, Set roles);

    /** Return the set of domain roles the principal has been assigned.
    @return The Set<Principal> for the application domain roles that the
     principal has been assigned.
     */
    public Set getUserRoles(String securityDomain, Principal principal);
}
