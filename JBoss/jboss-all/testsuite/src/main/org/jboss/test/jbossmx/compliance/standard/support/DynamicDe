/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.compliance.standard.support;

import javax.management.DynamicMBean;
import javax.management.AttributeNotFoundException;
import javax.management.MBeanException;
import javax.management.ReflectionException;
import javax.management.Attribute;
import javax.management.InvalidAttributeValueException;
import javax.management.AttributeList;
import javax.management.MBeanInfo;
import javax.management.MBeanAttributeInfo;
import javax.management.MBeanConstructorInfo;
import javax.management.MBeanOperationInfo;
import javax.management.MBeanNotificationInfo;

/**
 * implementing DynamicMBean means that we don't inherit the mbean info from
 * StandardParentMBean
 */
public class DynamicDerived1 extends StandardParent implements DynamicMBean
{
   public Object getAttribute(String attribute)
      throws AttributeNotFoundException, MBeanException, ReflectionException
   {
      return null;
   }

   public void setAttribute(Attribute attribute)
      throws AttributeNotFoundException, InvalidAttributeValueException,
      MBeanException, ReflectionException
   {
   }

   public AttributeList getAttributes(String[] attributes)
   {
      return new AttributeList();
   }

   public AttributeList setAttributes(AttributeList attributes)
   {
      return new AttributeList();
   }

   public Object invoke(String actionName,
                        Object[] params,
                        String[] signature)
      throws MBeanException, ReflectionException
   {
      return null;
   }

   public MBeanInfo getMBeanInfo()
   {
      return new MBeanInfo(this.getClass().getName(), "tester", new MBeanAttributeInfo[0],
                           new MBeanConstructorInfo[0], new MBeanOperationInfo[0], new MBeanNotificationInfo[0]);
   }
}
