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

import java.io.IOException;
import java.security.Principal;
import java.util.HashMap;

import javax.servlet.ServletException;
import javax.servlet.http.Cookie;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.apache.catalina.HttpRequest;
import org.apache.catalina.HttpResponse;
import org.apache.catalina.Lifecycle;
import org.apache.catalina.LifecycleException;
import org.apache.catalina.LifecycleListener;
import org.apache.catalina.Logger;
import org.apache.catalina.Request;
import org.apache.catalina.Response;
import org.apache.catalina.Session;
import org.apache.catalina.SessionEvent;
import org.apache.catalina.SessionListener;
import org.apache.catalina.ValveContext;
import org.apache.catalina.authenticator.Constants;
import org.apache.catalina.util.LifecycleSupport;
import org.apache.catalina.util.StringManager;
import org.apache.catalina.valves.ValveBase;


/**
 * A <strong>Valve</strong> that supports a "single sign on" user experience,
 * where the security identity of a user who successfully authenticates to one
 * web application is propogated to other web applications in the same
 * security domain.
 * <p>
 * <b>USAGE:</b> For successful use, the following requirements must be met:
 * be met:
 * <ul>
 * <li>This Valve must be configured on the Container that represents a
 *     virtual host (typically an implementation of <code>Host</code>).</li>
 * <li>The <code>Realm</code> that contains the shared user and role
 *     information must be configured on the same Container (or a higher
 *     one), and not overridden at the web application level.</li>
 * <li>The web applications themselves must use one of the Authenticators
 *     found in the <code>org.jboss.web.tomcat.tc4.authenticator</code> package.
 * </li>
 * </ul>
 *
 * @author B Stansberry, based on work by Craig R. McClanahan
 * @version $Revision: 1.1.2.2 $ $Date: 2003/11/26 06:26:20 $ based on Tomcat Revision 1.11
 */
