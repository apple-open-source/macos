// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: ClientCertAuthenticator.java,v 1.3.2.7 2003/06/04 04:47:40 starksm Exp $
// ========================================================================

package org.mortbay.http;

import java.io.IOException;
import java.security.Principal;
import javax.net.ssl.SSLSocket;
import org.mortbay.http.SecurityConstraint.Authenticator;
import org.mortbay.util.Code;

/* ------------------------------------------------------------ */
/** Client Certificate Authenticator.
 * This Authenticator uses a client certificate to authenticate the user.
 * Each client certificate supplied is tried against the realm using the
 * Principal name as the username and a string representation of the
 * certificate as the credential.
 * @version $Id: ClientCertAuthenticator.java,v 1.3.2.7 2003/06/04 04:47:40 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public class ClientCertAuthenticator implements Authenticator
{
    private int _maxHandShakeSeconds =60;
    
    /* ------------------------------------------------------------ */
    public ClientCertAuthenticator()
    {
        Code.warning("Client Cert Authentication is EXPERIMENTAL");
    }
    
    /* ------------------------------------------------------------ */
    public int getMaxHandShakeSeconds()
    {
        return _maxHandShakeSeconds;
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @param maxHandShakeSeconds Maximum time to wait for SSL handshake if
     * Client certification is required.
     */
    public void setMaxHandShakeSeconds(int maxHandShakeSeconds)
    {
        _maxHandShakeSeconds = maxHandShakeSeconds;
    }
    
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
        java.security.cert.X509Certificate[] certs =
            (java.security.cert.X509Certificate[])
            request.getAttribute("javax.servlet.request.X509Certificate");
        if (certs==null || certs.length==0 || certs[0]==null)
        {
            // No certs available so lets try and force the issue
            
            // Get the SSLSocket
            Object s = HttpConnection.getHttpConnection().getConnection();
            if (!(s instanceof SSLSocket))
                return null;
            SSLSocket socket = (SSLSocket)s;
            
            if (!socket.getNeedClientAuth())
            {
                // Need to re-handshake
                socket.setNeedClientAuth(true);
                socket.startHandshake();

                // Need to wait here - but not forever. The Handshake
                // Listener API does not look like a good option to
                // avoid waiting forever.  So we will take a slightly
                // busy timelimited approach. For now:
                for (int i=(_maxHandShakeSeconds*4);i-->0;)
                {
                    certs = (java.security.cert.X509Certificate[])
                        request.getAttribute("javax.servlet.request.X509Certificate");
                    if (certs!=null && certs.length>0 && certs[0]!=null)
                        break;
                    try{Thread.sleep(250);} catch (Exception e) {break;}
                }
            }
        }

        if (certs==null || certs.length==0 || certs[0]==null)
            return null;
        
        Principal principal = certs[0].getSubjectDN();
        if (principal==null)
            principal=certs[0].getIssuerDN();
        UserPrincipal user =
            realm.authenticate(principal==null?"clientcert":principal.getName(),
                               certs,request);
        
        request.setAuthType(SecurityConstraint.__CERT_AUTH);
        request.setAuthUser(user.getName());
        request.setUserPrincipal(user);                
        return user;
    }
    
    public String getAuthMethod()
    {
        return SecurityConstraint.__CERT_AUTH;
    }

}
