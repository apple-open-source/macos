/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.compliance.standard.support;

public class StandardDerived2 extends StandardParent implements StandardDerived2MBean
{
   /**
    * our own MBean interface overrides the StandardParentMBean one we inherited.
    */

   public void setDerivedValue(String derived)
   {
   }

   public String getParentValue()
   {
      return null;
   }
}