public class SingleSignOn
      extends ValveBase
      implements Lifecycle, SessionListener
{
   // -------------------------------------------------------------- Constants

   /**
    * The string manager for this package.
    */
   protected final static StringManager sm =
         StringManager.getManager(Constants.Package);


   // ----------------------------------------------------- Instance Variables


   /**
    * The cache of SingleSignOnEntry instances for authenticated Principals,
    * keyed by the cookie value that is used to select them.
    */
   protected HashMap cache = new HashMap();


   /**
    * The debugging detail level for this component.
    */
   protected int debug = 0;


   /**
    * Descriptive information about this Valve implementation.
    */
   protected static String info = SingleSignOn.class.getName();


   /**
    * The lifecycle event support for this component.
    */
   protected LifecycleSupport lifecycle = new LifecycleSupport(this);


   /**
    * The cache of single sign on identifiers, keyed by the Session that is
    * associated with them.
    */
   protected HashMap reverse = new HashMap();

   /**
    * Component started flag.
    */
   protected boolean started = false;


   // ------------------------------------------------------------- Properties


   /**
    * Return the debugging detail level.
    */
   public int getDebug()
   {

      return (this.debug);

   }

   /**
    * Set the debugging detail level.
    *
    * @param debug The new debugging detail level
    */
   public void setDebug(int debug)
   {

      this.debug = debug;

   }


   // ------------------------------------------------------ Lifecycle Methods


   /**
    * Add a lifecycle event listener to this component.
    *
    * @param listener The listener to add
    */
   public void addLifecycleListener(LifecycleListener listener)
   {

      lifecycle.addLifecycleListener(listener);

   }


   /**
    * Get the lifecycle listeners associated with this lifecycle. If this
    * Lifecycle has no listeners registered, a zero-length array is returned.
    */
   public LifecycleListener[] findLifecycleListeners()
   {

      return lifecycle.findLifecycleListeners();

   }


   /**
    * Remove a lifecycle event listener from this component.
    *
    * @param listener The listener to remove
    */
   public void removeLifecycleListener(LifecycleListener listener)
   {

      lifecycle.removeLifecycleListener(listener);

   }


   /**
    * Prepare for the beginning of active use of the public methods of this
    * component.  This method should be called after <code>configure()</code>,
    * and before any of the public methods of the component are utilized.
    *
    * @exception LifecycleException if this component detects a fatal error
    *  that prevents this component from being used
    */
   public void start() throws LifecycleException
   {

      // Validate and update our current component state
      if (started)
         throw new LifecycleException
               (sm.getString("authenticator.alreadyStarted"));
      lifecycle.fireLifecycleEvent(START_EVENT, null);
      started = true;

      if (debug >= 1)
         log("Started");

   }


   /**
    * Gracefully terminate the active use of the public methods of this
    * component.  This method should be the last one called on a given
    * instance of this component.
    *
    * @exception LifecycleException if this component detects a fatal error
    *  that needs to be reported
    */
   public void stop() throws LifecycleException
   {

      // Validate and update our current component state
      if (!started)
         throw new LifecycleException
               (sm.getString("authenticator.notStarted"));
      lifecycle.fireLifecycleEvent(STOP_EVENT, null);
      started = false;

      if (debug >= 1)
         log("Stopped");

   }


   // ------------------------------------------------ SessionListener Methods


   /**
    * Acknowledge the occurrence of the specified event.
    *
    * @param event SessionEvent that has occurred
    */
   public void sessionEvent(SessionEvent event)
   {

      // We only care about session destroyed events
      if (!Session.SESSION_DESTROYED_EVENT.equals(event.getType()))
         return;

      // Look up the single session id associated with this session (if any)
      Session session = event.getSession();
      if (debug >= 1)
         log("Process session destroyed on " + session);
      String ssoId = null;
      synchronized (reverse)
      {
         ssoId = (String) reverse.get(session);
      }
      if (ssoId == null)
         return;        

      /*
       *  Was the session destroyed as the result of a timeout?
       *  If so, we'll just remove the expired session from the
       *  SSO.  If the session was logged out, we'll log out
       *  of all session associated with the SSO.
       */
      if (System.currentTimeMillis() - session.getLastAccessedTime() >=
            session.getMaxInactiveInterval() * 1000) {            
         removeSession(ssoId, session);
      }
      else {
         // The session was logged out.
         // Deregister this single session id, invalidating associated sessions
         deregister(ssoId);
      }

   }


   // ---------------------------------------------------------- Valve Methods


   /**
    * Return descriptive information about this Valve implementation.
    */
   public String getInfo()
   {

      return (info);

   }


   /**
    * Perform single-sign-on support processing for this request.
    *
    * @param request The servlet request we are processing
    * @param response The servlet response we are creating
    * @param context The valve context used to invoke the next valve
    *  in the current processing pipeline
    *
    * @exception IOException if an input/output error occurs
    * @exception ServletException if a servlet error occurs
    */
   public void invoke(Request request, Response response,
                      ValveContext context)
         throws IOException, ServletException
   {

      // If this is not an HTTP request and response, just pass them on
      if (!(request instanceof HttpRequest) ||
            !(response instanceof HttpResponse))
      {
         context.invokeNext(request, response);
         return;
      }
      HttpServletRequest hreq =
            (HttpServletRequest) request.getRequest();
      HttpServletResponse hres =
            (HttpServletResponse) response.getResponse();
      request.removeNote(Constants.REQ_SSOID_NOTE);

      // Has a valid user already been authenticated?
      if (debug >= 1)
         log("Process request for '" + hreq.getRequestURI() + "'");
      if (hreq.getUserPrincipal() != null)
      {
         if (debug >= 1)
            log(" Principal '" + hreq.getUserPrincipal().getName() +
                "' has already been authenticated");
         context.invokeNext(request, response);
         return;
      }

      // Check for the single sign on cookie
      if (debug >= 1)
         log(" Checking for SSO cookie");
      Cookie cookie = null;
      Cookie cookies[] = hreq.getCookies();
      if (cookies == null)
         cookies = new Cookie[0];
      for (int i = 0; i < cookies.length; i++)
      {
         if (Constants.SINGLE_SIGN_ON_COOKIE.equals(cookies[i].getName()))
         {
            cookie = cookies[i];
            break;
         }
      }
      if (cookie == null)
      {
         if (debug >= 1)
            log(" SSO cookie is not present");
         context.invokeNext(request, response);
         return;
      }

      // Look up the cached Principal associated with this cookie value
      String ssoId = cookie.getValue();
      if (debug >= 1)
         log(" Checking for cached principal for " + ssoId);
      SingleSignOnEntry entry = lookup(ssoId);
      if (entry != null)
      {
         if (debug >= 1)
         {
            log(" Found cached principal '" +
                entry.getPrincipal().getName() + "' with auth type '" +
                entry.getAuthType() + "'");
         }
         request.setNote(Constants.REQ_SSOID_NOTE, ssoId);
      }
      else
      {
         if (debug >= 1)
            log(" No currently valid cached principal found, "
                + "erasing SSO cookie");
         cookie.setMaxAge(0);
         hres.addCookie(cookie);
      }

      // Invoke the next Valve in our pipeline
      context.invokeNext(request, response);
   }
   

   // --------------------------------------------------------- Public Methods


   /**
    * Updates the Jakarta version by ensuring that member
    * <code>container</code> is not <code>null</code> before using it.
    * <p>
    * If <code>getContainer()</code> returns <code>null</code>, this method
    * functions as per {@link Object#toString Object.toString()}.
    * Otherwise, it functions as per
    * {@link org.apache.catalina.authenticator.SingleSignOn#toString
    the superclass version}.
    *
    * @see org.apache.catalina.authenticator.SingleSignOn#getContainer
    * @see org.apache.catalina.authenticator.SingleSignOn#toString
    * @see Object#toString
    */
   public String toString()
   {

      StringBuffer sb = new StringBuffer();
      if (getContainer() != null)
      {
         sb.append("SingleSignOn[");
         sb.append(getContainer().getName());
         sb.append(']');
      }
      else
      {
         sb.append(getClass().getName());
         sb.append('@');
         sb.append(Integer.toHexString(hashCode()));
      }
      return (sb.toString());

   }


   // -------------------------------------------------------- Package Methods


   /**
    * Associate the specified single sign on identifier with the
    * specified Session.
    *
    * @param ssoId Single sign on identifier
    * @param session Session to be associated
    */
   void associate(String ssoId, Session session)
   {

      if (session == null)
         return;

      if (debug >= 1)
         log("Associate sso id " + ssoId + " with session " + session);

      SingleSignOnEntry sso = lookup(ssoId);
      if (sso != null)
         sso.addSession(this, session);
      synchronized (reverse)
      {
         reverse.put(session, ssoId);
      }

   }


   /**
    * Deregister the specified single sign on identifier, and invalidate
    * any associated sessions.
    *
    * @param ssoId Single sign on identifier to deregister
    */
   void deregister(String ssoId)
   {

      if (debug >= 1)
         log("Deregistering sso id '" + ssoId + "'");

      // Look up and remove the corresponding SingleSignOnEntry
      SingleSignOnEntry sso = null;
      synchronized (cache)
      {
         sso = (SingleSignOnEntry) cache.remove(ssoId);
      }
      if (sso == null)
         return;

      // Expire any associated sessions
      Session sessions[] = sso.findSessions();
      for (int i = 0; i < sessions.length; i++)
      {
         if (sessions[i] == null)
            continue;

         if (debug >= 2)
            log(" Invalidating session " + sessions[i]);
         // Remove from reverse cache first to avoid recursion
         synchronized (reverse)
         {
            reverse.remove(sessions[i]);
         }
         // Invalidate this session
         sessions[i].expire();
      }

      // NOTE:  Clients may still possess the old single sign on cookie,
      // but it will be removed on the next request since it is no longer
      // in the cache

   }


   /**
    * Register the specified Principal as being associated with the specified
    * value for the single sign on identifier.
    *
    * @param ssoId Single sign on identifier to register
    * @param principal Associated user principal that is identified
    * @param authType Authentication type used to authenticate this
    *  user principal
    * @param username Username used to authenticate this user
    * @param password Password used to authenticate this user
    */
   void register(String ssoId, Principal principal, String authType,
                 String username, String password)
   {

      if (debug >= 1)
         log("Registering sso id '" + ssoId + "' for user '" +
             principal.getName() + "' with auth type '" + authType + "'");

      synchronized (cache)
      {
         cache.put(ssoId, new SingleSignOnEntry(principal, authType,
                                                username, password));
      }

   }

    
   /**
    * Remove a single Session from a SingleSignOn.  Called when
    * a session is timed out and no longer active.
    *
    * @param ssoId Single sign on identifier from which to remove the session.
    * @param session the session to be removed.
    */
   void removeSession(String ssoId, Session session)
   {        
      if (debug >= 1)
      {
         log("Removing session " + session.toString() + " from sso id " + 
             ssoId );
      }
        
      // Get a reference to the SingleSignOn
      SingleSignOnEntry entry = lookup(ssoId);
      if (entry == null)
      {
            return;
      }
       
      // Remove the inactive session from SingleSignOnEntry
      entry.removeSession(session);
    
      // Remove the inactive session from the 'reverse' Map.
      synchronized(reverse) 
      {
         reverse.remove(session);
      }
    
      // If there are not sessions left in the SingleSignOnEntry,
      // deregister the entry.
      if (entry.findSessions().length == 0) 
      {
         deregister(ssoId);
      }
   }

   /**
    * Updates any <code>SingleSignOnEntry</code> found under key
    * <code>ssoId</code> with the given authentication data.
    * <p>
    * The purpose of this method is to allow an SSO entry that was
    * established without a username/password combination (i.e. established
    * following DIGEST or CLIENT-CERT authentication) to be updated with
    * a username and password if one becomes available through a subsequent
    * BASIC or FORM authentication.  The SSO entry will then be usable for
    * reauthentication.
    * <p>
    * <b>NOTE:</b> Only updates the SSO entry if a call to
    * <code>SingleSignOnEntry.getCanReauthenticate()</code> returns
    * <code>false</code>; otherwise, it is assumed that the SSO entry already
    * has sufficient information to allow reauthentication and that no update
    * is needed.
    *
    * @param ssoId identifier of Single sign to be updated
    * @param principal the <code>Principal</code> returned by the latest
    *                  call to <code>Realm.authenticate</code>.
    * @param authType  the type of authenticator used (BASIC, CLIENT-CERT,
    *                  DIGEST or FORM)
    * @param username  the username (if any) used for the authentication
    * @param password  the password (if any) used for the authentication
    */
   void update(String ssoId, Principal principal, String authType,
               String username, String password)
   {

      SingleSignOnEntry sso = lookup(ssoId);
      if (sso != null && !sso.getCanReauthenticate())
      {
         if (debug >= 1)
            log("Update sso id " + ssoId + " to auth type " + authType);

         synchronized (sso)
         {
            sso.updateCredentials(principal, authType, username, password);
         }

      }
   }


   // ------------------------------------------------------ Protected Methods


   /**
    * Log a message on the Logger associated with our Container (if any).
    *
    * @param message Message to be logged
    */
   protected void log(String message)
   {

      Logger logger = container.getLogger();
      if (logger != null)
         logger.log(this.toString() + ": " + message);
      else
         System.out.println(this.toString() + ": " + message);

   }


   /**
    * Log a message on the Logger associated with our Container (if any).
    *
    * @param message Message to be logged
    * @param throwable Associated exception
    */
   protected void log(String message, Throwable throwable)
   {

      Logger logger = container.getLogger();
      if (logger != null)
         logger.log(this.toString() + ": " + message, throwable);
      else
      {
         System.out.println(this.toString() + ": " + message);
         throwable.printStackTrace(System.out);
      }

   }


   /**
    * Look up and return the cached SingleSignOn entry associated with this
    * sso id value, if there is one; otherwise return <code>null</code>.
    *
    * @param ssoId Single sign on identifier to look up
    */
   protected SingleSignOnEntry lookup(String ssoId)
   {

      synchronized (cache)
      {
         return ((SingleSignOnEntry) cache.get(ssoId));
      }

   }


}