/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.implementation.persistence;

import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.Descriptor;
import javax.management.ObjectName;
import javax.management.modelmbean.DescriptorSupport;
import javax.management.modelmbean.ModelMBean;
import javax.management.modelmbean.ModelMBeanInfo;
import javax.management.modelmbean.ModelMBeanInfoSupport;
import javax.management.modelmbean.ModelMBeanAttributeInfo;
import javax.management.modelmbean.RequiredModelMBean;

import org.jboss.mx.modelmbean.ModelMBeanConstants;

import org.jboss.test.jbossmx.implementation.TestCase;

import org.jboss.test.jbossmx.implementation.persistence.support.Resource;

public class OnTimerPersistenceTestCase 
   extends TestCase
   implements ModelMBeanConstants
{
   
   public OnTimerPersistenceTestCase(String s)
   {
      super(s);
   }

   public void testOnTimerCallback()
   {
      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         
         Descriptor descriptor = new DescriptorSupport();
         descriptor.setField(NAME, "Active");
         descriptor.setField(DESCRIPTOR_TYPE, ATTRIBUTE_DESCRIPTOR);
         descriptor.setField(PERSIST_POLICY, ON_TIMER);
         descriptor.setField(PERSIST_PERIOD, "1000");
         
         ModelMBeanAttributeInfo attrInfo = new ModelMBeanAttributeInfo(
               "Active",
               boolean.class.getName(),
               "Test Attribute",
               IS_READABLE,
               !IS_WRITABLE,
               !IS_IS,
               descriptor
         );
         
         ModelMBeanInfo info = new ModelMBeanInfoSupport(
               Resource.class.getName(),
               "Test Resource",
               new ModelMBeanAttributeInfo[] { attrInfo },
               null,                      // constructors
               null,                      // operations
               null                       // notification
         );
         
         ModelMBean mmb = new RequiredModelMBean();
         mmb.setManagedResource(new Resource(), OBJECT_REF);
         mmb.setModelMBeanInfo(info);
         
         ObjectName oname = new ObjectName("test:name=OnTimerCallBack");
         server.registerMBean(mmb, oname);
         
         Thread.sleep(5000);
      }
      catch (Throwable t)
      {
         log.debug("failed", t);
         fail("Creating Required ModelMBean instance with default constructor failed: " + t.toString());
      }
   }

}
