/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.resource.connectionmanager;

import java.util.Collection;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

import javax.management.ObjectName;
import javax.naming.InitialContext;
import javax.resource.ResourceException;
import javax.resource.spi.ConnectionEvent;
import javax.resource.spi.ConnectionRequestInfo;
import javax.resource.spi.ManagedConnection;
import javax.security.auth.Subject;
import javax.transaction.RollbackException;
import javax.transaction.Status;
import javax.transaction.Synchronization;
import javax.transaction.SystemException;
import javax.transaction.Transaction;
import javax.transaction.TransactionManager;
import javax.transaction.xa.XAException;
import javax.transaction.xa.XAResource;
import javax.transaction.xa.Xid;

import org.jboss.logging.Logger;
import org.jboss.tm.TransactionLocal;
import org.jboss.tm.TxUtils;

/**
 * The LocalTxConnectionManager is a JBoss ConnectionManager
 * implementation for jca adapters implementing LocalTransaction support.
 * It implements a ConnectionEventListener that implements XAResource to
 * manage transactions through the Transaction Manager. To assure that all
 * work in a local transaction occurs over the same ManagedConnection, it
 * includes a xid to ManagedConnection map.  When a Connection is requested
 * or a transaction started with a connection handle in use, it checks to
 * see if a ManagedConnection already exists enrolled in the global
 * transaction and uses it if found. Otherwise a free ManagedConnection
 * has its LocalTransaction started and is used.  From the
 * BaseConnectionManager2, it includes functionality to obtain managed
 * connections from
 * a ManagedConnectionPool mbean, find the Subject from a SubjectSecurityDomain,
 * and interact with the CachedConnectionManager for connections held over
 * transaction and method boundaries.  Important mbean references are to a
 * ManagedConnectionPool supplier (typically a JBossManagedConnectionPool), and a
 * RARDeployment representing the ManagedConnectionFactory.
 *
 *
 *
 * This connection manager has to perform the following operations:
 *
 * 1. When an application component requests a new ConnectionHandle,
 *    it must find a ManagedConnection, and make sure a
 *    ConnectionEventListener is registered. It must inform the
 *    CachedConnectionManager that a connection handle has been given
 *    out. It needs to count the number of handles for each
 *    ManagedConnection.  If there is a current transaction, it must
 *    enlist the ManagedConnection's LocalTransaction in the transaction
 *    using the ConnectionEventListeners XAResource XAResource implementation.
 * Entry point: ConnectionManager.allocateConnection.
 * written.
 *
 * 2. When a ConnectionClosed event is received from the
 *    ConnectionEventListener, it must reduce the handle count.  If
 *    the handle count is zero, the XAResource should be delisted from
 *    the Transaction, if any. The CachedConnectionManager must be
 *    notified that the connection is closed.
 * Entry point: ConnectionEventListener.ConnectionClosed.
 * written
 *
 *3. When a transaction begun notification is received from the
 * UserTransaction (via the CachedConnectionManager, all
 * managedConnections associated with the current object must be
 * enlisted in the transaction.
 *  Entry point: (from
 * CachedConnectionManager)
 * ConnectionCacheListener.transactionStarted(Transaction,
 * Collection). The collection is of ConnectionRecord objects.
 * written.
 *
 *
 * 5. When an "entering object" notification is received from the
 * CachedConnectionInterceptor, all the connections for the current
 * object must be associated with a ManagedConnection.  if there is a
 * Transaction, the XAResource must be enlisted with it.
 *  Entry point: ConnectionCacheListener.reconnect(Collection conns) The Collection
 * is of ConnectionRecord objects.
 * written.
 *
 * 6. When a "leaving object" notification is received from the
 * CachedConnectionInterceptor, all the managedConnections for the
 * current object must have their XAResources delisted from the
 * current Transaction, if any, and cleanup called on each
 * ManagedConnection.
 * Entry point: ConnectionCacheListener.disconnect(Collection conns).
 * written.
 *
 * Created: Fri Sept 6  11:13:28 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 * @version $Revision: 1.2.2.17 $
 *
 * @jmx:mbean extends="BaseConnectionManager2MBean"
 */
