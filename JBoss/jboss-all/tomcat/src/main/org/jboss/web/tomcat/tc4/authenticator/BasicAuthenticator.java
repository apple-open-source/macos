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

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.apache.catalina.HttpRequest;
import org.apache.catalina.HttpResponse;
import org.apache.catalina.authenticator.Constants;
import org.apache.catalina.deploy.LoginConfig;
import org.apache.catalina.util.Base64;


/**
 * An <b>Authenticator</b> and <b>Valve</b> implementation of HTTP BASIC
 * Authentication, as outlined in RFC 2617:  "HTTP Authentication: Basic
 * and Digest Access Authentication."
 * <p>
 * Differs from the standard Tomcat version in that it associates the session
 * of any request with any single sign-on session that may exist.
 *
 * @see #authenticate
 * @see AuthenticatorBase#associate
 * @see SingleSignOn
 *
 * @author B Stansberry, based on work by Craig R. McClanahan
 * @version $Revision: 1.1.2.1 $ $Date: 2003/11/22 08:12:44 $ based on Tomcat Revision 1.12
 */

public class BasicAuthenticator
      extends AuthenticatorBase
{


   // ----------------------------------------------------- Instance Variables


   /**
    * The Base64 helper object for this class.
    */
   protected static final Base64 base64Helper = new Base64();


   /**
    * Descriptive information about this implementation.
    */
   protected static final String info =
         BasicAuthenticator.class.getName() + "/1.0";


   // ------------------------------------------------------------- Properties


   /**
    * Return descriptive information about this Valve implementation.
    */
   public String getInfo()
   {

      return (this.info);

   }


   // --------------------------------------------------------- Public Methods


   /**
    * Authenticate the user making this request, based on the specified
    * login configuration.  Return <code>true</code> if any specified
    * constraint has been satisfied, or <code>false</code> if we have
    * created a response challenge already.
    * <p>
    * Differs from the standard Tomcat version in that it associates the
    * session of any request with any single sign-on session that may exist.
    *
    * @see AuthenticatorBase#associate
    * @see SingleSignOn
    *
    * @param request   Request we are processing
    * @param response  Response we are creating
    * @param config    Login configuration describing how authentication
    *                  should be performed
    *
    * @exception IOException if an input/output error occurs
    */
   public boolean authenticate(HttpRequest request,
                               HttpResponse response,
                               LoginConfig config)
         throws IOException
   {

      HttpServletRequest hreq =
            (HttpServletRequest) request.getRequest();

      // Have we already authenticated someone?
      // Note: SingleSignOn does not call setUserPrincipal(),
      // so whoever did it would have to be some custom valve
      // in the pipeline
      Principal principal = hreq.getUserPrincipal();
      String ssoId = (String) request.getNote(Constants.REQ_SSOID_NOTE);
      if (principal != null)
      {
         if (debug >= 1)
            log("Found principal '" + principal.getName() + "'");
         // Associate the session with any existing SSO session
         if (ssoId != null)
            associate(ssoId, getSession(request, true));
         return true;
      }

      // Is there an SSO session against which we can try to reauthenticate?
      if (ssoId != null)
      {
         if (debug >= 1)
            log("SSO Id set");
         // Try to reauthenticate using data cached by SSO.  If this fails,
         // either the original SSO logon was of DIGEST or SSL (which
         // we can't reauthenticate ourselves because there is no
         // cached username and password), or the realm denied
         // the user's reauthentication for some reason.
         // In either case we have to prompt the user for a logon */
         if (reauthenticateFromSSO(ssoId, request))
            return true;
      }

      // Validate any credentials already included with this request
      String authorization = request.getAuthorization();
      String username = parseUsername(authorization);
      String password = parsePassword(authorization);
      principal = context.getRealm().authenticate(username, password);
      if (principal != null)
      {
         register(request, response, principal, Constants.BASIC_METHOD,
                  username, password);
         return (true);
      }

      // Send an "unauthorized" response and an appropriate challenge
      String realmName = config.getRealmName();
      if (realmName == null)
         realmName = hreq.getServerName() + ":" + hreq.getServerPort();
      //        if (debug >= 1)
      //            log("Challenging for realm '" + realmName + "'");
      HttpServletResponse hres =
            (HttpServletResponse) response.getResponse();
      hres.setHeader("WWW-Authenticate",
                     "Basic realm=\"" + realmName + "\"");
      hres.setStatus(HttpServletResponse.SC_UNAUTHORIZED);
      //      hres.flushBuffer();
      return (false);

   }


   // ------------------------------------------------------ Protected Methods


   /**
    * Parse the username from the specified authorization credentials.
    * If none can be found, return <code>null</code>.
    *
    * @param authorization Authorization credentials from this request
    */
   protected String parseUsername(String authorization)
   {

      if (authorization == null)
         return (null);
      if (!authorization.toLowerCase().startsWith("basic "))
         return (null);
      authorization = authorization.substring(6).trim();

      // Decode and parse the authorization credentials
      String unencoded =
            new String(base64Helper.decode(authorization.getBytes()));
      int colon = unencoded.indexOf(':');
      if (colon < 0)
         return (null);
      String username = unencoded.substring(0, colon).trim();
      //        String password = unencoded.substring(colon + 1).trim();
      return (username);

   }


   /**
    * Parse the password from the specified authorization credentials.
    * If none can be found, return <code>null</code>.
    *
    * @param authorization Authorization credentials from this request
    */
   protected String parsePassword(String authorization)
   {

      if (authorization == null)
         return (null);
      if (!authorization.startsWith("Basic "))
         return (null);
      authorization = authorization.substring(6).trim();

      // Decode and parse the authorization credentials
      String unencoded =
            new String(base64Helper.decode(authorization.getBytes()));
      int colon = unencoded.indexOf(':');
      if (colon < 0)
         return (null);
      //        String username = unencoded.substring(0, colon).trim();
      String password = unencoded.substring(colon + 1).trim();
      return (password);

   }


}
