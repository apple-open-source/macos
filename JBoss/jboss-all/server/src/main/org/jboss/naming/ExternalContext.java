/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.naming;

import java.io.IOException;
import java.io.InputStream;
import java.io.Serializable;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

import java.net.URL;
import java.util.Hashtable;
import java.util.Properties;

import javax.management.MBeanServer;
import javax.management.ObjectName;

import javax.naming.CompositeName;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.Name;
import javax.naming.NamingException;
import javax.naming.RefAddr;
import javax.naming.Reference;
import javax.naming.Referenceable;
import javax.naming.StringRefAddr;
import javax.naming.ldap.Control;
import javax.naming.spi.ObjectFactory;

import org.jboss.system.ServiceMBeanSupport;

/**
 * A MBean that binds an arbitrary InitialContext into the JBoss default
 * InitialContext as a Reference. If RemoteAccess is enabled, the reference
 * is a Serializable object that is capable of creating the InitialContext
 * remotely. If RemoteAccess if false, the reference is to a nonserializable object
 * that can only be used from within this VM.
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean"
 * 
 * @see org.jboss.naming.NonSerializableFactory
 * 
 * @version <tt>$Revision: 1.14 $</tt>
 * @author  Scott.Stark@jboss.org
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class ExternalContext
   extends ServiceMBeanSupport
   implements ExternalContextMBean
{
   private boolean remoteAccess;
   private SerializableInitialContext contextInfo = new SerializableInitialContext();

   /**
    * No-args constructor for JMX.
    */
   public ExternalContext()
   {
      super();
   }

   public ExternalContext(String jndiName, String contextPropsURL)
      throws IOException, NamingException
   {   
      setJndiName(jndiName);
      setPropertiesURL(contextPropsURL);
   }

   /**
    * Set the jndi name under which the external context is bound.
    *
    * @jmx:managed-attribute
    */
   public String getJndiName()
   {
      return contextInfo.getJndiName();
   }

   /**
    * Set the jndi name under which the external context is bound.
    *
    * @jmx:managed-attribute
    */
   public void setJndiName(String jndiName) throws NamingException
   {
      contextInfo.setJndiName(jndiName);
      if( super.getState() == STARTED )
      {
         unbind(jndiName);
         try
         {
            rebind();
         }
         catch(Exception e)
         {
            NamingException ne = new NamingException("Failed to update jndiName");
            ne.setRootCause(e);
            throw ne;
         }
      }
   }

   /**
    * @jmx:managed-attribute
    */
   public boolean getRemoteAccess()
   {
      return remoteAccess;
   }
   
   /**
    * @jmx:managed-attribute
    */
   public void setRemoteAccess(final boolean remoteAccess)
   {
      this.remoteAccess = remoteAccess;
   }
   
   /**
    * @jmx:managed-attribute
    */
   public boolean getCacheContext()
   {
      return contextInfo.getCacheContext();
   }
   
   /**
    * @jmx:managed-attribute
    */
   public void setCacheContext(boolean cacheContext)
   {
      contextInfo.setCacheContext(cacheContext);
   }

   /**
    * Get the class name of the InitialContext implementation to
    * use. Should be one of:
    * <ul>
    *   <li>javax.naming.InitialContext
    *   <li>javax.naming.directory.InitialDirContext
    *   <li>javax.naming.ldap.InitialLdapContext
    * </ul>
    *
    * @jmx:managed-attribute
    * 
    * @return the classname of the InitialContext to use
    */
   public String getInitialContext()
   {
      return contextInfo.getInitialContext();
   }

   /**
    * Set the class name of the InitialContext implementation to
    * use. Should be one of:
    * <ul>
    *   <li>javax.naming.InitialContext
    *   <li>javax.naming.directory.InitialDirContext
    *   <li>javax.naming.ldap.InitialLdapContext
    * </ul>
    *
    * @jmx:managed-attribute
    *
    * @param contextClass, the classname of the InitialContext to use
    */
   public void setInitialContext(String className) throws ClassNotFoundException
   {
      contextInfo.loadClass(className);
   }

   /**
    * Set the InitialContex class environment properties from the given URL.
    *
    * @jmx:managed-attribute
    */
   public void setPropertiesURL(String contextPropsURL) throws IOException
   {
      contextInfo.loadProperties(contextPropsURL);
   }

   /**
    * Set the InitialContex class environment properties.
    *
    * @jmx:managed-attribute
    */
   public void setProperties(final Properties props) throws IOException
   {
      contextInfo.setProperties(props);
   }

   /**
    * Get the InitialContex class environment properties.
    *
    * @jmx:managed-attribute
    */
   public Properties getProperties() throws IOException
   {
      return contextInfo.getProperties();
   }
   
   /**
    * Start the service by binding the external context into the
    * JBoss InitialContext.
    */
   protected void startService() throws Exception
   {
      rebind();
   }

   /**
    * Stop the service by unbinding the external context into the
    * JBoss InitialContext.
    */
   protected void stopService() throws Exception
   {
      if( contextInfo.getCacheContext() )
         unbind(contextInfo.getJndiName());
   }

   private static Context createContext(Context rootContext, Name name) throws NamingException
   {
      Context subctx = rootContext;
      for(int n = 0; n < name.size(); n ++)
      {
         String atom = name.get(n);
         try
         {
            Object obj = subctx.lookup(atom);
            subctx = (Context) obj;
         }
         catch(NamingException e)
         {
            // No binding exists, create a subcontext
            subctx = subctx.createSubcontext(atom);
         }
      }

      return subctx;
   }

   private void rebind() throws Exception
   {
      boolean debug = log.isDebugEnabled();
      Context ctx = contextInfo.newContext();
      Context rootCtx = (Context) new InitialContext();
      if (debug)
         log.debug("ctx="+ctx+", env="+ctx.getEnvironment());
      
      // Get the parent context into which we are to bind
      String jndiName = contextInfo.getJndiName();
      Name fullName = rootCtx.getNameParser("").parse(jndiName);
      if (debug)
         log.debug("fullName="+fullName);
      
      Name parentName = fullName;
      if( fullName.size() > 1 )
         parentName = fullName.getPrefix(fullName.size()-1);
      else
         parentName = new CompositeName();
      
      if (debug)
         log.debug("parentName="+parentName);
      
      Context parentCtx = createContext(rootCtx, parentName);
      if (debug)
         log.debug("parentCtx="+parentCtx);
      
      Name atomName = fullName.getSuffix(fullName.size()-1);
      String atom = atomName.get(0);
      boolean cacheContext = contextInfo.getCacheContext();
      
      if( remoteAccess == true )
      {
         // Bind contextInfo as a Referenceable
         parentCtx.rebind(atom, contextInfo);

         // Cache the context using NonSerializableFactory to avoid creating
         // more than one context for in VM lookups
         if( cacheContext == true )
         {
            // If cacheContext is true we need to wrap the Context in a
            // proxy that allows the user to issue close on the lookup
            // Context without closing the inmemory Context.
            ctx = CachedContext.createProxyContext(ctx);
            NonSerializableFactory.rebind(jndiName, ctx);
         }
      }
      else if( cacheContext == true )
      {
         // Bind a reference to the extern context using
         // NonSerializableFactory as the ObjectFactory. The Context must
         // be wrapped in a proxy that allows the user to issue close on the
         // lookup Context without closing the inmemory Context.

         Context proxyCtx = CachedContext.createProxyContext(ctx);
         NonSerializableFactory.rebind(rootCtx, jndiName, proxyCtx);
      }
      else
      {
         // Bind the contextInfo so that each lookup results in the creation
         // of a new Context object. The returned Context must be closed
         // by the user to prevent resource leaks.

         parentCtx.rebind(atom, contextInfo);
      }
   }

   private void unbind(String jndiName)
   {
      try
      {
         Context rootCtx = new InitialContext();
         Context ctx = (Context) rootCtx.lookup(jndiName);
         if( ctx != null )
            ctx.close();
         rootCtx.unbind(jndiName);
         NonSerializableFactory.unbind(jndiName);
      }
      catch(NamingException e)
      {
         log.error("unbind failed", e);
      }
   }

   /**
    * The external InitialContext information class. It acts as the
    * RefAddr and ObjectFactory for the external IntialContext and can
    * be marshalled to a remote client.
    */
   public static class SerializableInitialContext
      extends RefAddr
      implements Referenceable, Serializable, ObjectFactory
   {
      private static final long serialVersionUID = -6512260531255770463L;
      private String jndiName;
      private Class contextClass = javax.naming.InitialContext.class;
      private Properties contextProps;
      private boolean cacheContext = true;
      private transient Context initialContext;

      public SerializableInitialContext()
      {
         this("SerializableInitialContext");
      }
      
      public SerializableInitialContext(String addrType)
      {
         super(addrType);
      }

      public String getJndiName()
      {
         return jndiName;
      }
      
      public void setJndiName(final String jndiName)
      {
         this.jndiName = jndiName;
      }
      
      public boolean getCacheContext()
      {
         return cacheContext;
      }
      
      public void setCacheContext(final boolean cacheContext)
      {
         this.cacheContext = cacheContext;
      }
      
      public String getInitialContext()
      {
         return contextClass.getName();
      }
      
      public void loadClass(String className) throws ClassNotFoundException
      {
         ClassLoader loader = Thread.currentThread().getContextClassLoader();
         contextClass = loader.loadClass(className);
      }

      public void setProperties(final Properties props)
      {
         contextProps = props;
      }

      public Properties getProperties()
      {
         return contextProps;
      }
      
      public void loadProperties(String contextPropsURL) throws IOException
      {
         InputStream is = null;
         contextProps = new Properties();

         // See if this is a URL we can load
         try
         {
            URL url = new URL(contextPropsURL);
            is = url.openStream();
            contextProps.load(is);
            return;
         }
         catch (IOException e)
         {   // Failed, try to locate a classpath resource below
            is = null;
         }

         is = Thread.currentThread().getContextClassLoader().getResourceAsStream(contextPropsURL);
         if( is == null )
         {
            throw new IOException("Failed to locate context props as URL or resource:"+contextPropsURL);
         }
         contextProps.load(is);
      }

      Context newContext() throws Exception
      {
         // First check the NonSerializableFactory cache
         initialContext = (Context) NonSerializableFactory.lookup(jndiName);
         // Create the context from the contextClass and contextProps
         if( initialContext == null )
            initialContext = newContext(contextClass, contextProps);
         return initialContext;
      }

      static Context newContext(Class contextClass, Properties contextProps)
         throws Exception
      {
         Context ctx = null;
         try
         {
            ctx = newDefaultContext(contextClass, contextProps);
         }
         catch(NoSuchMethodException e)
         {
            ctx = newLdapContext(contextClass, contextProps);
         }
         return ctx;
      }
      
      private static Context newDefaultContext(Class contextClass, Properties contextProps)
         throws Exception
      {
         Context ctx = null;
         Class[] types = {Hashtable.class};
         Constructor ctor = contextClass.getConstructor(types);
         Object[] args = {contextProps};
         ctx = (Context) ctor.newInstance(args);
         return ctx;
      }
      
      private static Context newLdapContext(Class contextClass, Properties contextProps)
         throws Exception
      {
         Context ctx = null;
         Class[] types = {Hashtable.class, Control[].class};
         Constructor ctor = contextClass.getConstructor(types);
         Object[] args = {contextProps, null};
         ctx = (Context) ctor.newInstance(args);
         return ctx;
      }
        
      public Object getObjectInstance(Object obj, Name name, Context nameCtx, Hashtable environment)
         throws Exception
      {
         Reference ref = (Reference) obj;
         SerializableInitialContext sic = (SerializableInitialContext) ref.get(0);
         return sic.newContext();
      }
        
      public Reference getReference() throws NamingException
      {
         Reference ref = new Reference(Context.class.getName(), this, this.getClass().getName(), null);
         return ref;
      }

      public Object getContent()
      {
         return null;
      }
   }

   /**
    * A proxy implementation of Context that simply intercepts the
    * close() method and ignores it since the underlying Context
    * object is being maintained in memory.
    */
   static class CachedContext implements InvocationHandler
   {
      Context externalCtx;
      
      CachedContext(Context externalCtx)
      {
         this.externalCtx = externalCtx;
      }

      static Context createProxyContext(Context ctx)
      {
         ClassLoader loader = Thread.currentThread().getContextClassLoader();
         Class[] interfaces = ctx.getClass().getInterfaces();
         InvocationHandler handler = new CachedContext(ctx);
         Context proxyCtx = (Context) Proxy.newProxyInstance(loader, interfaces, handler);
         return proxyCtx;
      }

      public Object invoke(Object proxy, Method method, Object[] args) throws Throwable
      {
         Object value = null;
         if( method.getName().equals("close") )
         {
            // We just ignore the close method 
         }
         else
         {
            try
            {
               value = method.invoke(externalCtx, args);
            }
            catch(InvocationTargetException e)
            {
               throw e.getTargetException();
            }
         }
         return value;
      }
   }
}
