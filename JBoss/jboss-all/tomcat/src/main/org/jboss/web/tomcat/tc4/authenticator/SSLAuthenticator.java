/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 * This software is derived from works developed by the
 * Apache Software Foundation (http://www.apache.org/), and its
 * redistribution and use are further subject to the terms of the
 * Apache Software License (see below), which is herein incorporated
 * by reference.
 *
 * ====================================================================
 *
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 1999 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution, if
 *    any, must include the following acknowlegement:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowlegement may appear in the software itself,
 *    if and wherever such third-party acknowlegements normally appear.
 *
 * 4. The names "The Jakarta Project", "Tomcat", and "Apache Software
 *    Foundation" must not be used to endorse or promote products derived
 *    from this software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 * [Additional notices, if required by prior licensing conditions]
 *
 */
package org.jboss.web.tomcat.tc4.authenticator;


import java.io.IOException;
import java.security.Principal;
import java.security.cert.X509Certificate;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.apache.catalina.Globals;
import org.apache.catalina.HttpRequest;
import org.apache.catalina.HttpResponse;
import org.apache.catalina.LifecycleException;
import org.apache.catalina.authenticator.Constants;
import org.apache.catalina.deploy.LoginConfig;


/**
 * An <b>Authenticator</b> and <b>Valve</b> implementation of authentication
 * that utilizes SSL certificates to identify client users.
 * <p>
 * Differs from the standard Tomcat version in that it 1) associates the session
 * of any request with any single sign-on session that may exist and 2) only
 * ignores its own authentication process if a given request already has
 * a <code>Principal</code> bound to it AND this object's <code>cache</code>
 * property is set to <code>true</code>.
 *
 * @see #authenticate
 * @see AuthenticatorBase#associate
 * @see AuthenticatorBase#getCache
 * @see AuthenticatorBase#setCache
 * @see SingleSignOn
 *
 * @author B Stansberry, based on work by Craig R. McClanahan
 * @version $Revision: 1.1.2.1 $ $Date: 2003/11/22 08:12:44 $ based on Tomcat Revision 1.13
 */

public class SSLAuthenticator
      extends AuthenticatorBase
{


   // ------------------------------------------------------------- Properties


   /**
    * Descriptive information about this implementation.
    */
   protected static final String info =
         SSLAuthenticator.class.getName() + "/1.0";


   /**
    * Return descriptive information about this Valve implementation.
    */
   public String getInfo()
   {

      return (this.info);

   }


   // --------------------------------------------------------- Public Methods


   /**
    * Authenticate the user by checking for the existence of a certificate
    * chain (which should have been made visible by an instance of
    * <code>CertificatesValve</code), and optionally asking a trust
    * manager to validate that we trust this user.
    * <p>
    * Differs from the standard Tomcat version in that it
    * <ol>
    * <li>it associates the session of any request with any single sign-on
    * session that may exist.</li>
    * <li>If the request already has an authenticated <code>Principal</code>,
    * our own authentication is only disabled if <code>getCache()</code>
    * returns <code>true</code>.</li>
    * </ol>
    *
    * @see AuthenticatorBase#associate
    * @see SingleSignOn
    *
    * @param request Request we are processing
    * @param response Response we are creating
    * @param config    Login configuration describing how authentication
    *              should be performed
    *
    * @exception IOException if an input/output error occurs
    */
   public boolean authenticate(HttpRequest request,
                               HttpResponse response,
                               LoginConfig config)
         throws IOException
   {

      // Have we already authenticated someone?
      // Note: SingleSignOn does not call setUserPrincipal(),
      // so whoever did it would have to be some custom valve
      // in the pipeline.
      Principal principal =
            ((HttpServletRequest) request.getRequest()).getUserPrincipal();
      //String ssoId = (String) request.getNote(Constants.REQ_SSOID_NOTE);
      if (principal != null)
      {
         if (debug >= 1)
            log("Found principal '" + principal.getName() + "'");
         // Associate the session with any existing SSO session in order
         // to get coordinated session invalidation at logout
         String ssoId = (String) request.getNote(Constants.REQ_SSOID_NOTE);
         if (ssoId != null)
            associate(ssoId, getSession(request, true));
         return (true);
      }

      // NOTE: We don't try to reauthenticate using any existing SSO session,
      // because that will only work if the original authentication was
      // BASIC or FORM, which are less secure than the CLIENT-CERT auth-type
      // specified for this webapp
      //
      // Uncomment below to allow previous FORM or BASIC authentications
      // to authenticate users for this webapp
      // TODO make this a configurable attribute (in SingleSignOn??)
      /*
      // Is there an SSO session against which we can try to reauthenticate?
      if (ssoId != null) {
          if (debug >= 1)
              log("SSO Id " + ssoId + " set; attempting reauthentication");
          // Try to reauthenticate using data cached by SSO.  If this fails,
          // either the original SSO logon was of DIGEST or SSL (which
          // we can't reauthenticate ourselves because there is no
          // cached username and password), or the realm denied
          // the user's reauthentication for some reason.
          // In either case we have to prompt the user for a logon
          if (reauthenticateFromSSO(ssoId, request))
              return true;
      }
      */

      // Retrieve the certificate chain for this client
      HttpServletResponse hres =
            (HttpServletResponse) response.getResponse();
      if (debug >= 1)
         log(" Looking up certificates");
      X509Certificate certs[] = (X509Certificate[])
            request.getRequest().getAttribute(Globals.CERTIFICATES_ATTR);
      if ((certs == null) || (certs.length < 1))
      {
         certs = (X509Certificate[])
               request.getRequest().getAttribute(Globals.SSL_CERTIFICATE_ATTR);
      }
      if ((certs == null) || (certs.length < 1))
      {
         if (debug >= 1)
            log("  No certificates included with this request");
         hres.sendError(HttpServletResponse.SC_BAD_REQUEST,
                        sm.getString("authenticator.certificates"));
         return (false);
      }

      // Authenticate the specified certificate chain
      principal = context.getRealm().authenticate(certs);
      if (principal == null)
      {
         if (debug >= 1)
            log("  Realm.authenticate() returned false");
         hres.sendError(HttpServletResponse.SC_UNAUTHORIZED,
                        sm.getString("authenticator.unauthorized"));
         return (false);
      }

      // Cache the principal (if requested) and record this authentication
      // Note that if there already is an SSO session assoc with this request,
      // register() associates this app's session with the SSO session
      register(request, response, principal, Constants.CERT_METHOD,
               null, null);
      return (true);

   }


   // ------------------------------------------------------ Lifecycle Methods


   /**
    * Initialize the database we will be using for client verification
    * and certificate validation (if any).
    *
    * @exception LifecycleException if this component detects a fatal error
    *  that prevents this component from being used
    */
   public void start() throws LifecycleException
   {

      super.start();

   }


   /**
    * Finalize the database we used for client verification and
    * certificate validation (if any).
    *
    * @exception LifecycleException if this component detects a fatal error
    *  that prevents this component from being used
    */
   public void stop() throws LifecycleException
   {

      super.stop();

   }


}
