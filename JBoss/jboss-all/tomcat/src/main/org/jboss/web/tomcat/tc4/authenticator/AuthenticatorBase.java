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
 * Copyright (c) 1999-2001 The Apache Software Foundation.  All rights
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


import java.lang.reflect.Method;
import java.security.Principal;

import javax.servlet.http.Cookie;
import javax.servlet.http.HttpServletResponse;

import org.apache.catalina.Container;
import org.apache.catalina.HttpRequest;
import org.apache.catalina.HttpResponse;
import org.apache.catalina.LifecycleException;
import org.apache.catalina.Pipeline;
import org.apache.catalina.Realm;
import org.apache.catalina.Session;
import org.apache.catalina.Valve;
import org.apache.catalina.authenticator.Constants;


/**
 * Overrides the superclass version by using class
 * <code>org.jboss.web.tomcat.tc4.authenticator.SingleSignOn</code> instead
 * of <code>org.apache.catalina.authenticator.SingleSignOn</code> as its
 * method expected single sign-on valve.  This class also differs from
 * the standard Tomcat version in its implementation of method
 * {@link #register reqister}.
 * <p>
 * Basic implementation of the <b>Valve</b> interface that enforces the
 * <code>&lt;security-constraint&gt;</code> elements in the web application
 * deployment descriptor.  This functionality is implemented as a Valve
 * so that it can be ommitted in environments that do not require these
 * features.  Individual implementations of each supported authentication
 * method can subclass this base class as required.
 * <p>
 * <b>USAGE CONSTRAINT</b>:  When this class is utilized, the Context to
 * which it is attached (or a parent Container in a hierarchy) must have an
 * associated Realm that can be used for authenticating users and enumerating
 * the roles to which they have been assigned.
 * <p>
 * <b>USAGE CONSTRAINT</b>:  This Valve is only useful when processing HTTP
 * requests.  Requests of any other type will simply be passed through.
 *
 * @see #register
 * @see SingleSignOn
 * @see org.apache.catalina.authenticator.AuthenticatorBase
 *
 * @author B Stansberry, based on the work of Craig R. McClanahan
 * @version $Revision: 1.1.2.1 $ $Date: 2003/11/22 08:12:44 $
 */
