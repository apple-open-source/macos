/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.resource.security;


import java.security.Principal;
import java.security.acl.Group;
import java.util.Map;
import java.util.StringTokenizer;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.resource.spi.ManagedConnectionFactory;
import javax.resource.spi.security.PasswordCredential;
import javax.security.auth.Subject;
import javax.security.auth.callback.CallbackHandler;
import javax.security.auth.login.LoginException;
import org.jboss.security.SimpleGroup;
import org.jboss.security.SimplePrincipal;
import org.jboss.security.auth.spi.AbstractServerLoginModule;
import org.jboss.logging.Logger;

/**
 * A simple login module that simply associates the principal specified
 * in the module options with any subject authenticated against the module.
 * The type of Principal class used is
 * <code>org.jboss.security.SimplePrincipal.</code>
 * <p>
 * If no principal option is specified a principal with the name of 'guest'
 * is used.
 *
 * @see org.jboss.security.SimpleGroup
 * @see org.jboss.security.SimplePrincipal
 *
 * @author Scott.Stark@jboss.org
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version $Revision: 1.6.2.3 $
 */
public class ConfiguredIdentityLoginModule extends AbstractPasswordCredentialLoginModule
{
   private String principalName;
   private String userName;
   private String password;
   private ManagedConnectionFactory mcf;

   private static final Logger log = Logger.getLogger(ConfiguredIdentityLoginModule.class);


   public ConfiguredIdentityLoginModule()
   {
   }

   public void initialize(Subject subject, CallbackHandler handler, Map sharedState, Map options)
   {
      super.initialize(subject, handler, sharedState, options);
      principalName = (String) options.get("principal");
      if( principalName == null )
      {
         throw new IllegalArgumentException("Must supply a principal name!");
      }
      userName = (String) options.get("userName");
      if( userName == null )
      {
         throw new IllegalArgumentException("Must supply a user name!");
      }
      password = (String) options.get("password");
      if( password == null )
      {
         log.warn("Creating LoginModule with no configured password!");
         password = "";
      }
      log.trace("got principal: " + principalName + ", username: " + userName + ", password: " + password);

   }

   public boolean login() throws LoginException
   {
      log.trace("login called");
      if( super.login() == true )
         return true;

      Principal principal = new SimplePrincipal(principalName);
      subject.getPrincipals().add(principal);
      // Put the principal name into the sharedState map
      sharedState.put("javax.security.auth.login.name", principalName);
      PasswordCredential cred = new PasswordCredential(userName, password.toCharArray());
      cred.setManagedConnectionFactory(getMcf());
      subject.getPrivateCredentials().add(cred);
      super.loginOk = true;
      return true;
   }

   protected Principal getIdentity()
   {
      log.trace("getIdentity called");
      Principal principal = new SimplePrincipal(principalName);
      return principal;
   }

   /** This method simply returns an empty array of Groups which means that
   no role based permissions are assigned.
   */
   protected Group[] getRoleSets() throws LoginException
   {
      log.trace("getRoleSets called");
      return new Group[] {};
   }
   
}
