/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.local;

import java.lang.reflect.InvocationHandler; 
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.lang.reflect.Constructor;
import java.rmi.AccessException;
import java.rmi.NoSuchObjectException;
import java.security.Principal;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import javax.ejb.AccessLocalException;
import javax.ejb.EJBLocalHome;
import javax.ejb.EJBLocalObject;
import javax.ejb.NoSuchObjectLocalException;
import javax.ejb.TransactionRequiredLocalException;
import javax.ejb.TransactionRolledbackLocalException;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.transaction.Transaction;
import javax.transaction.TransactionManager;
import javax.transaction.TransactionRequiredException;
import javax.transaction.TransactionRolledbackException;

import org.jboss.ejb.Container;
import org.jboss.ejb.EJBProxyFactoryContainer;
import org.jboss.ejb.LocalProxyFactory;
import org.jboss.invocation.Invocation;
import org.jboss.invocation.InvocationType;
import org.jboss.invocation.MarshalledInvocation;
import org.jboss.invocation.LocalEJBInvocation;
import org.jboss.logging.Logger;
import org.jboss.metadata.BeanMetaData;
import org.jboss.naming.Util;
import org.jboss.security.SecurityAssociation;
import org.jboss.util.NestedRuntimeException;


/**
 * The LocalProxyFactory implementation that handles local ejb interface
 * proxies.
 *
 * @author <a href="mailto:docodan@mvcsoft.com">Daniel OConnor</a>
 * @author <a href="mailto:scott.stark@jboss.org">Scott Stark</a>
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 */
public class BaseLocalProxyFactory implements LocalProxyFactory
{
   // Attributes ----------------------------------------------------
   protected static Logger log = Logger.getLogger(BaseLocalProxyFactory.class);

   /**
    * A map of the BaseLocalProxyFactory instances keyed by localJndiName
    */
   protected static Map invokerMap = Collections.synchronizedMap(new HashMap());

   protected Container container;

   /**
    * The JNDI name of the local home interface binding
    */
   protected String localJndiName;

   protected TransactionManager transactionManager;

   // The home can be one.
   protected EJBLocalHome home;

   // The Stateless Object can be one.
   protected EJBLocalObject statelessObject;

   protected Map beanMethodInvokerMap;
   protected Map homeMethodInvokerMap;
   protected Class localHomeClass;
   protected Class localClass;

   protected Constructor proxyClassConstructor;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   // Public --------------------------------------------------------

   // ContainerService implementation -------------------------------

   public void setContainer(Container con)
   {
      this.container = con;
   }

   public void create() throws Exception
   {
      BeanMetaData metaData = container.getBeanMetaData();
      localJndiName = metaData.getLocalJndiName();
   }

   public void start()
           throws Exception
   {
      BeanMetaData metaData = container.getBeanMetaData();
      EJBProxyFactoryContainer invokerContainer =
              (EJBProxyFactoryContainer) container;
      localHomeClass = invokerContainer.getLocalHomeClass();
      localClass = invokerContainer.getLocalClass();
      if (localHomeClass == null || localClass == null)
      {
         log.debug(metaData.getEjbName() + " cannot be Bound, doesn't " +
                 "have local and local home interfaces");
         return;
      }

      // this is faster than newProxyInstance
      Class[] intfs = {localClass};
      Class proxyClass = Proxy.getProxyClass(localClass.getClassLoader(), intfs);
      final Class[] constructorParams =
              {InvocationHandler.class};

      proxyClassConstructor = proxyClass.getConstructor(constructorParams);

      Context iniCtx = new InitialContext();
      String beanName = metaData.getEjbName();

      // Set the transaction manager and transaction propagation
      // context factory of the GenericProxy class
      transactionManager =
              (TransactionManager) iniCtx.lookup("java:/TransactionManager");

      // Create method mappings for container invoker
      Method[] methods = localClass.getMethods();
      beanMethodInvokerMap = new HashMap();
      for (int i = 0; i < methods.length; i++)
      {
         long hash = MarshalledInvocation.calculateHash(methods[i]);
         beanMethodInvokerMap.put(new Long(hash), methods[i]);
      }

      methods = localHomeClass.getMethods();
      homeMethodInvokerMap = new HashMap();
      for (int i = 0; i < methods.length; i++)
      {
         long hash = MarshalledInvocation.calculateHash(methods[i]);
         homeMethodInvokerMap.put(new Long(hash), methods[i]);
      }

      // bind that referance to my name
      Util.rebind(iniCtx, localJndiName, getEJBLocalHome());
      invokerMap.put(localJndiName, this);
      log.debug("Bound EJBLocalHome of " + beanName + " to " + localJndiName);
   }

   public void stop()
   {
      // Clean up the home proxy binding
      try
      {
         if (invokerMap.remove(localJndiName) == this)
         {
            InitialContext ctx = new InitialContext();
            ctx.unbind(localJndiName);
         }
      }
      catch (Exception ignore)
      {
      }
   }

   public void destroy()
   {
      if (beanMethodInvokerMap != null)
         beanMethodInvokerMap.clear();
      if (homeMethodInvokerMap != null)
         homeMethodInvokerMap.clear();
      MarshalledInvocation.removeHashes(localHomeClass);
      MarshalledInvocation.removeHashes(localClass);

      container = null;
   }

   public Constructor getProxyClassConstructor()
   {
      if (proxyClassConstructor == null)
      {
      }
      return proxyClassConstructor;
   }

