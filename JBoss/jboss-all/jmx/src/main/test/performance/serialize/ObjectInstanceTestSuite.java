/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.performance.serialize;

import test.performance.serialize.support.Standard;

import junit.framework.Test;
import junit.framework.TestSuite;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;

import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.ObjectInstance;
import javax.management.ObjectName;

/**
 * Tests the size and speed of ObjectName serialization
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class ObjectInstanceTestSuite
   extends TestSuite
{
   // Attributes ----------------------------------------------------------------

   // Constructor ---------------------------------------------------------------

   public static void main(String[] args)
   {
      junit.textui.TestRunner.run(suite());
   }

   /**
    * Construct the tests
    */
   public static Test suite()
   {
      TestSuite suite = new TestSuite("All Object Instance tests");

      MBeanServer server = null;

      try
      {
         server = MBeanServerFactory.createMBeanServer();
         ObjectName name = new ObjectName("a:a=a");
         ObjectInstance instance = server.registerMBean(new Standard(), name);
         // Speed Tests
         suite.addTest(new SerializeTEST("testIt", instance, "ObjectInstance"));
      }
      catch (Exception e)
      {
         throw new Error(e.toString());
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }

      return suite;
   }
}
