/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.security.auth.login;

import java.util.Map;
import java.util.HashMap;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.security.PrivilegedActionException;
import java.security.PrivilegedExceptionAction;
import javax.security.auth.AuthPermission;
import javax.security.auth.Subject;
import javax.security.auth.spi.LoginModule;
import javax.security.auth.callback.CallbackHandler;

import org.jboss.logging.Logger;

/** An alternate implementation of the JAAS 1.0 Configuration class that deals
 * with ClassLoader shortcomings that were fixed in the JAAS included with
 * JDK1.4 and latter. This version allows LoginModules to be loaded from the
 * Thread context ClassLoader and uses an XML based configuration by default.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1 $
 */
public class LoginContext
{
   private static Logger log = Logger.getLogger(LoginContext.class);
   private static final AuthPermission CREATE_LC_PERM  = new AuthPermission("createLoginContext.*");

   private Subject subject;
   private CallbackHandler callbackHandler;
   AppConfigurationEntry[] configEntry;
   private State[] contextState;
   private boolean trace;

   public LoginContext(String name)
      throws LoginException
   {
      this(name, null, null);
   }
   public LoginContext(String name, CallbackHandler callbackHandler)
         throws LoginException
   {
      this(name, null, callbackHandler);
   }
   public LoginContext(String name, Subject subject)
         throws LoginException
   {
      this(name, subject, null);
   }
   /**
    * An AuthPermission("createLoginContext") is required when run with a security
    * manager.
    *
    * @param name The name of the login module configuration to use.
    * @param subject An optional external subject to pass to the login modules.
    *    May be null if a new Subject should be created.
    * @param callbackHandler The handler that will be passed to the login modules
    *    for callbacks to request the necessary authentication data.
    * @throws LoginException
    */
   public LoginContext(String name, Subject subject, CallbackHandler callbackHandler)
         throws LoginException
   {
      SecurityManager sm = System.getSecurityManager();
      if( sm != null )
      {
         sm.checkPermission(CREATE_LC_PERM);
      }
      this.subject = subject;
      this.callbackHandler = callbackHandler;
      this.trace = log.isTraceEnabled();

      // First load the requested login module configuration
      PrivilegedAction action = new PrivilegedAction()
      {
         public Object run()
         {
            return Configuration.getConfiguration();
         }
      };
      Configuration config = (Configuration) AccessController.doPrivileged(action);
      configEntry = config.getAppConfigurationEntry(name);
      if( configEntry == null )
         throw new LoginException("Failed to find login module config for name: "+name);
      if( configEntry.length == 0 )
         throw new LoginException("No login modules found in config for name: "+name);
   }

   /**
    *
    * @return Either the subject passed to the LoginContext ctor or the subject
    *    created for the login process if authentication was successful.
    */
   public Subject getSubject()
   {
      return subject;
   }