   // EJBProxyFactory implementation -------------------------------
   public synchronized EJBLocalHome getEJBLocalHome()
   {
      if (home == null)
      {
         EJBProxyFactoryContainer cic = (EJBProxyFactoryContainer) container;
         InvocationHandler handler = new LocalHomeProxy(localJndiName, this);
         ClassLoader loader = cic.getLocalHomeClass().getClassLoader();
         Class[] interfaces = {cic.getLocalHomeClass()};

         home = (EJBLocalHome) Proxy.newProxyInstance(
                 loader,
                 interfaces,
                 handler);
      }
      return home;
   }

   public EJBLocalObject getStatelessSessionEJBLocalObject()
   {
      if (statelessObject == null)
      {
         EJBProxyFactoryContainer cic = (EJBProxyFactoryContainer) container;
         InvocationHandler handler =
                 new StatelessSessionProxy(localJndiName, this);
         ClassLoader loader = cic.getLocalClass().getClassLoader();
         Class[] interfaces = {cic.getLocalClass()};

         statelessObject = (EJBLocalObject) Proxy.newProxyInstance(
                 loader,
                 interfaces,
                 handler);
      }
      return statelessObject;
   }

   public EJBLocalObject getStatefulSessionEJBLocalObject(Object id)
   {
      InvocationHandler handler =
              new StatefulSessionProxy(localJndiName, id, this);
      try
      {
         return (EJBLocalObject) proxyClassConstructor.newInstance(new Object[]{handler});
      }
      catch (Exception ex)
      {
         throw new NestedRuntimeException(ex);
      }
   }

   public EJBLocalObject getEntityEJBLocalObject(Object id)
   {
      InvocationHandler handler = new EntityProxy(localJndiName, id, this);
      try
      {
         return (EJBLocalObject) proxyClassConstructor.newInstance(new Object[]{handler});
      }
      catch (Exception ex)
      {
         throw new NestedRuntimeException(ex);
      }
   }

   public Collection getEntityLocalCollection(Collection ids)
   {
      ArrayList list = new ArrayList(ids.size());
      Iterator iter = ids.iterator();
      while (iter.hasNext())
      {
         list.add(getEntityEJBLocalObject(iter.next()));
      }
      return list;
   }

   /**
    *  Invoke a Home interface method.
    */
   public Object invokeHome(Method m, Object[] args) throws Exception
   {
      // Set the right context classloader
      final Thread currentThread = Thread.currentThread();
      ClassLoader oldCl = currentThread.getContextClassLoader();
      currentThread.setContextClassLoader(container.getClassLoader());

      try
      {
         LocalEJBInvocation invocation = new LocalEJBInvocation(
                 null,
                 m,
                 args,
                 getTransaction(),
                 getPrincipal(),
                 getCredential());
         invocation.setType(InvocationType.LOCALHOME);

         return container.invoke(invocation);
      }
      catch (AccessException ae)
      {
         throw new AccessLocalException(ae.getMessage(), ae);
      }
      catch (NoSuchObjectException nsoe)
      {
         throw new NoSuchObjectLocalException(nsoe.getMessage(), nsoe);
      }
      catch (TransactionRequiredException tre)
      {
         throw new TransactionRequiredLocalException(tre.getMessage());
      }
      catch (TransactionRolledbackException trbe)
      {
         throw new TransactionRolledbackLocalException(
                 trbe.getMessage(), trbe);
      }
      finally
      {
         currentThread.setContextClassLoader(oldCl);
      }
   }

   public String getJndiName()
   {
      return localJndiName;
   }

   /**
    *  Return the principal to use for invocations with this proxy.
    */
   Principal getPrincipal()
   {
      return SecurityAssociation.getPrincipal();
   }

   /**
    *  Return the credentials to use for invocations with this proxy.
    */
   Object getCredential()
   {
      return SecurityAssociation.getCredential();
   }

   /**
    *  Return the transaction associated with the current thread.
    *  Returns <code>null</code> if the transaction manager was never
    *  set, or if no transaction is associated with the current thread.
    */
   Transaction getTransaction() throws javax.transaction.SystemException
   {
      if (transactionManager == null)
      {
         return null;
      }
      return transactionManager.getTransaction();
   }

   /**
    *  Invoke a local interface method.
    */
   public Object invoke(Object id, Method m, Object[] args)
           throws Exception
   {
      // Set the right context classloader
      final Thread currentThread = Thread.currentThread();
      ClassLoader oldCl = currentThread.getContextClassLoader();
      currentThread.setContextClassLoader(container.getClassLoader());

      try
      {
         LocalEJBInvocation invocation = new LocalEJBInvocation(
                 id,
                 m,
                 args,
                 getTransaction(),
                 getPrincipal(),
                 getCredential());
         invocation.setType(InvocationType.LOCAL);

         return container.invoke(invocation);
      }
      catch (AccessException ae)
      {
         throw new AccessLocalException(ae.getMessage(), ae);
      }
      catch (NoSuchObjectException nsoe)
      {
         throw new NoSuchObjectLocalException(nsoe.getMessage(), nsoe);
      }
      catch (TransactionRequiredException tre)
      {
         throw new TransactionRequiredLocalException(tre.getMessage());
      }
      catch (TransactionRolledbackException trbe)
      {
         throw new TransactionRolledbackLocalException(
                 trbe.getMessage(), trbe);
      }
      finally
      {
         currentThread.setContextClassLoader(oldCl);
      }
   }

}
