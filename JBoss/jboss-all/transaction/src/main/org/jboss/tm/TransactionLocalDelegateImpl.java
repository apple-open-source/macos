/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license
 * See terms of license at gnu.org.
 */
package org.jboss.tm;

import java.util.HashMap;

import javax.transaction.RollbackException;
import javax.transaction.Synchronization;
import javax.transaction.SystemException;
import javax.transaction.Transaction;
import javax.transaction.TransactionManager;

import org.jboss.util.NestedRuntimeException;

import EDU.oswego.cs.dl.util.concurrent.ConcurrentHashMap;

/**
 * An implementation of the transaction local implementation
 * using Transaction synchronizations.
 *
 * There is one of these per transaction local
 *
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public class TransactionLocalDelegateImpl
   implements TransactionLocalDelegate
{
   // Attributes ----------------------------------------------------

   /** The transaction manager */
   protected TransactionManager manager;

   // Static --------------------------------------------------------

   /** The synchronizations for each transaction */
   protected static ConcurrentHashMap synchronizationsByTransaction = new ConcurrentHashMap();

   /**
    * Retrieve a synchronization for the transaction
    *
    * @param tx the transaction
    * @param create whether to create a synchronization if one doesn't exist
    */
   protected static TransactionLocalSynchronization getSynchronization(Transaction tx, boolean create)
   {
      TransactionLocalSynchronization result = (TransactionLocalSynchronization) synchronizationsByTransaction.get(tx);
      if (result == null && create == true)
      {
         result = new TransactionLocalSynchronization(tx);
         try
         {
            tx.registerSynchronization(result);
         }
         catch (RollbackException e)
         {
            throw new IllegalStateException("Transaction already rolled back or marked for rollback");
         }
         catch (SystemException e)
         {
            throw new NestedRuntimeException(e);
         }
         synchronizationsByTransaction.put(tx, result);
      }
      return result;
   }

   /**
    * Remove a synchronization
    *
    * @param tx the transaction to remove
    */
   protected static void removeSynchronization(Transaction tx)
   {
      synchronizationsByTransaction.remove(tx);
   }

   // Constructor ---------------------------------------------------

   /**
    * Construct a new delegate for the given transaction manager
    *
    * @param manager the transaction manager
    */
   public TransactionLocalDelegateImpl(TransactionManager manager)
   {
      this.manager = manager;
   }

   public Object getValue(TransactionLocal unused, Transaction tx)
   {
      TransactionLocalSynchronization sync = getSynchronization(tx, false);
      if (sync == null)
         return null;
      return sync.getValue(this);
   }

   public void storeValue(TransactionLocal unused, Transaction tx, Object value)
   {
      TransactionLocalSynchronization sync = getSynchronization(tx, true);
      sync.setValue(this, value);
   }

   public boolean containsValue(TransactionLocal unused, Transaction tx)
   {
      TransactionLocalSynchronization sync = getSynchronization(tx, false);
      if (sync == null)
         return false;
      return sync.containsValue(this);
   }

   // InnerClasses ---------------------------------------------------

   protected static class TransactionLocalSynchronization
      implements Synchronization
   {
      protected Transaction tx;

      protected HashMap valuesByLocal = new HashMap();

      public TransactionLocalSynchronization(Transaction tx)
      {
         this.tx = tx;
      }

      public void beforeCompletion()
      {
      }

      public void afterCompletion(int status)
      {
         removeSynchronization(tx);
         valuesByLocal.clear(); // Help the GC
      }

      public Object getValue(Object local)
      {
         return valuesByLocal.get(local);
      }

      public void setValue(Object local, Object value)
      {
         valuesByLocal.put(local, value);
      }

      public boolean containsValue(Object local)
      {
         return valuesByLocal.containsKey(local);
      }
   }
}
