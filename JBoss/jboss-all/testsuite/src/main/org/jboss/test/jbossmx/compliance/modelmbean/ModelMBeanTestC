/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.compliance.modelmbean;

import org.jboss.test.jbossmx.compliance.TestCase;
import org.jboss.test.jbossmx.compliance.modelmbean.support.Resource;

import javax.management.*;
import javax.management.modelmbean.*;

public class ModelMBeanTestCase
   extends TestCase
{
   public ModelMBeanTestCase(String s)
   {
      super(s);
   }

   public void testRequiredModelMBeanConstructors()
   {
      try
      {
         new RequiredModelMBean();
      }
      catch (Throwable t)
      {
         log.debug("failed", t);
         fail("Creating Required ModelMBean instance with default constructor failed: " + t.toString());
      }
   }
   
   public void testRMMSetManagedResource()
   {
      try
      {
         ModelMBean modelmbean = new RequiredModelMBean();
         Resource resource = new Resource();
         modelmbean.setManagedResource(resource, "ObjectReference");
      }
      catch (Throwable t)
      {
         log.debug("failed", t);
         fail("Setting resource object with 'ObjectReference' type failed: " + t.toString());
      }
   }
   
   public void testRMMSetModelMBeanInfo()
   {
      try
      {
         ModelMBean modelmbean = new RequiredModelMBean();
         modelmbean.setModelMBeanInfo(getModelMBeanInfo());
      }
      catch (Throwable t)
      {
         log.debug("failed", t);
         fail("Unable to set model mbean info for resource object: " + t.toString());
      }

   }

   public void testRMMInvocation()
   {
      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         
         Resource resource = new Resource();
         ModelMBean modelmbean = new RequiredModelMBean();
         modelmbean.setModelMBeanInfo(getModelMBeanInfo());
         modelmbean.setManagedResource(resource, "ObjectReference");
         
         ObjectName name = new ObjectName("rmm:invocationTest=true");
         server.registerMBean(modelmbean, name);
         
         assertTrue(((Boolean)server.invoke(name, "isActive", new Object[] {}, new String[] {})).booleanValue());
      }
      catch (Throwable t) 
      {
         log.debug("failed", t);
         fail("RMMInvocation: " + t.toString());
      }
   }
   
   public ModelMBeanInfo getModelMBeanInfo()
   {
      final boolean READABLE = true;
      final boolean WRITABLE = true;
      final boolean BOOLEAN  = true;
      
      // build 'RoomName' read-write attribute
      Descriptor descr1 = new DescriptorSupport();
      descr1.setField("name", "Room");
      descr1.setField("descriptorType", "attribute");
      descr1.setField("displayName", "Room Number");
      descr1.setField("default", "D325");

      ModelMBeanAttributeInfo roomNameInfo =
         new ModelMBeanAttributeInfo(
            "Room",                        // attribute name
            String.class.getName(),        // attribute type
            "Room name or number.",        // description
            READABLE, WRITABLE, !BOOLEAN,  // read write
            descr1                         // descriptor
         );


      // build 'Active' read-only attribute
      Descriptor descr2 = new DescriptorSupport();
      descr2.setField("name", "Active");
      descr2.setField("descriptorType", "attribute");
      descr2.setField("getMethod", "isActive");
      descr2.setField("currencyTimeLimit", "10");

      ModelMBeanAttributeInfo activeInfo =
         new ModelMBeanAttributeInfo(
            "Active",
            boolean.class.getName(),
            "Printer state.",
            READABLE, !WRITABLE, !BOOLEAN,
            descr2
         );

      // build 'isActive' getter operation
      Descriptor descr3 = new DescriptorSupport();
      descr3.setField("name", "isActive");
      descr3.setField("descriptorType", "operation");
      descr3.setField("role", "getter");

      ModelMBeanOperationInfo isActiveInfo =
         new ModelMBeanOperationInfo(
            "isActive",                   // name & description
            "Checks if the printer is currently active.",
            null,                         // signature
            boolean.class.getName(),      // return type
            MBeanOperationInfo.INFO,      // impact
            descr3                        // descriptor
         );

      // MBean descriptor
      Descriptor descr4 = new DescriptorSupport();
      descr4.setField("name", "Printer");
      descr4.setField("descriptorType", "MBean");

      // create ModelMBeanInfo
      ModelMBeanInfo info = new ModelMBeanInfoSupport(
                               RequiredModelMBean.class.getName(),  // class name
                               "Printer",                           // description
                               new ModelMBeanAttributeInfo[] {      // attributes
                                  roomNameInfo,
                                  activeInfo
                               },
                               null,                                // constructors
                               new ModelMBeanOperationInfo[] {      // operations
                                  isActiveInfo
                               },
                               null,                                // notifications
                               descr4                               // descriptor
                            );

      return info;
   }

}
