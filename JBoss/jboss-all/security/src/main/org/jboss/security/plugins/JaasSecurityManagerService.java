/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.security.plugins;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.security.Principal;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.Set;
import java.util.List;
import java.beans.PropertyEditorManager;

import javax.management.MalformedObjectNameException;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.naming.CommunicationException;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.Name;
import javax.naming.NameClassPair;
import javax.naming.NameParser;
import javax.naming.NamingEnumeration;
import javax.naming.NamingException;
import javax.naming.OperationNotSupportedException;
import javax.naming.RefAddr;
import javax.naming.Reference;
import javax.naming.StringRefAddr;
import javax.naming.spi.ObjectFactory;
import javax.security.auth.callback.CallbackHandler;

import org.jboss.logging.Logger;
import org.jboss.security.AuthenticationManager;
import org.jboss.security.SecurityAssociation;
import org.jboss.security.SecurityProxyFactory;
import org.jboss.security.SecurityDomain;
import org.jboss.security.propertyeditor.PrincipalEditor;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.util.CachePolicy;
import org.jboss.util.TimedCachePolicy;

/**
 * This is a JMX service which manages JAAS based SecurityManagers.
 * JAAS SecurityManagers are responsible for validating credentials
 * associated with principals. The service defaults to the
 * org.jboss.security.plugins.JaasSecurityManager implementation but
 * this can be changed via the securityManagerClass property.
 *
 * @see JaasSecurityManager
 * @see org.jboss.security.SubjectSecurityManager
 * 
 * @author <a href="on@ibis.odessa.ua">Oleg Nitz</a>
 * @author <a href="rickard@telkel.com">Rickard Oberg</a>
 * @author <a href="mailto:Scott.Stark@jboss.org">Scott Stark</a>
 * @version $Revision: 1.12.4.11 $
 */
