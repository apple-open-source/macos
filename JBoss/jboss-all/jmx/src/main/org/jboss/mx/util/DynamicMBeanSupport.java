/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.util;

import javax.management.DynamicMBean;
import javax.management.AttributeNotFoundException;
import javax.management.MBeanException;
import javax.management.ReflectionException;
import javax.management.Attribute;
import javax.management.InvalidAttributeValueException;
import javax.management.AttributeList;
import javax.management.MBeanInfo;

/** A noop stub implementation of DynamicMBean
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class DynamicMBeanSupport implements DynamicMBean
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
      return null;
   }

   public AttributeList setAttributes(AttributeList attributes)
   {
      return null;
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
      return null;
   }
}
