/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.cmp2.audit.interfaces;

import javax.security.auth.callback.Callback;
import javax.security.auth.callback.CallbackHandler;
import javax.security.auth.callback.NameCallback;
import javax.security.auth.callback.PasswordCallback;
import javax.security.auth.callback.UnsupportedCallbackException;
import javax.security.auth.login.LoginContext;
import javax.security.auth.login.LoginException;

/**
 * A callback handler for login with user and password.
 *   
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public class ApplicationCallbackHandler
   implements CallbackHandler
{
   // Attributes ----------------------------------------------------

   private String user;
   private char[] password;

   // Constructor ---------------------------------------------------

   private ApplicationCallbackHandler(String user, String password)
   {
      this.user = user;
      this.password = password.toCharArray();
   }

   // Public --------------------------------------------------------

   public void handle(Callback[] callbacks)
      throws UnsupportedCallbackException
   {
      for (int i = 0; i < callbacks.length; i++)
      {
         if (callbacks[i] instanceof NameCallback)
         {
            NameCallback nameCallback = (NameCallback) callbacks[i];
            nameCallback.setName(user);
         }
         else if (callbacks[i] instanceof PasswordCallback)
         {
            PasswordCallback passwordCallback = (PasswordCallback) callbacks[i];
            passwordCallback.setPassword(password);
         }
         else
         {
            throw new UnsupportedCallbackException(callbacks[i], "Unsupported callback");
         }
      }
   }

   // Static --------------------------------------------------------

   /**
    * Login
    */
   public static LoginContext login(String user, String password)
      throws LoginException
   {
      ApplicationCallbackHandler handler = new ApplicationCallbackHandler(user, password);
      LoginContext result = new LoginContext("client-login", handler);
      result.login();
      return result;
   }
}
