/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins;

import javax.ejb.EJBException;

import org.jboss.ejb.Interceptor;
import org.jboss.invocation.Invocation;
import org.jboss.invocation.InvocationKey;
import org.jboss.naming.ENCThreadLocalKey;

/** 
 * This interceptor injects the ProxyFactory into the ThreadLocal container variable
 * 
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:Scott.Stark@jboss.org">Scott Stark</a>
 * @version $Revision: 1.5.2.3 $
 */
public class ProxyFactoryFinderInterceptor
   extends AbstractInterceptor
{

   public void create() throws Exception
   {
   }

   protected void setProxyFactory(String invokerBinding, Invocation mi) throws Exception
   {
      //      if (BeanMetaData.LOCAL_INVOKER_PROXY_BINDING.equals(invokerBinding)) return;
      if (invokerBinding == null)
      {
         log.trace("invokerBInding is null in ProxyFactoryFinder");
         return;
      }
      /*
      if (invokerBinding == null)
      {
         log.error("***************** invokerBinding is null ********");
         log.error("Method name: " + mi.getMethod().getName());
         log.error("jmx name: " + container.getJmxName().toString());
         new Throwable().printStackTrace();
         log.error("*************************");
         throw new EJBException("Couldn't insert proxy factory, " +
               "invokerBinding was null");
      }
      */
      Object proxyFactory = container.lookupProxyFactory(invokerBinding);
      if (proxyFactory == null)
      {
         String methodName;
         if(mi.getMethod() != null) {
            methodName = mi.getMethod().getName();
         } else 
         {
            methodName ="<no method>";
         }

         log.error("***************** proxyFactory is null ********");
         log.error("Method name: " + methodName);
         log.error("jmx name: " + container.getJmxName().toString());
         log.error("invokerBinding: " + invokerBinding);
         log.error("Stack trace", new Throwable());
         log.error("*************************");
         throw new EJBException("Couldn't find proxy factory");
      }
      container.setProxyFactory(proxyFactory);
   }

   public Object invokeHome(Invocation mi)
      throws Exception
   {
      String invokerBinding = 
            (String)mi.getAsIsValue(InvocationKey.INVOKER_PROXY_BINDING);
      setProxyFactory(invokerBinding, mi);

      String oldInvokerBinding = ENCThreadLocalKey.getKey();
      // Only override current ENC binding if we're not local
      //      if ((!BeanMetaData.LOCAL_INVOKER_PROXY_BINDING.equals(invokerBinding)) || oldInvokerBinding == null)
      if (invokerBinding != null || oldInvokerBinding == null)
      {
         ENCThreadLocalKey.setKey(invokerBinding);
      }

      Interceptor next = getNext();
      Object value = null;
      try
      {
         value = next.invokeHome(mi);
      }
      finally
      {
         ENCThreadLocalKey.setKey(oldInvokerBinding);
      }

      return value;
   }

   public Object invoke(Invocation mi)
      throws Exception
   {
      String invokerBinding = 
            (String)mi.getAsIsValue(InvocationKey.INVOKER_PROXY_BINDING);
      setProxyFactory(invokerBinding, mi);

      String oldInvokerBinding = ENCThreadLocalKey.getKey();
      // Only override current ENC binding if we're not local or there has not been a previous call
      //      if ((!BeanMetaData.LOCAL_INVOKER_PROXY_BINDING.equals(invokerBinding)) || oldInvokerBinding == null)
      if (invokerBinding != null || oldInvokerBinding == null)
      {
         ENCThreadLocalKey.setKey(invokerBinding);
      }

      Interceptor next = getNext();
      Object value = null;
      try
      {
         value = next.invoke(mi);
      }
      finally
      {
         ENCThreadLocalKey.setKey(oldInvokerBinding);
      }

      return value;
   }

}