public abstract class AuthenticatorBase
      extends org.apache.catalina.authenticator.AuthenticatorBase
{


   // ----------------------------------------------------- Instance Variables


   /**
    * Descriptive information about this implementation.
    */
   protected static final String info =
         AuthenticatorBase.class.getName() + "/1.0";

   /**
    * The SingleSignOn implementation in our request processing chain,
    * if there is one.
    * <p>
    * <b>NOTE:</b> This is an instance of
    * <code>org.jboss.web.tomcat.tc4.authenticator.SingleSignOn</code>,
    * not <code>org.apache.catalina.authenticator.SingleSignOn</code>.
    *
    */
   protected SingleSignOn ourSSO = null;


   // ------------------------------------------------------------- Properties


   // --------------------------------------------------------- Public Methods


   // ------------------------------------------------------ Protected Methods



   /**
    * Associate the specified single sign on identifier with the
    * specified Session.
    * <p>
    * <b>IMPLEMENTATION NOTE:</b> Overrides the superclass version solely by
    * using a <code>org.jboss.web.tomcat.tc4.authenticator.SingleSignOn</code>
    * instead of an <code>org.apache.catalina.authenticator.SingleSignOn</code>
    *
    * @param ssoId Single sign on identifier
    * @param session Session to be associated
    */
   protected void associate(String ssoId, Session session)
   {

      if (ourSSO == null)
         return;
      ourSSO.associate(ssoId, session);

   }


   /**
    * Attempts reauthentication to the <code>Realm</code> using
    * the credentials included in argument <code>entry</code>.
    *
    * @param ssoId identifier of SingleSignOn session with which the
    *              caller is associated
    * @param request   the request that needs to be authenticated
    */
   protected boolean reauthenticateFromSSO(String ssoId, HttpRequest request)
   {

      if (ourSSO == null || ssoId == null)
         return false;

      boolean reauthenticated = false;

      SingleSignOnEntry entry = ourSSO.lookup(ssoId);
      if (entry != null && entry.getCanReauthenticate())
      {
         Principal reauthPrincipal = null;
         Container parent = getContainer();
         if (parent != null)
         {
            Realm realm = getContainer().getRealm();
            String username = entry.getUsername();
            if (realm != null && username != null)
            {
               reauthPrincipal =
                     realm.authenticate(username, entry.getPassword());
            }
         }

         if (reauthPrincipal != null)
         {
            associate(ssoId, getSession(request, true));
            request.setAuthType(entry.getAuthType());
            request.setUserPrincipal(reauthPrincipal);

            reauthenticated = true;
            if (debug >= 1)
            {
               log(" Reauthenticated cached principal '" +
                   entry.getPrincipal().getName() + "' with auth type '" +
                   entry.getAuthType() + "'");
            }
         }
      }

      return reauthenticated;
   }


   /**
    * Register an authenticated Principal and authentication type in our
    * request, in the current session (if there is one), and with our
    * SingleSignOn valve, if there is one.  Set the appropriate cookie
    * to be returned.
    * <p>
    * <b>IMPLEMENTATION NOTE:</b> Differs from the standard Tomcat
    * implementation in checking if any <code>SingleSignOn</code> valve
    * has added a note to the request.  If it has, it does not call
    * <code>SingleSignOn.register</code>, instead calling
    * <code>SingleSignOn.update</code>.  This behavior supports
    * authenticators like <code>SSLAuthenticator</code> that may attempt
    * to re-register with every request.
    *
    * @param request The servlet request we are processing
    * @param response The servlet response we are generating
    * @param principal The authenticated Principal to be registered
    * @param authType The authentication type to be registered
    * @param username Username used to authenticate (if any)
    * @param password Password used to authenticate (if any)
    */
   protected void register(HttpRequest request, HttpResponse response,
                           Principal principal, String authType,
                           String username, String password)
   {

      if (debug >= 1)
         log("Authenticated '" + principal.getName() + "' with type '" +
             authType + "'");

      // Cache the authentication information in our request
      request.setAuthType(authType);
      request.setUserPrincipal(principal);

      Session session = getSession(request, false);
      // Cache the authentication information in our session, if any
      if (cache)
      {
         if (session != null)
         {
            session.setAuthType(authType);
            session.setPrincipal(principal);
            if (username != null)
               session.setNote(Constants.SESS_USERNAME_NOTE, username);
            else
               session.removeNote(Constants.SESS_USERNAME_NOTE);
            if (password != null)
               session.setNote(Constants.SESS_PASSWORD_NOTE, password);
            else
               session.removeNote(Constants.SESS_PASSWORD_NOTE);
         }
      }

      // Construct a cookie to be returned to the client
      if (ourSSO == null)
         return;

      // Only create a new SSO entry if the SSO did not already set a note
      // for an existing entry (as it would do with subsequent requests
      // for DIGEST and SSL authenticated contexts)
      String ssoId = (String) request.getNote(Constants.REQ_SSOID_NOTE);
      if (ssoId == null)
      {
         // Construct a cookie to be returned to the client
         HttpServletResponse hres =
               (HttpServletResponse) response.getResponse();
         ssoId = generateSessionId();
         Cookie cookie = new Cookie(Constants.SINGLE_SIGN_ON_COOKIE, ssoId);
         cookie.setMaxAge(-1);
         cookie.setPath("/");
         hres.addCookie(cookie);

         // Register this principal with our SSO valve
         ourSSO.register(ssoId, principal, authType, username, password);
         request.setNote(Constants.REQ_SSOID_NOTE, ssoId);

      }
      else
      {
         // Update the SSO session with the latest authentication data
         ourSSO.update(ssoId, principal, authType, username, password);
      }

      // Fix for Tomcat Bug 10040
      // Always associate a session with a new SSO reqistration.
      // SSO entries are only removed from the SSO registry map when
      // associated sessions are destroyed; if a new SSO entry is created
      // above for this request and the user never revisits the context, the
      // SSO entry will never be cleared if we don't associate the session
      if (session == null)
         session = getSession(request, true);
      ourSSO.associate(ssoId, session);

   }


   // ------------------------------------------------------ Lifecycle Methods


   /**
    * Prepare for the beginning of active use of the public methods of this
    * component.  This method should be called after <code>configure()</code>,
    * and before any of the public methods of the component are utilized.
    * <p>
    * <b>IMPLEMENTATION NOTE:</b> Overrides the superclass version solely by
    * using a <code>org.jboss.web.tomcat.tc4.authenticator.SingleSignOn</code>
    * instead of an <code>org.apache.catalina.authenticator.SingleSignOn</code>
    *
    *
    * @exception LifecycleException if this component detects a fatal error
    *  that prevents this component from being used
    */
   public void start() throws LifecycleException
   {

      // Validate and update our current component state
      if (started)
      {
         throw new LifecycleException
               (sm.getString("authenticator.alreadyStarted"));
      }
      lifecycle.fireLifecycleEvent(START_EVENT, null);
      if ("org.apache.catalina.core.StandardContext".equals
            (context.getClass().getName()))
      {
         try
         {
            Class paramTypes[] = new Class[0];
            Object paramValues[] = new Object[0];
            Method method =
                  context.getClass().getMethod("getDebug", paramTypes);
            Integer result = (Integer) method.invoke(context, paramValues);
            setDebug(result.intValue());
         }
         catch (Exception e)
         {
            log("Exception getting debug value", e);
         }
      }
      started = true;

      // Look up the SingleSignOn implementation in our request processing
      // path, if there is one
      Container parent = context.getParent();
      while ((ourSSO == null) && (parent != null))
      {
         if (!(parent instanceof Pipeline))
         {
            parent = parent.getParent();
            continue;
         }
         Valve valves[] = ((Pipeline) parent).getValves();
         for (int i = 0; i < valves.length; i++)
         {
            if (valves[i] instanceof SingleSignOn)
            {
               ourSSO = (SingleSignOn) valves[i];
               break;
            }
         }
         if (ourSSO == null)
            parent = parent.getParent();
      }
      if (debug >= 1)
      {
         if (ourSSO != null)
         {
            log("Found SingleSignOn Valve at " + ourSSO);
         }
         else
         {
            log("No SingleSignOn Valve is present");
         }
      }

   }


   /**
    * Gracefully terminate the active use of the public methods of this
    * component.  This method should be the last one called on a given
    * instance of this component.
    * <p>
    * <b>IMPLEMENTATION NOTE:</b> Overrides the superclass version solely by
    * using a <code>org.jboss.web.tomcat.tc4.authenticator.SingleSignOn</code>
    * instead of an <code>org.apache.catalina.authenticator.SingleSignOn</code>
    *
    * @exception LifecycleException if this component detects a fatal error
    *  that needs to be reported
    */
   public void stop() throws LifecycleException
   {

      // Validate and update our current component state
      if (!started)
      {
         throw new LifecycleException
               (sm.getString("authenticator.notStarted"));
      }
      lifecycle.fireLifecycleEvent(STOP_EVENT, null);
      started = false;

      ourSSO = null;

   }


}
