// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: UserPrincipal.java,v 1.15.2.3 2003/06/04 04:47:42 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.http;

import java.security.Principal;


/* ------------------------------------------------------------ */
/** User Principal.
 * Extends the security principal with a method to check if the user is in a
 * role. 
 *
 * @version $Id: UserPrincipal.java,v 1.15.2.3 2003/06/04 04:47:42 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public interface UserPrincipal extends Principal
{    
    /* ------------------------------------------------------------ */
    /** Check authentication status.
     * 
     * Implementations of this method may adorn the calling context to
     * assoicate it with the authenticated principal (eg ThreadLocals). If
     * such context associations are made, they should be considered valid
     * until a UserRealm.deAuthenticate(UserPrincipal) call is made for this
     * UserPrincipal.
     *
     * @return True if this user is still authenticated.
     */
    public boolean isAuthenticated();
    
    /* ------------------------------------------------------------ */
    /** Check if the user is in a role. 
     * @param role A role name.
     * @return True if the user can act in that role.
     */
    public boolean isUserInRole(String role);   
}
