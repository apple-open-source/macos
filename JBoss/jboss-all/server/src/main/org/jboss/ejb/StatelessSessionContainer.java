/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb;

import java.lang.reflect.Method;
import java.util.Map;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Hashtable;
import java.rmi.RemoteException;

import javax.ejb.Handle;
import javax.ejb.HomeHandle;
import javax.ejb.EJBObject;
import javax.ejb.EJBHome;
import javax.ejb.EJBLocalObject;
import javax.ejb.EJBLocalHome;
import javax.ejb.EJBMetaData;
import javax.ejb.CreateException;
import javax.ejb.RemoveException;
import javax.ejb.EJBException;
import javax.management.ObjectName;

import org.jboss.invocation.Invocation;
import org.jboss.invocation.MarshalledInvocation;

/**
 * The container for <em>stateless</em> session beans.
 *
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="mailto:docodan@mvcsoft.com">Daniel OConnor</a>
 * @version $Revision: 1.37.2.7 $
 */
public class StatelessSessionContainer
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

   /** This is the instancepool that is to be used */
   protected InstancePool instancePool;

   /**
    * This is the first interceptor in the chain. The last interceptor must
    * be provided by the container itself
    */
   protected Interceptor interceptor;

   public LocalProxyFactory getLocalProxyFactory()
   {
      return localProxyFactory;
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

   public void addInterceptor(Interceptor in)
   {
      if (interceptor == null)
      {
         interceptor = in;
      } else
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

         // Init container invoker
         for (Iterator it = proxyFactories.keySet().iterator(); it.hasNext(); )
         {
            String invokerBinding = (String)it.next();
            EJBProxyFactory ci = (EJBProxyFactory)proxyFactories.get(invokerBinding);
            ci.create();
         }



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

         // Start the instance pool
         instancePool.start();

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

         // Destroy the pool
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

   public Object internalInvokeHome(Invocation mi) throws Exception
   {
      return getInterceptor().invokeHome(mi);
   }

   /**
   * This method does invocation interpositioning of tx and security,
   * retrieves the instance from an object table, and invokes the method
   * on the particular instance
   */
   public Object internalInvoke(Invocation mi)
      throws Exception
   {
      // Invoke through interceptors
      return getInterceptor().invoke(mi);
   }

   // EJBObject implementation --------------------------------------

   /**
    * No-op.
    */
   public void remove(Invocation mi)
      throws RemoteException, RemoveException
   {
      //TODO
   }

   /**
    * @return    Always null
    */
   public Handle getHandle(Invocation mi)
      throws RemoteException
   {
      // TODO
      return null;
   }

   /**
    * @return    Always null
    */
   public Object getPrimaryKey(Invocation mi)
      throws RemoteException
   {
      // TODO
      return null;
   }

   public EJBHome getEJBHome(Invocation mi)
      throws RemoteException
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
    * @return    Always false
    */
   public boolean isIdentical(Invocation mi)
      throws RemoteException
   {
      return false; // TODO
   }

   // EJBLocalObject implementation

   public EJBLocalHome getEJBLocalHome(Invocation mi)
   {
      return localProxyFactory.getEJBLocalHome();
   }

   // EJBLocalHome implementation

   public EJBLocalObject createLocalHome()
      throws CreateException
   {
      if (localProxyFactory == null)
      {
         String msg = "No ProxyFactory, check for ProxyFactoryFinderInterceptor";
         throw new IllegalStateException(msg);
      }
      createCount ++;
      return localProxyFactory.getStatelessSessionEJBLocalObject();
   }

   /**
    * No-op.
    */
   public void removeLocalHome(Object primaryKey)
   {
      // todo
   }

   // EJBHome implementation ----------------------------------------

   public EJBObject createHome()
      throws RemoteException, CreateException
   {
       EJBProxyFactory ci = getProxyFactory();
      if (ci == null)
      {
         String msg = "No ProxyFactory, check for ProxyFactoryFinderInterceptor";
         throw new IllegalStateException(msg);
      }
      createCount ++;
      Object obj = ci.getStatelessSessionEJBObject();
      return (EJBObject)obj;
   }

   /**
    * No-op.
    */
   public void removeHome(Handle handle)
      throws RemoteException, RemoveException
   {
      removeCount ++;
      // TODO
   }

   /**
    * No-op.
    */
   public void removeHome(Object primaryKey)
      throws RemoteException, RemoveException
   {
      removeCount ++;
      // TODO
   }

   /**
    * @return    Always null.
    */
   public EJBMetaData getEJBMetaDataHome()
      throws RemoteException
   {
      // TODO
      return null;
   }

   /**
    * @return    Always null.
    */
   public HomeHandle getHomeHandleHome()
      throws RemoteException
   {
      // TODO
      return null;
   }

   // Protected  ----------------------------------------------------

   protected void setupHomeMapping()
      throws NoSuchMethodException
   {
      boolean debug = log.isDebugEnabled();

      Map map = new HashMap();

      if (homeInterface != null)
      {
         Method[] m = homeInterface.getMethods();
         for (int i = 0; i < m.length; i++)
         {
            // Implemented by container
            if (debug)
               log.debug("Mapping "+m[i].getName());
            map.put(m[i], getClass().getMethod(m[i].getName()+"Home", m[i].getParameterTypes()));
         }
      }
      if (localHomeInterface != null)
      {
         Method[] m = localHomeInterface.getMethods();
         for (int i = 0; i < m.length; i++)
         {
            // Implemented by container
            if (debug)
               log.debug("Mapping "+m[i].getName());
            map.put(m[i], getClass().getMethod(m[i].getName()+"LocalHome", m[i].getParameterTypes()));
         }
      }

      homeMapping = map;
   }

   private void setUpBeanMappingImpl( Map map, Method[] m, String declaringClass )
      throws NoSuchMethodException
   {
      boolean debug = log.isDebugEnabled();

      for (int i = 0; i < m.length; i++)
      {
         if (!m[i].getDeclaringClass().getName().equals(declaringClass))
         {
            // Implemented by bean
            try {
               map.put(m[i], beanClass.getMethod(m[i].getName(), m[i].getParameterTypes()));
            }
            catch (NoSuchMethodException ex)
            {
               throw new org.jboss.util.NoSuchMethodException("Not found in bean class: ", m[i]);
            }

            if (debug)
               log.debug("Mapped "+m[i].getName()+" "+m[i].hashCode()+"to "+map.get(m[i]));
         }
         else
         {
            try
            {
               // Implemented by container
               if (debug)
                  log.debug("Mapped Container method "+m[i].getName() +" HASH "+m[i].hashCode());
               map.put(m[i], getClass().getMethod(m[i].getName(), new Class[] { Invocation.class }));
            }
            catch (NoSuchMethodException e)
            {
               log.error(m[i].getName() + " in bean has not been mapped", e);
            }
         }
      }
   }

   protected void setupBeanMapping()
      throws NoSuchMethodException
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

   Interceptor createContainerInterceptor()
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
         Method miMethod = mi.getMethod();
         Method m = (Method) homeMapping.get(miMethod);
         if( m == null )
         {
            String msg = "Invalid invocation, check your deployment packaging"
               +", method="+miMethod;
            throw new EJBException(msg);
         }

         try
         {
            return m.invoke(StatelessSessionContainer.this, mi.getArguments());
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
         EnterpriseContext ctx = (EnterpriseContext) mi.getEnterpriseContext();
         if (ctx.getTransaction() == null)
            ctx.setTransaction(mi.getTransaction());

         // Get method and instance to invoke upon
         Method miMethod = mi.getMethod();
         Method m = (Method) beanMapping.get(miMethod);
         if( m == null )
         {
            String msg = "Invalid invocation, check your deployment packaging"
               +", method="+miMethod;
            throw new EJBException(msg);
         }

         //If we have a method that needs to be done by the container (EJBObject methods)
         if (m.getDeclaringClass().equals(StatelessSessionContainer.class))
         {
            try
            {
               return m.invoke(StatelessSessionContainer.this, new Object[] { mi });
            }
            catch (Exception e)
            {
               rethrow(e);
            }
         }
         else // we have a method that needs to be done by a bean instance
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
