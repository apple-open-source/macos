// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: BasicAuthenticator.java,v 1.3.2.8 2003/06/04 04:47:40 starksm Exp $
// ========================================================================

package org.mortbay.http;

import java.io.IOException;
import org.mortbay.http.SecurityConstraint.Authenticator;
import org.mortbay.util.B64Code;
import org.mortbay.util.Code;
import org.mortbay.util.StringUtil;

/* ------------------------------------------------------------ */
/** BASIC authentication.
 *
 * @version $Id: BasicAuthenticator.java,v 1.3.2.8 2003/06/04 04:47:40 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public class BasicAuthenticator implements Authenticator
{
    /* ------------------------------------------------------------ */
    /** 
     * @return UserPrinciple if authenticated or null if not. If
     * Authentication fails, then the authenticator may have committed
     * the response as an auth challenge or redirect.
     * @exception IOException 
     */
    public UserPrincipal authenticated(UserRealm realm,
                                       String pathInContext,
                                       HttpRequest request,
                                       HttpResponse response)
        throws IOException
    {
        // Get the user if we can
        UserPrincipal user=null;
        String credentials = request.getField(HttpFields.__Authorization);
        
        if (credentials!=null )
        {
            try
            {
                Code.debug("Credentials: ",credentials);
                credentials = credentials.substring(credentials.indexOf(' ')+1);
                credentials = B64Code.decode(credentials,StringUtil.__ISO_8859_1);
                int i = credentials.indexOf(':');
                String username = credentials.substring(0,i);
                String password = credentials.substring(i+1);
            
                user = realm.authenticate(username,password,request);
                if (user!=null)
                {
                    request.setAuthType(SecurityConstraint.__BASIC_AUTH);
                    request.setAuthUser(username);
                    request.setUserPrincipal(user);                
                }
                else
                    Code.warning("AUTH FAILURE: user "+username);
            }
            catch (Exception e)
            {
                Code.warning("AUTH FAILURE: "+e.toString());
                Code.ignore(e);
            }
        }

        // Challenge if we have no user
        if (user==null)
            sendChallenge(realm,response);
        
        return user;
    }
    
    /* ------------------------------------------------------------ */
    public String getAuthMethod()
    {
        return SecurityConstraint.__BASIC_AUTH;
    }

    /* ------------------------------------------------------------ */
    public void sendChallenge(UserRealm realm,
                              HttpResponse response)
        throws IOException
    {
        response.setField(HttpFields.__WwwAuthenticate,
                          "basic realm=\""+realm.getName()+'"');
        response.sendError(HttpResponse.__401_Unauthorized);
    }
    
}
    
