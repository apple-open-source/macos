/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.resource.connectionmanager;

import javax.transaction.TransactionManager;

/**
 *
 * The XATxConnectionManager connection manager has to perform the following operations:
 *
 * 1. When an application component requests a new ConnectionHandle,
 *    it must find a ManagedConnection, and make sure a
 *    ConnectionEventListener is registered. It must inform the
 *    CachedConnectionManager that a connection handle has been given
 *    out. It needs to count the number of handles for each
 *    ManagedConnection.  If there is a current transaction, it must
 *    enlist the ManagedConnection's XAResource in the transaction.
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
 * 4. When a synchronization beforeCompletion event is received, any
 *    enlisted XAResources must be delisted.
 * Entry point: Synchronization.beforeCompletion() (implemented in
 * XAConnectionEventListener))
 * written.
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
 * In addition it inherits behavior from BaseConnectionManager2,  including
 *  functionality to obtain managed connections from
 * a ManagedConnectionPool mbean, find the Subject from a SubjectSecurityDomain,
 * and interact with the CachedConnectionManager for connections held over
 * transaction and method boundaries.  Important mbean references are to a
 * ManagedConnectionPool supplier (typically a JBossManagedConnectionPool), and a
 * RARDeployment representing the ManagedConnectionFactory.
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 * @version $Revision: 1.7.2.5 $
 *
 * @jmx:mbean name="jboss.jca:service=XATxConnectionManager"
 *            extends="TxConnectionManagerMBean"
 */
public class XATxConnectionManager
   extends TxConnectionManager implements XATxConnectionManagerMBean
{

   /**
    * Default managed XATxConnectionManager constructor for mbean instances.
    * @jmx:managed-constructor
    */
   public XATxConnectionManager()
   {
   }

   /**
    * Creates a new <code>XATxConnectionManager</code> instance.
    *for TESTING ONLY!!! not a managed constructor!!
    * @param mcf a <code>ManagedConnectionFactory</code> value
    * @param ccm a <code>CachedConnectionManager</code> value
    * @param poolingStrategy a <code>ManagedConnectionPool</code> value
    * @param tm a <code>TransactionManager</code> value
    */
   public XATxConnectionManager (final CachedConnectionManager ccm,
                                 final ManagedConnectionPool poolingStrategy,
                                 final TransactionManager tm)
   {
      super(ccm, poolingStrategy, tm);
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
      return super.isTrackConnectionByTx();
   }


   /**
    * Set the value of trackConnectionByTx
    * @param trackConnectionByTx  Value to assign to trackConnectionByTx
    *
    * @jmx:managed-attribute
    */
   public void setTrackConnectionByTx(boolean trackConnectionByTx)
   {
      super.setTrackConnectionByTx(trackConnectionByTx);
   }

}
