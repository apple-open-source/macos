
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jca.test;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;

import javax.resource.ResourceException;
import javax.resource.spi.ConnectionRequestInfo;
import javax.resource.spi.ManagedConnectionFactory;
import javax.security.auth.Subject;

import junit.framework.TestCase;

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

/**
 *  Unit Test for class ManagedConnectionPool
 *
 *
 * Created: Wed Jan  2 00:06:35 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */
public class BaseConnectionManagerUnitTestCase extends TestCase
{

   Logger log = Logger.getLogger(getClass());


   Subject subject = new Subject();
   ConnectionRequestInfo cri = new TestConnectionRequestInfo();
   CachedConnectionManager ccm = new CachedConnectionManager();


   /**
    * Creates a new <code>BaseConnectionManagerUnitTestCase</code> instance.
    *
    * @param name test name
    */
   public BaseConnectionManagerUnitTestCase (String name)
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

   public void testGetManagedConnections() throws Exception
   {
      InternalManagedConnectionPool.PoolParams pp = new InternalManagedConnectionPool.PoolParams();
      pp.minSize = 0;
      pp.maxSize = 5;
      pp.blockingTimeout = 100;
      pp.idleTimeout = 500;
      BaseConnectionManager2 cm = getCM(pp);
      try
      {
         ArrayList cs = new ArrayList();
         for (int i = 0; i < pp.maxSize; i++)
         {
            ConnectionListener cl = cm.getManagedConnection(null, null);
            assertTrue("Got a null connection!", cl.getManagedConnection() != null);
            cs.add(cl);
         } // end of for ()
         assertTrue("Wrong number of connections counted: " + cm.getConnectionCount(), cm.getConnectionCount() == pp.maxSize);
         try
         {
            cm.getManagedConnection(null, null);
            fail("Got a connection more than maxSize!");
         }
         catch (ResourceException re)
         {
            //expected
         } // end of try-catch
         for (Iterator i = cs.iterator(); i.hasNext();)
         {
            cm.returnManagedConnection((ConnectionListener)i.next(), true);
         } // end of for ()
         assertTrue("Wrong number of connections counted: " + cm.getConnectionCount(), cm.getConnectionCount() == 0);
      }
      finally
      {
         shutdown(cm);
      }
   }

   public void testIdleTimeout() throws Exception
   {
      InternalManagedConnectionPool.PoolParams pp = new InternalManagedConnectionPool.PoolParams();
      pp.minSize = 0;
      pp.maxSize = 5;
      pp.blockingTimeout = 10;
      pp.idleTimeout = 1000;
      BaseConnectionManager2 cm = getCM(pp);
      try
      {
         Collection mcs = new ArrayList(pp.maxSize);
         for (int i = 0 ; i < pp.maxSize; i++)
            mcs.add(cm.getManagedConnection(subject, cri));
         for (Iterator i =  mcs.iterator(); i.hasNext(); )
            cm.returnManagedConnection((ConnectionListener)i.next(), false);

         assertTrue("Wrong number of connections counted: " + cm.getConnectionCount(), cm.getConnectionCount() == pp.maxSize);
         // Let the idle remover kick in
         Thread.sleep(2500);
         assertTrue("Wrong number of connections counted: " + cm.getConnectionCount(), cm.getConnectionCount() == 0);
      }
      finally
      {
         shutdown(cm);
      }
   }

   public void testPartialIdleTimeout() throws Exception
   {
      InternalManagedConnectionPool.PoolParams pp = new InternalManagedConnectionPool.PoolParams();
      pp.minSize = 0;
      pp.maxSize = 5;
      pp.blockingTimeout = 10;
      pp.idleTimeout = 2000;
      BaseConnectionManager2 cm = getCM(pp);
      try
      {
         Collection mcs = new ArrayList(pp.maxSize);
         for (int i = 0 ; i < pp.maxSize; i++)
            mcs.add(cm.getManagedConnection(subject, cri));
         for (Iterator i =  mcs.iterator(); i.hasNext(); )
            cm.returnManagedConnection((ConnectionListener)i.next(), false);

         assertTrue("Wrong number of connections counted: " + cm.getConnectionCount(), cm.getConnectionCount() == pp.maxSize);
         Thread.sleep(1500);
         ConnectionListener cl = cm.getManagedConnection(subject, cri);
         cm.returnManagedConnection(cl, false);

         // Let the idle remover kick in
         Thread.sleep(1500);
         assertTrue("Wrong number of connections counted: " + cm.getConnectionCount(), cm.getConnectionCount() == 1);

         // Let the idle remover kick in
         Thread.sleep(1500);
         assertTrue("Wrong number of connections counted: " + cm.getConnectionCount(), cm.getConnectionCount() == 0);
      }
      finally
      {
         shutdown(cm);
      }
   }

