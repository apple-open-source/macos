// ===========================================================================
// Copyright (c) 2003 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: HashSSORealm.java,v 1.1.2.1 2003/06/04 04:47:41 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.http;

import java.util.HashMap;
import java.security.SecureRandom;
import java.util.Random;
import org.mortbay.util.Credential;
import javax.servlet.http.Cookie;
import org.mortbay.util.Code;


public class HashSSORealm implements SSORealm
{
    /* ------------------------------------------------------------ */
    public static final String SSO_COOKIE_NAME = "SSO_ID";
    private HashMap _ssoId2Principal = new HashMap();
    private HashMap _ssoUsername2Id = new HashMap();
    private HashMap _ssoPrincipal2Credential = new HashMap();
    private transient Random _random = new SecureRandom();
    
    /* ------------------------------------------------------------ */
    public Credential getSingleSignOn(HttpRequest request,
                                      HttpResponse response)
    {
        String ssoID = null;
        Cookie[] cookies = request.getCookies();
        for (int i = 0; i < cookies.length; i++)
        {
            if (cookies[i].getName().equals(SSO_COOKIE_NAME))
            {
                ssoID = cookies[i].getValue();
                break;
            }
        }
        Code.debug("get ssoID=",ssoID);
        
        UserPrincipal principal=null;
        Credential credential=null;
        synchronized(_ssoId2Principal)
        {
            principal=(UserPrincipal)_ssoId2Principal.get(ssoID);
            credential=(Credential)_ssoPrincipal2Credential.get(principal);
        }
        
        Code.debug("SSO principal=",principal);
        
        if (principal!=null && credential!=null)
        {
            if (principal.isAuthenticated())
            {
                request.setUserPrincipal(principal);
                request.setAuthUser(principal.getName());
                return credential;
            }
            else
            {
                synchronized(_ssoId2Principal)
                {
                    _ssoId2Principal.remove(ssoID);
                    _ssoPrincipal2Credential.remove(principal);
                    _ssoUsername2Id.remove(principal.getName());
                }    
            }
        }
        return null;
    }
    
    
    /* ------------------------------------------------------------ */
    public void setSingleSignOn(HttpRequest request,
                                HttpResponse response,
                                UserPrincipal principal,
                                Credential credential)
    {
        
        String ssoID=null;
        
        synchronized(_ssoId2Principal)
        {
            // Create new SSO ID
            while (true)
            {
                ssoID = Long.toString(Math.abs(_random.nextLong()),
                                      30 + (int)(System.currentTimeMillis() % 7));
                if (!_ssoId2Principal.containsKey(ssoID))
                    break;
            }
            
            Code.debug("set ssoID=",ssoID);
            _ssoId2Principal.put(ssoID,principal);
            _ssoPrincipal2Credential.put(principal,credential);
            _ssoUsername2Id.put(principal.getName(),ssoID);
        }
        
        Cookie cookie = new Cookie(SSO_COOKIE_NAME, ssoID);
        cookie.setPath("/");
        response.addSetCookie(cookie);
    }
    
    
    /* ------------------------------------------------------------ */
    public void clearSingleSignOn(String username)
    {
        synchronized(_ssoId2Principal)
        {
            Object ssoID=_ssoUsername2Id.remove(username);
            Object principal=_ssoId2Principal.remove(ssoID);
            _ssoPrincipal2Credential.remove(principal);
        }        
    }
}
