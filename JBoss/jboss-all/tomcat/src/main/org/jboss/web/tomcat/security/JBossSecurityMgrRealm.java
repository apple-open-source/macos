/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.tomcat.security;

import java.io.IOException;
import java.security.Principal;
import java.security.cert.X509Certificate;
import java.util.HashSet;
import java.util.Set;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.security.auth.Subject;
import javax.servlet.ServletException;

import org.apache.catalina.LifecycleException;
import org.apache.catalina.Realm;
import org.apache.catalina.Request;
import org.apache.catalina.Response;
import org.apache.catalina.Valve;
import org.apache.catalina.ValveContext;
import org.apache.catalina.Wrapper;
import org.apache.catalina.realm.RealmBase;

import org.jboss.logging.Logger;
import org.jboss.security.AuthenticationManager;
import org.jboss.security.RealmMapping;
import org.jboss.security.SimplePrincipal;
import org.jboss.security.SecurityAssociation;
import org.jboss.security.SubjectSecurityManager;

/** An implementation of the catelinz Realm and Valve interfaces. The Realm
 implementation handles authentication and authorization using the JBossSX
 security framework. It relieas on the JNDI ENC namespace setup by the
 AbstractWebContainer. In particular, it uses the java:comp/env/security
 subcontext to access the security manager interfaces for authorization and
 authenticaton.

 The Valve interface is used to associated the authenticated user with the
 SecurityAssociation class when a request begins so that web components may
 call EJBs and have the principal propagated. The security association is
 removed when the request completes.

@see org.jboss.web.AbstractWebContainer
@see org.jboss.security.AuthenticationManager
@see org.jboss.security.RealmMapping
@see org.jboss.security.SimplePrincipal
@see org.jboss.security.SecurityAssociation
@see org.jboss.security.SubjectSecurityManager

@author Scott.Stark@jboss.org
@version $Revision: 1.1.1.1.2.1 $
*/
public class JBossSecurityMgrRealm extends RealmBase implements Realm, Valve
{
    static Logger log = Logger.getLogger(JBossSecurityMgrRealm.class);
    private String subjectAttributeName = null;

    /** The name of the request attribute under with the authenticated JAAS
     Subject is stored on successful authentication. If null or empty then
     the Subject will not be stored.
     */
    public void setSubjectAttributeName(String subjectAttributeName)
    {
        this.subjectAttributeName = subjectAttributeName;
        if( subjectAttributeName != null && subjectAttributeName.length() == 0 )
           subjectAttributeName = null;
    }

    private Context getSecurityContext()
    {
        Context securityCtx = null;
        // Get the JBoss security manager from the ENC context
        try
        {
            InitialContext iniCtx = new InitialContext();
            securityCtx = (Context) iniCtx.lookup("java:comp/env/security");
        }
        catch(NamingException e)
        {
            // Apparently there is no security context?
        }
        return securityCtx;
    }

    /** Override to allow a single realm to be shared as a realm and valve
     */
    public void start() throws LifecycleException
    {
        if( super.started == true )
           return;
        super.start();
    }
   /** Override to allow a single realm to be shared as a realm and valve
    */
   public void stop() throws LifecycleException
   {
       if( super.started == false )
          return;
       super.stop();
   }

    /**
     * Return the Principal associated with the specified chain of X509
     * client certificates.  If there is none, return <code>null</code>.
     *
     * @param certs Array of client certificates, with the first one in
     * the array being the certificate of the client itself.
     */
    public Principal authenticate(X509Certificate[] certs)
    {
        SimplePrincipal principal = null;
        Context securityCtx = getSecurityContext();
        if( securityCtx == null )
        {
            return null;
        }

        try
        {
            // Get the JBoss security manager from the ENC context
            AuthenticationManager securityMgr = (AuthenticationManager) securityCtx.lookup("securityMgr");
            principal = new SimplePrincipal(certs[0].getSerialNumber() + " " + certs[0].getIssuerDN());
            if( securityMgr.isValid(principal, certs) )
            {
                log.trace("User: "+principal+" is authenticated");
                SecurityAssociation.setPrincipal(principal);
                SecurityAssociation.setCredential(certs);
            }
            else
            {
                log.trace("User: "+principal+" is NOT authenticated");
                principal = null;
            }
        }
        catch(NamingException e)
        {
            log.error("Error during authenticate", e);
        }
        return principal;
    }

