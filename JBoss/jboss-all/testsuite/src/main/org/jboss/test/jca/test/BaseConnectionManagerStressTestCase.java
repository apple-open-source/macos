
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jca.test;

import javax.resource.ResourceException;
import javax.resource.spi.ConnectionRequestInfo;
import javax.resource.spi.ManagedConnectionFactory;
import javax.security.auth.Subject;

import org.jboss.logging.Logger;
import org.jboss.resource.connectionmanager.BaseConnectionManager2;
import org.jboss.resource.connectionmanager.CachedConnectionManager;
import org.jboss.resource.connectionmanager.ConnectionListener;
import org.jboss.resource.connectionmanager.InternalManagedConnectionPool;
import org.jboss.resource.connectionmanager.JBossManagedConnectionPool;
import org.jboss.resource.connectionmanager.ManagedConnectionPool;
import org.jboss.resource.connectionmanager.NoTxConnectionManager;
import org.jboss.test.jca.adapter.TestConnectionRequestInfo;
import org.jboss.test.jca.adapter.TestManagedConnectionFactory;
import org.jboss.test.JBossTestCase;

/**
 *  Unit Test for class ManagedConnectionPool
 *
 *
 * Created: Wed Jan  2 00:06:35 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */
public class BaseConnectionManagerStressTestCase extends JBossTestCase
{

   Logger log = Logger.getLogger(getClass());

   boolean failed;
   ResourceException error;
   int startedThreadCount;
   final Object startedLock = new Object();
   int finishedThreadCount;
   final Object finishedLock = new Object();
   int connectionCount;
   int errorCount;
   float elapsed = 0;
   float held = 0;
   float getConnection = 0;
   float returnConnection = 0;

   Subject subject = new Subject();
   ConnectionRequestInfo cri = new TestConnectionRequestInfo();
   CachedConnectionManager ccm = new CachedConnectionManager();


   /**
    * Creates a new <code>BaseConnectionManagerStressTestCase</code> instance.
    *
    * @param name test name
    */
   public BaseConnectionManagerStressTestCase (String name)
   {
      super(name);
   }


   private BaseConnectionManager2 getCM(
      InternalManagedConnectionPool.PoolParams pp)
      throws Exception
   {
      ManagedConnectionFactory mcf = new TestManagedConnectionFactory();
      ManagedConnectionPool poolingStrategy = new JBossManagedConnectionPool.OnePool(mcf, pp, false, log);
      BaseConnectionManager2 cm = new NoTxConnectionManager(ccm, poolingStrategy);
      poolingStrategy.setConnectionListenerFactory(cm);
      return cm;
   }


   private void shutdown(BaseConnectionManager2 cm)
   {
      JBossManagedConnectionPool.OnePool pool = (JBossManagedConnectionPool.OnePool) cm.getPoolingStrategy();
      pool.shutdown();
   }

   protected void setUp()
   {
      log.debug("================> Start " + getName());
   }

   protected void tearDown()
   {
      log.debug("================> End " + getName());
   }

   public void testShortBlockingNoFill()
      throws Exception
   {
      doShortBlocking(20, 0, 5000);
   }

   public void testShortBlockingFill()
      throws Exception
   {
      doShortBlocking(20, getBeanCount(), 5000);
   }

   public void testShortBlockingPartFill()
      throws Exception
   {
      doShortBlocking(20, getBeanCount()/2, 5000);
   }

   public void testShortBlockingNearlyFill()
      throws Exception
   {
      doShortBlocking(20, getBeanCount() - 1, 5000);
   }

   public void testShortBlockingAggressiveRemoval()
      throws Exception
   {
      doShortBlocking(20, 0, 10);
   }

   public void testShortBlockingAggressiveRemovalAndFill()
      throws Exception
   {
      doShortBlocking(20, getBeanCount(), 10);
   }

