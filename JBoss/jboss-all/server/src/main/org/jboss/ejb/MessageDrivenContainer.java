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

import javax.ejb.Handle;
import javax.ejb.HomeHandle;
import javax.ejb.EJBObject;
import javax.ejb.EJBMetaData;
import javax.ejb.CreateException;
import javax.ejb.RemoveException;

import javax.jms.MessageListener;
import javax.jms.Message;
import javax.management.ObjectName;

import org.jboss.invocation.Invocation;
import org.jboss.ejb.EnterpriseContext;
import org.jboss.util.NullArgumentException;

/**
 * The container for <em>MessageDriven</em> beans.
 *
 * @author <a href="mailto:peter.antman@tim.se">Peter Antman</a>.
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="mailto:docodan@mvcsoft.com">Daniel OConnor</a>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author <a href="mailto:Scott.Stark@jboss.org">Scott Stark</a>
 * @version $Revision: 1.23.2.5 $
 *
 * @jmx:mbean extends="org.jboss.ejb.ContainerMBean"
 */
public class MessageDrivenContainer
      extends Container
      implements EJBProxyFactoryContainer, InstancePoolContainer, MessageDrivenContainerMBean
{
   /**
    * These are the mappings between the remote interface methods
    * and the bean methods.
    */
   protected Map beanMapping;

   /** This is the instancepool that is to be used. */
   protected InstancePool instancePool;

   /**
    * This is the first interceptor in the chain.
    * The last interceptor must be provided by the container itself.
    */
   protected Interceptor interceptor;

   protected long messageCount;

   public LocalProxyFactory getLocalProxyFactory()
   {
      return localProxyFactory;
   }

   public void setInstancePool(final InstancePool instancePool)
   {
      if (instancePool == null)
         throw new NullArgumentException("instancePool");

      this.instancePool = instancePool;
      this.instancePool.setContainer(this);
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
      }
      else
      {
         Interceptor current = interceptor;

         while (current.getNext() != null)
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

   /**
    * @jmx:managed-attribute
    * @return the number of messages delivered
    */
   public long getMessageCount()
   {
      return messageCount;
   }

   /**
    * EJBProxyFactoryContainer - not needed, should we skip inherit this
    * or just throw Error??
    */
   public Class getHomeClass()
   {
      //throw new Error("HomeClass not valid for MessageDriven beans");
      return null;
   }

   public Class getRemoteClass()
   {
      //throw new Error("RemoteClass not valid for MessageDriven beans");
      return null;
   }

   public Class getLocalClass()
   {
      return null;
   }

   public Class getLocalHomeClass()
   {
      //throw new Error("LocalHomeClass not valid for MessageDriven beans");
      return null;
   }

   // Container implementation - overridden here ----------------------

   protected void createService() throws Exception
   {
      // Associate thread with classloader
      ClassLoader oldCl = Thread.currentThread().getContextClassLoader();
      Thread.currentThread().setContextClassLoader(getClassLoader());

      try
      {
         // Call default init
         super.createService();

         // Map the bean methods
         Map map = new HashMap();
         Method m = MessageListener.class.getMethod("onMessage", new Class[]{Message.class});
         map.put(m, beanClass.getMethod(m.getName(), m.getParameterTypes()));
         log.debug("Mapped " + m.getName() + " " + m.hashCode() + " to " + map.get(m));
         beanMapping = map;

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

         for (Iterator it = proxyFactories.keySet().iterator(); it.hasNext();)
         {
            String invokerBinding = (String) it.next();
            EJBProxyFactory ci = (EJBProxyFactory) proxyFactories.get(invokerBinding);
            ci.create();
            // Try to register the container invoker as an MBean
            try
            {
               ObjectName containerName = super.getJmxName();
               Hashtable props = containerName.getKeyPropertyList();
               props.put("plugin", "invoker");
               props.put("binding", invokerBinding);
               ObjectName invokerName = new ObjectName(containerName.getDomain(), props);
               server.registerMBean(ci, invokerName);
            }
            catch(Throwable t)
            {
               log.debug("Failed to register invoker binding as mbean", t);
            }
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
         for (Iterator it = proxyFactories.keySet().iterator(); it.hasNext();)
         {
            String invokerBinding = (String) it.next();
            EJBProxyFactory ci = (EJBProxyFactory) proxyFactories.get(invokerBinding);
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
      log.info("Stopping");

      // Associate thread with classloader
      ClassLoader oldCl = Thread.currentThread().getContextClassLoader();
      Thread.currentThread().setContextClassLoader(getClassLoader());

      try
      {
         // Call default stop
         super.stopService();

         // Stop container invoker
         for (Iterator it = proxyFactories.keySet().iterator(); it.hasNext();)
         {
            String invokerBinding = (String) it.next();
            EJBProxyFactory ci = (EJBProxyFactory) proxyFactories.get(invokerBinding);
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
         for (Iterator it = proxyFactories.keySet().iterator(); it.hasNext();)
         {
            String invokerBinding = (String) it.next();
            EJBProxyFactory ci = (EJBProxyFactory) proxyFactories.get(invokerBinding);
            ci.destroy();
            ci.setContainer(null);
            try
            {
               ObjectName containerName = super.getJmxName();
               Hashtable props = containerName.getKeyPropertyList();
               props.put("plugin", "invoker");
               props.put("binding", invokerBinding);
               ObjectName invokerName = new ObjectName(containerName.getDomain(), props);
               server.unregisterMBean(invokerName);
            }
            catch(Throwable ignore)
            {
            }
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

         // Call default destroy
         super.destroyService();
      }
      finally
      {
         // Reset classloader
         Thread.currentThread().setContextClassLoader(oldCl);
      }
   }

   /**
    * @throws Error   Not valid for MDB
    */
   public Object internalInvokeHome(Invocation mi)
         throws Exception
   {
      throw new Error("invokeHome not valid for MessageDriven beans");
   }

   /**
    * This method does invocation interpositioning of tx and security,
    * retrieves the instance from an object table, and invokes the method
    * on the particular instance
    */
   public Object internalInvoke(Invocation mi) throws Exception
   {
      // Invoke through interceptors
      return getInterceptor().invoke(mi);
   }


   // EJBHome implementation ----------------------------------------

   public EJBObject createHome()
         throws java.rmi.RemoteException, CreateException
   {
      throw new Error("createHome not valid for MessageDriven beans");
   }


   public void removeHome(Handle handle)
         throws java.rmi.RemoteException, RemoveException
   {
      throw new Error("removeHome not valid for MessageDriven beans");
      // TODO
   }

   public void removeHome(Object primaryKey)
         throws java.rmi.RemoteException, RemoveException
   {
      throw new Error("removeHome not valid for MessageDriven beans");
      // TODO
   }

   public EJBMetaData getEJBMetaDataHome()
         throws java.rmi.RemoteException
   {
      // TODO
      //return null;
      throw new Error("getEJBMetaDataHome not valid for MessageDriven beans");
   }

   public HomeHandle getHomeHandleHome()
         throws java.rmi.RemoteException
   {
      // TODO
      //return null;
      throw new Error("getHomeHandleHome not valid for MessageDriven beans");
   }

   Interceptor createContainerInterceptor()
   {
      return new ContainerInterceptor();
   }

   /**
    * This is the last step before invocation - all interceptors are done
    */
   class ContainerInterceptor
         extends AbstractContainerInterceptor
   {
      /**
       * @throws Error   Not valid for MDB
       */
      public Object invokeHome(Invocation mi) throws Exception
      {
         throw new Error("invokeHome not valid for MessageDriven beans");
      }

      /**
       * FIXME Design problem, who will do the acknowledging for
       * beans with bean managed transaction?? Probably best done in the
       * listener "proxys"
       */
      public Object invoke(Invocation mi)
            throws Exception
      {
         EnterpriseContext ctx = (EnterpriseContext) mi.getEnterpriseContext();

         // wire the transaction on the context,
         // this is how the instance remember the tx
         if (ctx.getTransaction() == null)
         {
            ctx.setTransaction(mi.getTransaction());
         }

         // Get method and instance to invoke upon
         Method m = (Method) beanMapping.get(mi.getMethod());

         // we have a method that needs to be done by a bean instance
         try
         {
            messageCount++;
            return m.invoke(ctx.getInstance(), mi.getArguments());
         }
         catch (Exception e)
         {
            rethrow(e);
         }

         // We will never get this far, but the compiler does not know that
         throw new org.jboss.util.UnreachableStatementException();
      }
   }
}
