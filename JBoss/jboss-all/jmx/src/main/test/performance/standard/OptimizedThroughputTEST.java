/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.performance.standard;

import junit.framework.TestCase;
import test.performance.PerformanceSUITE;
import test.performance.standard.support.Standard;

import javax.management.*;

import org.jboss.mx.server.ServerConstants;


public class OptimizedThroughputTEST extends TestCase
    implements ServerConstants
{

   public OptimizedThroughputTEST(String s)
   {
      super(s);
   }

   public void testThroughput() throws Exception
   {
   
      class MyThread implements Runnable 
      {

         private boolean keepRunning = true;
         
         public void run() 
         {
            try
            {
               Thread.sleep(PerformanceSUITE.THROUGHPUT_TIME);
            }
            catch (InterruptedException e)
            {
               
            }
            
            keepRunning = false;
         }
         
         public boolean isKeepRunning()
         {
            return keepRunning;
         }
      }
   
      MyThread myThread = new MyThread();
      Thread t = new Thread(myThread);
      Standard std = new Standard();

      String method      = "mixedArguments";
      String[] signature = new String[] { 
                              Integer.class.getName(),
                              int.class.getName(),
                              Object[][][].class.getName(),
                              Attribute.class.getName()
                           };
                           
      Object[] args      = new Object[] {
                              new Integer(1234),
                              new Integer(455617),
                              new Object[][][] {
                                 { 
                                    { "1x1x1", "1x1x2", "1x1x3" },
                                    { "1x2x1", "1x2x2", "1x2x3" },
                                    { "1x3x1", "1x3x2", "1x3x3" }
                                 },
                                 
                                 {
                                    { "2x1x1", "2x1x2", "2x1x3" },
                                    { "2x2x1", "2x2x2", "2x2x3" },
                                    { "2x3x1", "2x3x2", "2x3x3" }
                                 },
                                 
                                 {
                                    { "3x1x1", "3x1x2", "3x1x3" },
                                    { "3x2x1", "3x2x2", "3x2x3" },
                                    { "3x3x1", "3x3x2", "3x3x3" }
                                 }
                              },
                              new Attribute("attribute", "value")
                           };
   
      System.setProperty(OPTIMIZE_REFLECTED_DISPATCHER, "true");

      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         ObjectName name = new ObjectName("test:test=test");
      
         server.registerMBean(std, name);
         
         t.start();
         while(myThread.isKeepRunning())
         {
            server.invoke(name, method, args, signature);
         }

         System.out.println("\nStandard MBean Throughput (OPTIMIZED): " + 
                             std.getCount() / (PerformanceSUITE.THROUGHPUT_TIME / PerformanceSUITE.SECOND) +
                            " invocations per second.");
         System.out.println("(Total: " + std.getCount() + ")\n");
      }
      finally
      {
         System.setProperty(OPTIMIZE_REFLECTED_DISPATCHER, "false");
      }
   }

   
}
