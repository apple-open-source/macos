/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/

package org.jboss.test.deadlock.test;

import javax.naming.Context;
import javax.naming.InitialContext;
import javax.ejb.DuplicateKeyException;
import javax.ejb.ObjectNotFoundException;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Date;
import java.util.Random;

import junit.framework.Test;

import org.jboss.test.deadlock.interfaces.BeanOrder;
import org.jboss.test.deadlock.interfaces.EnterpriseEntityHome;
import org.jboss.test.deadlock.interfaces.EnterpriseEntity;
import org.jboss.test.deadlock.interfaces.StatelessSessionHome;
import org.jboss.test.deadlock.interfaces.StatelessSession;
import org.jboss.test.JBossTestCase;
import org.jboss.ejb.plugins.TxInterceptorCMT;

/**
 * Sample client for the jboss container.
 *
 * @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>
 * @version $Id: BeanStressTestCase.java,v 1.5.2.7 2003/08/28 09:20:12 ejort Exp $
 */
public class BeanStressTestCase
   extends JBossTestCase
{
   org.apache.log4j.Category log = getLog();

   static boolean deployed = false;
   static int test = 0;
   static Date startDate = new Date();

   protected final String namingFactory =
      System.getProperty(Context.INITIAL_CONTEXT_FACTORY);

   protected final String providerURL =
      System.getProperty(Context.PROVIDER_URL);

   public BeanStressTestCase(String name)
   {
      super(name);
   }

   boolean failed = false;

   private StatelessSession getSession() throws Exception
   {

      StatelessSessionHome home = (StatelessSessionHome) new InitialContext().lookup("nextgen.StatelessSession");
      return home.create();
   }

   public class RunTest implements Runnable
   {
      public String test;

      public RunTest(String test)
      {
         this.test = test;
      }

      public void run()
      {
         if (test.equals("AB"))
            runAB();
         else
            runBA();
      }

      private void runAB()
      {
         log.debug("running AB");
         try
         {
            getSession().callAB();
         }
         catch (Exception ex)
         {
            failed = true;
         }
      }

      private void runBA()
      {
         log.debug("running BA");
         try
         {
            getSession().callBA();
         }
         catch (Exception ex)
         {
            failed = true;
         }
      }
   }

   public void testDeadLock()
      throws Exception
   {
      EnterpriseEntityHome home = (EnterpriseEntityHome) new InitialContext().lookup("nextgenEnterpriseEntity");
      try
      {
         EnterpriseEntity A = home.findByPrimaryKey("A");
      }
      catch (ObjectNotFoundException ex)
      {
         home.create("A");
      }
      try
      {
         EnterpriseEntity B = home.findByPrimaryKey("B");
      }
      catch (ObjectNotFoundException ex)
      {
         home.create("B");
      }
      Thread one = new Thread(new RunTest("AB"));
      Thread two = new Thread(new RunTest("BA"));
      one.start();
      two.start();
      one.join();
      two.join();
      if (failed)
      {
         fail("testing of deadlock AB BA scenario failed");
      }
   }

   Random random = new Random();

   int target;
   int iterations;

   Object lock = new Object();
   int completed = 0;

   Exception unexpected;

   public class OrderTest
      implements Runnable
   {
      BeanOrder beanOrder;
      EnterpriseEntityHome home;

      String toStringCached;

      public OrderTest(EnterpriseEntityHome home, int beanCount, int depth)
      {
         // Create the list of beans
         ArrayList list = new ArrayList();
         for (int i = 0; i < depth; i++)
            list.add(new Integer(i % beanCount).toString());

         // Shuffle them
         Collections.shuffle(list, random);

         beanOrder = new BeanOrder((String[]) list.toArray(new String[beanCount]));
         this.home = home;
      }

      public void run()
      {
         try
         {
            EnterpriseEntity bean = home.findByPrimaryKey(beanOrder.order[0]);
            home = null;
            for (int i = 0; i < iterations; i++)
            {
               log.debug("Before: iter=" + i + " " + this);
               bean.callAnotherBean(beanOrder);
               log.debug("After : iter=" + i + " " + this);
            }
         }
         catch (Exception e)
         {
            if (TxInterceptorCMT.isADE(e) == null)
            {
               log.debug("Saw exception for " + this, e);
               unexpected = e;
            }
         }
      }

      public String toString()
      {
         if (toStringCached != null)
            return toStringCached;

         StringBuffer buffer = new StringBuffer();
         buffer.append(" hash=").append(hashCode());
         buffer.append(" order=").append(Arrays.asList(beanOrder.order));

         toStringCached = buffer.toString();
         return toStringCached;
      }
   }

   public class TestThread
      extends Thread
   {
      OrderTest test;

      public TestThread(OrderTest test)
      {
         super(test);
         this.test = test;
      }

      public void run()
      {
         super.run();
         synchronized (lock)
         {
            completed++;
            log.debug("Completed " + completed + " of " + target);
            lock.notifyAll();
         }
      }
   }

   public void waitForCompletion()
      throws Exception
   {
      log.debug("Waiting for completion");
      synchronized (lock)
      {
         while (completed < target)
         {
            lock.wait();
         }
      }
      if (unexpected != null)
      {
         log.error("Unexpected exception", unexpected);
         fail("Unexpected exception");
      }
   }

   /**
    * Creates a number of threads to invoke on the
    * session beans at random to produce deadlocks.
    * The test will timeout if a deadlock detection is missed.
    */
   public void testAllCompleteOrFail()
      throws Exception
   {
      doAllCompleteOrFail("nextgenEnterpriseEntity" ,2);
   }

   /**
    * Creates a number of threads to invoke on the
    * session beans at random to produce deadlocks.
    * The test will timeout if a deadlock detection is missed.
    */
   public void testAllCompleteOrFailReentrant()
      throws Exception
   {
      doAllCompleteOrFail("nextgenEnterpriseEntityReentrant", 4);
   }

   /**
    * Creates a number of threads to invoke on the
    * session beans at random to produce deadlocks.
    * The test will timeout if a deadlock detection is missed.
    */
   public void testAllCompleteOrFailNotSupported()
      throws Exception
   {
      doAllCompleteOrFail("nextgenEnterpriseEntityNotSupported", 2);
   }

   /**
    * Creates a number of threads to invoke on the
    * session beans at random to produce deadlocks.
    * The test will timeout if a deadlock detection is missed.
    */
   public void testAllCompleteOrFailNotSupportedReentrant()
      throws Exception
   {
      doAllCompleteOrFail("nextgenEnterpriseEntityNotSupportedReentrant", 4);
   }

   /**
    * Creates a number of threads to invoke on the
    * session beans at random to produce deadlocks.
    * The test will timeout if a deadlock detection is missed.
    */
   public void doAllCompleteOrFail(String jndiName, int depth)
      throws Exception
   {
      log.debug("========= Starting " + getName());

      iterations = getIterationCount();

      // Non-standard: We want a lot of threads and a small number of beans
      // for maximum contention
      // target = getThreadCount();
      // int beanCount = getBeanCount();
      target = 40;
      int beanCount = 2;
      completed = 0;
      unexpected = null;

      // Create some beans
      EnterpriseEntityHome home = (EnterpriseEntityHome) new InitialContext().lookup(jndiName);
      for (int i = 0; i < beanCount; i++)
      {
         try
         {
            home.create(new Integer(i).toString());
         }
         catch (DuplicateKeyException weDontCare)
         {
         }
      }

      // Create some threads
      TestThread[] threads = new TestThread[target];
      for (int i = 0; i < target; i++)
         threads[i] = new TestThread(new OrderTest(home, beanCount, depth));

      // Start the threads
      for (int i = 0; i < target; i++)
      {
         log.debug("Starting " + threads[i].test);
         threads[i].start();
      }

      waitForCompletion();

      log.debug("========= Completed " + getName());
   }

   public class CMRTest
      implements Runnable
   {
      StatelessSession session;
      String jndiName;
      String start;

      public CMRTest(StatelessSession session, String jndiName, String start)
      {
         this.session = session;
         this.jndiName = jndiName;
         this.start = start;
      }

      public void run()
      {
         try
         {
            for (int i = 0; i < iterations; ++i)
               session.cmrTest(jndiName, start);
         }
         catch (Exception e)
         {
            if (TxInterceptorCMT.isADE(e) == null)
            {
               log.debug("Saw exception for " + this, e);
               unexpected = e;
            }
         }
      }

      public String toString()
      {
         return hashCode() + " " + start;
      }
   }

   public class CMRTestThread
      extends Thread
   {
      CMRTest test;

      public CMRTestThread(CMRTest test)
      {
         super(test);
         this.test = test;
      }

      public void run()
      {
         super.run();
         synchronized (lock)
         {
            completed++;
            log.debug("Completed " + completed + " of " + target);
            lock.notifyAll();
         }
      }
   }

   /**
    * Creates a number of threads to CMR relationships.
    * The test will timeout if a deadlock detection is missed.
    */
   public void testAllCompleteOrFailCMR()
      throws Exception
   {
      doAllCompleteOrFailCMR("local/nextgenEnterpriseEntity");
   }

   /**
    * Creates a number of threads to CMR relationships.
    * The test will timeout if a deadlock detection is missed.
    */
   public void doAllCompleteOrFailCMR(String jndiName)
      throws Exception
   {
      log.debug("========= Starting " + getName());

      iterations = getIterationCount();

      // Non-standard: We want a lot of threads and a small number of beans
      // for maximum contention
      // target = getThreadCount();
      // int beanCount = getBeanCount();
      target = 40;
      completed = 0;
      unexpected = null;

      // Create some beans
      StatelessSessionHome home = (StatelessSessionHome) new InitialContext().lookup("nextgen.StatelessSession");
      StatelessSession session = home.create();
      session.createCMRTestData(jndiName);

      // Create some threads
      CMRTestThread[] threads = new CMRTestThread[target];
      for (int i = 0; i < target; i++)
         threads[i] = new CMRTestThread(new CMRTest(session, jndiName, i % 2 == 0 ? "First" : "Second"));

      // Start the threads
      for (int i = 0; i < target; i++)
      {
         log.debug("Starting " + threads[i].test);
         threads[i].start();
      }

      waitForCompletion();

      log.debug("========= Completed " + getName());
   }

   /*   
   public void testRequiresNewDeadlock() 
      throws Exception
   {
      
      EnterpriseEntityHome home = (EnterpriseEntityHome)new InitialContext().lookup("nextgenEnterpriseEntity");
      try
      {
	 EnterpriseEntity C = home.findByPrimaryKey("C");
      }
      catch (ObjectNotFoundException ex)
      {
	 home.create("C");
      }

      boolean deadlockExceptionThrown = false;
      try
      {
         getSession().requiresNewTest(true);
      }
      catch (RemoteException ex)
      {
         if (ex.detail instanceof ApplicationDeadlockException)
         {
            deadlockExceptionThrown = true;
         }
      }
      assertTrue("ApplicationDeadlockException was not thrown", deadlockExceptionThrown);
   }
   */

   public void testCleanup() throws Exception
   {
      // Restart the db pool
      super.restartDBPool();
   }

   public static Test suite() throws Exception
   {
      return getDeploySetup(BeanStressTestCase.class, "deadlock.jar");
   }
}
