/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;
import java.util.Map;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Hashtable;
import java.rmi.RemoteException;

import javax.ejb.Handle;
import javax.ejb.HomeHandle;
import javax.ejb.EJBObject;
import javax.ejb.EJBLocalObject;
import javax.ejb.EJBLocalHome;
import javax.ejb.EJBHome;
import javax.ejb.EJBMetaData;
import javax.ejb.RemoveException;
import javax.ejb.EJBException;
import javax.management.ObjectName;

import org.jboss.invocation.Invocation;
import org.jboss.invocation.MarshalledInvocation;

/**
 * The container for <em>stateful</em> session beans.
 *
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:docodan@mvcsoft.com">Daniel OConnor</a>
 * @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:scott.stark@jboss.org">Scott Stark</a>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @version <tt>$Revision: 1.49.2.9 $</tt>
 *
 * @jmx:mbean extends="org.jboss.ejb.ContainerMBean"
 */
public class StatefulSessionContainer
   extends Container
   implements EJBProxyFactoryContainer, InstancePoolContainer
{
   /**
    * These are the mappings between the home interface methods and the
    * container methods.
    */
   protected Map homeMapping;

   /**
    * These are the mappings between the remote interface methods and the
    * bean methods.
    */
   protected Map beanMapping;

   /**
    * This is the first interceptor in the chain. The last interceptor must
    * be provided by the container itself.
    */
   protected Interceptor interceptor;

   /** This is the instancepool that is to be used */
   protected InstancePool instancePool;

   /** This is the persistence manager for this container */
   protected StatefulSessionPersistenceManager persistenceManager;

   /** The instance cache. */
   protected InstanceCache instanceCache;

   public LocalProxyFactory getLocalProxyFactory()
   {
      return localProxyFactory;
   }

   public void setInstanceCache(InstanceCache ic)
   {
      this.instanceCache = ic;
      ic.setContainer(this);
   }

   public InstanceCache getInstanceCache()
   {
      return instanceCache;
   }

   public void setInstancePool(InstancePool ip)
   {
      if (ip == null)
         throw new IllegalArgumentException("Null pool");

      this.instancePool = ip;
      ip.setContainer(this);
   }

   public InstancePool getInstancePool()
   {
      return instancePool;
   }

   public StatefulSessionPersistenceManager getPersistenceManager()
   {
      return persistenceManager;
   }

   public void setPersistenceManager(StatefulSessionPersistenceManager pm)
   {
      persistenceManager = pm;
      pm.setContainer(this);
   }

   public void addInterceptor(Interceptor in)
   {
      if (interceptor == null)
      {
         interceptor = in;
      }
      else
      {
         Interceptor current = interceptor;
         while ( current.getNext() != null)
         {
            current = current.getNext();
         }

         current.setNext(in);
      }
   }

   public Interceptor getInterceptor()
   {
      return interceptor;
   }

   public Class getHomeClass()
   {
      return homeInterface;
   }

   public Class getRemoteClass()
   {
      return remoteInterface;
   }

   // Container implementation --------------------------------------

   protected void createService() throws Exception
   {
      // Associate thread with classloader
      ClassLoader oldCl = Thread.currentThread().getContextClassLoader();
      Thread.currentThread().setContextClassLoader(getClassLoader());

      try
      {
         // Acquire classes from CL
         if (metaData.getHome() != null)
            homeInterface = classLoader.loadClass(metaData.getHome());
         if (metaData.getRemote() != null)
            remoteInterface = classLoader.loadClass(metaData.getRemote());

         // Call default init
         super.createService();

         // Make some additional validity checks with regards to the container configuration
         checkCoherency ();

         // Map the bean methods
         setupBeanMapping();

         // Map the home methods
         setupHomeMapping();

         // Map the interfaces to Long
         setupMarshalledInvocationMapping();

         // Init container invoker
         for (Iterator it = proxyFactories.keySet().iterator(); it.hasNext(); )
         {
            String invokerBinding = (String)it.next();
            EJBProxyFactory ci = (EJBProxyFactory)proxyFactories.get(invokerBinding);
            ci.create();
         }

         // Init instance cache
         instanceCache.create();
         // Try to register the instance cache as an MBean
         try
         {
            ObjectName containerName = super.getJmxName();
            Hashtable props = containerName.getKeyPropertyList();
            props.put("plugin", "cache");
            ObjectName cacheName = new ObjectName(containerName.getDomain(), props);
            server.registerMBean(instanceCache, cacheName);
         }
         catch(Throwable t)
         {
            log.debug("Failed to register cache as mbean", t);
         }

         // Initialize pool
         instancePool.create();
         // Try to register the instance pool as an MBean
         try
         {
            ObjectName containerName = super.getJmxName();
            Hashtable props = containerName.getKeyPropertyList();
            props.put("plugin", "pool");
            ObjectName poolName = new ObjectName(containerName.getDomain(), props);
            server.registerMBean(instancePool, poolName);
         }
         catch(Throwable t)
         {
            log.debug("Failed to register pool as mbean", t);
         }

         // Init persistence
         persistenceManager.create();

         // Initialize the interceptor by calling the chain
         Interceptor in = interceptor;
         while (in != null)
         {
            in.setContainer(this);
            in.create();
            in = in.getNext();
         }
      }
      finally
      {
         // Reset classloader
         Thread.currentThread().setContextClassLoader(oldCl);
      }
   }

   protected void startService() throws Exception
   {
      // Associate thread with classloader
      ClassLoader oldCl = Thread.currentThread().getContextClassLoader();
      Thread.currentThread().setContextClassLoader(getClassLoader());

      try
      {
         // Call default start
         super.startService();

         // Start container invoker
         for (Iterator it = proxyFactories.keySet().iterator(); it.hasNext(); )
         {
            String invokerBinding = (String)it.next();
            EJBProxyFactory ci = (EJBProxyFactory)proxyFactories.get(invokerBinding);
            ci.start();
         }

         // Start instance cache
         instanceCache.start();

         // Start pool
         instancePool.start();

         // Start persistence
         persistenceManager.start();

         // Start all interceptors in the chain
         Interceptor in = interceptor;
         while (in != null)
         {
            in.start();
            in = in.getNext();
         }
      }
      finally
      {
         // Reset classloader
         Thread.currentThread().setContextClassLoader(oldCl);
      }
   }

   protected void stopService() throws Exception
   {
      // Associate thread with classloader
      ClassLoader oldCl = Thread.currentThread().getContextClassLoader();
      Thread.currentThread().setContextClassLoader(getClassLoader());

      try
      {
         // Call default stop
         super.stopService();

         // Stop container invoker
         for (Iterator it = proxyFactories.keySet().iterator(); it.hasNext(); )
         {
            String invokerBinding = (String)it.next();
            EJBProxyFactory ci = (EJBProxyFactory)proxyFactories.get(invokerBinding);
            ci.stop();
         }

         // Stop instance cache
         instanceCache.stop();

         // Stop pool
         instancePool.stop();

         // Stop persistence
         persistenceManager.stop();

         // Stop the instance pool
         instancePool.stop();

         // Stop all interceptors in the chain
         Interceptor in = interceptor;
         while (in != null)
         {
            in.stop();
            in = in.getNext();
         }
      }
      finally
      {
         // Reset classloader
         Thread.currentThread().setContextClassLoader(oldCl);
      }
   }

   protected void destroyService() throws Exception
   {
      // Associate thread with classloader
      ClassLoader oldCl = Thread.currentThread().getContextClassLoader();
      Thread.currentThread().setContextClassLoader(getClassLoader());

      try
      {
         // Destroy container invoker
         for (Iterator it = proxyFactories.keySet().iterator(); it.hasNext(); )
         {
            String invokerBinding = (String)it.next();
            EJBProxyFactory ci = (EJBProxyFactory)proxyFactories.get(invokerBinding);
            ci.destroy();
            ci.setContainer(null);
         }

         // Destroy instance cache
         instanceCache.destroy();
         instanceCache.setContainer(null);
         try
         {
            ObjectName containerName = super.getJmxName();
            Hashtable props = containerName.getKeyPropertyList();
            props.put("plugin", "cache");
            ObjectName cacheName = new ObjectName(containerName.getDomain(), props);
            server.unregisterMBean(cacheName);
         }
         catch(Throwable ignore)
         {
         }

         // Destroy pool
         instancePool.destroy();
         instancePool.setContainer(null);
         try
         {
            ObjectName containerName = super.getJmxName();
            Hashtable props = containerName.getKeyPropertyList();
            props.put("plugin", "pool");
            ObjectName poolName = new ObjectName(containerName.getDomain(), props);
            server.unregisterMBean(poolName);
         }
         catch(Throwable ignore)
         {
         }

         // Destroy persistence
         persistenceManager.destroy();
         persistenceManager.setContainer(null);

         // Destroy all the interceptors in the chain
         Interceptor in = interceptor;
         while (in != null)
         {
            in.destroy();
            in.setContainer(null);
            in = in.getNext();
         }

         MarshalledInvocation.removeHashes(homeInterface);
         MarshalledInvocation.removeHashes(remoteInterface);

         // Call default destroy
         super.destroyService();
      }
      finally
      {
         // Reset classloader
         Thread.currentThread().setContextClassLoader(oldCl);
      }
   }

   public Object internalInvokeHome(Invocation mi)
      throws Exception
   {
      return getInterceptor().invokeHome(mi);
   }

   /**
    * This method retrieves the instance from an object table, and invokes
    * the method on the particular instance through the chain of interceptors.
    */
   public Object internalInvoke(Invocation mi)
      throws Exception
   {

      // Invoke through interceptors
      return getInterceptor().invoke(mi);
   }

   // EJBObject implementation --------------------------------------

   public void remove(Invocation mi)
      throws RemoteException, RemoveException
   {
      // 7.6 EJB2.0, it is illegal to remove a bean while in a transaction
      // if (((EnterpriseContext) mi.getEnterpriseContext()).getTransaction() != null)
      // throw new RemoveException("StatefulSession bean in transaction, cannot remove (EJB2.0 7.6)");

      // if the session is removed already then let the user know they have a problem
      StatefulSessionEnterpriseContext ctx = (StatefulSessionEnterpriseContext)mi.getEnterpriseContext();
      if (ctx.getId() == null)
      {
         throw new RemoveException("SFSB has been removed already");
      }

      // Remove from storage
      getPersistenceManager().removeSession(ctx);

      // We signify "removed" with a null id
      ctx.setId(null);
      removeCount ++;
   }

   /**
    * While the following methods are implemented in the client in the case
    * of JRMP we would need to implement them to fully support other transport
    * protocols
    *
    * @return  Always null
    */
   public Handle getHandle(Invocation mi) throws RemoteException
   {
      // TODO
      return null;
   }

   /**
    * @return  Always null
    */
   public Object getPrimaryKey(Invocation mi) throws RemoteException
   {
      // TODO
      return null;
   }

   public EJBHome getEJBHome(Invocation mi) throws RemoteException
   {
      EJBProxyFactory ci = getProxyFactory();
      if (ci == null)
      {
         String msg = "No ProxyFactory, check for ProxyFactoryFinderInterceptor";
         throw new IllegalStateException(msg);
      }

      return (EJBHome) ci.getEJBHome();
   }

   /**
    * @return   Always false
    */
   public boolean isIdentical(Invocation mi) throws RemoteException
   {
      return false; // TODO
   }

   // Home interface implementation ---------------------------------

   private void createSession(final Method m,
                              final Object[] args,
                              final StatefulSessionEnterpriseContext ctx)
      throws Exception
   {
      boolean debug = log.isDebugEnabled();

      // Create a new ID and set it
      Object id = getPersistenceManager().createId(ctx);
      if (debug) {
         log.debug("Created new session ID: " + id);
      }
      ctx.setId(id);

      // Invoke ejbCreate<METHOD>()
      try
      {
         // Build the ejbCreate<METHOD> from the home create<METHOD> sig
         String createName = m.getName();
         String ejbCreateName = "ejbC" + createName.substring(1);
         Method createMethod = getBeanClass().getMethod(ejbCreateName, m.getParameterTypes());
         if (debug) {
            log.debug("Using create method for session: " + createMethod);
         }
         createMethod.invoke(ctx.getInstance(), args);
         createCount++;
      }
      catch (IllegalAccessException e)
      {
         ctx.setId(null);

         throw new EJBException(e);
      }
      catch (InvocationTargetException e)
      {
         ctx.setId(null);

         Throwable t = e.getTargetException();
         if (t instanceof RuntimeException)
         {
            if (t instanceof EJBException)
               throw (EJBException)t;
            // Wrap runtime exceptions
            throw new EJBException((Exception)t);
         }
         else if (t instanceof Exception)
         {
            // Remote, Create, or custom app. exception
            throw (Exception)t;
         }
         else if (t instanceof Error)
         {
            throw (Error)t;
         }
         else {
            throw new org.jboss.util.UnexpectedThrowable(t);
         }
      }

      // call back to the PM to let it know that ejbCreate has been called with success
      getPersistenceManager().createdSession(ctx);

      // Insert in cache
      getInstanceCache().insert(ctx);

      // Create EJBObject
      if (getProxyFactory() != null)
         ctx.setEJBObject((EJBObject)getProxyFactory().getStatefulSessionEJBObject(id));

      // Create EJBLocalObject
      if (getLocalHomeClass() != null)
         ctx.setEJBLocalObject(getLocalProxyFactory().getStatefulSessionEJBLocalObject(id));
   }

   public EJBObject createHome(Invocation mi)
      throws Exception
   {
      createSession(mi.getMethod(), mi.getArguments(),
                    (StatefulSessionEnterpriseContext)mi.getEnterpriseContext());

      return ((StatefulSessionEnterpriseContext)mi.getEnterpriseContext()).getEJBObject();
   }

   // local object interface implementation

   public EJBLocalHome getEJBLocalHome(Invocation mi)
   {
      return localProxyFactory.getEJBLocalHome();
   }

   // local home interface implementation

   /**
    * @throws Error    Not yet implemented
    */
   public void removeLocalHome(Invocation mi)
      throws RemoteException, RemoveException
   {
      throw new Error("Not Yet Implemented");
   }

   public EJBLocalObject createLocalHome(Invocation mi)
      throws Exception
   {
      createSession(mi.getMethod(), mi.getArguments(),
                    (StatefulSessionEnterpriseContext)mi.getEnterpriseContext());

      return ((StatefulSessionEnterpriseContext)mi.getEnterpriseContext()).getEJBLocalObject();
   }

   /**
    * A method for the getEJBObject from the handle
    */
   public EJBObject getEJBObject(Invocation mi) throws RemoteException
   {
      // All we need is an EJBObject for this Id, the first argument is the Id
      EJBProxyFactory ci = getProxyFactory();
      if (ci == null)
      {
         String msg = "No ProxyFactory, check for ProxyFactoryFinderInterceptor";
         throw new IllegalStateException(msg);
      }

      Object id = mi.getArguments()[0];
      if (id == null)
         throw new IllegalStateException("Cannot get a session interface with a null id");

      // Does the session still exist?
      InstanceCache cache = getInstanceCache();
      BeanLock lock = getLockManager().getLock(id);
      lock.sync();
      try
      {
         if (cache.get(id) == null)
            throw new RemoteException("Session no longer exists: " + id);
      }
      finally
      {
         lock.releaseSync();
      }

      // Ok lets create the proxy
      return (EJBObject) ci.getStatefulSessionEJBObject(id);
   }


   // EJBHome implementation ----------------------------------------

   //
   // These are implemented in the local proxy
   //

   /**
    * @throws Error    Not yet implemented
    */
   public void removeHome(Invocation mi)
      throws RemoteException, RemoveException
   {
      throw new Error("Not Yet Implemented");
   }

   public EJBMetaData getEJBMetaDataHome(Invocation mi)
      throws RemoteException
   {
      EJBProxyFactory ci = getProxyFactory();
      if (ci == null)
      {
         String msg = "No ProxyFactory, check for ProxyFactoryFinderInterceptor";
         throw new IllegalStateException(msg);
      }

      return ci.getEJBMetaData();
   }

   /**
    * @throws Error    Not yet implemented
    */
   public HomeHandle getHomeHandleHome(Invocation mi)
      throws RemoteException
   {
      throw new Error("Not Yet Implemented");
   }

   // Private -------------------------------------------------------

   protected void setupHomeMapping() throws Exception
   {
      // Adrian Brock: This should go away when we don't support EJB1x
      boolean isEJB1x = metaData.getApplicationMetaData().isEJB1x();

      Map map = new HashMap();

      if (homeInterface != null)
      {

         Method[] m = homeInterface.getMethods();
         for (int i = 0; i < m.length; i++)
         {
            try
            {
               // Implemented by container
               if (isEJB1x == false && m[i].getName().startsWith("create")) {
                  map.put(m[i], getClass().getMethod("createHome",
                                                     new Class[] { Invocation.class }));
               }
               else {
                  map.put(m[i], getClass().getMethod(m[i].getName()+"Home",
                                                     new Class[] { Invocation.class }));
               }
            }
            catch (NoSuchMethodException e)
            {
               log.info(m[i].getName() + " in bean has not been mapped");
            }
         }
      }

      if (localHomeInterface != null)
      {
         Method[] m = localHomeInterface.getMethods();
         for (int i = 0; i < m.length; i++)
         {
            try
            {
               // Implemented by container
               if (isEJB1x == false && m[i].getName().startsWith("create")) {
                  map.put(m[i], getClass().getMethod("createLocalHome",
                                                     new Class[] { Invocation.class }));
               }
               else {
                  map.put(m[i], getClass().getMethod(m[i].getName()+"LocalHome",
                                                     new Class[] { Invocation.class }));
               }
            }
            catch (NoSuchMethodException e)
            {
               log.info(m[i].getName() + " in bean has not been mapped");
            }
         }
      }

      try
      {
         // Get getEJBObject from on Handle, first get the class
         Class handleClass = Class.forName("javax.ejb.Handle");

         //Get only the one called handle.getEJBObject
         Method getEJBObjectMethod = handleClass.getMethod("getEJBObject", new Class[0]);

         //Map it in the home stuff
         map.put(getEJBObjectMethod, getClass().getMethod("getEJBObject",
                                                          new Class[] {Invocation.class}));
      }
      catch (NoSuchMethodException e)
      {
         log.debug("Couldn't find getEJBObject method on container");
      }

      homeMapping = map;
   }


   private void setUpBeanMappingImpl(Map map,
                                     Method[] m,
                                     String declaringClass)
      throws NoSuchMethodException
   {
      for (int i = 0; i < m.length; i++)
      {
         if (!m[i].getDeclaringClass().getName().equals(declaringClass))
         {
            // Implemented by bean
            map.put(m[i], beanClass.getMethod(m[i].getName(),
                  m[i].getParameterTypes()));
         }
         else
         {
            try
            {
               // Implemented by container
               map.put(m[i], getClass().getMethod(m[i].getName(),
                     new Class[]
                     { Invocation.class }));
            } catch (NoSuchMethodException e)
            {
               log.error(m[i].getName() + " in bean has not been mapped", e);
            }
         }
      }
   }

   protected void setupBeanMapping() throws NoSuchMethodException
   {
      Map map = new HashMap();

      if (remoteInterface != null)
      {
         Method[] m = remoteInterface.getMethods();
         setUpBeanMappingImpl( map, m, "javax.ejb.EJBObject" );
      }

      if (localInterface != null)
      {
         Method[] m = localInterface.getMethods();
         setUpBeanMappingImpl( map, m, "javax.ejb.EJBLocalObject" );
      }

      beanMapping = map;
   }

   protected void setupMarshalledInvocationMapping() throws Exception
   {
      // Create method mappings for container invoker
      if (homeInterface != null)
      {
         Method [] m = homeInterface.getMethods();
         for (int i = 0 ; i<m.length ; i++)
         {
            marshalledInvocationMapping.put( new Long(MarshalledInvocation.calculateHash(m[i])), m[i]);
         }
      }

      if (remoteInterface != null)
      {
         Method [] m = remoteInterface.getMethods();
         for (int j = 0 ; j<m.length ; j++)
         {
            marshalledInvocationMapping.put( new Long(MarshalledInvocation.calculateHash(m[j])), m[j]);
         }
      }
      // Get the getEJBObjectMethod
      Method getEJBObjectMethod = Class.forName("javax.ejb.Handle").getMethod("getEJBObject", new Class[0]);

      // Hash it
      marshalledInvocationMapping.put(new Long(MarshalledInvocation.calculateHash(getEJBObjectMethod)),getEJBObjectMethod);
   }

   protected Interceptor createContainerInterceptor()
   {
      return new ContainerInterceptor();
   }

   protected void checkCoherency () throws Exception
   {
      // Check clustering cohrency wrt metadata
      //
      if (metaData.isClustered())
      {
         boolean clusteredProxyFactoryFound = false;
         for (Iterator it = proxyFactories.keySet().iterator(); it.hasNext(); )
         {
            String invokerBinding = (String)it.next();
            EJBProxyFactory ci = (EJBProxyFactory)proxyFactories.get(invokerBinding);
            if (ci instanceof org.jboss.proxy.ejb.ClusterProxyFactory)
               clusteredProxyFactoryFound = true;
         }

         if (!clusteredProxyFactoryFound)
         {
            log.warn("*** EJB '" + this.metaData.getEjbName() + "' deployed as CLUSTERED but not a single clustered-invoker is bound to container ***");
         }
      }
   }

   /**
    * This is the last step before invocation - all interceptors are done
    */
   class ContainerInterceptor
      extends AbstractContainerInterceptor
   {
      public Object invokeHome(Invocation mi) throws Exception
      {
         boolean trace = log.isTraceEnabled();

         if (trace)
         {
            log.trace("HOMEMETHOD coming in ");
            log.trace(""+mi.getMethod());
            log.trace("HOMEMETHOD coming in hashcode"+mi.getMethod().hashCode());
            log.trace("HOMEMETHOD coming in classloader"+mi.getMethod().getDeclaringClass().getClassLoader().hashCode());
            log.trace("CONTAINS "+homeMapping.containsKey(mi.getMethod()));
         }

         Method miMethod = mi.getMethod();
         Method m = (Method) homeMapping.get(miMethod);
         if( m == null )
         {
            String msg = "Invalid invocation, check your deployment packaging"
               +", method="+miMethod;
            throw new EJBException(msg);
         }

         // Invoke and handle exceptions
         if (trace)
         {
            log.trace("HOMEMETHOD m "+m);
            java.util.Iterator iterator = homeMapping.keySet().iterator();
            while(iterator.hasNext())
            {
               Method me = (Method) iterator.next();

               if (me.getName().endsWith("create"))
               {
                  log.trace(me.toString());
                  log.trace(""+me.hashCode());
                  log.trace(""+me.getDeclaringClass().getClassLoader().hashCode());
                  log.trace("equals "+me.equals(mi.getMethod())+ " "+mi.getMethod().equals(me));
               }
            }
         }

         try
         {
            return m.invoke(StatefulSessionContainer.this, new Object[] { mi });
         }
         catch (Exception e)
         {
            rethrow(e);
         }

         // We will never get this far, but the compiler does not know that
         throw new org.jboss.util.UnreachableStatementException();
      }

      public Object invoke(Invocation mi) throws Exception
      {
         // wire the transaction on the context, this is how the instance remember the tx
         // Unlike Entity beans we can't do that in the previous interceptors (ordering)
         EnterpriseContext ctx = (EnterpriseContext) mi.getEnterpriseContext();
         if (ctx.getTransaction() == null)
            ctx.setTransaction(mi.getTransaction());

         // Get method
         Method miMethod = mi.getMethod();
         Method m = (Method) beanMapping.get(miMethod);
         if( m == null )
         {
            String msg = "Invalid invocation, check your deployment packaging"
               +", method="+miMethod;
            throw new EJBException(msg);
         }

         // Select instance to invoke (container or bean)
         if (m.getDeclaringClass().equals(StatefulSessionContainer.this.getClass()))
         {
            // Invoke and handle exceptions
            try
            {
               return m.invoke(StatefulSessionContainer.this, new Object[] { mi });
            }
            catch (Exception e)
            {
               rethrow(e);
            }
         }
         else
         {
            // Invoke and handle exceptions
            try
            {
               Object bean = ctx.getInstance();
               return m.invoke(bean, mi.getArguments());
            }
            catch (Exception e)
            {
               rethrow(e);
            }
         }

         // We will never get this far, but the compiler does not know that
         throw new org.jboss.util.UnreachableStatementException();
      }
   }
}
