// ===========================================================================
// Copyright (c) 1996-2002 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: FormAuthenticator.java,v 1.4.2.13 2003/06/04 04:47:51 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.jetty.servlet;

import java.io.IOException;
import java.io.Serializable;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;
import javax.servlet.http.HttpSessionBindingEvent;
import javax.servlet.http.HttpSessionBindingListener;
import org.mortbay.http.HttpContext;
import org.mortbay.http.HttpRequest;
import org.mortbay.http.HttpResponse;
import org.mortbay.http.SecurityConstraint;
import org.mortbay.http.SecurityConstraint.Authenticator;
import org.mortbay.http.UserPrincipal;
import org.mortbay.http.UserRealm;
import org.mortbay.http.SSORealm;
import org.mortbay.util.Code;
import org.mortbay.util.URI;
import org.mortbay.util.Credential;
import org.mortbay.util.Password;

/* ------------------------------------------------------------ */
/** FORM Authentication Authenticator.
 * The HTTP Session is used to store the authentication status of the
 * user, which can be distributed.
 * If the realm implements SSORealm, SSO is supported.
 *
 * @version $Id: FormAuthenticator.java,v 1.4.2.13 2003/06/04 04:47:51 starksm Exp $
 * @author Greg Wilkins (gregw)
 * @author dan@greening.name
 */
public class FormAuthenticator implements Authenticator
{
    /* ------------------------------------------------------------ */
    public final static String __J_URI="org.mortbay.jetty.URI";
    public final static String __J_AUTHENTICATED="org.mortbay.jetty.Auth";
    public final static String __J_SECURITY_CHECK="j_security_check";
    public final static String __J_USERNAME="j_username";
    public final static String __J_PASSWORD="j_password";
    public final static String __SSO_SIGNOFF="SSO_SIGNOFF";

    private String _formErrorPage;
    private String _formErrorPath;
    private String _formLoginPage;
    private String _formLoginPath;
    private transient SSORealm _ssoRealm;
    
    /* ------------------------------------------------------------ */
    public String getAuthMethod()
    {
        return HttpServletRequest.FORM_AUTH;
    }

    /* ------------------------------------------------------------ */
    public void setLoginPage(String path)
    {
        if (!path.startsWith("/"))
        {
            Code.warning("form-login-page must start with /");
            path="/"+path;
        }
        _formLoginPage=path;
        _formLoginPath=path;
        if (_formLoginPath.indexOf('?')>0)
            _formLoginPath=_formLoginPath.substring(0,_formLoginPath.indexOf('?'));
    }

    /* ------------------------------------------------------------ */
    public String getLoginPage()
    {
        return _formLoginPage;
    }
    
    /* ------------------------------------------------------------ */
    public void setErrorPage(String path)
    {
        if (!path.startsWith("/"))
        {
            Code.warning("form-error-page must start with /");
            path="/"+path;
        }
        _formErrorPage=path;
        _formErrorPath=path;
        if (_formErrorPath.indexOf('?')>0)
            _formErrorPath=_formErrorPath.substring(0,_formErrorPath.indexOf('?'));
    }

    /* ------------------------------------------------------------ */
    public String getErrorPage()
    {
        return _formErrorPage;
    }
    
