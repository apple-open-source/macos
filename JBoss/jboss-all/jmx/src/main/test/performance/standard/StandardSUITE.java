/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.performance.standard;

import junit.framework.Test;
import junit.framework.TestSuite;

import javax.management.JMException;
import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.ObjectName;

/**
 * Suite of performance tests for Standard MBeans.
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.3 $  
 */
public class StandardSUITE extends TestSuite
{
   public static void main(String[] args)
   {
      junit.textui.TestRunner.run(suite());
   }

   public static Test suite()
   {
      TestSuite suite = new TestSuite("Performance tests for Standard MBeans");

      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
      
         if ("JBossMX".equalsIgnoreCase((String)server.getAttribute(new ObjectName("JMImplementation:type=MBeanServerDelegate"), "ImplementationName")))
         {
            //suite.addTest(new TestSuite(OptimizedInvocationTEST.class));
            suite.addTest(new TestSuite(OptimizedThroughputTEST.class));
         }
      }
      catch (JMException e)
      {
         System.err.println("Unable to run optimized tests: " + e.toString());
      }
         
      
      //suite.addTest(new TestSuite(InvocationTEST.class));
      suite.addTest(new TestSuite(ThroughputTEST.class));
      
      return suite;
   }

}
