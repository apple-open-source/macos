/*******************************************************************************
 * SingleSignOnEntry.java
 *
 * Version 4.2
 * Date         Author          Changes
 * -----------  --------------- -----------
 * 2003/06/01   B Stansberry    Extracted from SingleSignOn; made public;
 *                              fields made private w/ package protected
 *                              accessors
 *
 * Copyright (c) 2003 WAN Concepts, Inc. All rights reserved.
 ******************************************************************************/
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

import java.security.Principal;

import org.apache.catalina.Session;
import org.apache.catalina.authenticator.Constants;

/**
 * Extraction of the private inner class
 * <code>org.apache.catalina.authenticator.SingleSignOn.SingleSignOnEntry</code>
 * into a package protected class.  This is necessary to make it available to
 * <code>AuthenticatorBase</code> subclasses that need it in order to perform
 * reauthentications when SingleSignOn is in use.
 *
 * @author  B Stansberry, based on work by Craig R. McClanahan
 * @version $Revision: 1.1.2.1 $ $Date: 2003/11/22 08:12:44 $
 */
class SingleSignOnEntry
{
   // ------------------------------------------------------  Instance Fields

   private String authType = null;

   private String password = null;

   private Principal principal = null;

   private Session sessions[] = new Session[0];

   private String username = null;

   private boolean canReauthenticate = false;

   // ---------------------------------------------------------  Constructors

   /**
    * Creates a new SingleSignOnEntry
    *
    * @param principal the <code>Principal</code> returned by the latest
    *                  call to <code>Realm.authenticate</code>.
    * @param authType  the type of authenticator used (BASIC, CLIENT-CERT,
    *                  DIGEST or FORM)
    * @param username  the username (if any) used for the authentication
    * @param password  the password (if any) used for the authentication
    */
   SingleSignOnEntry(Principal principal, String authType,
                     String username, String password)
   {
      super();
      updateCredentials(principal, authType, username, password);
   }

   // ------------------------------------------------------- Package Methods

   /**
    * Adds a <code>Session</code> to the list of those associated with
    * this SSO.
    *
    * @param sso       The <code>SingleSignOn</code> valve that is managing
    *                  the SSO session.
    * @param session   The <code>Session</code> being associated with the SSO.
    */
   synchronized void addSession(SingleSignOn sso, Session session)
   {
      for (int i = 0; i < sessions.length; i++)
      {
         if (session == sessions[i])
            return;
      }
      Session results[] = new Session[sessions.length + 1];
      System.arraycopy(sessions, 0, results, 0, sessions.length);
      results[sessions.length] = session;
      sessions = results;
      session.addSessionListener(sso);
   }

   /**
    * Removes the given <code>Session</code> from the list of those
    * associated with this SSO.
    *
    * @param session  the <code>Session</code> to remove.
    */
   synchronized void removeSession(Session session)
   {
      Session[] nsessions = new Session[sessions.length - 1];
      for (int i = 0, j = 0; i < sessions.length; i++)
      {
         if (session == sessions[i])
            continue;
         nsessions[j++] = sessions[i];
      }
      sessions = nsessions;
   }

   /**
    * Returns the <code>Session</code>s associated with this SSO.
    */
   synchronized Session[] findSessions()
   {
      return (this.sessions);
   }

   /**
    * Gets the name of the authentication type originally used to authenticate
    * the user associated with the SSO.
    *
    * @return "BASIC", "CLIENT-CERT", "DIGEST", "FORM" or "NONE"
    */
   String getAuthType()
   {
      return (this.authType);
   }

   /**
    * Gets whether the authentication type associated with the original
    * authentication supports reauthentication.
    *
    * @return  <code>true</code> if <code>getAuthType</code> returns
    *          "BASIC" or "FORM", <code>false</code> otherwise.
    */
   boolean getCanReauthenticate()
   {
      return (this.canReauthenticate);
   }

   /**
    * Gets the password credential (if any) associated with the SSO.
    *
    * @return  the password credential associated with the SSO, or
    *          <code>null</code> if the original authentication type
    *          does not involve a password.
    */
   String getPassword()
   {
      return (this.password);
   }

   /**
    * Gets the <code>Principal</code> that has been authenticated by
    * the SSO.
    */
   Principal getPrincipal()
   {
      return (this.principal);
   }

   /**
    * Gets the username provided by the user as part of the authentication
    * process.
    */
   String getUsername()
   {
      return (this.username);
   }


   /**
    * Updates the SingleSignOnEntry to reflect the latest security
    * information associated with the caller.
    *
    * @param principal the <code>Principal</code> returned by the latest
    *                  call to <code>Realm.authenticate</code>.
    * @param authType  the type of authenticator used (BASIC, CLIENT-CERT,
    *                  DIGEST or FORM)
    * @param username  the username (if any) used for the authentication
    * @param password  the password (if any) used for the authentication
    */
   void updateCredentials(Principal principal, String authType,
                          String username, String password)
   {

      this.principal = principal;
      this.authType = authType;
      this.username = username;
      this.password = password;
      this.canReauthenticate =
            (Constants.BASIC_METHOD.equals(authType)
            || Constants.FORM_METHOD.equals(authType));
   }

}
