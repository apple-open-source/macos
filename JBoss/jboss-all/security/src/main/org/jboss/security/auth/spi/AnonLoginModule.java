/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.security.auth.spi;

import java.security.acl.Group;
import javax.security.auth.login.LoginException;

import org.jboss.security.SimpleGroup;
import org.jboss.security.auth.spi.UsernamePasswordLoginModule;

/**
 * A simple login module that simply allows for the specification of the
 * identity of unauthenticated users via the unauthenticatedIdentity property.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.5 $
 */
public class AnonLoginModule extends UsernamePasswordLoginModule
{
   /**
    * Override to return an empty Roles set.
    * @return an array comtaning an empty 'Roles' Group.
    */
   protected Group[] getRoleSets() throws LoginException
   {
      SimpleGroup roles = new SimpleGroup("Roles");
      Group[] roleSets = {roles};
      return roleSets;
   }

   /**
    * Overriden to return null.
    * @return null always
    */
   protected String getUsersPassword() throws LoginException
   {
      return null;
   }
}