public class TxConnectionManager
   extends BaseConnectionManager2
   implements TxConnectionManagerMBean
{

   private ObjectName transactionManagerService;
   //use the object name, please
   private String tmName;

   private TransactionManager tm;

   private boolean trackConnectionByTx = false;

   private boolean localTransactions;

   private TransactionLocal txToConnectionListenerMap;

   /**
    * Default managed LocalTxConnectionManager constructor for mbean instances.
    * @jmx:managed-constructor
    */
   public TxConnectionManager()
   {
   }

   /**
    * Creates a new <code>TxConnectionManager</code> instance.
    *for TESTING ONLY!!! not a managed constructor!!
    * @param mcf a <code>ManagedConnectionFactory</code> value
    * @param ccm a <code>CachedConnectionManager</code> value
    * @param poolingStrategy a <code>ManagedConnectionPool</code> value
    * @param tm a <code>TransactionManager</code> value
    */
   public TxConnectionManager (final CachedConnectionManager ccm,
                               final ManagedConnectionPool poolingStrategy,
                               final TransactionManager tm)
   {
      super(ccm, poolingStrategy);
      this.tm = tm;
   }




   /**
    * mbean get-set pair for field transactionManagerService
    * Get the value of transactionManagerService
    * @return value of transactionManagerService
    *
    * @jmx:managed-attribute
    */
   public ObjectName getTransactionManagerService()
   {
      return transactionManagerService;
   }


   /**
    * Set the value of transactionManagerService
    * @param transactionManagerService  Value to assign to transactionManagerService
    *
    * @jmx:managed-attribute
    */
   public void setTransactionManagerService(ObjectName transactionManagerService)
   {
      this.transactionManagerService = transactionManagerService;
   }



   /**
    *  The TransactionManager attribute contains the jndi name of the
    * TransactionManager.  This is normally java:/TransactionManager.
    *
    * @param name an <code>String</code> value
    * @deprecated use the ObjectName TransactionManagerService instead
    * @jmx:managed-attribute
    */
   public void setTransactionManager(final String tmName)
   {
      this.tmName = tmName;
   }

   /**
    * Describe <code>getTransactionManager</code> method here.
    *
    * @return an <code>String</code> value
    * @deprecated use the ObjectName TransactionManagerService instead
    * @jmx:managed-attribute
    */
   public String getTransactionManager()
   {
      return this.tmName;
   }



   /**
    * mbean get-set pair for field trackConnectionByTx
    * Get the value of trackConnectionByTx
    * @return value of trackConnectionByTx
    *
    * @jmx:managed-attribute
    */
   public boolean isTrackConnectionByTx()
   {
      return trackConnectionByTx;
   }


   /**
    * Set the value of trackConnectionByTx
    * @param trackConnectionByTx  Value to assign to trackConnectionByTx
    *
    * @jmx:managed-attribute
    */
   public void setTrackConnectionByTx(boolean trackConnectionByTx)
   {
      this.trackConnectionByTx = trackConnectionByTx;
   }




   /**
    * mbean get-set pair for field localTransactions
    * Get the value of localTransactions
    * @return value of localTransactions
    *
    * @jmx:managed-attribute
    */
   public boolean isLocalTransactions()
   {
      return localTransactions;
   }


   /**
    * Set the value of localTransactions
    * @param localTransactions  Value to assign to localTransactions
    *
    * @jmx:managed-attribute
    */
   public void setLocalTransactions(boolean localTransactions)
   {
      this.localTransactions = localTransactions;
   }





   protected void startService() throws Exception
   {
      if (transactionManagerService != null)
      {
         tm = (TransactionManager)getServer().getAttribute(transactionManagerService, "TransactionManager");
      } // end of if ()
      else
      {
         log.warn("----------------------------------------------------------");
         log.warn("----------------------------------------------------------");
         log.warn("Please change your datasource setup to use <depends optional-attribute-name\"TransactionManagerService\">jboss:service=TransactionManager</depends>");
         log.warn("instead of <attribute name=\"TransactionManager\">java:/TransactionManager</attribute>");
         log.warn("Better still, use a *-ds.xml file");
         log.warn("----------------------------------------------------------");
         log.warn("----------------------------------------------------------");
         tm = (TransactionManager)new InitialContext().lookup(tmName);
      } // end of else

      txToConnectionListenerMap = new TransactionLocal(tm);
      super.startService();
   }

   protected void stopService() throws Exception
   {
      this.tm = null;
      super.stopService();
   }


   public ConnectionListener getManagedConnection(Subject subject, ConnectionRequestInfo cri)
      throws ResourceException
   {
      try
      {
         if (trackConnectionByTx && tm.getStatus() != Status.STATUS_NO_TRANSACTION)
         {
            Transaction tx = tm.getTransaction();
            ConnectionListener cl = (ConnectionListener) txToConnectionListenerMap.get(tx);
            if (cl != null)
            {
               if (trace)
                  log.trace("getManagedConnection returning connection " + cl.getManagedConnection() + " already associated with tx " + tx);
               return (ConnectionListener) cl;
            } // end of if ()
         } // end of if ()
      }
      catch (SystemException xae)
      {
         throw new ResourceException("couldn't find current tx" + xae);
      } // end of try-catch
      if (trace)
         log.trace("getManagedConnection returning unassociated connection");

      return super.getManagedConnection(subject, cri);
   }

   //reimplementation from ConnectionCacheListener interface.

   public void transactionStarted(Collection crs) throws SystemException
   {
      Set cls = new HashSet();
      for (Iterator i = crs.iterator(); i.hasNext(); )
      {
         ConnectionRecord cr = (ConnectionRecord)i.next();
         ConnectionListener cl = cr.cl;
         if (!cls.contains(cl))
         {
            cls.add(cl);
            cl.enlist();
         } // end of if ()

      } // end of for ()
   }

   protected void managedConnectionReconnected(ConnectionListener cl) throws ResourceException
   {
      try
      {
         cl.enlist();
      }
      catch (SystemException se)
      {
         log.info("Could not enlist in transaction on entering meta-aware object!", se);
         throw new ResourceException("Could not enlist in transaction on entering meta-aware object!" + se);
      } // end of try-catch

   }

   protected void managedConnectionDisconnected(ConnectionListener cl) throws ResourceException
   {
      Throwable throwable = null;
      try
      {
         cl.delist();
      }
      catch (Throwable t)
      {
         throwable = t;
      }

      //if there are no more handles and tx is complete, we can return to pool.
      if (cl.isManagedConnectionFree())
         returnManagedConnection(cl, false);

      // Rethrow the error
      if (throwable != null)
         rethrowAsResourceException("Could not delist resource, probably a transaction rollback?", throwable);      
   }

   public ConnectionListener createConnectionListener(ManagedConnection mc, Object context)
      throws ResourceException
   {
      XAResource xaResource = null;
      if (localTransactions)
      {
         xaResource = new LocalXAResource(log);
      } // end of if ()
      else
      {
         xaResource = mc.getXAResource();
      } // end of else


      ConnectionListener cli = new TxConnectionEventListener(mc, poolingStrategy, context, log, xaResource);
      mc.addConnectionEventListener(cli);
      return cli;
   }

   public boolean isTransactional()
   {
      return TxUtils.isActive(tm);
   }

   // implementation of javax.resource.spi.ConnectionEventListener interface
   //there is one of these for each ManagedConnection instance.  It lives as long as the ManagedConnection.
   protected class TxConnectionEventListener
      extends BaseConnectionManager2.BaseConnectionEventListener
   {
      /** Use our own logger to prevent MNFE caused by compiler bug with nested classes. */
      protected Logger log;

      protected Transaction currentTx;

      private final XAResource xaResource;

      public TxConnectionEventListener(final ManagedConnection mc, final ManagedConnectionPool mcp, final Object context, Logger log, final XAResource xaResource) throws ResourceException
      {
         super(mc, mcp, context, log);
         this.log = log;
         this.xaResource = xaResource;
         if (xaResource instanceof LocalXAResource)
            ((LocalXAResource) xaResource).setConnectionListener(this);
      }

      public void enlist() throws SystemException
      {
         if (!isTrackConnectionByTx() && currentTx != null)
         {
            log.warn("in Enlisting tx, illegal state: " + currentTx);

            throw new IllegalStateException("Can't enlist - already a tx!");
         } // end of if ()

         if (tm.getStatus() != Status.STATUS_NO_TRANSACTION)
         {
            Transaction newCurrentTx = tm.getTransaction();
            //check to see if it is still the original tx.
            //currentTx can only be non-null if we are tracking cx by tx.
            if (currentTx != null && currentTx.equals(newCurrentTx) == false)
            {
               log.warn("in Enlisting tx, trying to change tx. illegal state: old: " + currentTx + ", new: " + newCurrentTx + ", cel: " + this);
               throw new IllegalStateException("Trying to change Tx in enlist!");
            } // end of if ()
            if (currentTx != null)
            {
               if (trace)
                  log.trace("currenttx: " + currentTx + ", already enlisted for ManagedConnection: " + this.getManagedConnection());
               return;
            }
            //It is a new tx for us.
            currentTx = newCurrentTx;
            if (trace)
               log.trace("enlisting currenttx: " + currentTx + ", ManagedConnection: " + this.getManagedConnection());

         } // end of if ()
         if (currentTx != null)
         {
            boolean succeeded = false;
            try
            {
               succeeded = currentTx.enlistResource(getXAResource());
            }
            catch (SystemException se)
            {
               throw new SystemException("Could not get XAResource from ManagedConnection!" + se);
            } // end of try-catch
            catch (RollbackException re)
            {
               log.info("Could not enlist XAResource!", re);
               throw new SystemException("Could not enlist XAResource!" + re);
            } // end of catch
            if (!succeeded)
            {
               throw new SystemException("enlistResource failed");
            }
            if (isTrackConnectionByTx())
            {
               //we are not actually delisting, so we need a synch
               //to tell use about the end of the tx so we can re-pool.
               try
               {
                  currentTx.registerSynchronization(new TxRemover(currentTx));
               }
               catch (RollbackException re)
               {
                  throw new SystemException("Could not register synchronization with tx: " + re);
               } // end of try-catch

               //Here we actually track the cx by tx.
               txToConnectionListenerMap.set(currentTx, this);

            } // end of if ()


         } // end of if ()

      }

      public void delist() throws ResourceException
      {
         if (trace)
            log.trace("delisting currenttx: " + currentTx + ", ManagedConnection: " + this.getManagedConnection());

         try
         {
            if (!isTrackConnectionByTx())
            {
               currentTx = null;
               if (tm.getStatus() != Status.STATUS_NO_TRANSACTION)
               {
                  if (!tm.getTransaction().delistResource(getXAResource(), XAResource.TMSUSPEND))
                  {
                     throw new ResourceException("Failure to delist resource");
                  }

               } // end of if ()
            } // end of if ()
         }
         catch (SystemException se)
         {
            throw new ResourceException("SystemException in delist!" + se);
         } // end of try-catch
      }

      //local will return this, xa will return one from mc.
      protected XAResource getXAResource()
      {
         return xaResource;
      }

      /**
       *
       * @param param1 <description>
       */
      public void connectionClosed(ConnectionEvent ce)
      {
         log.trace("connectionClosed called");
         if (this.getManagedConnection() != (ManagedConnection)ce.getSource())
         {
            throw new IllegalArgumentException("ConnectionClosed event received from wrong ManagedConnection! Expected: " + this.getManagedConnection() + ", actual: " + ce.getSource());
         } // end of if ()
         //log.trace("about to call unregisterConnection");
         try
         {
            getCcm().unregisterConnection(TxConnectionManager.this, ce.getConnectionHandle());         }
         catch (Throwable t)
         {
            log.info("throwable from unregister connection", t);
         } // end of try-catch

         //log.trace("unregisterConnection returned from");
         try
         {
            //log.trace("about to call unregisterAssociation");
            unregisterAssociation(this, ce.getConnectionHandle());
            if (isManagedConnectionFree())
            {
               //log.trace("called unregisterAssociation, delisting");
               //no more handles
               delist();
               //log.trace("called unregisterAssociation, returning");
               returnManagedConnection(this, false);
            }
            //log.trace("called unregisterAssociation");
         }
         catch (ResourceException re)
         {
            log.error("ResourceException while closing connection handle!", re);
         } // end of try-catch

      }

      /**
       *
       * @param param1 <description>
       */
      public void localTransactionStarted(ConnectionEvent ce)
      {
         if (currentTx != null)
         {
            throw new IllegalStateException("Attempt to start local transaction while xa transaction is active!");
         } // end of if ()

      }

      /**
       *
       * @param param1 <description>
       */
      public void localTransactionCommitted(ConnectionEvent ce)
      {
         if (currentTx != null)
         {
            throw new IllegalStateException("Attempt to commit local transaction while xa transaction is active!");
         } // end of if ()
      }

      /**
       *
       * @param param1 <description>
       */
      public void localTransactionRolledback(ConnectionEvent ce)
      {
         if (currentTx != null)
         {
            throw new IllegalStateException("Attempt to roll back local transaction while xa transaction is active!");
         } // end of if ()
      }

      /**
       *
       * @param param1 <description>
       */
      public void connectionErrorOccurred(ConnectionEvent ce)
      {
         if (currentTx != null)
         {
            txToConnectionListenerMap.set(currentTx, null);
         }
         currentTx = null;
         super.connectionErrorOccurred(ce);
      }

      //Important method!!
      public boolean isManagedConnectionFree()
      {
         if (trackConnectionByTx && currentTx != null)
         {
            return false;
         } // end of if ()
         return super.isManagedConnectionFree();
      }


      private class TxRemover implements Synchronization
      {
         private Transaction tx;

         public TxRemover(final Transaction tx)
         {
            this.tx = tx;
         }

         public void beforeCompletion()
         {
         }

         public void afterCompletion(int status)
         {
            if (currentTx.equals(tx) == false)
            {
               log.info("afterCompletion called with wrong tx! Expected: " + currentTx + ", actual: " + tx);
            } // end of if ()
            currentTx = null;
            if (isManagedConnectionFree())
            {
               returnManagedConnection(TxConnectionEventListener.this, false);
            } // end of if ()
         }
      }//end of TxRemover
   }//end of LocalConnectionEventListener.


   private class LocalXAResource
      implements XAResource
   {
      protected Logger log;

      private ConnectionListener cl;

      /**
       * <code>warned</code> is set after one warning about a local participant
       * in a multi-branch jta transaction is logged.
       *
       */
      private boolean warned = false;

      private Xid currentXid;

      public LocalXAResource(final Logger log)
      {
         this.log = log;
      }

      void setConnectionListener(ConnectionListener cl)
      {
         this.cl = cl;
      }

      // implementation of javax.transaction.xa.XAResource interface

      /**
       *
       * @param param1 <description>
       * @param param2 <description>
       * @exception javax.transaction.xa.XAException <description>
       */
      public void start(Xid xid, int flags) throws XAException
      {
         if (trace)
            log.trace("start, xid: " + xid + ", flags: " + flags);
         if (currentXid  != null && flags == XAResource.TMNOFLAGS)
            throw new JBossLocalXAException("Trying to start a new tx when old is not complete! old: " + currentXid  + ", new " + xid + ", flags " + flags);
         if (currentXid  == null && flags != XAResource.TMNOFLAGS)
            throw new JBossLocalXAException("Trying to start a new tx with wrong flags!  new " + xid + ", flags " + flags);
         if (currentXid == null)
         {
            try
            {
               cl.getManagedConnection().getLocalTransaction().begin();
            }
            catch (ResourceException re)
            {
               throw new JBossLocalXAException("Error trying to start local tx: ", re);
            } // end of try-catch
            catch (Throwable t)
            {
               log.info("Throwable trying to start local transaction!", t);
               throw new JBossLocalXAException("Throwable trying to start local transaction!", t);
            } // end of catch


            currentXid = xid;
         } // end of if ()
      }

      /**
       *
       * @param param1 <description>
       * @param param2 <description>
       * @exception javax.transaction.xa.XAException <description>
       */
      public void end(Xid xid, int flags) throws XAException
      {
         if (trace)
            log.trace("end on xid: " + xid + " called with flags " + flags);
      }

      /**
       *
       * @param param1 <description>
       * @param param2 <description>
       * @exception javax.transaction.xa.XAException <description>
       */
      public void commit(Xid xid, boolean onePhase) throws XAException
      {
         if (xid.equals(currentXid) == false)
         {
            throw new JBossLocalXAException("wrong xid in commit: expected: " + currentXid + ", got: " + xid);
         } // end of if ()
         currentXid = null;
         try
         {
            cl.getManagedConnection().getLocalTransaction().commit();
         }
         catch (ResourceException re)
         {
            returnManagedConnection(cl, true);
            if (trace)
               log.trace("commit problem: ", re);
            throw new JBossLocalXAException("could not commit local tx", re);
         } // end of try-catch
      }

      /**
       *
       * @param param1 <description>
       * @exception javax.transaction.xa.XAException <description>
       */
      public void forget(Xid xid) throws XAException
      {
         throw new JBossLocalXAException("forget not supported in local tx");
      }

      /**
       *
       * @return <description>
       * @exception javax.transaction.xa.XAException <description>
       */
      public int getTransactionTimeout() throws XAException
      {
         // TODO: implement this javax.transaction.xa.XAResource method
         return 0;
      }

      /**
       *
       * @param param1 <description>
       * @return <description>
       * @exception javax.transaction.xa.XAException <description>
       */
      public boolean isSameRM(XAResource xaResource) throws XAException
      {
         return xaResource == this;
      }

      /**
       *
       * @param param1 <description>
       * @return <description>
       * @exception javax.transaction.xa.XAException <description>
       */
      public int prepare(Xid xid) throws XAException
      {
         if (!warned)
         {
            log.warn("Prepare called on a local tx. Use of local transactions on a jta transaction with more than one branch may result in inconsistent data in some cases of failure.");
         } // end of if ()
         warned = true;
         return XAResource.XA_OK;
      }

      /**
       *
       * @param param1 <description>
       * @return <description>
       * @exception javax.transaction.xa.XAException <description>
       */
      public Xid[] recover(int flag) throws XAException
      {
         throw new JBossLocalXAException("no recover with local-tx only resource managers");
      }

      /**
       *
       * @param param1 <description>
       * @exception javax.transaction.xa.XAException <description>
       */
      public void rollback(Xid xid) throws XAException
      {
         if (xid.equals(currentXid) == false)
         {
            throw new JBossLocalXAException("wrong xid in rollback: expected: " + currentXid + ", got: " + xid);
         } // end of if ()
         currentXid = null;
         try
         {
            cl.getManagedConnection().getLocalTransaction().rollback();
         }
         catch (ResourceException re)
         {
            returnManagedConnection(cl, true);
            if (trace)
               log.trace("rollback problem: ", re);
            throw new JBossLocalXAException("could not rollback local tx", re);
         } // end of try-catch
      }

      /**
       *
       * @param param1 <description>
       * @return <description>
       * @exception javax.transaction.xa.XAException <description>
       */
      public boolean setTransactionTimeout(int seconds) throws XAException {
         // TODO: implement this javax.transaction.xa.XAResource method
         return false;
      }


   }//end of LocalXAResource.



}//
