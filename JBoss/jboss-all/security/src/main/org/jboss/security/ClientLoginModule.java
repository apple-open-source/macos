/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.security;


import java.util.Map;
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
 
 @author <a href="mailto:on@ibis.odessa.ua">Oleg Nitz</a>
 @author Scott.Stark@jboss.org
 */
public class ClientLoginModule implements LoginModule
{
   private Subject subject;
   private CallbackHandler callbackHandler;
   /** Shared state between login modules */
   private Map sharedState;
   /** Flag indicating if the shared password should be used */
   private boolean useFirstPass;
   
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
         try
         {
            Object user = sharedState.get("javax.security.auth.login.name");
            Principal principal;
            if( (user instanceof Principal) == false )
            {
               String username = user != null ? user.toString() : "";
               principal = new SimplePrincipal(username);
            }
            else
            {
               principal = (Principal) user;
            }
            Object credential = sharedState.get("javax.security.auth.login.password");
            SecurityAssociation.setPrincipal(principal);
            SecurityAssociation.setCredential(credential);
            SecurityAssociation.setSubject(subject);
            return true;
         }
         catch(Exception e)
         {   // Dump the exception and continue
            e.printStackTrace();
         }
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
         String username;
         char[] password = null;
         char[] tmpPassword;
         
         callbackHandler.handle(callbacks);
         username = nc.getName();
         SecurityAssociation.setPrincipal(new SimplePrincipal(username));
         tmpPassword = pc.getPassword();
         if (tmpPassword != null)
         {
            password = new char[tmpPassword.length];
            System.arraycopy(tmpPassword, 0, password, 0, tmpPassword.length);
            pc.clearPassword();
         }
         SecurityAssociation.setCredential(password);
         SecurityAssociation.setSubject(subject);
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

   /**
    * Method to commit the authentication process (phase 2).
    */
   public boolean commit() throws LoginException
   {
      return true;
   }
   
   /**
    * Method to abort the authentication process (phase 2).
    */
   public boolean abort() throws LoginException
   {
      SecurityAssociation.clear();
      return true;
   }
   
   public boolean logout() throws LoginException
   {
      SecurityAssociation.clear();
      return true;
   }
}
