/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.security.srp;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Proxy;
import java.lang.reflect.UndeclaredThrowableException;
import java.rmi.server.RMIClientSocketFactory;
import java.rmi.server.RMIServerSocketFactory;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import javax.naming.InitialContext;
import javax.naming.Name;
import javax.naming.NamingException;

import org.jboss.invocation.Invocation;
import org.jboss.invocation.MarshalledInvocation;
import org.jboss.naming.NonSerializableFactory;
import org.jboss.security.srp.SRPRemoteServer;
import org.jboss.security.srp.SRPServerListener;
import org.jboss.security.srp.SRPServerInterface;
import org.jboss.security.srp.SRPServerSession;
import org.jboss.security.srp.SRPVerifierStore;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.util.CachePolicy;
import org.jboss.util.TimedCachePolicy;

/** The JMX mbean interface for the SRP service. This mbean sets up an
 RMI implementation of the 'Secure Remote Password' cryptographic authentication
 system described in RFC2945.
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.9.4.4 $
 */
public class SRPService extends ServiceMBeanSupport
   implements SRPServiceMBean, SRPServerListener
{
   /**
    * @supplierRole RMI Access
    * @supplierCardinality 1
    * @clientCardinality 1
    * @clientRole service mangement
    */
   private SRPRemoteServer server;
   private int serverPort = 10099;

   /**
    * @supplierRole password store
    * @supplierCardinality 1
    * @clientRole configures
    */
   private SRPVerifierStore verifierStore;
   private String verifierSourceJndiName = "srp/DefaultVerifierSource";
   private String serverJndiName = "srp/SRPServerInterface";
   private String cacheJndiName = "srp/AuthenticationCache";
   private CachePolicy cachePolicy;
   private int cacheTimeout = 1800;
   private int cacheResolution = 60;
   /** A flag indicating if a successful user auth for an existing session
    should overwrite the current session.
    */
   private boolean overwriteSessions;
   /** A flag indicating if an aux challenge must be presented in verify */
   private boolean requireAuxChallenge;
   /** An optional custom client socket factory */
   private RMIClientSocketFactory clientSocketFactory;
   /** An optional custom server socket factory */
   private RMIServerSocketFactory serverSocketFactory;
   /** The class name of the optional custom client socket factory */
   private String clientSocketFactoryName;
   /** The class name of the optional custom server socket factory */
   private String serverSocketFactoryName;
   /** A <Long,Method> mapping of the SRPRemoteServerInterface */
   private Map marshalledInvocationMapping = new HashMap();

// --- Begin SRPServiceMBean interface methods
   /** Get the jndi name for the SRPVerifierSource implementation binding.
    */
   public String getVerifierSourceJndiName()
   {
      return verifierSourceJndiName;
   }
   /** set the jndi name for the SRPVerifierSource implementation binding.
    */
   public void setVerifierSourceJndiName(String jndiName)
   {
      this.verifierSourceJndiName = jndiName;
   }
   /** Get the jndi name under which the SRPServerInterface proxy should be bound
    */
   public String getJndiName()
   {
      return serverJndiName;
   }
   /** Set the jndi name under which the SRPServerInterface proxy should be bound
    */
   public void setJndiName(String jndiName)
   {
      this.serverJndiName = jndiName;
   }
   /** Get the jndi name under which the SRPServerInterface proxy should be bound
    */
   public String getAuthenticationCacheJndiName()
   {
      return cacheJndiName;
   }
   /** Set the jndi name under which the SRPServerInterface proxy should be bound
    */
   public void setAuthenticationCacheJndiName(String jndiName)
   {
      this.cacheJndiName = jndiName;
   }
   
   /** Get the auth cache timeout period in seconds
    */
   public int getAuthenticationCacheTimeout()
   {
      return cacheTimeout;
   }
   /** Set the auth cache timeout period in seconds
    */
   public void setAuthenticationCacheTimeout(int timeoutInSecs)
   {
      this.cacheTimeout = timeoutInSecs;
   }
   /** Get the auth cache resolution period in seconds
    */
   public int getAuthenticationCacheResolution()
   {
      return cacheResolution;
   }
   /** Set the auth cache resolution period in seconds
    */
   public void setAuthenticationCacheResolution(int resInSecs)
   {
      this.cacheResolution = resInSecs;
   }

   public boolean getRequireAuxChallenge()
   {
      return this.requireAuxChallenge;
   }
   public void setRequireAuxChallenge(boolean flag)
   {
      this.requireAuxChallenge = flag;
   }

   public boolean getOverwriteSessions()
   {
      return this.overwriteSessions;
   }
   public void setOverwriteSessions(boolean flag)
   {
      this.overwriteSessions = flag;
   }

   /** Get the RMIClientSocketFactory implementation class. If null the default
    RMI client socket factory implementation is used.
    */
   public String getClientSocketFactory()
   {
      return serverSocketFactoryName;
   }
   /** Set the RMIClientSocketFactory implementation class. If null the default
    RMI client socket factory implementation is used.
    */
   public void setClientSocketFactory(String factoryClassName)
      throws ClassNotFoundException, InstantiationException, IllegalAccessException
   {
      this.clientSocketFactoryName = factoryClassName;
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      Class clazz = loader.loadClass(clientSocketFactoryName);
      clientSocketFactory = (RMIClientSocketFactory) clazz.newInstance();
   }
   
   /** Get the RMIServerSocketFactory implementation class. If null the default
    RMI server socket factory implementation is used.
    */
   public String getServerSocketFactory()
   {
      return serverSocketFactoryName;
   }
   /** Set the RMIServerSocketFactory implementation class. If null the default
    RMI server socket factory implementation is used.
    */
   public void setServerSocketFactory(String factoryClassName)
      throws ClassNotFoundException, InstantiationException, IllegalAccessException
   {
      this.serverSocketFactoryName = factoryClassName;
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      Class clazz = loader.loadClass(serverSocketFactoryName);
      serverSocketFactory = (RMIServerSocketFactory) clazz.newInstance();
   }
   /** Get the RMI port for the SRPServerInterface
    */
   public int getServerPort()
   {
      return serverPort;
   }
   /** Get the RMI port for the SRPServerInterface
    */
   public void setServerPort(int serverPort)
   {
      this.serverPort = serverPort;
   }
// --- End SRPServiceMBean interface methods
   
   /** Called when username has sucessfully completed the SRP login. This
    places the SRP session into the credential cache using a
    SimplePrincipal based on the username as the key.
    */
   public void verifiedUser(SRPSessionKey key, SRPServerSession session)
   {
      try
      {
         synchronized( cachePolicy )
         {
            // We only insert a principal if there is no current entry.
            if( cachePolicy.peek(key) == null )
            {
               cachePolicy.insert(key, session);
               log.trace("Cached SRP session for user="+key);
            }
            else if( overwriteSessions )
            {
               cachePolicy.remove(key);
               cachePolicy.insert(key, session);
               log.trace("Replaced SRP session for user="+key);
            }
            else
            {
               log.debug("Ignoring SRP session due to existing session for user="+key);
            }
         }
      }
      catch(Exception e)
      {
         log.error("Failed to update SRP cache for user="+key, e);
      }
   }
   public void closedUserSession(SRPSessionKey key)
   {
      try
      {
         synchronized( cachePolicy )
         {
            // We only insert a principal if there is no current entry.
            if( cachePolicy.peek(key) == null )
            {
               log.warn("No SRP session found for user="+key);
            }
            cachePolicy.remove(key);
         }
      }
      catch(Exception e)
      {
         log.error("Failed to update SRP cache for user="+key, e);
      }
   }

   public String getName()
   {
      return "SRPService";
   }

   public Object invoke(Invocation invocation) throws Exception
   {
      // Set the method hash to Method mapping
      if (invocation instanceof MarshalledInvocation)
      {
         MarshalledInvocation mi = (MarshalledInvocation) invocation;
         mi.setMethodMap(marshalledInvocationMapping);
      }
      // Invoke the SRPRemoteServer method via reflection
      Method method = invocation.getMethod();
      Object[] args = invocation.getArguments();
      Object value = null;
      try
      {
         value = method.invoke(server, args);
      }
      catch(InvocationTargetException e)
      {
         Throwable t = e.getTargetException();
         if( t instanceof Exception )
            throw (Exception) e;
         else
            throw new UndeclaredThrowableException(t, method.toString());
      }

      return value;
   }

   protected void startService() throws Exception
   {
      loadStore();
      server = new SRPRemoteServer(verifierStore, serverPort,
      clientSocketFactory, serverSocketFactory);
      server.addSRPServerListener(this);
      server.setRequireAuxChallenge(this.requireAuxChallenge);

      // Bind a proxy to the SRPRemoteServer into jndi
      SRPServerProxy proxyHandler = new SRPServerProxy(server);
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      Class[] interfaces = {SRPServerInterface.class};
      Object proxy = Proxy.newProxyInstance(loader, interfaces, proxyHandler);
      InitialContext ctx = new InitialContext();
      Name name = ctx.getNameParser("").parse(serverJndiName);
      if( name.size() > 1 )
      {
         int size = name.size();
         Name prefix = name.getPrefix(size-1);
         org.jboss.naming.Util.createSubcontext(ctx, prefix);
      }
      ctx.rebind(serverJndiName, proxy);
      log.debug("Bound SRPServerProxy at "+serverJndiName);

      // First check for an existing CachePolicy binding
      try
      {
         cachePolicy = (CachePolicy) ctx.lookup(cacheJndiName);
         log.debug("Found AuthenticationCache at: "+cacheJndiName);
      }
      catch(Exception e)
      {
         log.trace("Failed to find existing cache at: "+cacheJndiName, e);
         // Not found, default to a TimedCachePolicy
         cachePolicy = new TimedCachePolicy(cacheTimeout, true, cacheResolution);
         cachePolicy.create();
         cachePolicy.start();
         // Bind a reference to store using NonSerializableFactory as the ObjectFactory
         name = ctx.getNameParser("").parse(cacheJndiName);
         NonSerializableFactory.rebind(name, cachePolicy, true);
         log.debug("Bound AuthenticationCache at "+cacheJndiName);
      }

      // Build the SRPRemoteServerInterface method map
      HashMap tmpMap = new HashMap(13);
      Method[] methods = SRPRemoteServerInterface.class.getMethods();
      for(int m = 0; m < methods.length; m ++)
      {
         Method method = methods[m];
         Long hash = new Long(MarshalledInvocation.calculateHash(method));
         tmpMap.put(hash, method);
      }
      marshalledInvocationMapping = Collections.unmodifiableMap(tmpMap);
   }

   protected void stopService() throws Exception
   {
      // Bind a reference to store using NonSerializableFactory as the ObjectFactory
      InitialContext ctx = new InitialContext();
      ctx.unbind(serverJndiName);
      log.debug("Unbound SRPServerProxy at "+serverJndiName);
      NonSerializableFactory.unbind(cacheJndiName);
      ctx.unbind(cacheJndiName);
      log.debug("Unbound AuthenticationCache at "+cacheJndiName);
   }

   private void loadStore() throws NamingException
   {
      InitialContext ctx = new InitialContext();
      // Get the SRPVerifierStore implementation
      verifierStore = (SRPVerifierStore) ctx.lookup(verifierSourceJndiName);
      if( server != null )
      {
         server.setVerifierStore(verifierStore);
      }
   }
   
}
