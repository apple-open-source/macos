
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jca.test;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;
import org.jboss.test.JBossTestCase;
import org.jboss.test.jca.bank.interfaces.TellerHome;
import org.jboss.test.jca.bank.interfaces.Teller;
import org.jboss.test.jca.bank.interfaces.Account;
import javax.management.ObjectName;
import javax.management.Attribute;

import org.apache.log4j.Category;

/**
 * CachedConnectionBankStressTestCase.java
 * Tests connection disconnect-reconnect mechanism.
 *
 * Created: Mon Mar 18 07:57:41 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class CachedConnectionBankStressTestCase extends JBossTestCase
{

   private TellerHome th;
   private Teller t;

   private Exception exc;

   private int iter;

   public CachedConnectionBankStressTestCase (String name)
   {
      super(name);
   }

   protected void setUp() throws Exception
   {
      ObjectName CCM = new ObjectName("jboss.jca:service=CachedConnectionManager");
      getServer().setAttribute(CCM, new Attribute("SpecCompliant", Boolean.TRUE));
      th = (TellerHome)getInitialContext().lookup("Teller");
      t = th.create();
      t.setUp();
   }

   protected void tearDown() throws Exception
   {
      if (t != null)
      {
         t.tearDown();
      } // end of if ()
      ObjectName CCM = new ObjectName("jboss.jca:service=CachedConnectionManager");
      getServer().setAttribute(CCM, new Attribute("SpecCompliant", Boolean.FALSE));

   }

   public static Test suite() throws Exception
   {
      return getDeploySetup(CachedConnectionBankStressTestCase.class, "jcabanktest.jar");
   }

   public void testCachedConnectionBank() throws Exception
   {
      Account[] accounts = new Account[getThreadCount()];
      for (int i = 0; i < getThreadCount(); i++)
      {
         accounts[i] = t.createAccount(new Integer(i));
      } // end of for ()
      final Object lock = new Object();

      iter = 0;
      getLog().info("Start test. "+getThreadCount()+ " threads, "+getIterationCount()+" iterations");
      long start = System.currentTimeMillis();

      for (int i = 0; i < getThreadCount() - 1; i++)
      {
         //Thread.sleep(500); // Wait between each client
         new Thread(new TransferThread(accounts[i],
                            accounts[(i + 1) % getThreadCount()],
                            getIterationCount(),
                            lock)).start();
         synchronized (lock)
         {
            iter++;
         }
      }

      synchronized(lock)
      {
         while(iter > 0)
         {
            lock.wait();
         }
      }

      if (exc != null) throw exc;

      for (int i = 1; i < getThreadCount() - 1; i++)
      {
         assertTrue("nonzero final balance for" + i, accounts[i].getBalance() == 0);
      } // end of for ()


      long end = System.currentTimeMillis();

      getLog().info("Time:"+(end-start));
      getLog().info("Avg. time/call(ms):"+((end-start)/(getThreadCount()*getIterationCount())));
}




   public class TransferThread implements Runnable
   {
      Category log = Category.getInstance(getClass().getName());
      Account to;
      Account from;
      int iterationCount;
      Object lock;

      public TransferThread(final Account to,
                            final Account from,
                            final int iterationCount,
                            final Object lock) throws Exception
      {
         this.to = to;
         this.from = from;
         this.iterationCount = iterationCount;
         this.lock = lock;
      }

      public void run()
      {
         try
         {

            for (int j = 0; j < iterationCount; j++)
            {
               if (exc != null) break;

               t.transfer(from,to, 1);
            }
         } catch (Exception e)
         {
            exc = e;
         }

         synchronized(lock)
         {
            iter--;
            log.info("Only "+iter+" left");
            lock.notifyAll();
         }
      }
   }
}// CachedConnectionSessionUnitTestCase
