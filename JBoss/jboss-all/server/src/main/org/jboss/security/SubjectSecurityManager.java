/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.security;

import java.security.Principal;
import javax.security.auth.Subject;


/** An extension of the AuthenticationManager that adds the notion of the active
Subject and security domain.

@author Scott.Stark@jboss.org
@version $Revision: 1.6.4.1 $
*/
public interface SubjectSecurityManager extends AuthenticationManager
{
    /** Get the security domain from which the security manager is from. Every
        security manager belongs to a named domain. The meaning of the security
        domain name depends on the implementation. Examples range from as fine
        grained as the name of EJBs to J2EE application names to DNS domain names.
    @return the security domain name. May be null in which case the security
        manager belongs to the logical default domain.
    */
    public String getSecurityDomain();
    /** Get the currently authenticated subject. After a successful isValid()
        call, a SubjectSecurityManager has a Subject associated with the current
        thread. This Subject will typically contain the Principal passed to isValid
        as well as any number of additional Principals, and credentials. Note
        that although the Subject is local to the thread, its internal state
        may not be if there are multiple threads for the same principal active.
    @see AuthenticationManager#isValid(Principal, Object)
    @see #isValid(Principal, Object, Subject)
    @return The previously authenticated Subject if isValid succeeded, null if
        isValid failed or has not been called for the active thread.
    */
    public Subject getActiveSubject();

   /** The isValid method is invoked to see if a user identity and associated
       credentials as known in the operational environment are valid proof of the
       user identity. This extends AuthenticationManager version to provide a
       copy of the resulting authenticated Subject. This allows a caller to
       authenticate a user and obtain a Subject whose state cannot be modified
       by other threads associated with the same principal.
    @param principal, the user identity in the operation environment 
    @param credential, the proof of user identity as known in the
    operation environment 
    @return true if the principal, credential pair is valid, false otherwise.
   */
   public boolean isValid(Principal principal, Object credential,
      Subject activeSubject);
}