   public void testFillToMin() throws Exception
   {
      InternalManagedConnectionPool.PoolParams pp = new InternalManagedConnectionPool.PoolParams();
      pp.minSize = 3;
      pp.maxSize = 5;
      pp.blockingTimeout = 10;
      pp.idleTimeout = 2000;
      BaseConnectionManager2 cm = getCM(pp);
      try
      {
         ConnectionListener cl = cm.getManagedConnection(subject, cri);
         cm.returnManagedConnection(cl, false);
         // Allow fill to min to work
         Thread.sleep(1000);
         assertTrue("Wrong number of connections counted: " + cm.getConnectionCount(), cm.getConnectionCount() == pp.minSize);
         // Allow the idle remover to work
         Thread.sleep(3000);
         assertTrue("Wrong number of connections counted: " + cm.getConnectionCount(), cm.getConnectionCount() == pp.minSize);
      }
      finally
      {
         shutdown(cm);
      }
   }

   public void testMisConfiguredFillToMin() throws Exception
   {
      InternalManagedConnectionPool.PoolParams pp = new InternalManagedConnectionPool.PoolParams();
      pp.minSize = 6;
      pp.maxSize = 5;
      pp.blockingTimeout = 10;
      pp.idleTimeout = 2000;
      BaseConnectionManager2 cm = getCM(pp);
      try
      {
         ConnectionListener cl = cm.getManagedConnection(subject, cri);
         cm.returnManagedConnection(cl, false);
         // Allow fill to min to work
         Thread.sleep(1000);
         assertTrue("Wrong number of connections counted: " + cm.getConnectionCount(), cm.getConnectionCount() == pp.maxSize);
         // Allow the idle remover to work
         Thread.sleep(3000);
         assertTrue("Wrong number of connections counted: " + cm.getConnectionCount(), cm.getConnectionCount() == pp.maxSize);
      }
      finally
      {
         shutdown(cm);
      }
   }

   public void testChangedMaximum() throws Exception
   {
      InternalManagedConnectionPool.PoolParams pp = new InternalManagedConnectionPool.PoolParams();
      pp.minSize = 0;
      pp.maxSize = 5;
      pp.blockingTimeout = 100;
      pp.idleTimeout = 0;
      BaseConnectionManager2 cm = getCM(pp);
      JBossManagedConnectionPool.OnePool pool = (JBossManagedConnectionPool.OnePool) cm.getPoolingStrategy();
      try
      {
         // Checkout all the connections
         ArrayList cs = new ArrayList();
         for (int i = 0; i < pp.maxSize; i++)
         {
            ConnectionListener cl = cm.getManagedConnection(null, null);
            assertTrue("Got a null connection!", cl.getManagedConnection() != null);
            cs.add(cl);
         }
         assertTrue("Wrong number of connections counted: " + cm.getConnectionCount(), cm.getConnectionCount() == pp.maxSize);

         // Reconfigure
         pp.maxSize = 6;
         pool.flush();

         // Put the connections back (should destroy/close them with no errors)
         for (Iterator i = cs.iterator(); i.hasNext();)
         {
            cm.returnManagedConnection((ConnectionListener)i.next(), true);
         }

         // Checkout all the connections with the new maximum size
         cs = new ArrayList();
         for (int i = 0; i < pp.maxSize; i++)
         {
            ConnectionListener cl = cm.getManagedConnection(null, null);
            assertTrue("Got a null connection!", cl.getManagedConnection() != null);
            cs.add(cl);
         }
         assertTrue("Wrong number of connections counted: " + cm.getConnectionCount(), cm.getConnectionCount() == pp.maxSize);

         try
         {
            cm.getManagedConnection(null, null);
            fail("Got a connection more than maxSize!");
         }
         catch (ResourceException expected)
         {
         }

         // Put the connections back into the new pool
         for (Iterator i = cs.iterator(); i.hasNext();)
            cm.returnManagedConnection((ConnectionListener)i.next(), true);
         assertTrue("Wrong number of connections counted: " + cm.getConnectionCount(), cm.getConnectionCount() == 0);
      }
      finally
      {
         shutdown(cm);
      }
   }


}//
