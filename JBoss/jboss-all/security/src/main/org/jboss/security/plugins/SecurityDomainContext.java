package org.jboss.security.plugins;

import javax.naming.InvalidNameException;
import javax.naming.NamingException;
import javax.security.auth.Subject;

import org.jboss.security.RealmMapping;
import org.jboss.security.AuthenticationManager;
import org.jboss.security.SubjectSecurityManager;
import org.jboss.util.CachePolicy;

/** An encapsulation of the JNDI security context infomation
 *
 * @author  Scott.Stark@jboss.org
 * @version 
 */
public class SecurityDomainContext
{
   static final String ACTIVE_SUBJECT = "subject";
   static final String AUTHENTICATION_MGR = "securityMgr";
   static final String AUTORIZATION_MGR = "realmMapping";
   static final String AUTH_CACHE = "authenticationCache";

   AuthenticationManager securityMgr;
   CachePolicy authenticationCache;

   /** Creates new SecurityDomainContextHandler */
   public SecurityDomainContext(AuthenticationManager securityMgr, CachePolicy authenticationCache)
   {
      this.securityMgr = securityMgr;
      this.authenticationCache = authenticationCache;
   }

   public Object lookup(String name) throws NamingException
   {
      Object binding = null;
      if( name == null || name.length() == 0 )
         throw new InvalidNameException("name cannot be null or empty");

      if( name.equals(ACTIVE_SUBJECT) )
         binding = getSubject();
      else if( name.equals(AUTHENTICATION_MGR) )
         binding = securityMgr;
      else if( name.equals(AUTORIZATION_MGR) )
         binding = getRealmMapping();
      else if( name.equals(AUTH_CACHE) )
         binding = authenticationCache;
      return binding;
   }
   public Subject getSubject()
   {
      Subject subject = null;
      if( securityMgr instanceof SubjectSecurityManager )
      {
         subject = ((SubjectSecurityManager)securityMgr).getActiveSubject();
      }
      return subject;
   }
   public AuthenticationManager getSecurityManager()
   {
      return securityMgr;
   }
   public RealmMapping getRealmMapping()
   {
      RealmMapping realmMapping = null;
      if( securityMgr instanceof RealmMapping )
      {
         realmMapping = (RealmMapping)securityMgr;
      }
      return realmMapping;
   }
   public CachePolicy getAuthenticationCache()
   {
      return authenticationCache;
   }

}
