/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.auth.spi;


import java.security.Principal;
import java.security.acl.Group;
import java.util.Enumeration;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;

import javax.security.auth.Subject;
import javax.security.auth.callback.CallbackHandler;
import javax.security.auth.login.LoginException;
import javax.security.auth.spi.LoginModule;

import org.jboss.logging.Logger;
import org.jboss.security.NestableGroup;
import org.jboss.security.SimpleGroup;

/**
 * This class implements the common functionality required for a JAAS
 * server side LoginModule and implements the JBossSX standard Subject usage
 * pattern of storing identities and roles. Subclass this module to create your
 * own custom LoginModule and override the login(), getRoleSets() and getIdentity()
 * methods.
 * <p>
 * You may also wish to override
 * <pre>
 *    public void initialize(Subject subject, CallbackHandler callbackHandler, Map sharedState, Map options)
 * </pre>
 * In which case the first line of your initialize() method should be:
 * <pre>
 *    super.initialize(subject, callbackHandler, sharedState, options);
 * </pre>
 * <p>
 * You may also wish to override
 * <pre>
 *    public boolean login() throws LoginException
 * </pre>
 * In which case the last line of your login() method should be
 * <pre>
 *    return super.login();
 * </pre>
 *
 *@author <a href="edward.kenworthy@crispgroup.co.uk">Edward Kenworthy</a>, 12th Dec 2000
 *@author Scott.Stark@jboss.org
 *@version $Revision: 1.6.4.3 $
 */
public abstract class AbstractServerLoginModule implements LoginModule
{
   protected Subject subject;
   protected CallbackHandler callbackHandler;
   protected Map sharedState;
   protected Map options;
   protected Logger log;
   /** Flag indicating if the shared credential should be used */
   protected boolean useFirstPass;
   /** Flag indicating if the login phase succeeded. Subclasses that override
    the login method must set this to true on successful completion of login
    */
   protected boolean loginOk;

//--- Begin LoginModule interface methods
   /**
    * Initialize the login module. This stores the subject, callbackHandler
    * and sharedState and options for the login session. Subclasses should override
    * if they need to process their own options. A call to super.initialize(...)
    * must be made in the case of an override.
    * <p>
    * The options are checked for the <em>password-stacking</em> parameter.
    * If this is set to "useFirstPass", the login identity will be taken from the
    * <code>javax.security.auth.login.name</code> value of the sharedState map,
    * and the proof of identity from the
    * <code>javax.security.auth.login.password</code> value of the sharedState map.
    *
    * @param subject the Subject to update after a successful login.
    * @param callbackHandler the CallbackHandler that will be used to obtain the
    *    the user identity and credentials.
    * @param sharedState a Map shared between all configured login module instances
    * @param options the parameters passed to the login module.
    */
   public void initialize(Subject subject, CallbackHandler callbackHandler, Map sharedState, Map options)
   {
      this.subject = subject;
      this.callbackHandler = callbackHandler;
      this.sharedState = sharedState;
      this.options = options;
      log = Logger.getLogger(getClass());
      log.trace("initialize");
     /* Check for password sharing options. Any non-null value for
         password_stacking sets useFirstPass as this module has no way to
         validate any shared password.
      */
      String passwordStacking = (String) options.get("password-stacking");
      if( passwordStacking != null && passwordStacking.equalsIgnoreCase("useFirstPass") )
         useFirstPass = true;
   }

   /** Looks for javax.security.auth.login.name and javax.security.auth.login.password
    values in the sharedState map if the useFirstPass option was true and returns
    true if they exist. If they do not or are null this method returns false.

    Note that subclasses that override the login method must set the loginOk
    ivar to true if the login succeeds in order for the commit phase to
    populate the Subject. This implementation sets loginOk to true if the
    login() method returns true, otherwise, it sets loginOk to false.
    */
   public boolean login() throws LoginException
   {
      log.trace("login");
      loginOk = false;
      // If useFirstPass is true, look for the shared password
      if( useFirstPass == true )
      {
         try
         {
            Object identity = sharedState.get("javax.security.auth.login.name");
            Object credential = sharedState.get("javax.security.auth.login.password");
            if( identity != null && credential != null )
            {
               loginOk = true;
               return true;
            }
            // Else, fall through and perform the login
         }
         catch(Exception e)
         {   // Dump the exception and continue
            log.error("login failed", e);
         }
      }
      return false;
   }

   /** Method to commit the authentication process (phase 2). If the login
    method completed successfully as indicated by loginOk == true, this
    method adds the getIdentity() value to the subject getPrincipals() Set.
    It also adds the members of each Group returned by getRoleSets()
    to the subject getPrincipals() Set.
    
    @see javax.security.auth.Subject;
    @see java.security.acl.Group;
    @return true always.
    */
   public boolean commit() throws LoginException
   {
      log.trace("commit, loginOk="+loginOk);
      if( loginOk == false )
         return false;

      Set principals = subject.getPrincipals();
      Principal identity = getIdentity();
      principals.add(identity);
      Group[] roleSets = getRoleSets();
      for(int g = 0; g < roleSets.length; g ++)
      {
         Group group = roleSets[g];
         String name = group.getName();
         Group subjectGroup = createGroup(name, principals);
         if( subjectGroup instanceof NestableGroup )
         {
            /* A NestableGroup only allows Groups to be added to it so we
            need to add a SimpleGroup to subjectRoles to contain the roles
            */
            SimpleGroup tmp = new SimpleGroup("Roles");
            subjectGroup.addMember(tmp);
            subjectGroup = tmp;
         }
         // Copy the group members to the Subject group
         Enumeration members = group.members();
         while( members.hasMoreElements() )
         {
            Principal role = (Principal) members.nextElement();
            subjectGroup.addMember(role);
         }
      }
      return true;
   }

   /** Method to abort the authentication process (phase 2).
    @return true alaways
    */
   public boolean abort() throws LoginException
   {
      log.trace("abort");
      return true;
   }
   
   /** Remove the user identity and roles added to the Subject during commit.
    @return true always.
    */
   public boolean logout() throws LoginException
   {
      log.trace("logout");
      // Remove the user identity
      Principal identity = getIdentity();
      Set principals = subject.getPrincipals();
      principals.remove(identity);
      // Remove any added Groups...
      return true;
   }
   //--- End LoginModule interface methods
   
   // --- Protected methods
   
   /** Overriden by subclasses to return the Principal that corresponds to
    the user primary identity.
    */
   abstract protected Principal getIdentity();
   /** Overriden by subclasses to return the Groups that correspond to the
    to the role sets assigned to the user. Subclasses should create at
    least a Group named "Roles" that contains the roles assigned to the user.
    A second common group is "CallerPrincipal" that provides the application
    identity of the user rather than the security domain identity.
    @return Group[] containing the sets of roles
    */
   abstract protected Group[] getRoleSets() throws LoginException;
   
   protected boolean getUseFirstPass()
   {
      return useFirstPass;
   }
   
   /** Find or create a Group with the given name. Subclasses should use this
    method to locate the 'Roles' group or create additional types of groups.
    @return A named Group from the principals set.
    */
   protected Group createGroup(String name, Set principals)
   {
      Group roles = null;
      Iterator iter = principals.iterator();
      while( iter.hasNext() )
      {
         Object next = iter.next();
         if( (next instanceof Group) == false )
            continue;
         Group grp = (Group) next;
         if( grp.getName().equals(name) )
         {
            roles = grp;
            break;
         }
      }
      // If we did not find a group create one
      if( roles == null )
      {
         roles = new NestableGroup(name);
         principals.add(roles);
      }
      return roles;
   }
}
