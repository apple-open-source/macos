/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.interceptor;

import javax.management.MBeanInfo;
import javax.management.ReflectionException;
import javax.management.DynamicMBean;
import javax.management.IntrospectionException;
import javax.management.Attribute;
import javax.management.modelmbean.ModelMBeanInfo;
import javax.management.modelmbean.ModelMBeanInfoSupport;

import org.jboss.mx.capability.DispatcherFactory;
import org.jboss.mx.server.MBeanInvoker;

/**
 * Lifted the functionality seen in MBeanAttributeInterceptor to
 * delegate to a dispatcher which implements DynamicMBean.
 *
 * @author  <a href="mailto:trevor@protocool.com">Trevor Squires</a>.
 */
public class ObjectReferenceInterceptor
   extends AbstractInterceptor
{

   // Attributes ----------------------------------------------------
   private DynamicMBean dynmicMBean;

   // Constructors --------------------------------------------------
   public ObjectReferenceInterceptor(MBeanInfo info, MBeanInvoker invoker)
      throws ReflectionException
   {
      super(info, invoker);
      try
      {
         ModelMBeanInfo modelInfo = (ModelMBeanInfo) info;
         ModelMBeanInfoSupport support = new ModelMBeanInfoSupport(modelInfo);
         Object resource = invoker.getResource();
         this.dynmicMBean = DispatcherFactory.create(support, resource);
      }
      catch (IntrospectionException e)
      {
         throw new ReflectionException(e);
      }
   }

   // Public ------------------------------------------------------------

   // Interceptor overrides ----------------------------------------------
   public Object invoke(Invocation invocation) throws InvocationException
   {
      try
      {
         if (invocation.getInvocationType() == Invocation.OPERATION)
            return dynmicMBean.invoke(invocation.getName(), invocation.getArgs(), invocation.getSignature());

         if (invocation.getImpact() == Invocation.WRITE)
         {
            dynmicMBean.setAttribute(new Attribute(invocation.getName(), invocation.getArgs()[0]));
            return null;
         }
         else
         {
            return dynmicMBean.getAttribute(invocation.getName());
         }
      }
      catch (Throwable t)
      {
         // FIXME: need to check this exception handling
         throw new InvocationException(t);
      }
   }

}


