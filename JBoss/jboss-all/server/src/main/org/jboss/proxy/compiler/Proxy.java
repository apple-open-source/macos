/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.proxy.compiler;

import java.io.Serializable;

import org.jboss.util.NestedRuntimeException;

/**
 * A factory for creating proxy objects.
 *      
 * @version <tt>$Revision: 1.3 $</tt>
 * @author Unknown
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class Proxy
{
   /**
    * Create a new proxy instance.  
    * 
    * <p>Proxy instances will also implement {@link Serializable}.
    *
    * <p>Delegates the actual creation of the proxy to
    *    {@link Proxies#newTarget}.
    *
    * @param loader       The class loader for the new proxy instance.
    * @param interfaces   A list of classes which the proxy will implement.
    * @param h            The handler for method invocations.
    * @return             A new proxy instance.
    *
    * @throws RuntimeException    Failed to create new proxy target.
    */
   public static Object newProxyInstance(final ClassLoader loader,
                                         final Class[] interfaces,
                                         final InvocationHandler h)
   {
      // Make all proxy instances implement Serializable
      Class[] interfaces2 = new Class[interfaces.length + 1];
      System.arraycopy(interfaces, 0, interfaces2, 0, interfaces.length);
      interfaces2[interfaces2.length - 1] = Serializable.class;

      try {
         // create a new proxy
         return Proxies.newTarget(loader, h, interfaces2);
      }
      catch (Exception e) {
         throw new NestedRuntimeException("Failed to create new proxy target", e);
      }
   }

   public static void forgetProxyForClass(Class clazz)
   {
      Proxies.forgetProxyForClass(clazz);
   }

}

