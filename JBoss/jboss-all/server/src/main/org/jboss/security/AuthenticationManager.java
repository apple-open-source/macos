/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security;

import java.security.Principal;

/** The SecurityManager is responsible for validating credentials
 * associated with principals.
 *      
 *   @author Scott.Stark@jboss.org
 *   @version $Revision: 1.2 $
 */
public interface AuthenticationManager
{
   /** The isValid method is invoked to see if a user identity and associated
    credentials as known in the operational environment are valid proof of the
    user identity.
    @param principal, the user identity in the operation environment 
    @param credential, the proof of user identity as known in the
    operation environment 
    @return true if the principal, credential pair is valid, false otherwise.
   */
   public boolean isValid(Principal principal, Object credential);
}
