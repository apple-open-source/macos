/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.security.auth.spi;

import java.security.Principal;
import java.security.acl.Group;
import java.util.Map;
import java.util.StringTokenizer;
import javax.security.auth.Subject;
import javax.security.auth.callback.CallbackHandler;
import javax.security.auth.login.LoginException;

import org.jboss.security.SimpleGroup;
import org.jboss.security.SimplePrincipal;
import org.jboss.security.auth.spi.AbstractServerLoginModule;

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
 * @version $Revision: 1.5.4.1 $
 */
public class IdentityLoginModule extends AbstractServerLoginModule
{
   private String principalName;
   private String roleNames;

   public IdentityLoginModule()
   {
   }

   public void initialize(Subject subject, CallbackHandler handler, Map sharedState, Map options)
   {
      super.initialize(subject, handler, sharedState, options);
      principalName = (String) options.get("principal");
      if( principalName == null )
         principalName = "guest";
      roleNames = (String) options.get("roles");
   }

   public boolean login() throws LoginException
   {
      if( super.login() == true )
         return true;

      Principal principal = new SimplePrincipal(principalName);
      subject.getPrincipals().add(principal);
      // Put the principal name into the sharedState map
      sharedState.put("javax.security.auth.login.name", principalName);
      super.loginOk = true;
      return true;
   }

   protected Principal getIdentity()
   {
      Principal principal = new SimplePrincipal(principalName);
      return principal;
   }

   protected Group[] getRoleSets() throws LoginException
   {
      SimpleGroup roles = new SimpleGroup("Roles");
      Group[] roleSets = {roles};
      if( roleNames != null )
      {
         StringTokenizer tokenizer = new StringTokenizer(roleNames, ",");
         while( tokenizer.hasMoreTokens() )
         {
            String roleName = tokenizer.nextToken();
            roles.addMember(new SimplePrincipal(roleName));
         }
      }
      return roleSets;
   }
}
