/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.security;


import java.util.Map;
import java.util.Set;
import java.security.Principal;
import javax.security.auth.Subject;
import javax.security.auth.callback.Callback;
import javax.security.auth.callback.CallbackHandler;
import javax.security.auth.callback.NameCallback;
import javax.security.auth.callback.PasswordCallback;
import javax.security.auth.callback.UnsupportedCallbackException;
import javax.security.auth.login.LoginException;
import javax.security.auth.spi.LoginModule;

/** A simple implementation of LoginModule for use by JBoss clients for
 the establishment of the caller identity and credentials. This simply sets
 the SecurityAssociation principal to the value of the NameCallback
 filled in by the CallbackHandler, and the SecurityAssociation credential
 to the value of the PasswordCallback filled in by the CallbackHandler.
 This is a variation of the original ClientLoginModule that does not set the
 SecurityAssociation information until commit and that uses the Subject
 principal over a SimplePrincipal if available.

 It has the following options:
 <ul>
 <li>multi-threaded=[true|false]
 When the multi-threaded option is set to true, the SecurityAssociation.setServer()
 so that each login thread has its own principal and credential storage.
 <li>password-stacking=tryFirstPass|useFirstPass
 When password-stacking option is set, this module first looks for a shared
 username and password using "javax.security.auth.login.name" and
 "javax.security.auth.login.password" respectively. This allows a module configured
 prior to this one to establish a valid username and password that should be passed
 to JBoss.
 </ul>
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.2.1 $
 */
public class AltClientLoginModule implements LoginModule
{
   private Subject subject;
   private CallbackHandler callbackHandler;
   /** Shared state between login modules */
   private Map sharedState;
   /** Flag indicating if the shared password should be used */
   private boolean useFirstPass;
   private String username;
   private char[] password = null;

   /**
    * Initialize this LoginModule.
    */
   public void initialize(Subject subject, CallbackHandler callbackHandler,
      Map sharedState, Map options)
   {
      this.subject = subject;
      this.callbackHandler = callbackHandler;
      this.sharedState = sharedState;
      // Check for multi-threaded option
      String mt = (String) options.get("multi-threaded");
      if( mt != null && Boolean.valueOf(mt).booleanValue() == true )
      {   /* Turn on the server mode which uses thread local storage for
                the principal information.
         */
         SecurityAssociation.setServer();
      }
      
        /* Check for password sharing options. Any non-null value for
            password_stacking sets useFirstPass as this module has no way to
            validate any shared password.
         */
      String passwordStacking = (String) options.get("password-stacking");
      useFirstPass = passwordStacking != null;
   }

   /**
    * Method to authenticate a Subject (phase 1).
    */
   public boolean login() throws LoginException
   {
      // If useFirstPass is true, look for the shared password
      if( useFirstPass == true )
      {
            return true;
      }

     /* There is no password sharing or we are the first login module. Get
         the username and password from the callback hander.
      */
      if (callbackHandler == null)
         throw new LoginException("Error: no CallbackHandler available " +
            "to garner authentication information from the user");
      
      PasswordCallback pc = new PasswordCallback("Password: ", false);
      NameCallback nc = new NameCallback("User name: ", "guest");
      Callback[] callbacks = {nc, pc};
      try
      {
         char[] tmpPassword;
         
         callbackHandler.handle(callbacks);
         username = nc.getName();
         tmpPassword = pc.getPassword();
         if (tmpPassword != null)
         {
            password = new char[tmpPassword.length];
            System.arraycopy(tmpPassword, 0, password, 0, tmpPassword.length);
            pc.clearPassword();
         }
      }
      catch (java.io.IOException ioe)
      {
         throw new LoginException(ioe.toString());
      }
      catch (UnsupportedCallbackException uce)
      {
         throw new LoginException("Error: " + uce.getCallback().toString() +
         " not available to garner authentication information " +
         "from the user");
      }
      return true;
   }

   /** Method to commit the authentication process (phase 2). This is where the
    * SecurityAssociation information is set. The principal is obtained from:
    * The shared state javax.security.auth.login.name property when useFirstPass
    * is true. If the value is a Principal it is used as is, else a SimplePrincipal
    * using the value.toString() as its name is used. If useFirstPass the
    * username obtained from the callback handler is used to build the
    * SimplePrincipal. Both may be overriden if the resulting authenticated
    * Subject principals set it not empty.
    * 
    */
   public boolean commit() throws LoginException
   {
      Set principals = subject.getPrincipals();
      Principal p = null;
      Object credential = password;
      if( useFirstPass == true )
      {
         Object user = sharedState.get("javax.security.auth.login.name");
         if( (user instanceof Principal) == false )
         {
            username = user != null ? user.toString() : "";
            p = new SimplePrincipal(username);
         }
         else
         {
            p = (Principal) user;
         }
         credential = sharedState.get("javax.security.auth.login.password");
      }
      else
      {
         p = new SimplePrincipal(username);
      }

      if( principals.isEmpty() == false )
         p = (Principal) principals.iterator().next();
      SecurityAssociation.setPrincipal(p);
      SecurityAssociation.setCredential(credential);
      SecurityAssociation.setSubject(subject);
      return true;
   }

   /**
    * Method to abort the authentication process (phase 2).
    */
   public boolean abort() throws LoginException
   {
      int length = password != null ? password.length : 0;
      for(int n = 0; n < length; n ++)
         password[n] = 0;
      SecurityAssociation.clear();
      return true;
   }

   public boolean logout() throws LoginException
   {
      SecurityAssociation.clear();
      return true;
   }
}
