/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.server;

import javax.management.MBeanInfo;
import javax.management.NotCompliantMBeanException;
import javax.management.ReflectionException;

import org.jboss.mx.metadata.StandardMetaData;
import org.jboss.mx.interceptor.Interceptor;
import org.jboss.mx.interceptor.StandardMBeanInterceptor;
import org.jboss.mx.interceptor.LogInterceptor;
import org.jboss.mx.interceptor.SecurityInterceptor;


/**
 * Represents standard MBean in the server.
 *
 * @see org.jboss.mx.interceptor.StandardMBeanInterceptor
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.5.8.2 $
 *
 */
public class StandardMBeanInvoker
   extends MBeanInvoker
{

   // Attributes ----------------------------------------------------

   // Constructors --------------------------------------------------
   public StandardMBeanInvoker(Object resource)
      throws NotCompliantMBeanException, ReflectionException
   {
      super.resource = resource;
      this.info = new StandardMetaData(resource).build();

      Interceptor security = new SecurityInterceptor(info, this);
      Interceptor li = new LogInterceptor(info, this);
      security.setNext(li);
      Interceptor smbi = new StandardMBeanInterceptor(info, this);
      li.setNext(smbi);
      stack = security;
   }

   // Public --------------------------------------------------------
   public static Class getMBeanInterface(Object resource)
   {
      Class clazz = resource.getClass();

      while (clazz != null)
      {
         Class[] interfaces = clazz.getInterfaces();

         for (int i = 0; i < interfaces.length; ++i)
         {
            if (interfaces[i].getName().equals(clazz.getName() + "MBean"))
               return interfaces[i];

            Class[] superInterfaces = interfaces[i].getInterfaces();
            for (int j = 0; j < superInterfaces.length; ++j)
            {
               if (superInterfaces[j].getName().equals(clazz.getName() + "MBean"))
                  return superInterfaces[j];
            }
         }
         clazz = clazz.getSuperclass();
      }

      return null;
   }

   // DynamicMBean implementation -----------------------------------

   public MBeanInfo getMBeanInfo()
   {
      return info;
   }

}


