/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.plugins;

import java.lang.reflect.Method;
import java.lang.reflect.UndeclaredThrowableException;
import java.security.Principal;
import java.security.acl.Group;
import java.util.Arrays;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;
import javax.security.auth.Subject;
import javax.security.auth.callback.CallbackHandler;
import javax.security.auth.login.LoginContext;
import javax.security.auth.login.LoginException;

import org.jboss.logging.Logger;
import org.jboss.security.AnybodyPrincipal;
import org.jboss.security.NobodyPrincipal;
import org.jboss.security.RealmMapping;
import org.jboss.security.SecurityAssociation;
import org.jboss.security.SubjectSecurityManager;
import org.jboss.security.auth.callback.SecurityAssociationHandler;
import org.jboss.util.CachePolicy;
import org.jboss.util.TimedCachePolicy;

/** The JaasSecurityManager is responsible both for authenticating credentials
 associated with principals and for role mapping. This implementation relies
 on the JAAS LoginContext/LoginModules associated with the security
 domain name associated with the class for authentication,
 and the context JAAS Subject object for role mapping.
 
 @see #isValid(Principal, Object)
 @see Principal getPrincipal(Principal)
 @see #doesUserHaveRole(Principal, Set)
 
 @author <a href="on@ibis.odessa.ua">Oleg Nitz</a>
 @author Scott.Stark@jboss.org
 @version $Revision: 1.27.2.9 $
*/
public class JaasSecurityManager implements SubjectSecurityManager, RealmMapping
{
   /** The authentication cache object.
    */
   public static class DomainInfo implements TimedCachePolicy.TimedEntry
   {
      private static Logger log = Logger.getLogger(DomainInfo.class);
      LoginContext loginCtx;
      Subject subject;
      Object credential;
      Principal callerPrincipal;
      Group roles;
      long expirationTime;

      public DomainInfo(int lifetime)
      {
         expirationTime = 1000 * lifetime;
      }
      public void init(long now)
      {
         expirationTime += now;
      }
      public boolean isCurrent(long now)
      {
         return expirationTime > now;
      }
      public boolean refresh()
      {
         return false;
      }
      public void destroy()
      {
         try
         {
            loginCtx.logout();
         }
         catch(Exception e)
         {
            if( log.isTraceEnabled() )
               log.trace("Cache entry logout failed", e);
         }
      }
      public Object getValue()
      {
         return this;
      }        
   }

   /** The name of the domain this instance is securing. It is used as
    the appName into the SecurityPolicy.
    */
   private String securityDomain;
   /** A cache of DomainInfo objects keyd by Principal. This is now
    always set externally by our security manager service.
    */
   private CachePolicy domainCache;
   /** The JAAS callback handler to use in defaultLogin */
   private CallbackHandler handler;
   /** The setSecurityInfo(Principal, Object) method of the handler obj */
   private Method setSecurityInfo;

   /** The log4j category for the security manager domain
    */
   protected Logger log;

   /** Get the currently authenticated Subject in securityDomain.
    @return The Subject for securityDomain if one exists, false otherwise.
    */
   public static Subject getActiveSubject(String securityDomain)
   {
      Subject subject = null;
      return subject;
   }
   /** Create a LoginContext for the currently authenticated Subject in
    securityDomain.
    */
   public static LoginContext getActiveSubjectLoginContext(String securityDomain, CallbackHandler handler)
      throws LoginException
   {
      LoginContext lc = null;
      Subject subject = getActiveSubject(securityDomain);
      if( subject == null )
         throw new LoginException("No active subject found in securityDomain: "+securityDomain);
      
      if( handler != null )
         lc = new LoginContext(securityDomain, subject, handler);
      else
         lc = new LoginContext(securityDomain, subject);
      
      return lc;
   }

   /** Creates a default JaasSecurityManager for with a securityDomain
    name of 'other'.
    */
   public JaasSecurityManager()
   {
      this("other", new SecurityAssociationHandler());
   }
   /** Creates a JaasSecurityManager for with a securityDomain
    name of that given by the 'securityDomain' argument.
    @param securityDomain the name of the security domain
    @param handler the JAAS callback handler instance to use
    @exception UndeclaredThrowableException thrown if handler does not
      implement a setSecurityInfo(Princpal, Object) method
    */
   public JaasSecurityManager(String securityDomain, CallbackHandler handler)
   {
      this.securityDomain = securityDomain;
      this.handler = handler;
      String categoryName = getClass().getName()+'.'+securityDomain;
      this.log = Logger.getLogger(categoryName);

      // Get the setSecurityInfo(Principal principal, Object credential) method
      Class[] sig = {Principal.class, Object.class};
      try
      {
         setSecurityInfo = handler.getClass().getMethod("setSecurityInfo", sig);
      }
      catch (Exception e)
      {
         String msg = "Failed to find setSecurityInfo(Princpal, Object) method in handler";
         throw new UndeclaredThrowableException(e, msg);
      }
   }

