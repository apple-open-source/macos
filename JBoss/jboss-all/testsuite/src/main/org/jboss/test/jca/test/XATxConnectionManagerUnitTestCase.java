
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jca.test; // Generated package name

import java.util.HashSet;
import java.util.Set;

import javax.resource.spi.ConnectionRequestInfo;
import javax.transaction.TransactionManager;

import org.jboss.logging.Logger;
import org.jboss.resource.connectionmanager.CachedConnectionManager;
import org.jboss.resource.connectionmanager.InternalManagedConnectionPool;
import org.jboss.resource.connectionmanager.JBossManagedConnectionPool;
import org.jboss.resource.connectionmanager.ManagedConnectionPool;
import org.jboss.resource.connectionmanager.XATxConnectionManager;
import org.jboss.test.JBossTestCase;
import org.jboss.test.jca.adapter.TestConnection;
import org.jboss.test.jca.adapter.TestConnectionRequestInfo;
import org.jboss.test.jca.adapter.TestManagedConnectionFactory;
import org.jboss.tm.TxManager;
import org.jboss.tm.usertx.client.ServerVMClientUserTransaction;

/**
 * XATxConnectionManagerUnitTestCase.java
 *
 *
 * Created: Mon Jan 14 00:43:40 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class XATxConnectionManagerUnitTestCase extends JBossTestCase
{
   Logger log = Logger.getLogger(getClass());

   private TransactionManager tm;
   private ServerVMClientUserTransaction ut;
   private CachedConnectionManager ccm;
   private TestManagedConnectionFactory mcf;
   private XATxConnectionManager cm;
   private ConnectionRequestInfo cri;

   private int poolSize = 5;

   public XATxConnectionManagerUnitTestCase (String name)
   {
      super(name);
   }

   protected void setUp() throws Exception
   {
      log.debug("================> Start " + getName());
      tm = TxManager.getInstance();
      ut = new ServerVMClientUserTransaction(tm);
      ccm = new CachedConnectionManager();
      ut.registerTxStartedListener(ccm);

      mcf = new TestManagedConnectionFactory();
      InternalManagedConnectionPool.PoolParams pp = new InternalManagedConnectionPool.PoolParams();
      pp.minSize = 0;
      pp.maxSize = poolSize;
      pp.blockingTimeout = 100;
      pp.idleTimeout = 500;
      ManagedConnectionPool poolingStrategy = new JBossManagedConnectionPool.OnePool(mcf, pp, false, log);
      cri = new TestConnectionRequestInfo();
      cm = new XATxConnectionManager(ccm, poolingStrategy, tm);
      poolingStrategy.setConnectionListenerFactory(cm);
   }

   protected void tearDown() throws Exception
   {
      JBossManagedConnectionPool.OnePool pool = (JBossManagedConnectionPool.OnePool) cm.getPoolingStrategy();
      pool.shutdown();
      ut = null;
      log.debug("================> End " + getName());
   }

   public void testGetConnection() throws Exception
   {
      getLog().info("testGetConnection");
      TestConnection c = (TestConnection)cm.allocateConnection(mcf, cri);
      assertTrue("Connection is null", c != null);
      c.close();
   }

   public void testEnlistInExistingTx() throws Exception
   {
      getLog().info("testEnlistInExistingTx");
      ut.begin();
      TestConnection c = (TestConnection)cm.allocateConnection(mcf, cri);
      assertTrue("Connection not enlisted in tx!", c.isInTx());
      c.close();
      assertTrue("Connection still enlisted in tx!", !c.isInTx());
      ut.commit();
      assertTrue("Connection still enlisted in tx!", !c.isInTx());
   }

   public void testEnlistCheckedOutConnectionInNewTx() throws Exception
   {
      getLog().info("testEnlistCheckedOutConnectionInNewTx");
      Object key = this;
      Set unshared = new HashSet();
      ccm.pushMetaAwareObject(key, unshared);
      TestConnection c = (TestConnection)cm.allocateConnection(mcf, cri);
      assertTrue("Connection already enlisted in tx!", !c.isInTx());
      ut.begin();
      assertTrue("Connection not enlisted in tx!", c.isInTx());

      ut.commit();
      assertTrue("Connection still enlisted in tx!", !c.isInTx());
      c.close();
      ccm.popMetaAwareObject(unshared);
   }

   /** Tests the spec required behavior of reconnecting connection
    * handles left open on return from an ejb method call.  Since this
    * behavior is normally turned off, we must set SpecCompliant on
    * the ccm to true first.
    */
   public void testReconnectConnectionHandlesOnNotification() throws Exception
   {
      getLog().info("testReconnectConnectionHandlesOnNotification");
      ccm.setSpecCompliant(true);
      Object key1 = new Object();
      Object key2 = new Object();
      Set unshared = new HashSet();
      ccm.pushMetaAwareObject(key1, unshared);
      ut.begin();
      ccm.pushMetaAwareObject(key2, unshared);
      TestConnection c = (TestConnection)cm.allocateConnection(mcf, cri);
      assertTrue("Connection not enlisted in tx!", c.isInTx());
      ccm.popMetaAwareObject(unshared);//key2
      ut.commit();
      ut.begin();
      ccm.pushMetaAwareObject(key2, unshared);
      assertTrue("Connection not enlisted in tx!", c.isInTx());

      ccm.popMetaAwareObject(unshared);//key2
      ut.commit();
      assertTrue("Connection still enlisted in tx!", !c.isInTx());
      ccm.pushMetaAwareObject(key2, unshared);
      c.close();
      ccm.popMetaAwareObject(unshared);//key2
      ccm.popMetaAwareObject(unshared);//key1
  }

  public void testEnlistAfterMarkRollback() throws Exception
  {
     // Get a transaction and mark it for rollback
     tm.begin();
     try
     {
        tm.setRollbackOnly();
        // Allocate a connection upto the pool size all should fail
        for (int i = 0; i < poolSize; ++i)
        {
           try
           {
              cm.allocateConnection(mcf, cri);
              fail("Should not be allowed to allocate a connection with setRollbackOnly()");
           }
           catch (Exception e)
           {
              log.debug("Error allocating connection", e);
           }
        }
     }
     finally
     {
        tm.rollback();
     }

     // We should be able to get a connection now
     testGetConnection();
  }

}// XATxConnectionManagerUnitTestCase
