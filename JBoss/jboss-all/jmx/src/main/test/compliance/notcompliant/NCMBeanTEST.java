/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package test.compliance.notcompliant;

import junit.framework.TestCase;

import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import javax.management.MBeanServerFactory;
import javax.management.MBeanServer;
import javax.management.NotCompliantMBeanException;
import javax.management.ObjectInstance;
import java.util.Hashtable;

import test.compliance.notcompliant.support.OverloadedAttribute1;
import test.compliance.notcompliant.support.OverloadedAttribute2;
import test.compliance.notcompliant.support.OverloadedAttribute3;
import test.compliance.notcompliant.support.OverloadedAttribute4;
import test.compliance.notcompliant.support.OverloadedAttribute5;
import test.compliance.notcompliant.support.InterfaceProblems;
import test.compliance.notcompliant.support.DynamicAndStandard;

public class NCMBeanTEST extends TestCase
{
   public NCMBeanTEST(String s)
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

   public void testOverloadedAttribute4()
   {
      registerAndTest(new OverloadedAttribute4());
   }

   public void testOverloadedAttribute5()
   {
      registerAndTest(new OverloadedAttribute5());
   }

   public void testMixedDynamicStandard()
   {
      registerAndTest(new DynamicAndStandard());
   }

   public void testNoConstructor()
   {
      registerAndTest(new NoConstructor());
   }

   public void testInterfaceProblems()
   {
      registerAndDontTest(new InterfaceProblems());
   }

   private void registerAndTest(Object mbean)
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
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
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   private void registerAndDontTest(Object mbean)
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         server.registerMBean(mbean, new ObjectName("test:foo=bar"));
      }
      catch (NotCompliantMBeanException e)
      {
         fail("FAILS IN RI: Cannot cope with overriden get/is in interfaces");
      }
      catch (Exception e)
      {
         fail("unexpected exception when registering " + mbean.getClass().getName() + ": " + e.getMessage());
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }
}
