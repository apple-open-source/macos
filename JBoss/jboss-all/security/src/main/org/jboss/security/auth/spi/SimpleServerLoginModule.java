/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.security.auth.spi;

import java.security.Principal;
import java.security.acl.Group;
import javax.security.auth.login.LoginException;

import org.jboss.security.SimpleGroup;
import org.jboss.security.SimplePrincipal;
import org.jboss.security.auth.spi.UsernamePasswordLoginModule;

/**
 * A simple server login module useful to quick setup of security for testing
 * purposes. It implements the following simple algorithm:
 * <ul>
 * <li> if password is null, authenticate the user and assign an identity of "guest"
 *        and a role of "guest".
 * <li> else if password is equal to the user name, assign an identity equal to
 *        the username and both "user" and "guest" roles
 * <li> else authentication fails.
 * </ul>
 *
 * @author <a href="on@ibis.odessa.ua">Oleg Nitz</a>
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.4 $
 */
public class SimpleServerLoginModule extends UsernamePasswordLoginModule
{
   private SimplePrincipal user;
   private boolean guestOnly;

   protected Principal getIdentity()
   {
      Principal principal = user;
      if( principal == null )
         principal = super.getIdentity();
      return principal;
   }

   protected boolean validatePassword(String inputPassword, String expectedPassword)
   {
      boolean isValid = false;
      if( inputPassword == null )
      {
         guestOnly = true;
         isValid = true;
         user = new SimplePrincipal("guest");
      }
      else
      {
         isValid = inputPassword.equals(expectedPassword);
      }
      return isValid;
   }

   protected Group[] getRoleSets() throws LoginException
   {
      Group[] roleSets = {new SimpleGroup("Roles")};
      if( guestOnly == false )
         roleSets[0].addMember(new SimplePrincipal("user"));
      roleSets[0].addMember(new SimplePrincipal("guest"));
      return roleSets;
   }

   protected String getUsersPassword() throws LoginException
   {
      return getUsername();
   }

}
