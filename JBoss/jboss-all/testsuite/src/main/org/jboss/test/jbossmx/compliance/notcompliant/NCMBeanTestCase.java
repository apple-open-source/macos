/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jbossmx.compliance.notcompliant;

import org.jboss.test.jbossmx.compliance.TestCase;

import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import javax.management.MBeanServerFactory;
import javax.management.MBeanServer;
import javax.management.NotCompliantMBeanException;
import javax.management.ObjectInstance;
import java.util.Hashtable;

import org.jboss.test.jbossmx.compliance.notcompliant.support.OverloadedAttribute1;
import org.jboss.test.jbossmx.compliance.notcompliant.support.OverloadedAttribute3;
import org.jboss.test.jbossmx.compliance.notcompliant.support.OverloadedAttribute2;
import org.jboss.test.jbossmx.compliance.notcompliant.support.DynamicAndStandard;

public class NCMBeanTestCase
   extends TestCase
{
   public NCMBeanTestCase(String s)
   {
      super(s);
   }

   public void testOverloadedAttribute1()
   {
      registerAndTest(new OverloadedAttribute1());
   }

   public void testOverloadedAttribute2()
   {
      registerAndTest(new OverloadedAttribute2());
   }

   public void testOverloadedAttribute3()
   {
      registerAndTest(new OverloadedAttribute3());
   }

   public void testMixedDynamicStandard()
   {
      registerAndTest(new DynamicAndStandard());
   }

   public void testNoConstructor()
   {
      registerAndTest(new NoConstructor());
   }

   private void registerAndTest(Object mbean)
   {
      try
      {
         MBeanServer server = MBeanServerFactory.newMBeanServer();
         server.registerMBean(mbean, new ObjectName("test:foo=bar"));
         fail("expected a NotCompliantMBeanException for " + mbean.getClass().getName());
      }
      catch (NotCompliantMBeanException e)
      {
         // this is what we want
      }
      catch (Exception e)
      {
         fail("unexpected exception when registering " + mbean.getClass().getName() + ": " + e.getMessage());
      }
   }
}
