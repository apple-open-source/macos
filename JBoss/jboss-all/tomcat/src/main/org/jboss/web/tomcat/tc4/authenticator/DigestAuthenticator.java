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
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.Principal;
import java.util.Hashtable;
import java.util.StringTokenizer;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.apache.catalina.HttpRequest;
import org.apache.catalina.HttpResponse;
import org.apache.catalina.Realm;
import org.apache.catalina.authenticator.Constants;
import org.apache.catalina.deploy.LoginConfig;
import org.apache.catalina.util.MD5Encoder;


/**
 * An <b>Authenticator</b> and <b>Valve</b> implementation of HTTP DIGEST
 * Authentication (see RFC 2069).
 * <p>
 * Differs from the standard Tomcat version in that it associates the session
 * of any request with any single sign-on session that may exist.
 *
 * @see #authenticate
 * @see AuthenticatorBase#associate
 * @see SingleSignOn
 *
 * @author B Stansberry, based on work by Craig R. McClanahan and Remy Maucherat
 * @version $Revision: 1.1.2.1 $ $Date: 2003/11/22 08:12:44 $ based on Tomcat Revision 1.12
 */
public class DigestAuthenticator
      extends AuthenticatorBase
{

   // -------------------------------------------------------------- Constants


   /**
    * Indicates that no once tokens are used only once.
    */
   protected static final int USE_ONCE = 1;


   /**
    * Indicates that no once tokens are used only once.
    */
   protected static final int USE_NEVER_EXPIRES = Integer.MAX_VALUE;


   /**
    * Indicates that no once tokens are used only once.
    */
   protected static final int TIMEOUT_INFINITE = Integer.MAX_VALUE;


   /**
    * The MD5 helper object for this class.
    */
   protected static final MD5Encoder md5Encoder = new MD5Encoder();


   /**
    * Descriptive information about this implementation.
    */
   protected static final String info =
         DigestAuthenticator.class.getName() + "/1.0";


   // ----------------------------------------------------------- Constructors


   public DigestAuthenticator()
   {
      super();
      try
      {
         if (md5Helper == null)
            md5Helper = MessageDigest.getInstance("MD5");
      }
      catch (NoSuchAlgorithmException e)
      {
         e.printStackTrace();
         throw new IllegalStateException();
      }
   }


   // ----------------------------------------------------- Instance Variables


   /**
    * MD5 message digest provider.
    */
   protected static MessageDigest md5Helper;


   /**
    * No once hashtable.
    */
   protected Hashtable nOnceTokens = new Hashtable();


   /**
    * No once expiration (in millisecond). A shorter amount would mean a
    * better security level (since the token is generated more often), but at
    * the expense of a bigger server overhead.
    */
   protected long nOnceTimeout = TIMEOUT_INFINITE;


   /**
    * No once expiration after a specified number of uses. A lower number
    * would produce more overhead, since a token would have to be generated
    * more often, but would be more secure.
    */
   protected int nOnceUses = USE_ONCE;


   /**
    * Private key.
    */
   protected String key = "Catalina";


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
      // BASIC or FORM, which are less secure than the DIGEST auth-type
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

      // Validate any credentials already included with this request
      HttpServletRequest hreq =
            (HttpServletRequest) request.getRequest();
      HttpServletResponse hres =
            (HttpServletResponse) response.getResponse();
      String authorization = request.getAuthorization();
      if (authorization != null)
      {
         principal = findPrincipal(hreq, authorization, context.getRealm());
         if (principal != null)
         {
            // Cache the principal (if requested) and record this
            // authentication.
            // Note that if there already is an SSO session assoc with this
            // request, register() associates this app's session with the
            // existing SSO session
            String username = parseUsername(authorization);
            register(request, response, principal,
                     Constants.DIGEST_METHOD,
                     username, null);
            return (true);
         }
      }

      // Send an "unauthorized" response and an appropriate challenge

      // First, generate a nOnce token (that is a token which is supposed
      // to be unique).
      String nOnce = generateNOnce(hreq);

      setAuthenticateHeader(hreq, hres, config, nOnce);
      hres.setStatus(HttpServletResponse.SC_UNAUTHORIZED);
      //      hres.flushBuffer();
      return (false);

   }


   // ------------------------------------------------------ Protected Methods


   /**
    * Parse the specified authorization credentials, and return the
    * associated Principal that these credentials authenticate (if any)
    * from the specified Realm.  If there is no such Principal, return
    * <code>null</code>.
    *
    * @param request HTTP servlet request
    * @param authorization Authorization credentials from this request
    * @param realm Realm used to authenticate Principals
    */
   protected static Principal findPrincipal(HttpServletRequest request,
                                            String authorization,
                                            Realm realm)
   {

      //System.out.println("Authorization token : " + authorization);
      // Validate the authorization credentials format
      if (authorization == null)
         return (null);
      if (!authorization.startsWith("Digest "))
         return (null);
      authorization = authorization.substring(7).trim();


      StringTokenizer commaTokenizer =
            new StringTokenizer(authorization, ",");

      String userName = null;
      String realmName = null;
      String nOnce = null;
      String nc = null;
      String cnonce = null;
      String qop = null;
      String uri = null;
      String response = null;
      String method = request.getMethod();

      while (commaTokenizer.hasMoreTokens())
      {
         String currentToken = commaTokenizer.nextToken();
         int equalSign = currentToken.indexOf('=');
         if (equalSign < 0)
            return null;
         String currentTokenName =
               currentToken.substring(0, equalSign).trim();
         String currentTokenValue =
               currentToken.substring(equalSign + 1).trim();
         if ("username".equals(currentTokenName))
            userName = removeQuotes(currentTokenValue);
         if ("realm".equals(currentTokenName))
            realmName = removeQuotes(currentTokenValue);
         if ("nonce".equals(currentTokenName))
            nOnce = removeQuotes(currentTokenValue);
         if ("nc".equals(currentTokenName))
            nc = currentTokenValue;
         if ("cnonce".equals(currentTokenName))
            cnonce = removeQuotes(currentTokenValue);
         if ("qop".equals(currentTokenName)) {
             //support both quoted and non-quoted
             if (currentTokenValue.startsWith("\"") &&
                 currentTokenValue.endsWith("\""))
               qop = removeQuotes(currentTokenValue);
             else
               qop = currentTokenValue;
         }
         if ("uri".equals(currentTokenName))
            uri = removeQuotes(currentTokenValue);
         if ("response".equals(currentTokenName))
            response = removeQuotes(currentTokenValue);
      }

      if ((userName == null) || (realmName == null) || (nOnce == null)
            || (uri == null) || (response == null))
         return null;

      if (qop != null && (cnonce == null || nc == null))
         return null;

      // Second MD5 digest used to calculate the digest :
      // MD5(Method + ":" + uri)
      String a2 = method + ":" + uri;
      //System.out.println("A2:" + a2);

      String md5a2 = md5Encoder.encode(md5Helper.digest(a2.getBytes()));

      return (realm.authenticate(userName, response, nOnce, nc, cnonce, qop,
                                 realmName, md5a2));

   }


   /**
    * Parse the username from the specified authorization string.  If none
    * can be identified, return <code>null</code>
    *
    * @param authorization Authorization string to be parsed
    */
   protected String parseUsername(String authorization)
   {

      //System.out.println("Authorization token : " + authorization);
      // Validate the authorization credentials format
      if (authorization == null)
         return (null);
      if (!authorization.startsWith("Digest "))
         return (null);
      authorization = authorization.substring(7).trim();

      StringTokenizer commaTokenizer =
            new StringTokenizer(authorization, ",");

      while (commaTokenizer.hasMoreTokens())
      {
         String currentToken = commaTokenizer.nextToken();
         int equalSign = currentToken.indexOf('=');
         if (equalSign < 0)
            return null;
         String currentTokenName =
               currentToken.substring(0, equalSign).trim();
         String currentTokenValue =
               currentToken.substring(equalSign + 1).trim();
         if ("username".equals(currentTokenName))
            return (removeQuotes(currentTokenValue));
      }

      return (null);

   }


   /**
    * Removes the quotes on a string.
    */
   protected static String removeQuotes(String quotedString)
   {
      if (quotedString.length() > 2)
      {
         return quotedString.substring(1, quotedString.length() - 1);
      }
      else
      {
         return new String();
      }
   }


   /**
    * Generate a unique token. The token is generated according to the
    * following pattern. NOnceToken = Base64 ( MD5 ( client-IP ":"
    * time-stamp ":" private-key ) ).
    *
    * @param request HTTP Servlet request
    */
   protected String generateNOnce(HttpServletRequest request)
   {

      long currentTime = System.currentTimeMillis();

      String nOnceValue = request.getRemoteAddr() + ":" +
            currentTime + ":" + key;

      byte[] buffer = md5Helper.digest(nOnceValue.getBytes());
      nOnceValue = md5Encoder.encode(buffer);

      // Updating the value in the no once hashtable
      nOnceTokens.put(nOnceValue, new Long(currentTime + nOnceTimeout));

      return nOnceValue;
   }


   /**
    * Generates the WWW-Authenticate header.
    * <p>
    * The header MUST follow this template :
    * <pre>
    *      WWW-Authenticate    = "WWW-Authenticate" ":" "Digest"
    *                            digest-challenge
    *
    *      digest-challenge    = 1#( realm | [ domain ] | nOnce |
    *                  [ digest-opaque ] |[ stale ] | [ algorithm ] )
    *
    *      realm               = "realm" "=" realm-value
    *      realm-value         = quoted-string
    *      domain              = "domain" "=" <"> 1#URI <">
    *      nonce               = "nonce" "=" nonce-value
    *      nonce-value         = quoted-string
    *      opaque              = "opaque" "=" quoted-string
    *      stale               = "stale" "=" ( "true" | "false" )
    *      algorithm           = "algorithm" "=" ( "MD5" | token )
    * </pre>
    *
    * @param request HTTP Servlet request
    * @param response HTTP Servlet response
    * @param config    Login configuration describing how authentication
    *              should be performed
    * @param nOnce nonce token
    */
   protected void setAuthenticateHeader(HttpServletRequest request,
                                        HttpServletResponse response,
                                        LoginConfig config,
                                        String nOnce)
   {

      // Get the realm name
      String realmName = config.getRealmName();
      if (realmName == null)
         realmName = request.getServerName() + ":"
               + request.getServerPort();

      byte[] buffer = md5Helper.digest(nOnce.getBytes());

      String authenticateHeader = "Digest realm=\"" + realmName + "\", "
            + "qop=\"auth\", nonce=\"" + nOnce + "\", " + "opaque=\""
            + md5Encoder.encode(buffer) + "\"";
      // System.out.println("Authenticate header value : "
      //                   + authenticateHeader);
      response.setHeader("WWW-Authenticate", authenticateHeader);

   }


}
