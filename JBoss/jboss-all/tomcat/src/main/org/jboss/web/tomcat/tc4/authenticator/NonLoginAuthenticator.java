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

import org.apache.catalina.HttpRequest;
import org.apache.catalina.HttpResponse;
import org.apache.catalina.deploy.LoginConfig;


/**
 * An <b>Authenticator</b> and <b>Valve</b> implementation that checks
 * only security constraints not involving user authentication.
 * <p>
 * Differs from the standard Tomcat version in that it associates the session
 * of any request with any single sign-on session that may exist.
 *
 * @see #authenticate
 * @see AuthenticatorBase#associate
 * @see SingleSignOn
 *
 * @author B Stansberry, based on work by Craig R. McClanahan
 * @version $Revision: 1.1.2.1 $ $Date: 2003/11/22 08:12:44 $ based on Tomcat Revision 1.3
 */

public final class NonLoginAuthenticator
      extends AuthenticatorBase
{


   // ----------------------------------------------------- Instance Variables


   /**
    * Descriptive information about this implementation.
    */
   private static final String info =
         NonLoginAuthenticator.class.getName() + "/1.0";


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

      /*  Associating this request's session with an SSO would allow
          coordinated session invalidation, but should the session for
          a webapp that the user didn't log into be invalidated when
          another session is logged out?
      String ssoId = (String) request.getNote(Constants.REQ_SSOID_NOTE);
      if (ssoId != null)
      {
          associate(ssoId, getSession(request, true));
      }
      */

      if (debug >= 1)
      {
         log("User authentication is not required");
      }

      return (true);
   }


}
