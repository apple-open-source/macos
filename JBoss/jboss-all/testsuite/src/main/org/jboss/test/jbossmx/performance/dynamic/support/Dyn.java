/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.performance.dynamic.support;

import javax.management.*;

/**
 * Dynamic MBean with a single void management operation.
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $
 *   
 */
public class Dyn
         implements DynamicMBean
{

   private int counter = 0;

   public Object getAttribute(String attribute)
   throws AttributeNotFoundException, MBeanException, ReflectionException
   {
      return null;
   }

   public void setAttribute(Attribute attribute)
   throws AttributeNotFoundException, InvalidAttributeValueException, MBeanException, ReflectionException
      {}

   public AttributeList getAttributes(String[] attributes)
   {
      return null;
   }

   public AttributeList setAttributes(AttributeList attributes)
   {
      return null;
   }

   public Object invoke(String actionName, Object[] params, String[] signature)
   throws MBeanException, ReflectionException
   {
      if (actionName.equals("methodInvocation"))
      {
         methodInvocation();
         return null;
      }

      else if (actionName.equals("counter"))
      {
         countInvocation();
         return null;
      }

      else if (actionName.equals("mixedArguments"))
      {
         myMethod((Integer)params[0], ((Integer)params[1]).intValue(),
                  (Object[][][])params[2], (Attribute)params[3]);
      
         return null;
      }
      
      return null;
   }

   public MBeanInfo getMBeanInfo()
   {

      return new MBeanInfo(
                "test.performance.dynamic.support.Dynamic", "",
                null,
                null,
                new MBeanOperationInfo[] { 
                     new MBeanOperationInfo(
                           "methodInvocation", "",
                           null, void.class.getName(), 0)
                     ,
                     new MBeanOperationInfo(
                           "counter", "",
                           null, void.class.getName(), 0)
                     },      
                null
             );
   }

   private void methodInvocation()
   {}

   private void countInvocation()
   {
      ++counter;
   }

   public void myMethod(Integer int1, int int2, Object[][][] space, Attribute attr)
   {
      
   }
   
   public int getCount()
   {
      return counter;
   }
}




