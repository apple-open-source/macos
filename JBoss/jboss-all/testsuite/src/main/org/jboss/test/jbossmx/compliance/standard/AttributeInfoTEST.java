/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.compliance.standard;

import junit.framework.TestCase;

import javax.management.MBeanAttributeInfo;
import javax.management.MBeanInfo;

/**
 * @author  <a href="mailto:trevor@protocool.com">Trevor Squires</a>.
 */

public class AttributeInfoTEST 
   extends TestCase
{
   private String failureHint;
   private MBeanInfo info;
   private String attributeName;
   private String type;
   private boolean read;
   private boolean write;
   private boolean is;

   public AttributeInfoTEST(String failureHint, MBeanInfo info, String attributeName, String type, boolean read, boolean write, boolean is)
   {
      super("testValidAttribute");
      this.failureHint = failureHint;
      this.info = info;
      this.attributeName = attributeName;
      this.type = type;
      this.read= read;
      this.write= write;
      this.is= is;
   }

   public void testValidAttribute()
   {
      MBeanAttributeInfo[] attributes = info.getAttributes();
      MBeanAttributeInfo attribute = InfoUtil.findAttribute(attributes, attributeName);

      assertNotNull(failureHint + ": " + info.getClassName() + ": " + attributeName + " was not found", attribute);
      assertEquals(failureHint + ": " + info.getClassName() + ": " + attributeName + " type", type, attribute.getType());
      assertEquals(failureHint + ": " + info.getClassName() + ": " + attributeName + " readable", read, attribute.isReadable());
      assertEquals(failureHint + ": " + info.getClassName() + ": " + attributeName + " writable", write, attribute.isWritable());
      assertEquals(failureHint + ": " + info.getClassName() + ": " + attributeName + " isIS", is, attribute.isIs());
   }
}
