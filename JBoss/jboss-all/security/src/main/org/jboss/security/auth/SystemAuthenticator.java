/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.security.auth;

import javax.security.auth.Subject;
import javax.security.auth.callback.CallbackHandler;
import javax.security.auth.login.LoginContext;
import javax.security.auth.login.LoginException;

import org.jboss.system.ServiceMBeanSupport;

/** An MBean that requires a JAAS login in order for it to startup. This is
 * used to require authentication to startup a JBoss instance.
 *
 * @version $Revision: 1.1.2.2 $
 * @author Scott.Stark@jboss.org
 */
public class SystemAuthenticator extends ServiceMBeanSupport
   implements SystemAuthenticatorMBean
{
   /** The Subject that results from the login. Not used currently */
   private Subject systemSubject;
   /** The name of the security domain to authenticate under */
   private String securityDomain;
   /** The CallbackHandler that knows how to provide Callbacks for the
      security domain login modules
    */
   private CallbackHandler callbackHandler;

   /** Get the name of the security domain used for authentication
    */
   public String getSecurityDomain()
   {
      return this.securityDomain;
   }

   /** Set the name of the security domain used for authentication
    */
   public void setSecurityDomain(String name)
   {
      this.securityDomain = name;
   }

   /** Get the CallbackHandler to use to obtain the authentication
    information.
    @see javax.security.auth.callback.CallbackHandler
    */
   public Class getCallbackHandler()
   {
      Class clazz = null;
      if( callbackHandler != null )
         clazz = callbackHandler.getClass();
      return clazz;
   }
   /** Specify the CallbackHandler to use to obtain the authentication
    information.
    @see javax.security.auth.callback.CallbackHandler
    */
   public void setCallbackHandler(Class callbackHandlerClass)
      throws InstantiationException, IllegalAccessException
   {
      callbackHandler = (CallbackHandler) callbackHandlerClass.newInstance();
   }

   protected void startService() throws Exception
   {
      try
      {
         LoginContext lc = new LoginContext(securityDomain, callbackHandler);
         lc.login();
         this.systemSubject = lc.getSubject();
      }
      catch(Throwable t)
      {
         log.fatal("SystemAuthenticator failed, server will shutdown NOW!", t);
         LoginException le = new LoginException("SystemAuthenticator failed, msg="+t.getMessage());
         Thread shutdownThread = new Thread("SystemAuthenticatorExitThread")
         {
            public void run()
            {
               System.exit(1);
            }
         };
         shutdownThread.start();
      }
   }

   protected void stopService() throws Exception
   {
      if( systemSubject != null )
      {
         LoginContext lc = new LoginContext(securityDomain, systemSubject, callbackHandler);
         lc.logout();
      }
   }
}
