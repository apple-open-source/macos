/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jbossmq.perf;

import junit.textui.TestRunner;
import EDU.oswego.cs.dl.util.concurrent.Semaphore;

/**
 * JBossMQPerfStressTestCase.java Some simple tests of JBossMQ
 *
 * @author    <a href="mailto:hiram.chirino@jboss.org">Hiram Chirino</a>
 * @version   $Revision: 1.1.2.2 $
 */

public class RMIInvocationLayerStressTestCase extends InvocationLayerStressTest
{
   /**
    * Constructor for the JBossMQPerfStressTestCase object
    *
    * @param name           Description of Parameter
    * @exception Exception  Description of Exception
    */
   public RMIInvocationLayerStressTestCase(String name) throws Exception
   {
      super(name);
   }

   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testRMIMutliSessionOneConnection() throws Exception
   {
      getLog().debug("Starting RMI MutliSessionOneConnection test");

      connect("RMIConnectionFactory", "RMIConnectionFactory");
      queueConnection.start();
      exitSemaphore = new Semaphore(-WORKER_COUNT);
      exitSemaphore.release();

      getLog().debug("Creating workers.");
      QueueWorker workers[] = new QueueWorker[WORKER_COUNT];
      for (int i = 0; i < WORKER_COUNT; i++)
      {
         workers[i] = new QueueWorker("ConnectionTestQueue-" + i, "RMI");
      }

      getLog().debug("Starting workers.");
      for (int i = 0; i < WORKER_COUNT; i++)
      {
         workers[i].start();
      }

      getLog().debug("Waiting for workers to finish.");
      exitSemaphore.acquire();

      disconnect();
      getLog().debug("RMI MutliSessionOneConnection passed");
   }

   /**
    * The main entry-point for the JBossMQPerfStressTestCase class
    *
    * @param args  The command line arguments
    */
   public static void main(String[] args)
   {

      String newArgs[] = { "org.jboss.test.jbossmq.perf.RMIInvocationLayerStressTestCase" };
      TestRunner.main(newArgs);

   }

}
