/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.pm;

import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;

import javax.jms.JMSException;
import org.jboss.mq.ConnectionToken;

import org.jboss.mq.SpyDestination;
import org.jboss.mq.SpyJMSException;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.xml.XElement;

/**
 *  This class allows provides the base for user supplied persistence packages.
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @author     Paul Kendall (paul.kendall@orion.co.nz)
 * @created    August 16, 2001
 * @version    $Revision: 1.2 $
 */
public class TxManager {

   // Maps (Long)txIds to LinkedList of Runnable tasks
   HashMap          postCommitTasks = new HashMap();
   // Maps (Long)txIds to LinkedList of Runnable tasks
   HashMap          postRollbackTasks = new HashMap();
   // Maps Global transactions to local transactions
   HashMap          globalToLocal = new HashMap();
   //pool of linked lists to use for storing txs tasks
   java.util.ArrayList listPool = new java.util.ArrayList();

   PersistenceManager persistenceManager;

   protected static int MAX_POOL_SIZE = 500;

   public TxManager( PersistenceManager pm ) {
      persistenceManager = pm;
   }

   /**
    *  Return the local transaction id for a distributed transaction id.
    *
    * @param  dc                          Description of Parameter
    * @param  xid                         Description of Parameter
    * @return                             The Prepared value
    * @exception  javax.jms.JMSException  Description of Exception
    */
   public final Tx getPrepared( ConnectionToken dc, Object xid )
      throws javax.jms.JMSException {

      GlobalXID gxid = new GlobalXID( dc, xid );
      Tx txid;
      synchronized ( globalToLocal ) {
         txid = ( Tx )globalToLocal.get( gxid );
      }
      if ( txid == null ) {
         throw new SpyJMSException( "Transaction does not exist from: " + dc.getClientID() + " xid=" + xid );
      }

      return txid;
   }

   /**
    *  Create and return a unique transaction id.
    *
    * @return                             Description of the Returned Value
    * @exception  javax.jms.JMSException  Description of Exception
    */
   public final Tx createTx()
      throws javax.jms.JMSException {
      Tx txId = persistenceManager.createPersistentTx();
      synchronized ( postCommitTasks ) {
         postCommitTasks.put( txId, getList() );
         postRollbackTasks.put( txId, getList() );
      }
      return txId;
   }

   /**
    *  Commit the transaction to the persistent store.
    *
    * @param  txId                        Description of Parameter
    * @exception  javax.jms.JMSException  Description of Exception
    */
   public final void commitTx( Tx txId )
      throws javax.jms.JMSException {

      LinkedList tasks;
      synchronized ( postCommitTasks ) {
         tasks = ( LinkedList )postCommitTasks.remove( txId );
         releaseList( ( LinkedList )postRollbackTasks.remove( txId ) );
      }
      if ( tasks == null ) {
         throw new javax.jms.JMSException( "Transaction is not active for commit." );
      }

      persistenceManager.commitPersistentTx( txId );

      synchronized ( tasks ) {
         Iterator iter = tasks.iterator();
         while ( iter.hasNext() ) {
            Runnable task = ( Runnable )iter.next();
            task.run();
         }
      }
      releaseList( tasks );
   }

   public final void addPostCommitTask( Tx txId, Runnable task )
      throws javax.jms.JMSException {

      if ( txId == null ) {
         task.run();
         return;
      }

      LinkedList tasks;
      synchronized ( postCommitTasks ) {
         tasks = ( LinkedList )postCommitTasks.get( txId );
      }
      if ( tasks == null ) {
         throw new javax.jms.JMSException( "Transaction is not active." );
      }
      synchronized ( tasks ) {
         tasks.addLast( task );
      }

   }

   /**
    *  Rollback the transaction.
    *
    * @param  txId                        Description of Parameter
    * @exception  javax.jms.JMSException  Description of Exception
    */
   public final void rollbackTx( Tx txId )
      throws javax.jms.JMSException {

      LinkedList tasks;
      synchronized ( postCommitTasks ) {
         tasks = ( LinkedList )postRollbackTasks.remove( txId );
         releaseList( ( LinkedList )postCommitTasks.remove( txId ) );
      }
      if ( tasks == null ) {
         throw new javax.jms.JMSException( "Transaction is not active 3." );
      }

      persistenceManager.rollbackPersistentTx( txId );

      synchronized ( tasks ) {
         Iterator iter = tasks.iterator();
         while ( iter.hasNext() ) {
            Runnable task = ( Runnable )iter.next();
            task.run();
         }
      }

      releaseList( tasks );
   }

   public final void addPostRollbackTask( Tx txId, Runnable task )
      throws javax.jms.JMSException {

      if ( txId == null ) {
         return;
      }

      LinkedList tasks;
      synchronized ( postCommitTasks ) {
         tasks = ( LinkedList )postRollbackTasks.get( txId );
      }
      if ( tasks == null ) {
         throw new javax.jms.JMSException( "Transaction is not active 4." );
      }
      synchronized ( tasks ) {
         tasks.addLast( task );
      }

   }

   /**
    *  Create and return a unique transaction id. Given a distributed connection
    *  and a transaction id object, allocate a unique local transaction id if
    *  the remote id is not already known.
    *
    * @param  dc                          Description of Parameter
    * @param  xid                         Description of Parameter
    * @return                             Description of the Returned Value
    * @exception  javax.jms.JMSException  Description of Exception
    */
   public final Tx createTx( ConnectionToken dc, Object xid )
      throws javax.jms.JMSException {

      GlobalXID gxid = new GlobalXID( dc, xid );
      synchronized ( globalToLocal ) {
         if ( globalToLocal.containsKey( gxid ) ) {
            throw new SpyJMSException( "Duplicate transaction from: " + dc.getClientID() + " xid=" + xid );
         }
      }

      Tx txId = createTx();
      synchronized ( globalToLocal ) {
         globalToLocal.put( gxid, txId );
      }

      //Tasks to remove the global to local mappings on commit/rollback
      addPostCommitTask( txId, gxid );
      addPostRollbackTask( txId, gxid );

      return txId;
   }

   protected LinkedList getList() {
      synchronized ( listPool ) {
         if ( listPool.isEmpty() ) {
            return new LinkedList();
         } else {
            return ( LinkedList )listPool.remove( listPool.size() - 1 );
         }
      }
   }

   protected void releaseList( LinkedList list ) {
      synchronized ( listPool ) {
         if ( listPool.size() < MAX_POOL_SIZE ) {
            list.clear();
            listPool.add( list );
         }
      }
   }


   /**
    * @created    August 16, 2001
    */
   class GlobalXID implements Runnable {
      ConnectionToken dc;
      Object        xid;

      GlobalXID( ConnectionToken dc, Object xid ) {
         this.dc = dc;
         this.xid = xid;
      }

      public boolean equals( Object obj ) {
         if ( obj == null ) {
            return false;
         }
         if ( obj.getClass() != GlobalXID.class ) {
            return false;
         }
         return ( ( GlobalXID )obj ).xid.equals( xid ) &&
               ( ( GlobalXID )obj ).dc.equals( dc );
      }

      public int hashCode() {
         return xid.hashCode();
      }

      public void run() {
         synchronized ( globalToLocal ) {
            globalToLocal.remove( this );
         }
      }
   }
}
