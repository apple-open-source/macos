// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: SSORealm.java,v 1.1.2.1 2003/06/04 04:47:42 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.http;

import org.mortbay.util.Credential;

/* ------------------------------------------------------------ */
/** Single Sign On Realm.
 * This interface is a mix-in interface for the UserRealm interface. If an
 * implementation of UserRealm also implements SSORealm, then single signon
 * is supported for that realm.
 
 * @see UserRealm
 * @version $Id: SSORealm.java,v 1.1.2.1 2003/06/04 04:47:42 starksm Exp $
 * @author Greg Wilkins (gregw)
 */

public interface SSORealm
{
    /** Get SSO credentials.
     * This call is used by an authenticator to check if a SSO exists for a request.
     * If SSO authentiation is successful, the requests UserPrincipal and
     * AuthUser fields are set.  If available, the credential used to
     * authenticate the user is returned. If recoverable credentials are not required then
     * null may be return.
     * @param request The request to SSO.
     * @param response The response to SSO.
     * @return A credential if available for SSO authenticated requests.
     */
    public Credential getSingleSignOn(HttpRequest request,
                                      HttpResponse response);
    
    /** Set SSO principal and credential.
     * This call is used by an authenticator to inform the SSO mechanism that
     * a user has signed on. The SSO mechanism should record the principal
     * and credential and update the response with any cookies etc. required. 
     * @param request The authenticated request.
     * @param response The authenticated response/
     * @param principal The principal that has been authenticated.
     * @param credential The credentials used to authenticate.
     */
    
    public void setSingleSignOn(HttpRequest request,
                                HttpResponse response,
                                UserPrincipal principal,
                                Credential credential);
    
    /** Clear SSO for user.
     * @param username The user to clear.
     */
    public void clearSingleSignOn(String username);
}