    /**
     * <p>Perform request processing as required by this Valve.</p>
     *
     * <p>An individual Valve <b>MAY</b> perform the following actions, in
     * the specified order:</p>
     * <ul>
     * <li>Examine and/or modify the properties of the specified Request and
     *    Response.
     * <li>Examine the properties of the specified Request, completely generate
     *    the corresponding Response, and return control to the caller.
     * <li>Examine the properties of the specified Request and Response, wrap
     *    either or both of these objects to supplement their functionality,
     *    and pass them on.
     * <li>If the corresponding Response was not generated (and control was not
     *    returned, call the next Valve in the pipeline (if there is one) by
     *    executing <code>context.invokeNext()</code>.
     * <li>Examine, but not modify, the properties of the resulting Response
     *    (which was created by a subsequently invoked Valve or Container).
     * </ul>
     *
     * <p>A Valve <b>MUST NOT</b> do any of the following things:</p>
     * <ul>
     * <li>Change request properties that have already been used to direct
     *    the flow of processing control for this request (for instance,
     *    trying to change the virtual host to which a Request should be
     *    sent from a pipeline attached to a Host or Context in the
     *    standard implementation).
     * <li>Create a completed Response <strong>AND</strong> pass this
     *    Request and Response on to the next Valve in the pipeline.
     * <li>Consume bytes from the input stream associated with the Request,
     *    unless it is completely generating the response, or wrapping the
     *    request before passing it on.
     * <li>Modify the HTTP headers included with the Response after the
     *    <code>invokeNext()</code> method has returned.
     * <li>Perform any actions on the output stream associated with the
     *    specified Response after the <code>invokeNext()</code> method has
     *    returned.
     * </ul>
     *
     * @param request The servlet request to be processed
     * @param response The servlet response to be created
     * @param context The valve context used to invoke the next valve
     * in the current processing pipeline
     *
     * @exception IOException if an input/output error occurs, or is thrown
     * by a subsequently invoked Valve, Filter, or Servlet
     * @exception ServletException if a servlet error occurs, or is thrown
     * by a subsequently invoked Valve, Filter, or Servlet
     */
   public void invoke(Request request, Response response, ValveContext context)
       throws IOException, ServletException
   {
      try
      {
         try
         {
            Context securityCtx = getSecurityContext();
            if( subjectAttributeName != null && securityCtx != null )
            {
               // Get the JBoss security manager from the ENC context
               AuthenticationManager securityMgr = (AuthenticationManager) securityCtx.lookup("securityMgr");
               if(  securityMgr instanceof SubjectSecurityManager )
               {
                  SubjectSecurityManager subjectMgr = (SubjectSecurityManager) securityMgr;
                  Subject subject = subjectMgr.getActiveSubject();
                  request.getRequest().setAttribute(subjectAttributeName, subject);
               }
            }
         }
         catch(Throwable ignore)
         {
         }
         // Perform the request
         context.invokeNext(request, response);
      }
      finally
      {
         SecurityAssociation.setPrincipal(null);
         SecurityAssociation.setCredential(null);
      }
   }

    /**
     * Return the Principal associated with the specified username, which
     * matches the digest calculated using the given parameters using the
     * method described in RFC 2069; otherwise return <code>null</code>.
     *
     * @param username Username of the Principal to look up
     * @param digest Digest which has been submitted by the client
     * @param nonce Unique (or supposedly unique) token which has been used
     * for this request
     * @param realm Realm name
     * @param md5a2 Second MD5 digest used to calculate the digest :
     * MD5(Method + ":" + uri)
     */
    public Principal authenticate(String username, String digest, String nonce,
       String nc, String cnonce, String qop, String realm, String md5a2)
    {
       return super.authenticate(username, digest, nonce,
         nc, cnonce, qop, realm, md5a2);
    }