   /** The domainCache is typically a shared object that is populated
    by the login code(LoginModule, etc.) and read by this class in the
    isValid() method.
    @see #isValid(Principal, Object)
    */
   public void setCachePolicy(CachePolicy domainCache)
   {
      this.domainCache = domainCache;
      log.debug("CachePolicy set to: "+domainCache);
   }

   /** Not really used anymore as the security manager service manages the
    security domain authentication caches.
    */
   public void flushCache()
   {
      if( domainCache != null )
         domainCache.flush();
   }

   /** Get the name of the security domain associated with this security mgr.
    @return Name of the security manager security domain.
    */
   public String getSecurityDomain()
   {
      return securityDomain;
   }

   /** Get the currently authenticated Subject. This is a thread local
    property shared across all JaasSecurityManager instances.
    @return The Subject authenticated in the current thread if one
    exists, null otherwise.
    */
   public Subject getActiveSubject()
   {
      return SecurityAssociation.getSubject();
   }

   /** Validate that the given credential is correct for principal. This
    returns the value from invoking isValid(principal, credential, null).
    @param principal, the security domain principal attempting access
    @param credential, the proof of identity offered by the principal
    @return true if the principal was authenticated, false otherwise.
    */
   public boolean isValid(Principal principal, Object credential)
   {
      return isValid(principal, credential, null);
   }

   /** Validate that the given credential is correct for principal. This first
    will check the current CachePolicy object if one exists to see if the
    user's cached credentials match the given credential. If there is no
    credential cache or the cache information is invalid or does not match,
    the user is authenticated against the JAAS login modules configured for
    the security domain.
    @param principal, the security domain principal attempting access
    @param credential, the proof of identity offered by the principal
    @param activeSubject, if not null, a Subject that will be populated with
      the state of the authenticated Subject.
    @return true if the principal was authenticated, false otherwise.
    */
   public synchronized boolean isValid(Principal principal, Object credential,
      Subject activeSubject)
   {
      // Check the cache first
      DomainInfo cacheInfo = getCacheInfo(principal, true);

      boolean isValid = false;
      if( cacheInfo != null )
         isValid = validateCache(cacheInfo, credential);
      if( isValid == false )
         isValid = authenticate(principal, credential);
      if( isValid == true && activeSubject != null )
      {
         // Copy the current subject into the activeSubject
         Subject theSubject = getActiveSubject();
         if( theSubject != null )
         {
            Set principals = theSubject.getPrincipals();
            Set principals2 = activeSubject.getPrincipals();
            Iterator iter = principals.iterator();
            while( iter.hasNext() )
               principals2.add(iter.next());
            Set privateCreds = theSubject.getPrivateCredentials();
            Set privateCreds2 = activeSubject.getPrivateCredentials();
            iter = privateCreds.iterator();
            while( iter.hasNext() )
               privateCreds2.add(iter.next());
            Set publicCreds = theSubject.getPublicCredentials();
            Set publicCreds2 = activeSubject.getPublicCredentials();
            iter = publicCreds.iterator();
            while( iter.hasNext() )
               publicCreds2.add(iter.next());
         }
      }
      return isValid;
   }

   /** Map the argument principal from the deployment environment principal
    to the developer environment. This is called by the EJB context
    getCallerPrincipal() to return the Principal as described by
    the EJB developer domain.
    @return a Principal object that is valid in the deployment environment
    if one exists. If no Subject exists or the Subject has no principals
    then the argument principal is returned.
    */
   public Principal getPrincipal(Principal principal)
   {
      Principal result = principal;
      // Get the CallerPrincipal group member
      synchronized( domainCache )
      {
         DomainInfo info = getCacheInfo(principal, false);
         if( info != null )
         {
            result = info.callerPrincipal;
            // If the mapping did not have a callerPrincipal just use principal
            if( result == null )
               result = principal;
         }
      }

      return result;
   }

   /** Does the current Subject have a role(a Principal) that equates to one
    of the role names. This method obtains the Group named 'Roles' from
    the principal set of the currently authenticated Subject and then
    creates a SimplePrincipal for each name in roleNames. If the role is
    a member of the Roles group, then the user has the role.
    @param principal, ignored. The current authenticated Subject determines
    the active user and assigned user roles.
    @param rolePrincipals, a Set of Principals for the roles to check.
    
    @see java.security.acl.Group;
    @see Subject#getPrincipals()
    */
   public boolean doesUserHaveRole(Principal principal, Set rolePrincipals)
   {
      boolean hasRole = false;
      // Check that the caller is authenticated to the current thread
      Subject subject = getActiveSubject();
      if( subject != null )
      {
         // Check the caller's roles
         synchronized( domainCache )
         {
            DomainInfo info = getCacheInfo(principal, false);
            Group roles = null;
            if( info != null )
               roles = info.roles;
            if( roles != null )
            {
               Iterator iter = rolePrincipals.iterator();
               while( hasRole == false && iter.hasNext() )
               {
                  Principal role = (Principal) iter.next();
                  hasRole = doesRoleGroupHaveRole(role, roles);
               }
            }
         }
      }
      return hasRole;
   }

