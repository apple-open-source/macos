/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.interceptor;

import javax.management.MBeanInfo;

import org.jboss.mx.server.MBeanInvoker;


/**
 * Base class for all interceptors.
 *
 * @see org.jboss.mx.interceptor.StandardMBeanInterceptor
 * @see org.jboss.mx.interceptor.LogInterceptor
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.4.2.2 $
 *
 */
public class AbstractInterceptor implements Interceptor
{
   // Attributes ----------------------------------------------------
   protected Interceptor next = null;
   protected String name = null;
   protected MBeanInfo info;
   protected MBeanInvoker invoker;

   // Constructors --------------------------------------------------
   public AbstractInterceptor()
   {
      this(null);
   }
   public AbstractInterceptor(String name)
   {
      this.name = name;
   }
   public AbstractInterceptor(MBeanInfo info, MBeanInvoker invoker)
   {
      this.name = getClass().getName();
      this.info = info;
      this.invoker = invoker;
   }

   // Public --------------------------------------------------------
   public Object invoke(Invocation invocation) throws InvocationException
   {
      return getNext().invoke(invocation);
   }

   public Interceptor getNext()
   {
      return next;
   }

   public Interceptor setNext(Interceptor interceptor)
   {
      this.next = interceptor;
      return interceptor;
   }

}
