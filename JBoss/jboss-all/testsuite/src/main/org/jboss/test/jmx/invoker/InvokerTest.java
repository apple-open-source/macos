/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jmx.invoker;

/**
 * Used in JMX invoker adaptor test.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 *
 * @jmx:mbean name="jboss.test:service=InvokerTest"
 */
public class InvokerTest
   implements InvokerTestMBean
{
   private CustomClass custom = new CustomClass("InitialValue");

   /**
    * @jmx:managed-attribute
    */
   public String getSomething()
   {
      return "something";
   }

   /**
    * @jmx:managed-attribute
    */
   public CustomClass getCustom()
   {
      return custom;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setCustom(CustomClass custom)
   {
      this.custom = custom;
   }

   /**
    * @jmx:managed-operation
    */
   public CustomClass doSomething(CustomClass custom)
   {
      return new CustomClass(custom.getValue());
   }
}