   /** Validates operational environment Principal against the specified
    application domain role.
    @param principal, the caller principal as known in the operation environment.
    @param role, the application domain role that the principal is to be validated against.
    @return true if the principal has the role, false otherwise.
    */
   public boolean doesUserHaveRole(Principal principal, Principal role)
   {
      boolean hasRole = false;
      // Check that the caller is authenticated to the current thread
      Subject subject = getActiveSubject();
      if( subject != null )
      {
         // Check the caller's roles
         synchronized( domainCache )
         {
            DomainInfo info = getCacheInfo(principal, false);
            Group roles = null;
            if( info != null )
               roles = info.roles;
            if( roles != null )
            {
               hasRole = doesRoleGroupHaveRole(role, roles);
            }
         }
      }
      return hasRole;
   }

   /** Return the set of domain roles the principal has been assigned.
   @return The Set<Principal> for the application domain roles that the
   principal has been assigned.
   */
   public Set getUserRoles(Principal principal)
   {
      HashSet userRoles = null;
      // Check that the caller is authenticated to the current thread
      Subject subject = getActiveSubject();
      if( subject != null )
      {
         // Copy the caller's roles
         synchronized( domainCache )
         {
            DomainInfo info = getCacheInfo(principal, false);
            Group roles = null;
            if( info != null )
               roles = info.roles;
            if( roles != null )
            {
               userRoles = new HashSet();
               Enumeration members = roles.members();
               while( members.hasMoreElements() )
               {
                  Principal role = (Principal) members.nextElement();
                  userRoles.add(role);
               }
            }
         }
      }
      return userRoles;
   }

   /** Check that the indicated application domain role is a member of the
    user's assigned roles. This handles the special AnybodyPrincipal and
    NobodyPrincipal independent of the Group implementation.

    @param role , the application domain role required for access
    @param userRoles , the set of roles assigned to the user
    @return true if role is in userRoles or an AnybodyPrincipal instance, false
    if role is a NobodyPrincipal or no a member of userRoles
    */
   protected boolean doesRoleGroupHaveRole(Principal role, Group userRoles)
   {
      // First check that role is not a NobodyPrincipal
      if (role instanceof NobodyPrincipal)
         return false;

      // Check for inclusion in the user's role set
      boolean isMember = userRoles.isMember(role);
      if (isMember == false)
      {   // Check the AnybodyPrincipal special cases
         isMember = (role instanceof AnybodyPrincipal);
      }

      return isMember;
   }

   /** Currently this simply calls defaultLogin() to do a JAAS login using the
    security domain name as the login module configuration name.
    
    * @param principal, the user id to authenticate
    * @param credential, an opaque credential.
    * @return false on failure, true on success.
    */
   private boolean authenticate(Principal principal, Object credential)
   {
      Subject subject = null;
      boolean authenticated = false;

      try
      {
         // Clear any current subject
         SecurityAssociation.setSubject(null);
         // Validate the principal using the login configuration for this domain
         LoginContext lc = defaultLogin(principal, credential);
         subject = lc.getSubject();

         // Set the current subject if login was successful
         if( subject != null )
         {
            SecurityAssociation.setSubject(subject);
            authenticated = true;
            // Build the Subject based DomainInfo cache value
            updateCache(lc, subject, principal, credential);
         }
      }
      catch(LoginException e)
      {
         // Don't log anonymous user failures unless trace level logging is on
         if( principal != null && principal.getName() != null || log.isTraceEnabled() )
            log.debug("Login failure", e);
      }

      return authenticated;
   }

   /** Pass the security info to the login modules configured for
    this security domain using our SecurityAssociationHandler.
    @return The authenticated Subject if successful.
    @exception LoginException throw if login fails for any reason.
    */
   private LoginContext defaultLogin(Principal principal, Object credential)
      throws LoginException
   {
      // We use our internal CallbackHandler to provide the security info
      Object[] securityInfo = {principal, credential};
      try
      {
         setSecurityInfo.invoke(handler, securityInfo);
      }
      catch (Exception e)
      {
         if( log.isTraceEnabled() )
            log.trace("Failed to setSecurityInfo on handler", e);
         throw new LoginException("Failed to setSecurityInfo on handler, msg="
            + e.getMessage());
      }
      Subject subject = new Subject();
      LoginContext lc = new LoginContext(securityDomain, subject, handler);
      lc.login();
      return lc;
   }

