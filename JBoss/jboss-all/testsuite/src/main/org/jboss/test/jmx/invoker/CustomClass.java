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
 */
public class CustomClass
   implements java.io.Serializable
{
   private String value;

   public CustomClass(String value)
   {
      this.value = value;
   }

   public String getValue()
   {
      return value;
   }
}