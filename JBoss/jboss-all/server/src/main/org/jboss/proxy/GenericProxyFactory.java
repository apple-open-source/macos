/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.proxy;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.Iterator;
import javax.management.ObjectName;

import org.jboss.invocation.Invoker;
import org.jboss.invocation.InvocationContext;
import org.jboss.invocation.InvocationKey;
import org.jboss.proxy.Interceptor;
import org.jboss.proxy.ClientContainer;
import org.jboss.system.Registry;
import org.jboss.util.NestedRuntimeException;


/** A generic factory of java.lang.reflect.Proxy that constructs a proxy
 * that is a composite of ClientContainer/Interceptors/Invoker
 *
 * @todo generalize the proxy/invoker factory object
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.3 $
 */
public class GenericProxyFactory
{

   /** Create a composite proxy for the given interfaces, invoker.
    @param id, the cache id for the target object if any
    @param targetName, the name of the server side service
    @param invoker, the detached invoker stub to embed in the proxy
    @param jndiName, the JNDI name the proxy will be bound under if not null
    @param proxyBindingName, the invoker-proxy-binding name if not null
    @param interceptorClasses, the Class objects for the interceptors
    @param loader, the ClassLoader to associate the the Proxy
    @param ifaces, the Class objects for the interfaces the Proxy implements
    */
   public Object createProxy(Object id, ObjectName targetName,
      Invoker invoker, String jndiName, String proxyBindingName,
      ArrayList interceptorClasses, ClassLoader loader, Class[] ifaces)
   {
      InvocationContext context = new InvocationContext();
      Integer nameHash = new Integer(targetName.hashCode());
      context.setObjectName(nameHash);
      context.setCacheId(id);
      if( jndiName != null )
         context.setValue(InvocationKey.JNDI_NAME, jndiName);

      if( invoker == null )
         throw new RuntimeException("Null invoker given for name: " + targetName);
      context.setInvoker(invoker);
      if( proxyBindingName != null )
         context.setInvokerProxyBinding(proxyBindingName);

      ClientContainer client = new ClientContainer(context);
      try
      {
         loadInterceptorChain(interceptorClasses, client);
      }
      catch(Exception e)
      {
         throw new NestedRuntimeException("Failed to load interceptor chain", e);
      }

      return Proxy.newProxyInstance(
         // Classloaders
         loader,
         // Interfaces
         ifaces,
         // Client container as invocation handler
         client);
   }

   /** Create a composite proxy for the given interfaces, invoker.
    @param id, the cache id for the target object if any
    @param targetName, the name of the server side service
    @param invokerName, the name of the server side JMX invoker
    @param jndiName, the JNDI name the proxy will be bound under if not null
    @param proxyBindingName, the invoker-proxy-binding name if not null
    @param interceptorClasses, the Class objects for the interceptors
    @param loader, the ClassLoader to associate the the Proxy
    @param ifaces, the Class objects for the interfaces the Proxy implements
    */
   public Object createProxy(Object id, ObjectName targetName, ObjectName invokerName,
      String jndiName, String proxyBindingName,
      ArrayList interceptorClasses, ClassLoader loader, Class[] ifaces)
   {
      InvocationContext context = new InvocationContext();
      Integer nameHash = new Integer(targetName.hashCode());
      context.setObjectName(nameHash);
      context.setCacheId(id);
      if( jndiName != null )
         context.setValue(InvocationKey.JNDI_NAME, jndiName);

      Invoker invoker = (Invoker) Registry.lookup(invokerName);
      if (invoker == null)
         throw new RuntimeException("Failed to find invoker for name: " + invokerName);
      context.setInvoker(invoker);
      if( proxyBindingName != null )
         context.setInvokerProxyBinding(proxyBindingName);

      ClientContainer client = new ClientContainer(context);
      try
      {
         loadInterceptorChain(interceptorClasses, client);
      }
      catch(Exception e)
      {
         throw new NestedRuntimeException("Failed to load interceptor chain", e);
      }

      return Proxy.newProxyInstance(
         // Classloaders
         loader,
         // Interfaces
         ifaces,
         // Client container as invocation handler
         client);
   }

   /** The loadInterceptorChain create instances of interceptor
    * classes from the list of classes given by the chain array.
    *
    * @exception Exception if an error occurs
    */
   protected void loadInterceptorChain(ArrayList chain, ClientContainer client)
      throws Exception
   {
      Interceptor last = null;
      for (int i = 0; i < chain.size(); i++)
      {
         Class clazz = (Class)chain.get(i);
         Interceptor interceptor = (Interceptor) clazz.newInstance(); 
         if (last == null)
         {
            last = interceptor;
            client.setNext(interceptor);
         }
         else
         {
            last.setNext(interceptor);
            last = interceptor;
         }
      }
   }
}
