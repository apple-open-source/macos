/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.naming;

import java.io.InputStream;
import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.UndeclaredThrowableException;
import java.net.UnknownHostException;
import java.util.Collections;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Hashtable;
import java.util.Map;
import java.util.Properties;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.RefAddr;
import javax.naming.Reference;
import javax.naming.StringRefAddr;

import org.jboss.invocation.Invocation;
import org.jboss.invocation.MarshalledInvocation;
import org.jboss.system.ServiceMBeanSupport;
import org.jnp.interfaces.Naming;
import org.jnp.server.Main;

/**
 * A JBoss service that starts the jnp JNDI server.
 *
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:Scott.Stark@jboss.org">Scott Stark</a>.
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @version $Revision: 1.34.2.7 $
 *
 * @jmx:mbean name="jboss:service=Naming"
 *            extends="org.jboss.system.ServiceMBean, org.jnp.server.MainMBean"
 **/
public class NamingService
   extends ServiceMBeanSupport
   implements NamingServiceMBean
{
   private Main naming;
   private Map marshalledInvocationMapping = new HashMap();

   public NamingService()
   {
      naming = new Main(this.getClass().getName());
   }

   public void setPort(int port)
   {
      naming.setPort(port);
   }

   public int getPort()
   {
      return naming.getPort();
   }

   public void setRmiPort(int port)
   {
      naming.setRmiPort(port);
   }

   public int getRmiPort()
   {
      return naming.getRmiPort();
   }

   public String getBindAddress()
   {
      return naming.getBindAddress();
   }

   public void setBindAddress(String host) throws UnknownHostException
   {
      naming.setBindAddress(host);
   }

   public String getRmiBindAddress()
   {
      return naming.getRmiBindAddress();
   }

   public void setRmiBindAddress(String host) throws UnknownHostException
   {
      naming.setRmiBindAddress(host);
   }

   public int getBacklog()
   {
      return naming.getBacklog();
   }

   public void setBacklog(int backlog)
   {
      naming.setBacklog(backlog);
   }

   public boolean getInstallGlobalService()
   {
      return naming.getInstallGlobalService();
   }
   public void setInstallGlobalService(boolean flag)
   {
      naming.setInstallGlobalService(flag);
   }

   public String getClientSocketFactory()
   {
      return naming.getClientSocketFactory();
   }

   public void setClientSocketFactory(String factoryClassName)
      throws ClassNotFoundException, InstantiationException, IllegalAccessException
   {
      naming.setClientSocketFactory(factoryClassName);
   }

   public String getServerSocketFactory()
   {
      return naming.getServerSocketFactory();
   }

   public void setServerSocketFactory(String factoryClassName)
      throws ClassNotFoundException, InstantiationException, IllegalAccessException
   {
      naming.setServerSocketFactory(factoryClassName);
   }

   public void setJNPServerSocketFactory(String factoryClassName)
      throws ClassNotFoundException, InstantiationException, IllegalAccessException
   {
      naming.setJNPServerSocketFactory(factoryClassName);
   }

   protected void startService()
      throws Exception
   {
      boolean debug = log.isDebugEnabled();

      // Read jndi.properties into system properties
      // RO: this is necessary because some components (=Tomcat servlets) use a
      // buggy classloader that disallows finding the resource properly
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      InputStream is = loader.getResourceAsStream("jndi.properties");
      Properties props = new Properties();
      props.load(is);

      for (Enumeration keys = props.propertyNames(); keys.hasMoreElements(); )
      {
         String key = (String) keys.nextElement();
         String value = props.getProperty(key);
         if (debug) {
            log.debug("System.setProperty, key="+key+", value="+value);
         }
         System.setProperty(key, value);
      }
      naming.start();

      /* Create a default InitialContext and dump out its env to show what properties
         were used in its creation. If we find a Context.PROVIDER_URL property
         issue a warning as this means JNDI lookups are going through RMI.
      */
      InitialContext iniCtx = new InitialContext();
      Hashtable env = iniCtx.getEnvironment();
      if (debug)
         log.debug("InitialContext Environment:");
      String providerURL = null;
      for (Enumeration keys = env.keys(); keys.hasMoreElements(); )
      {
         String key = (String) keys.nextElement();
         String value = (String) env.get(key);
         if (debug) {
            log.debug("key="+key+", value="+value);
         }
         if( key.equals(Context.PROVIDER_URL) )
            providerURL = value;
      }
      // Warn if there was a Context.PROVIDER_URL
      if( providerURL != null )
         log.warn("Context.PROVIDER_URL in server jndi.properties, url="+providerURL);

      /* Bind an ObjectFactory to "java:comp" so that "java:comp/env" lookups
         produce a unique context for each thread contexxt ClassLoader that
         performs the lookup.
      */
      ClassLoader topLoader = Thread.currentThread().getContextClassLoader();
      ENCFactory.setTopClassLoader(topLoader);
      RefAddr refAddr = new StringRefAddr("nns", "ENC");
      Reference envRef = new Reference("javax.naming.Context", refAddr, ENCFactory.class.getName(), null);
      Context ctx = (Context)iniCtx.lookup("java:");
      ctx.rebind("comp", envRef);
      if (log.isInfoEnabled())
         log.info("Listening on port "+naming.getPort());

      // Build the Naming interface method map
      HashMap tmpMap = new HashMap(13);
      Method[] methods = Naming.class.getMethods();
      for(int m = 0; m < methods.length; m ++)
      {
         Method method = methods[m];
         Long hash = new Long(MarshalledInvocation.calculateHash(method));
         tmpMap.put(hash, method);
      }
      marshalledInvocationMapping = Collections.unmodifiableMap(tmpMap);
   }

   protected void stopService()
      throws Exception
   {
      naming.stop();
      log.debug("JNP server stopped");
   }

   /**
    * The <code>getNamingServer</code> method makes this class
    * extensible, but it is a hack.  The NamingServer should be
    * exposed directly as an xmbean, and the startup logic put in
    * either an interceptor, the main class itself, or an auxilliary
    * mbean (for the enc).
    *
    * @return a <code>Main</code> value
    */
   protected Main getNamingServer()
   {
      return naming;
   } // end of main()


   /** Expose the Naming service interface mapping as a read-only attribute
    *
    * @jmx:managed-attribute
    *
    * @return A Map<Long hash, Method> of the Naming interface
    */
   public Map getMethodMap()
   {
      return marshalledInvocationMapping;
   }

   /** Expose the Naming service via JMX to invokers.
    *
    * @jmx:managed-operation
    *
    * @param invocation    A pointer to the invocation object
    * @return              Return value of method invocation.
    *
    * @throws Exception    Failed to invoke method.
    */
   public Object invoke(Invocation invocation) throws Exception
   {
      Naming theServer = naming.getServer();
      // Set the method hash to Method mapping
      if (invocation instanceof MarshalledInvocation)
      {
         MarshalledInvocation mi = (MarshalledInvocation) invocation;
         mi.setMethodMap(marshalledInvocationMapping);
      }
      // Invoke the Naming method via reflection
      Method method = invocation.getMethod();
      Object[] args = invocation.getArguments();
      Object value = null;
      try
      {
         value = method.invoke(theServer, args);
      }
      catch(InvocationTargetException e)
      {
         Throwable t = e.getTargetException();
         if( t instanceof Exception )
            throw (Exception) t;
         else
            throw new UndeclaredThrowableException(t, method.toString());
      }

      return value;
   }
}