   /** Perform the authentication.
    *
    * @throws LoginException
    */
   public void login()
      throws LoginException
   {
      boolean requiredLoginOk = true;
      boolean optionalLoginOk = false;
      int requiredCount = 0;
      int ignoreCount = 0;
      Throwable firstRequiredEx = null;
      Throwable firstOptionalEx = null;

      HashMap sharedState = new HashMap();
      Subject loginSubject = this.subject;
      if( loginSubject == null )
         loginSubject = new Subject();

      // Step 1a, initialize the login modules
      contextState = new State[configEntry.length];
      for(int e = 0; e < contextState.length; e ++)
      {
         try
         {
            contextState[e] = new State(loginSubject, sharedState, callbackHandler, configEntry[e]);
         }
         catch(Exception ex)
         {
            throw new LoginException("Failed to create LoginModule("
               + configEntry[e].getLoginModuleName()+"), msg="+ex.getMessage());
         }

         State state = contextState[e];
         state.initialize();
         if( state.isRequisite() == true || state.isRequired() == true )
         {
            requiredCount ++;
            if( state.initEx != null && firstRequiredEx == null )
               firstRequiredEx = state.initEx;
         }
         else
         {
            if( state.initEx != null && firstOptionalEx == null )
               firstOptionalEx = state.initEx;
         }
      }

      // Step 1b, invoke login on the login modules
      for(int e = 0; e < contextState.length; e ++)
      {
         State state = contextState[e];
         boolean thisLoginOk = false;
         if( state.initEx == null )
         {
            // Do the login operation
            thisLoginOk = state.login();
            // Update the first seen exceptions
            if( firstOptionalEx == null )
               firstOptionalEx = state.optionalException();
            if( firstRequiredEx == null )
               firstRequiredEx = state.requiredException();

            if( trace )
               log.trace("thisLoginOk="+thisLoginOk+", state="+state);

            // Success on a sufficient login module short circuts login phase
            if( thisLoginOk == true && state.loginEx == null && state.isSufficient() )
            {
               optionalLoginOk = true;
               break;
            }
            // Failure on a requisite login module short circuts login phase
            else if( state.loginEx != null && state.isRequisite() )
            {
               requiredLoginOk = false;
               break;
            }
            // The login method returned true so update the overall status
            else if( thisLoginOk )
            {
               // All required and requisite login modules must succeed
               if( state.isRequisite() || state.isRequired() )
                  requiredLoginOk &= (state.loginEx == null);
               // Any optional or sufficient login module can succeed
               else
                  optionalLoginOk |= (state.loginEx == null);
            }
            // The login method returned false to ignore the result
            else
            {
               ignoreCount ++;
            }
         }
         // If a required module failed initialzation we fail
         else if( state.isRequisite() || state.isRequired() )
         {
            requiredLoginOk = false;
         }
      }

      // All required login modules must have succeeded
      boolean loginOk = requiredLoginOk;
      // If there were no required modules any optional success is sufficient
      if( requiredCount == 0 )
         loginOk = optionalLoginOk;

      if( trace )
      {
         log.trace("ignoreCount="+ignoreCount
               + ", requiredCount="+requiredCount
               + ", requiredLoginOk="+requiredLoginOk
               + ", optionalLoginOk="+optionalLoginOk
               + ", firstRequiredEx="+firstRequiredEx
               + ", firstOptionalEx="+firstOptionalEx);
      }

      /* Step 1c, invoke commit on the login modules if loginOk is true,
         else invoke abort on the login modules
      */
      for(int e = 0; e < contextState.length; e ++)
      {
         State state = contextState[e];
         if( loginOk == true )
            state.commit();
         else
            state.abort();
      }

      // If the login failed throw the first exception seen
      if( loginOk == false )
      {
         if( firstRequiredEx != null )
         {
            if( firstRequiredEx instanceof LoginException )
               throw (LoginException) firstRequiredEx;
            throw new LoginException("Login failed, msg="+firstRequiredEx.getMessage());
         }
         if( firstOptionalEx != null )
         {
            if( firstOptionalEx instanceof LoginException )
               throw (LoginException) firstOptionalEx;
            throw new LoginException("Login failed, msg="+firstOptionalEx.getMessage());
         }
         if( ignoreCount == contextState.length )
            throw new LoginException("Login failed, all modules ignored");
      }
      else
      {
         this.subject = loginSubject;
      }
   }

   /** Invoke logout on the login modules
    *
    * @throws LoginException
    */
   public void logout()
      throws LoginException
   {
      if( subject == null )
         throw new LoginException("No non-null subject exists");

      for(int e = 0; e < contextState.length; e ++)
      {
         State state = contextState[e];
         state.logout();
      }
   }

   /** Maintains the state of a login module
    */
   private static class State
   {
      private Subject subject;
      private CallbackHandler handler;
      private Map sharedMap;
      private AppConfigurationEntry entry;
      private LoginModule module;
      Throwable initEx;
      Throwable loginEx;

      State(Subject subject, Map sharedMap, CallbackHandler handler, AppConfigurationEntry entry)
         throws ClassNotFoundException, InstantiationException, IllegalAccessException
      {
         this.subject = subject;
         this.sharedMap = sharedMap;
         this.handler = handler;
         this.entry = entry;
         // Load the login module class and create an instance
         ClassLoader cl = Thread.currentThread().getContextClassLoader();
         Class lmClass = cl.loadClass(entry.getLoginModuleName());
         module = (LoginModule) lmClass.newInstance();
      }

