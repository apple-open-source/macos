/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.standard;

import junit.framework.TestCase;

import javax.management.MBeanAttributeInfo;
import javax.management.MBeanInfo;

/**
 * @author  <a href="mailto:trevor@protocool.com">Trevor Squires</a>.
 */

public class SpuriousAttributeTEST extends TestCase
{
   private String failureHint;
   private MBeanInfo info;
   private String attributeName;

   public SpuriousAttributeTEST(String failureHint, MBeanInfo info, String attributeName)
   {
      super("testForSpuriousAttribute");
      this.failureHint = failureHint;
      this.info = info;
      this.attributeName = attributeName;
   }

   public void testForSpuriousAttribute()
   {
      MBeanAttributeInfo[] attributes = info.getAttributes();
      assertNull(failureHint + ": attribute " + info.getClassName() + ": " + attributeName + " should not be present",
                 InfoUtil.findAttribute(attributes, attributeName));
   }
}
