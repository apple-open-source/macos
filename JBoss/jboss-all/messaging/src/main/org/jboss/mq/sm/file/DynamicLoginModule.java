/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.sm.file;

import java.util.Map;
import java.util.ArrayList;

import java.security.acl.Group;
import javax.security.auth.Subject;
import javax.security.auth.callback.CallbackHandler;
import javax.security.auth.login.LoginException;

import org.jboss.security.SimpleGroup;
import org.jboss.security.SimplePrincipal;
import org.jboss.security.auth.spi.UsernamePasswordLoginModule;

/**
 * JAAS LoginModule that is backed by the DynamicStateManager.
 *
 * Must have the attribute sm.objectname set,
 * and may have the unauthenticatedIdentity set to some value.
 * @author     <a href="pra@tim.se">Peter Antman</a>
 * @version $Revision: 1.1.4.2 $
 */

public class DynamicLoginModule extends UsernamePasswordLoginModule
{
   static final String DEFAULT_SM_NAME = "jboss.mq:service=StateManager";

   DynamicStateManager sm = null;

   public DynamicLoginModule()
   {

   }

   public void initialize(Subject subject, CallbackHandler callbackHandler, Map sharedState, Map options)
   {
      super.initialize(subject, callbackHandler, sharedState, options);
      try
      {
         String smName = (String) options.get("sm.objectname");
         if (smName == null)
            smName = DEFAULT_SM_NAME;

         javax.management.ObjectName smObjectName = new javax.management.ObjectName(smName);

         // Lokup the state manager. FIXME
         javax.management.MBeanServer server = org.jboss.mx.util.MBeanServerLocator.locateJBoss();
         sm = (DynamicStateManager) server.getAttribute(smObjectName, "Instance");

      }
      catch (Exception ex)
      {
         super.log.error("Failed to load DynamicSecurityManager", ex);
      }

   }

   /**
    * Check we have contact to a state manager.
    */
   public boolean login() throws LoginException
   {
      if (sm == null)
         throw new LoginException("StateManager is null");

      return super.login();
   }


   /** Overriden to return an empty password string as typically one cannot
    obtain a user's password. We also override the validatePassword so
    this is ok.
    @return and empty password String
    */
   protected String getUsersPassword() throws LoginException
   {
      return "";
   }

   /**
    * Validate the password againts the state manager.
    *
    * @param inputPassword the password to validate.
    * @param expectedPassword ignored
    */
   protected boolean validatePassword(String inputPassword, String expectedPassword)
   {
      boolean valid = false;
      try
      {
         valid = sm.validatePassword(getUsername(), inputPassword);
      }
      catch (Exception ex)
      {
         super.log.debug("Could not validate password for user " + getUsername(), ex);
      }
      return valid;
   }

   /** Overriden by subclasses to return the Groups that correspond to the
    *  to the role sets assigned to the user. Subclasses should create at
    *   least a Group named "Roles" that contains the roles assigned to the user.
    *  A second common group is "CallerPrincipal" that provides the application
    *   identity of the user rather than the security domain identity.
    *
    * Only a Roles Group is returned.
    *  @return Group[] containing the sets of roles
    */
   protected Group[] getRoleSets() throws LoginException
   {
      SimpleGroup userRoles = new SimpleGroup("Roles");
      String[] roles = null;
      try
      {
         roles = sm.getRoles(getUsername());
      }
      catch (Exception ex)
      {
         super.log.error("Could not get roleSets for user " + getUsername(), ex);
         throw new LoginException("Could not get roleSets for user");
      }
      if (roles != null)
      {
         for (int i = 0; i < roles.length; i++)
         {
            userRoles.addMember(new SimplePrincipal(roles[i]));
         }
      }

      Group[] roleSets = {userRoles};
      return roleSets;
   }
} // DynamicLoginModule



