/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.metadata;

import javax.management.DynamicMBean;
import javax.management.MBeanInfo;
import javax.management.NotCompliantMBeanException;
import java.lang.reflect.Modifier;

/**
 * This class is a bit bogus.
 *
 * @author  <a href="mailto:trevor@protocool.com">Trevor Squires</a>.
 */
public class MBeanCapability
{
   public static final int DYNAMIC_MBEAN = 0x321;
   public static final int STANDARD_MBEAN = 0x123;
   public static final int NOT_AN_MBEAN = 0xc0de;

   protected int mbeanType = NOT_AN_MBEAN;
   protected Class mbeanClass = null;
   protected MBeanInfo standardInfo = null;

   public static MBeanCapability of(Class mbeanClass) throws NotCompliantMBeanException
   {
      if (null == mbeanClass)
      {
         throw new IllegalArgumentException("Class cannot be null");
      }

      if (Modifier.isAbstract(mbeanClass.getModifiers()))
      {
         throw new NotCompliantMBeanException("Class is abstract: " + mbeanClass.getName());
      }

      if (mbeanClass.getConstructors().length == 0)
      {
         throw new NotCompliantMBeanException("Class has no public constructors: " + mbeanClass.getName());
      }

      if (DynamicMBean.class.isAssignableFrom(mbeanClass))
      {
         // Compliance check (this ought to disappear in next rev of JMX spec).
         // You can't implement both Standard and DynamicMBean.  So, if this class
         // is a DynamicMBean and it directly implements a standard MBean interface
         // then it's not compliant.
         if (StandardMetaData.findStandardInterface(mbeanClass, mbeanClass.getInterfaces()) != null)
         {
            throw new NotCompliantMBeanException("Class supplies a standard MBean interface and is a DynamicMBean: " +
                                                 mbeanClass.getName());
         }

         return new MBeanCapability(mbeanClass);
      }
      else if (StandardMetaData.findStandardInterface(mbeanClass) != null)
      {
         return new MBeanCapability(mbeanClass, new StandardMetaData(mbeanClass).build());
      }

      throw new NotCompliantMBeanException("Class does not expose a management interface: " + mbeanClass.getName());
   }

   protected MBeanCapability(Class mbeanClass)
   {
      mbeanType = DYNAMIC_MBEAN;
   }

   protected MBeanCapability(Class mbeanClass, MBeanInfo info)
   {
      mbeanType = STANDARD_MBEAN;
      standardInfo = info;
   }

   public int getMBeanType()
   {
      return mbeanType;
   }

   public MBeanInfo getStandardMBeanInfo()
   {
      return standardInfo;
   }
}