   /** Validate the cache credential value against the provided credential
    */
   private boolean validateCache(DomainInfo info, Object credential)
   {
      if( log.isTraceEnabled() )
         log.trace("validateCache, info="+info);

      Object subjectCredential = info.credential;
      boolean isValid = false;
      // Check for a null credential as can be the case for an anonymous user
      if( credential == null || subjectCredential == null )
      {
         // Both credentials must be null
         isValid = (credential == null) && (subjectCredential == null);
      }
      // See if the credential is assignable to the cache value
      else if( subjectCredential.getClass().isAssignableFrom(credential.getClass()) )
      {
        /* Validate the credential by trying Comparable, char[], byte[],
         and finally Object.equals()
         */
         if( subjectCredential instanceof Comparable )
         {
            Comparable c = (Comparable) subjectCredential;
            isValid = c.compareTo(credential) == 0;
         }
         else if( subjectCredential instanceof char[] )
         {
            char[] a1 = (char[]) subjectCredential;
            char[] a2 = (char[]) credential;
            isValid = Arrays.equals(a1, a2);
         }
         else if( subjectCredential instanceof byte[] )
         {
            byte[] a1 = (byte[]) subjectCredential;
            byte[] a2 = (byte[]) credential;
            isValid = Arrays.equals(a1, a2);
         }
         else
         {
            isValid = subjectCredential.equals(credential);
         }
      }
      
      // If the credentials match set the thread's active Subject
      if( isValid )
      {
         SecurityAssociation.setSubject(info.subject);
      }
      
      return isValid;
   }
 
   /** An accessor method that synchronizes access on the domainCache
    to avoid a race condition that can occur when the cache entry expires
    in the presence of multi-threaded access. The allowRefresh flag should
    be true for authentication accesses and false for authorization accesses.
    If it were to be true for an authorization access a previously authenticated
    user could be seen to not have their expected permissions due to a cache
    expiration.

    @param principal, the caller identity whose cached credentials are to
    be accessed.
    @param allowRefresh, a flag indicating if the cache access should flush
    any expired entries.
    */
   private DomainInfo getCacheInfo(Principal principal, boolean allowRefresh)
   {
      if( domainCache == null )
         return null;

      DomainInfo cacheInfo = null;
      synchronized( domainCache )
      {
          if( allowRefresh == true )
            cacheInfo = (DomainInfo) domainCache.get(principal);
          else
            cacheInfo = (DomainInfo) domainCache.peek(principal);
      }
      return cacheInfo;
   }

   private void updateCache(LoginContext lc, Subject subject, Principal principal, Object credential)
   {
      // If we don't have a cache there is nothing to update
      if( domainCache == null )
         return;

      int lifetime = 0;
      if( domainCache instanceof TimedCachePolicy )
      {
         TimedCachePolicy cache = (TimedCachePolicy) domainCache;
         lifetime = cache.getDefaultLifetime();
      }
      DomainInfo info = new DomainInfo(lifetime);
      info.loginCtx = lc;
      info.subject = subject;
      info.credential = credential;

      if( log.isTraceEnabled() )
         log.trace("updateCache, subject="+subject);

     /* Get the Subject callerPrincipal by looking for a Group called
        'CallerPrincipal' and roles by looking for a Group called 'Roles'
      */
      Set subjectGroups = subject.getPrincipals(Group.class);
      Iterator iter = subjectGroups.iterator();
      while( iter.hasNext() )
      {
         Group grp = (Group) iter.next();
         String name = grp.getName();
         if( name.equals("CallerPrincipal") )
         {
            Enumeration members = grp.members();
            if( members.hasMoreElements() )
               info.callerPrincipal = (Principal) members.nextElement();
         }
         else if( name.equals("Roles") )
            info.roles = grp;
      }
      
     /* Handle null principals with no callerPrincipal. This is an indication
        of an user that has not provided any authentication info, but
        has been authenticated by the domain login module stack. Here we look
        for the first non-Group Principal and use that.
      */
      if( principal == null && info.callerPrincipal == null )
      {
         Set subjectPrincipals = subject.getPrincipals(Principal.class);
         iter = subjectPrincipals.iterator();
         while( iter.hasNext() )
         {
            Principal p = (Principal) iter.next();
            if( (p instanceof Group) == false )
               info.callerPrincipal = p;
         }
      }

     /* If the user already exists another login is active. Currently
        only one is allowed so remove the old and insert the new. Synchronize
        on the domainCache to ensure the removal and addition are an atomic
        operation so that getCacheInfo cannot see stale data.
      */
      synchronized( domainCache )
      {
         if( domainCache.peek(principal) != null )
            domainCache.remove(principal);
         domainCache.insert(principal, info);
      }
   }
   
}