   /**
    * The testShortBlocking test tries to simulate extremely high load on the pool,
    * with a short blocking timeout.  It tests fairness in scheduling servicing
    * requests. The work time is modeled by sleepTime. Allowing overhead of
    * 15 ms/pool request, the blocking is calculated at
    * (worktime + overhead) * (threadsPerConnection)
    *
    * @exception Exception if an error occurs
    */
   public void doShortBlocking(long sleep, int min, long idle) throws Exception
   {  
      startedThreadCount = 0;
      finishedThreadCount = 0;
      connectionCount = 0;
      errorCount = 0;

      final int reps = getIterationCount();
      final int threadsPerConnection = getThreadCount();
      final long sleepTime = sleep;
      failed = false;
      InternalManagedConnectionPool.PoolParams pp = new InternalManagedConnectionPool.PoolParams();
      pp.minSize = min;
      pp.maxSize = getBeanCount();
      pp.blockingTimeout =  (threadsPerConnection) * ((int)sleepTime + 15);
      if (pp.blockingTimeout < 1000)
         pp.blockingTimeout = 1000;
      pp.idleTimeout = idle;
      final BaseConnectionManager2 cm = getCM(pp);
      try
      {
         int totalThreads = pp.maxSize * threadsPerConnection;
         log.info("ShortBlocking test with connections: " + pp.maxSize + " totalThreads: " + totalThreads + " reps: " + reps);
         for (int i = 0; i < totalThreads; i++)
         {
            Runnable t = new Runnable()
            {
               int id;
               public void run()
               {
                  synchronized (startedLock)
                  {
                     id = startedThreadCount;
                     startedThreadCount++;
                     startedLock.notify();
                  }
                  long duration = 0;
                  long getConnection = 0;
                  long returnConnection = 0;
                  long heldConnection = 0;
                  for (int j = 0; j < reps; j++)
                  {
                     try
                     {
                        long startGetConnection = System.currentTimeMillis();
                        ConnectionListener cl = cm.getManagedConnection(null, null);
                        long endGetConnection = System.currentTimeMillis();
                        //maybe should be synchronized
                        BaseConnectionManagerStressTestCase.this.connectionCount++;
                        Thread.sleep(sleepTime);
                        long startReturnConnection = System.currentTimeMillis();
                        cm.returnManagedConnection(cl, false);
                        long endReturnConnection = System.currentTimeMillis();
                        
                        duration += (endReturnConnection - startGetConnection);
                        getConnection += (endGetConnection - startGetConnection);
                        returnConnection += (endReturnConnection - startReturnConnection);
                        heldConnection += (startReturnConnection - endGetConnection);
                      }
                      catch (ResourceException re)
                      {
                         BaseConnectionManagerStressTestCase.this.log.info("error: iterationCount: " + j + ", connectionCount: " + BaseConnectionManagerStressTestCase.this.connectionCount + " " + re.getMessage());
                         BaseConnectionManagerStressTestCase.this.errorCount++;
                         BaseConnectionManagerStressTestCase.this.error = re;
                         BaseConnectionManagerStressTestCase.this.failed = true;
                      } // end of try-catch
                      catch (InterruptedException ie)
                      {
                         break;
                      } // end of catch


                   }
                   synchronized (BaseConnectionManagerStressTestCase.this)
                   {
                      BaseConnectionManagerStressTestCase.this.elapsed += duration;
                      BaseConnectionManagerStressTestCase.this.getConnection += getConnection;
                      BaseConnectionManagerStressTestCase.this.returnConnection += returnConnection;
                      BaseConnectionManagerStressTestCase.this.held += heldConnection;
                   }
                   synchronized (finishedLock)
                   {
                     finishedThreadCount++;
                     finishedLock.notify();
                  }
               }
            };
            new Thread(t).start();
            synchronized (startedLock)
            {
               while (startedThreadCount < i + 1)
               {
                  startedLock.wait();
               } // end of while ()
            }
         } // end of for ()
         synchronized (finishedLock)
         {
            while (finishedThreadCount < totalThreads)
            {
               finishedLock.wait();
            } // end of while ()
         }
         float expected = totalThreads * reps;
         float lessWaiting = getConnection - (threadsPerConnection - 1) * held;
         log.info("completed " + getName() + " with connectionCount: " + connectionCount + ", expected : " + expected);
         log.info("errorCount: " + errorCount + " %error=" + ((100 * errorCount) / expected));
         log.info("Total time elapsed: " + elapsed  + ", perRequest: " + (elapsed / connectionCount));
         log.info("Total time held   : " + held  + ", perRequest: " + (held / connectionCount));
         log.info("Time getConnection: " + getConnection  + ", perRequest: " + (getConnection / connectionCount));
         log.info("     lessWaiting  : " + lessWaiting  + ", perRequest: " + (lessWaiting / connectionCount));
         log.info("Time retConnection: " + returnConnection  + ", perRequest: " + (returnConnection / connectionCount));
         assertTrue("Wrong number of connections counted: " + cm.getConnectionCount(), cm.getConnectionCount() == pp.maxSize);
         assertTrue("Blocking Timeout occurred in ShortBlocking test: " + error, !failed);
      }
      finally
      {
         shutdown(cm);
      }
   }
}//