    /* ------------------------------------------------------------ */
    /** Perform form authentication.
     * Called from SecurityHandler.
     * @return UserPrincipal if authenticated else null.
     */
    public UserPrincipal authenticated(UserRealm realm,
                                       String pathInContext,
                                       HttpRequest httpRequest,
                                       HttpResponse httpResponse)
        throws IOException
    {
        HttpServletRequest request =(ServletHttpRequest)httpRequest.getWrapper();
        HttpServletResponse response =(HttpServletResponse) httpResponse.getWrapper();

        if (realm instanceof SSORealm)
            _ssoRealm=(SSORealm)realm;
        else if (_ssoRealm!=null)
        {
            Code.warning("Mixed realms");
            _ssoRealm=null;
        }
        
        // Handle paths
        String uri = pathInContext;

        // Setup session 
        HttpSession session=request.getSession(true);
        
        // Handle a request for authentication.
        if ( uri.substring(uri.lastIndexOf("/")+1).startsWith(__J_SECURITY_CHECK) )
        {
            // Check the session object for login info.
            FormCredential form_cred=new FormCredential();
            form_cred._jUserName = request.getParameter(__J_USERNAME);
            form_cred._jPassword = request.getParameter(__J_PASSWORD);
            
            form_cred._userPrincipal = realm.authenticate(form_cred._jUserName,
                                                           form_cred._jPassword,
                                                           httpRequest);
            
            String nuri=(String)session.getAttribute(__J_URI);
            if (nuri==null || nuri.length()==0)
                nuri="/";
            
            if (form_cred._userPrincipal!=null)
            {
                // Authenticated OK
                Code.debug("Form authentication OK for ",form_cred._jUserName);
                session.removeAttribute(__J_URI); // Remove popped return URI.
                httpRequest.setAuthType(SecurityConstraint.__FORM_AUTH);
                httpRequest.setAuthUser(form_cred._jUserName);
                httpRequest.setUserPrincipal(form_cred._userPrincipal);
                session.setAttribute(__J_AUTHENTICATED,form_cred);

                // Sign-on to SSO mechanism
                if (_ssoRealm!=null)
                {
                    _ssoRealm.setSingleSignOn(httpRequest,
                                              httpResponse,
                                              form_cred._userPrincipal,
                                              new Password(form_cred._jPassword));
                    session.setAttribute(__SSO_SIGNOFF,
                                         new SSOSignoff(form_cred._userPrincipal));
                }

                // Redirect to original request
                response.sendRedirect(response.encodeRedirectURL(nuri));
            }
            else
            {
                Code.debug("Form authentication FAILED for ",form_cred._jUserName);
                if (_formErrorPage!=null)
                {
                    response.sendRedirect(response.encodeRedirectURL
                                          (URI.addPaths(request.getContextPath(),
                                                        _formErrorPage)));
                }
                else
                {
                    response.sendError(HttpResponse.__403_Forbidden);
                }
            }
            
            // Security check is always false, only true after final redirection.
            return null;
        }
        
        // Check if the session is already authenticated.
        FormCredential form_cred = (FormCredential) session.getAttribute(__J_AUTHENTICATED);
        
        if (form_cred != null)
        {
            // We have a form credential. Has it been distributed?
            if (form_cred._userPrincipal==null)
            {
                // This form_cred appears to have been distributed.  Need to reauth
                form_cred._userPrincipal = realm.authenticate(form_cred._jUserName,
                                                              form_cred._jPassword,
                                                              httpRequest);
                // Sign-on to SSO mechanism
                if (_ssoRealm!=null)
                {
                    _ssoRealm.setSingleSignOn(httpRequest,
                                              httpResponse,
                                              form_cred._userPrincipal,
                                              new Password(form_cred._jPassword));
                    session.setAttribute(__SSO_SIGNOFF,
                                         new SSOSignoff(form_cred._userPrincipal));
                }
            }
            
            // Check that it is still authenticated.
            else if (!form_cred._userPrincipal.isAuthenticated())
                form_cred._userPrincipal=null;

            // If this credential is still authenticated
            if (form_cred._userPrincipal!=null)
            {
                Code.debug("FORM Authenticated for ",form_cred._userPrincipal.getName());
                httpRequest.setAuthType(SecurityConstraint.__FORM_AUTH);
                httpRequest.setAuthUser(form_cred._userPrincipal.getName());
                httpRequest.setUserPrincipal(form_cred._userPrincipal);
                return form_cred._userPrincipal;
            }
            else
                session.setAttribute(__J_AUTHENTICATED,null);
        }
        else if (_ssoRealm!=null)
        {
            // Try a single sign on.
            Credential cred = _ssoRealm.getSingleSignOn(httpRequest,httpResponse);
            
            if (request.getUserPrincipal()!=null)
            {
                form_cred=new FormCredential();
                form_cred._userPrincipal=(UserPrincipal)request.getUserPrincipal();
                form_cred._jUserName=form_cred._userPrincipal.toString();
                if (cred!=null)
                    form_cred._jPassword=cred.toString();
                Code.debug("SSO for ",form_cred._userPrincipal);
                           
                httpRequest.setAuthType(SecurityConstraint.__FORM_AUTH);
                session.setAttribute(__J_AUTHENTICATED,form_cred);
                session.setAttribute(__SSO_SIGNOFF,
                                     new SSOSignoff(form_cred._userPrincipal));
                return form_cred._userPrincipal;
            }
        }
        
        // Don't authenticate authform or errorpage
        if (pathInContext!=null &&
            pathInContext.equals(_formErrorPath) || pathInContext.equals(_formLoginPath))
            return SecurityConstraint.__NOBODY;
        
        // redirect to login page
        if (httpRequest.getQuery()!=null)
            uri+="?"+httpRequest.getQuery();
        session.setAttribute(__J_URI, 
        	request.getScheme() + "://" + request.getServerName() + ":" + request.getServerPort() 
        	+ URI.addPaths(request.getContextPath(),uri));
        response.sendRedirect(response.encodeRedirectURL(URI.addPaths(request.getContextPath(),
                                                                      _formLoginPage)));
        return null;
    }


    /* ------------------------------------------------------------ */
    /** FORM Authentication credential holder.
     */
    private static class FormCredential implements Serializable
    {
        private String _jUserName;
        private String _jPassword;
        private transient UserPrincipal _userPrincipal;
        
        public int hashCode()
        {
            return _jUserName.hashCode()+_jPassword.hashCode();
        }

        public boolean equals(Object o)
        {
            if (!(o instanceof FormCredential))
                return false;
            FormCredential fc = (FormCredential)o;
            return
                _jUserName.equals(fc._jUserName) &&
                _jPassword.equals(fc._jPassword);
        }

        public String toString()
        {
            return "Cred["+_jUserName+"]";
        }

    }

    /* ------------------------------------------------------------ */
    private class SSOSignoff implements Serializable,HttpSessionBindingListener
    {
        private String _username;

        SSOSignoff(UserPrincipal principal){_username=principal.getName();}
        
        public void valueBound(HttpSessionBindingEvent event) {}
        
        public void valueUnbound(HttpSessionBindingEvent event)
        {
            Code.debug("SSO signoff",_username);
            if(_ssoRealm!=null)
                _ssoRealm.clearSingleSignOn(_username);
        }

        public String toString()
        {
            return _username;
        }
    }
}