    /**
     * Return the Principal associated with the specified username and
     * credentials, if there is one; otherwise return <code>null</code>.
     *
     * @param username Username of the Principal to look up
     * @param credentials Password or other credentials to use in
     * authenticating this username
     */
    public Principal authenticate(String username, String credentials)
    {
       boolean trace = log.isTraceEnabled();
       if( trace )
          log.trace("Begin authenticate, username="+username);
        SimplePrincipal principal = null;
        Context securityCtx = getSecurityContext();
        if( securityCtx == null )
        {
            return null;
        }

       Principal caller = (Principal) SecurityAssociationValve.userPrincipal.get();
       if( caller == null && username == null && credentials == null )
         return null;

        try
        {
            // Get the JBoss security manager from the ENC context
            AuthenticationManager securityMgr = (AuthenticationManager) securityCtx.lookup("securityMgr");
            principal = new SimplePrincipal(username);
            char[] passwordChars = null;
            if( credentials != null )
               passwordChars = credentials.toCharArray();
            if( securityMgr.isValid(principal, passwordChars) )
            {
                log.trace("User: "+username+" is authenticated");
                SecurityAssociation.setPrincipal(principal);
                SecurityAssociation.setCredential(passwordChars);
            }
            else
            {
                principal = null;
                log.trace("User: "+username+" is NOT authenticated");
            }
        }
        catch(NamingException e)
        {
            principal = null;
            log.error("Error during authenticate", e);
        }
       if( trace )
          log.trace("End authenticate, principal="+principal);
        return principal;
    }

    /**
     * Return the Principal associated with the specified username and
     * credentials, if there is one; otherwise return <code>null</code>.
     *
     * @param username Username of the Principal to look up
     * @param credentials Password or other credentials to use in
     * authenticating this username
     */
    public Principal authenticate(String username, byte[] credentials)
    {
       return authenticate(username, new String(credentials));
    }

    /**
     * Return <code>true</code> if the specified Principal has the specified
     * security role, within the context of this Realm; otherwise return
     * <code>false</code>.
     *
     * @param principal Principal for whom the role is to be checked
     * @param role Security role to be checked
     */
    public boolean hasRole(Principal principal, String role)
    {
       boolean trace = log.isTraceEnabled();
       if( trace )
          log.trace("Begin hasRole, principal="+principal+", role="+role);
       boolean hasRole = false;
        try
        {
            Set requiredRoles = new HashSet();
            requiredRoles.add(new SimplePrincipal(role));
            // Get the JBoss security manager from the ENC context
            Context securityCtx = getSecurityContext();
            if( securityCtx != null )
            {
                RealmMapping securityMgr = (RealmMapping) securityCtx.lookup("realmMapping");
                hasRole = securityMgr.doesUserHaveRole(principal, requiredRoles);
            }
            else
            {
                log.warn("Warning: no security context available");
            }

            if( hasRole )
            {
                log.trace("User: "+principal+" is authorized");
            }
            else
            {
                RealmMapping securityMgr = (RealmMapping) securityCtx.lookup("realmMapping");
                Set userRoles = securityMgr.getUserRoles(principal);
                log.trace("User: "+principal+" is NOT authorized, requiredRoles="+requiredRoles+", userRoles="+userRoles);
            }
        }
        catch(NamingException e)
        {
            log.error("Error during authorize", e);
        }
       if( trace )
          log.trace("End hasRole, principal="+principal+", role="+role+", hasRole="+hasRole);
       return hasRole;
    }

    /**
     * Return a short name for this Realm implementation, for use in
     * log messages.
     */
    protected String getName()
    {
       return getClass().getName();
    }

    /**
     * Return the password associated with the given principal's user name.
     */
    protected String getPassword(String username)
    {
       String password = null;
       return password;
    }

    /**
     * Return the Principal associated with the given user name.
     */
    protected Principal getPrincipal(String username)
    {
       return new SimplePrincipal(username);
    }
}