      Throwable requiredException()
      {
         Throwable t = loginEx;
         if( isRequired() == false && isRequisite() == false )
            t = null;
         return t;
      }
      Throwable optionalException()
      {
         Throwable t = loginEx;
         if( isSufficient() == false && isOptional() == false )
            t = null;
         return t;
      }
      boolean isRequired()
      {
         return entry.getControlFlag() == AppConfigurationEntry.LoginModuleControlFlag.REQUIRED;
      }
      boolean isRequisite()
      {
         return entry.getControlFlag() == AppConfigurationEntry.LoginModuleControlFlag.REQUISITE;
      }
      boolean isSufficient()
      {
         return entry.getControlFlag() == AppConfigurationEntry.LoginModuleControlFlag.SUFFICIENT;
      }
      boolean isOptional()
      {
         return entry.getControlFlag() == AppConfigurationEntry.LoginModuleControlFlag.OPTIONAL;
      }
      void initialize()
      {
         PrivilegedExceptionAction action = new PrivilegedExceptionAction()
         {
            public Object run() throws Exception
            {
               module.initialize(subject, handler, sharedMap, entry.getOptions());
               return null;
            }
         };

         try
         {
            AccessController.doPrivileged(action);
         }
         catch(PrivilegedActionException e)
         {
            initEx = e.getException();
         }
         catch(Throwable t)
         {
            initEx = t;
         }
      }
      boolean login() throws LoginException
      {
         boolean loginOk = true;
         PrivilegedExceptionAction action = new PrivilegedExceptionAction()
         {
            public Object run() throws LoginException
            {
               boolean ok = module.login();
               return new Boolean(ok);
            }
         };

         try
         {
            Boolean ok = (Boolean) AccessController.doPrivileged(action);
            loginOk = ok.booleanValue();
         }
         catch(PrivilegedActionException e)
         {
            loginEx = e.getException();
         }
         catch(Throwable t)
         {
            loginEx = t;
         }
         return loginOk;
      }

      boolean commit() throws LoginException
      {
         boolean commitOk = true;
         PrivilegedExceptionAction action = new PrivilegedExceptionAction()
         {
            public Object run() throws LoginException
            {
               boolean ok = module.commit();
               return new Boolean(ok);
            }
         };

         try
         {
            Boolean ok = (Boolean) AccessController.doPrivileged(action);
            commitOk = ok.booleanValue();
         }
         catch(PrivilegedActionException e)
         {
            throw (LoginException) e.getException();
         }
         return commitOk;
      }

      boolean abort() throws LoginException
      {
         boolean abortOk = true;
         PrivilegedExceptionAction action = new PrivilegedExceptionAction()
         {
            public Object run() throws LoginException
            {
               boolean ok = module.abort();
               return new Boolean(ok);
            }
         };

         try
         {
            Boolean ok = (Boolean) AccessController.doPrivileged(action);
            abortOk = ok.booleanValue();
         }
         catch(PrivilegedActionException e)
         {
            throw (LoginException) e.getException();
         }
         return abortOk;
      }

      boolean logout() throws LoginException
      {
         boolean logoutOk = true;
         PrivilegedExceptionAction action = new PrivilegedExceptionAction()
         {
            public Object run() throws LoginException
            {
               boolean ok = module.logout();
               return new Boolean(ok);
            }
         };

         try
         {
            Boolean ok = (Boolean) AccessController.doPrivileged(action);
            logoutOk = ok.booleanValue();
         }
         catch(PrivilegedActionException e)
         {
            throw (LoginException) e.getException();
         }
         return logoutOk;
      }

      public String toString()
      {
         StringBuffer tmp = new StringBuffer(entry.getLoginModuleName());
         tmp.append('{');
         tmp.append("controlFlag: ");
         tmp.append(entry.getControlFlag());
         tmp.append(", options: ");
         tmp.append(entry.getOptions());
         tmp.append(", initEx: ");
         tmp.append(initEx);
         tmp.append(", loginEx: ");
         tmp.append(loginEx);
         tmp.append('}');
         return tmp.toString();
      }
   }
}