public class JaasSecurityManagerService
   extends ServiceMBeanSupport
   implements JaasSecurityManagerServiceMBean
{
   private static final String SECURITY_MGR_PATH = "java:/jaas";
   private static final String DEFAULT_CACHE_POLICY_PATH = "java:/timedCacheFactory";
   /** The log4j interface */
   private static Logger log;
   /** The class that provides the security manager implementation */
   private static String securityMgrClassName = "org.jboss.security.plugins.JaasSecurityManager";
   /** The loaded securityMgrClassName */
   private static Class securityMgrClass;
   /** The JAAS CallbackHandler interface implementation to use */
   private static String callbackHandlerClassName = "org.jboss.security.auth.callback.SecurityAssociationHandler";
   private static Class callbackHandlerClass = org.jboss.security.auth.callback.SecurityAssociationHandler.class;

   /** The location of the security credential cache policy. This is first treated
    as a ObjectFactory location that is capable of returning CachePolicy instances
    on a per security domain basis by appending a '/security-domain-name' string
    to this name when looking up the CachePolicy for a domain. If this fails then
    the location is treated as a single CachePolicy for all security domains.
    */
   private static String cacheJndiName = DEFAULT_CACHE_POLICY_PATH;
   private static int defaultCacheTimeout = 30*60;
   private static int defaultCacheResolution = 60;
   /** The class that provides the SecurityProxyFactory implementation */
   private static String securityProxyFactoryClassName = "org.jboss.security.SubjectSecurityProxyFactory";
   private static Class securityProxyFactoryClass = org.jboss.security.SubjectSecurityProxyFactory.class;
   /** A mapping from security domain name to a SecurityDomainContext object */
   private static Hashtable securityDomainCtxMap = new Hashtable();
   private static NameParser parser;
   private static MBeanServer server;

   static
   {
      // use thread-local principal and credential propagation
      SecurityAssociation.setServer();

      // Get a log interface, required for some statics below
      // can not use instance field inherited from ServiceMBeanSupport
      log = Logger.getLogger(JaasSecurityManagerService.class);

   }

   /** The constructor does nothing as the security manager is created
    on each lookup into java:/jaas/xxx. This is also why all variables
    in this class are static.
    */
   public JaasSecurityManagerService()
   {
   }

   public String getSecurityManagerClassName()
   {
      return securityMgrClassName;
   }
   public void setSecurityManagerClassName(String className)
      throws ClassNotFoundException, ClassCastException
   {
      securityMgrClassName = className;
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      securityMgrClass = loader.loadClass(securityMgrClassName);
      if( AuthenticationManager.class.isAssignableFrom(securityMgrClass) == false )
         throw new ClassCastException(securityMgrClass+" does not implement "+AuthenticationManager.class);
   }
   public String getSecurityProxyFactoryClassName()
   {
      return securityProxyFactoryClassName;
   }
   public void setSecurityProxyFactoryClassName(String className)
      throws ClassNotFoundException
   {
      securityProxyFactoryClassName = className;
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      securityProxyFactoryClass = loader.loadClass(securityProxyFactoryClassName);
   }

   /** Get the default CallbackHandler implementation class name
    *
    * @return The fully qualified classname of the
    */
   public String getCallbackHandlerClassName()
   {
      return callbackHandlerClassName;
   }
   /** Set the default CallbackHandler implementation class name
    * @see javax.security.auth.callback.CallbackHandler
    */
   public void setCallbackHandlerClassName(String className)
      throws ClassNotFoundException
   {
      callbackHandlerClassName = className;
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      callbackHandlerClass = loader.loadClass(callbackHandlerClassName);
   }

   /** Get the jndi name under which the authentication cache policy is found
    */
   public String getAuthenticationCacheJndiName()
   {
      return cacheJndiName;
   }
   /** Set the jndi name under which the authentication cache policy is found
    */
   public void setAuthenticationCacheJndiName(String jndiName)
   {
      cacheJndiName = jndiName;
   }
   /** Get the default timed cache policy timeout.
    @return the default cache timeout in seconds.
    */
   public int getDefaultCacheTimeout()
   {
      return defaultCacheTimeout;
   }
   /** Set the default timed cache policy timeout. This has no affect if the
    AuthenticationCacheJndiName has been changed from the default value.
    @param timeoutInSecs, the cache timeout in seconds.
    */
   public void setDefaultCacheTimeout(int timeoutInSecs)
   {
      defaultCacheTimeout = timeoutInSecs;
   }
   /** Get the default timed cache policy resolution.
    */
   public int getDefaultCacheResolution()
   {
      return defaultCacheResolution;
   }
   /** Set the default timed cache policy resolution. This has no affect if the
    AuthenticationCacheJndiName has been changed from the default value.
    @param resInSecs, resolution of timeouts in seconds.
    */
   public void setDefaultCacheResolution(int resInSecs)
   {
      defaultCacheResolution = resInSecs;
   }

   /** flush the cache policy for the indicated security domain if one exists.
    */
   public void flushAuthenticationCache(String securityDomain)
   {
      CachePolicy cache = getCachePolicy(securityDomain);
      if( cache != null )
      {
         cache.flush();
      }
      else
      {
         log.warn("Failed to find cache policy for securityDomain='"
            + securityDomain + "'");
      }
   }

   /** Flush a principal's authentication cache entry associated with the
    * given securityDomain.
    *
    * @param securityDomain the name of the security domain cache
    * @param user the principal of the user to flush
    */
   public void flushAuthenticationCache(String securityDomain, Principal user)
   {
      CachePolicy cache = getCachePolicy(securityDomain);
      if( cache != null )
      {
         cache.remove(user);
      }
      else
      {
         log.warn("Failed to find cache policy for securityDomain='"
            + securityDomain + "'");
      }
   }

   /** Return the active principals in the indicated security domain auth cache.
    * @param securityDomain the name of the security to lookup the cache for
    * @return List<Principal> of active keys found in the auth cache if
    *    the cache exists and is accessible, null otherwise.
    */
   public List getAuthenticationCachePrincipals(String securityDomain)
   {
      CachePolicy cache = getCachePolicy(securityDomain);
      List validPrincipals = null;
      if( cache instanceof TimedCachePolicy )
      {
         TimedCachePolicy tcache = (TimedCachePolicy) cache;
         validPrincipals = tcache.getValidKeys();
      }
      return validPrincipals;
   }

// Begin SecurityManagerMBean interface methods
   public boolean isValid(String securityDomain, Principal principal, Object credential)
   {
      boolean isValid = false;
      try
      {
         SecurityDomainContext sdc = lookupSecurityDomain(securityDomain);
         isValid = sdc.getSecurityManager().isValid(principal, credential);
      }
      catch(NamingException e)
      {
         log.debug("isValid("+securityDomain+") failed", e);
      }
      return isValid;
   }

   public Principal getPrincipal(String securityDomain, Principal principal)
   {
      Principal realmPrincipal = null;
      try
      {
         SecurityDomainContext sdc = lookupSecurityDomain(securityDomain);
         realmPrincipal = sdc.getRealmMapping().getPrincipal(principal);
      }
      catch(NamingException e)
      {
         log.debug("getPrincipal("+securityDomain+") failed", e);
      }
      return realmPrincipal;
   }

    public boolean doesUserHaveRole(String securityDomain, Principal principal, Set roles)
    {
       boolean doesUserHaveRole = false;
       try
       {
          SecurityDomainContext sdc = lookupSecurityDomain(securityDomain);
          doesUserHaveRole = sdc.getRealmMapping().doesUserHaveRole(principal, roles);
       }
       catch(NamingException e)
       {
          log.debug("doesUserHaveRole("+securityDomain+") failed", e);
       }
       return doesUserHaveRole;
    }

    public Set getUserRoles(String securityDomain, Principal principal)
    {
       Set userRoles = null;
       try
       {
          SecurityDomainContext sdc = lookupSecurityDomain(securityDomain);
          userRoles = sdc.getRealmMapping().getUserRoles(principal);
       }
       catch(NamingException e)
       {
          log.debug("getUserRoles("+securityDomain+") failed", e);
       }
       return userRoles;
    }
// End SecurityManagerMBean interface methods

   protected ObjectName getObjectName(MBeanServer server, ObjectName name)
      throws MalformedObjectNameException
   {
      JaasSecurityManagerService.server = server;
      return name == null ? OBJECT_NAME : name;
   }

   protected void startService() throws Exception
   {
      Context ctx = new InitialContext();
      parser = ctx.getNameParser("");

      /* Create a mapping from the java:/jaas context to a SecurityDomainObjectFactory
       so that any lookup against java:/jaas/domain returns an instance of our
       security manager class.
      */
      RefAddr refAddr = new StringRefAddr("nns", "JSM");
      String factoryName = SecurityDomainObjectFactory.class.getName();
      Reference ref = new Reference("javax.naming.Context", refAddr, factoryName, null);
      ctx.rebind(SECURITY_MGR_PATH, ref);
      log.debug("securityMgrCtxPath="+SECURITY_MGR_PATH);

      refAddr = new StringRefAddr("nns", "JSMCachePolicy");
      factoryName = DefaultCacheObjectFactory.class.getName();
      ref = new Reference("javax.naming.Context", refAddr, factoryName, null);
      ctx.rebind(DEFAULT_CACHE_POLICY_PATH, ref);
      log.debug("cachePolicyCtxPath="+cacheJndiName);

      // Bind the default SecurityProxyFactory instance under java:/SecurityProxyFactory
      SecurityProxyFactory proxyFactory = (SecurityProxyFactory) securityProxyFactoryClass.newInstance();
      ctx.bind("java:/SecurityProxyFactory", proxyFactory);
      log.debug("SecurityProxyFactory="+proxyFactory);

      // Register the Principal property editor
      PropertyEditorManager.registerEditor(Principal.class, PrincipalEditor.class);
   }

   protected void stopService() throws Exception
   {
      InitialContext ic = new InitialContext();
      
      try
      {
         ic.unbind(SECURITY_MGR_PATH);
      }
      catch(CommunicationException e)
      {
         // Do nothing, the naming services is already stopped
      }
      finally
      {
         ic.close();
      }
   }

   /** Register a SecurityDomain implmentation
    */
   public void registerSecurityDomain(String securityDomain, SecurityDomain instance)
   {
      log.info("Added "+securityDomain+", "+instance+" to map");
      CachePolicy authCache = lookupCachePolicy(securityDomain);
      SecurityDomainContext sdc = new SecurityDomainContext(instance, authCache);
      securityDomainCtxMap.put(securityDomain, sdc);
      // See if the security mgr supports an externalized cache policy
      setSecurityDomainCache(instance, authCache);
   }

   /** Access the CachePolicy for the securityDomain.
    * @param securityDomain the name of the security domain
    * @return The CachePolicy if found, null otherwise.
    */
   private static CachePolicy getCachePolicy(String securityDomain)
   {
      CachePolicy cache = null;
      try
      {
         SecurityDomainContext sdc = lookupSecurityDomain(securityDomain);
         if( sdc != null )
            cache = sdc.getAuthenticationCache();
      }
      catch(NamingException e)
      {
         log.debug("getCachePolicy("+securityDomain+") failure", e);
      }
      return cache;
   }

   /** Lookup the authentication CachePolicy object for a security domain. This
    method first treats the cacheJndiName as a ObjectFactory location that is
    capable of returning CachePolicy instances on a per security domain basis
    by appending a '/security-domain-name' string to the cacheJndiName when
    looking up the CachePolicy for a domain. If this fails then the cacheJndiName
    location is treated as a single CachePolicy for all security domains.
    */
   private static CachePolicy lookupCachePolicy(String securityDomain)
   {
      CachePolicy authCache = null;
      String domainCachePath = cacheJndiName + '/' + securityDomain;
      try
      {
         InitialContext iniCtx = new InitialContext();
         authCache = (CachePolicy) iniCtx.lookup(domainCachePath);
      }
      catch(Exception e)
      {
         // Failed, treat the cacheJndiName name as a global CachePolicy binding
         try
         {
            InitialContext iniCtx = new InitialContext();
            authCache = (CachePolicy) iniCtx.lookup(cacheJndiName);
         }
         catch(Exception e2)
         {
            log.warn("Failed to locate auth CachePolicy at: "+cacheJndiName
               + " for securityDomain="+securityDomain);
         }
      }
      return authCache;
   }

   /** Use reflection to attempt to set the authentication cache on the
    * securityMgr argument.
    * @param securityMgr the security manager
    * @param cachePolicy the cache policy implementation
    */
   private static void setSecurityDomainCache(AuthenticationManager securityMgr,
      CachePolicy cachePolicy)
   {
      try
      {
         Class[] setCachePolicyTypes = {CachePolicy.class};
         Method m = securityMgrClass.getMethod("setCachePolicy", setCachePolicyTypes);
         Object[] setCachePolicyArgs = {cachePolicy};
         m.invoke(securityMgr, setCachePolicyArgs);
         log.info("setCachePolicy, c="+setCachePolicyArgs[0]);
      }
      catch(Exception e2)
      {   // No cache policy support, this is ok
         log.debug("setCachePolicy failed", e2);
      }
   }

   /** Lookup or create the SecurityDomainContext for securityDomain.
    * @param securityDomain
    * @return
    * @throws NamingException
    */
   private static SecurityDomainContext lookupSecurityDomain(String securityDomain)
         throws NamingException
   {
      SecurityDomainContext securityDomainCtx = (SecurityDomainContext) securityDomainCtxMap.get(securityDomain);
      if( securityDomainCtx == null )
      {
         securityDomainCtx = newSecurityDomainCtx(securityDomain);
         securityDomainCtxMap.put(securityDomain, securityDomainCtx);
         log.info("Added "+securityDomain+", "+securityDomainCtx+" to map");
      }
      return securityDomainCtx;
   }

   /** Create a new SecurityDomainContext for securityDomain.
    * @param securityDomain
    * @return
    * @throws NamingException
    */
   private static SecurityDomainContext newSecurityDomainCtx(String securityDomain)
      throws NamingException
   {
      SecurityDomainContext sdc = null;
      try
      {
         // Create instance of securityMgrClass
         Class[] parameterTypes = {String.class, CallbackHandler.class};
         Constructor ctor = securityMgrClass.getConstructor(parameterTypes);
         CallbackHandler handler = (CallbackHandler) callbackHandlerClass.newInstance();
         Object[] args = {securityDomain, handler};
         AuthenticationManager securityMgr = (AuthenticationManager) ctor.newInstance(args);
         log.info("Created securityMgr="+securityMgr);
         CachePolicy cachePolicy = lookupCachePolicy(securityDomain);
         sdc = new SecurityDomainContext(securityMgr, cachePolicy);
         // See if the security mgr supports an externalized cache policy
         setSecurityDomainCache(securityMgr, cachePolicy);
      }
      catch(Exception e2)
      {
         log.error("Failed to create sec mgr", e2);
         throw new NamingException("Failed to create sec mgr:"+e2.getMessage());
      }
      return sdc;
   }

   // java:/jaas context ObjectFactory implementation
   
   public static class SecurityDomainObjectFactory
      implements InvocationHandler, ObjectFactory
   {
      /** Object factory implementation. This method returns a Context proxy
       that is only able to handle a lookup operation for an atomic name of
       a security domain.
      */
      public Object getObjectInstance(Object obj, Name name, Context nameCtx,
         Hashtable environment)
         throws Exception
      {
         ClassLoader loader = Thread.currentThread().getContextClassLoader();
         Class[] interfaces = {Context.class};
         Context ctx = (Context) Proxy.newProxyInstance(loader, interfaces, this);
         return ctx;
      }


      /** This is the InvocationHandler callback for the Context interface that
       was created by out getObjectInstance() method. We handle the java:/jaas/domain
       level operations here.
       */
      public Object invoke(Object obj, Method method, Object[] args) throws Throwable
      {
         String methodName = method.getName();
         if( methodName.equals("toString") == true )
            return SECURITY_MGR_PATH + " Context proxy";
         if( methodName.equals("list") == true )
            return new DomainEnumeration(securityDomainCtxMap.keys(), securityDomainCtxMap);

         if( methodName.equals("lookup") == false )
            throw new OperationNotSupportedException("Only lookup is supported, op="+method);
         String securityDomain = null;
         Name name = null;
         if( args[0] instanceof String )
            name = parser.parse((String) args[0]);
         else
           name = (Name)args[0];
         securityDomain = name.get(0);
         SecurityDomainContext securityDomainCtx = lookupSecurityDomain(securityDomain);
         Object binding = securityDomainCtx.getSecurityManager();
         // Look for requests against the security domain context
         if( name.size() == 2 )
         {
            String request = name.get(1);
            binding = securityDomainCtx.lookup(request);
         }
         return binding;
      }
   }
   static class DomainEnumeration implements NamingEnumeration
   {
      Enumeration domains;
      Hashtable ctxMap;
      DomainEnumeration(Enumeration domains, Hashtable ctxMap)
      {
         this.domains = domains;
         this.ctxMap = ctxMap;
      }

      public void close()
      {
      }
      public boolean hasMoreElements()
      {
         return domains.hasMoreElements();
      }
      public boolean hasMore()
      {
         return domains.hasMoreElements();
      }
      public Object next()
      {
         String name = (String) domains.nextElement();
         Object value = ctxMap.get(name);
         String className = value.getClass().getName();
         NameClassPair pair = new NameClassPair(name, className);
         return pair;
      }
      public Object nextElement()
      {
         return domains.nextElement();
      }
   }

   /** java:/timedCacheFactory ObjectFactory implementation
    */
   public static class DefaultCacheObjectFactory implements InvocationHandler, ObjectFactory
   {
      /** Object factory implementation. This method returns a Context proxy
       that is only able to handle a lookup operation for an atomic name of
       a security domain.
      */
      public Object getObjectInstance(Object obj, Name name, Context nameCtx, Hashtable environment)
         throws Exception
      {
         ClassLoader loader = Thread.currentThread().getContextClassLoader();
         Class[] interfaces = {Context.class};
         Context ctx = (Context) Proxy.newProxyInstance(loader, interfaces, this);
         return ctx;
      }
      /** This is the InvocationHandler callback for the Context interface that
       was created by out getObjectInstance() method. All this does is create
       a new TimedCache instance.
       */
      public Object invoke(Object obj, Method method, Object[] args) throws Throwable
      {
         TimedCachePolicy cachePolicy = new TimedCachePolicy(defaultCacheTimeout,
            true, defaultCacheResolution);
         cachePolicy.create();
         cachePolicy.start();
         return cachePolicy;
      }
   }
}
