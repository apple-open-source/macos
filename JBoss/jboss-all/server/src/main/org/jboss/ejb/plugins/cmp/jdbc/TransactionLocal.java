/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc;

import javax.transaction.TransactionManager;
import javax.transaction.Synchronization;
import javax.transaction.Transaction;
import javax.transaction.SystemException;
import javax.naming.NamingException;
import javax.naming.InitialContext;
import java.util.HashMap;
import java.util.Map;

/**
 * A TransactionLocal is similar to ThreadLocal except it is keyed on the
 * Transactions. A transaction local variable is cleared after the transaction
 * completes. A synchronization call back can be registered with the
 * transaction local variable during creation
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.1.2.2 $
 */
public class TransactionLocal
{
   /**
    * To simplify null values handling in the preloaded data pool we use
    * this value instead of 'null'
    */
   private static final Object NULL_VALUE = new Object();

   /**
    * A map for the transactions to the local variable.
    */
   private final Map transactionMap = new HashMap();

   /**
    * The transaction manager is maintained by the system and
    * manges the assocation of transaction to threads.
    */
   protected final TransactionManager transactionManager;

   /**
    * This is the users callback interface for synchronization
    */
   private final Synchronization synchronization;

   /**
    * Creates a thread local variable.
    * @throws IllegalStateException if there is no system transaction manager
    */
   public TransactionLocal()
   {
      this(null);
   }

   /**
    * Creates a thread local variable.
    *
    * If the synchronization parameter is not null the synchronization will
    * be called when every a transaction with a thread local variable
    * completes.  The thread local variable will still be accessable in the
    * afterCompletion callback.
    *
    * @param synchronization a synchronication call back; as transactions
    * complete this synchronization will be notified.
    * @throws IllegalStateException if an error occus while looking up the
    * transaction manager
    */
   public TransactionLocal(Synchronization synchronization)
   {
      try
      {
         InitialContext context = new InitialContext();
         transactionManager = (TransactionManager) context.lookup(
               "java:/TransactionManager");
      }
      catch(NamingException e)
      {
         throw new IllegalStateException("An error occured while " +
               "looking up the transaction manager: " + e);
      }
      this.synchronization = synchronization;
   }

   /**
    * Returns the initial value for this thransaction local.  This method
    * will be called once per accessing transaction for each TransactionLocal,
    * the first time each transaction accesses the variable with get or set.
    * If the programmer desires TransactionLocal variables to be initialized to
    * some value other than null, TransactionLocal must be subclassed, and this
    * method overridden. Typically, an anonymous inner class will be used.
    * Typical implementations of initialValue will call an appropriate
    * constructor and return the newly constructed object.
    *
    * @return the initial value for this TransactionLocal
    */
   protected Object initialValue()
   {
      return null;
   }

   /**
    * Returns the value of this ThreadLocal variable associated with the thread
    * context transaction. Creates and initializes the copy if this is the
    * first time the method is called in a transaction.
    *
    * @return the value of this TransactionLocal
    * @throws IllegalStateException if an error occures while registering
    * a synchronization callback with the transaction
    */
   public synchronized Object get()
   {
      // get the current value
      Transaction transaction = getTransaction();
      if(transaction == null)
         return null;

      Object value = transactionMap.get(transaction);

      // is we didn't get a value initalize this object with initialValue()
      if(value == null)
      {
         // register for synchroniztion callbacks
         registerSynchronization(transaction);

         // get the initial value
         value = initialValue();

         // if value is null replace it with the null value standin
         if(value == null)
         {
            value = NULL_VALUE;
         }

         // store the value
         transactionMap.put(transaction, value);
      }

      // if the value is the null standin return null
      if(value == NULL_VALUE)
      {
         return null;
      }

      // finall return the value
      return value;
   }

   /**
    * Sets the value of this ThreadLocal variable associtated with the thread
    * context transaction. This is only used to change the value from the
    * one assigned by the initialValue method, and many applications will
    * have no need for this functionality.
    *
    * @param value the value to be associated with the thread context
    * transactions's TransactionLocal
    * @throws IllegalStateException if an error occures while registering
    * a synchronization callback with the transaction
    */
   public synchronized void set(Object value)
   {
      Transaction transaction = getTransaction();
      if(transaction == null)
         return;

      // If this transaction is unknown, register for synchroniztion callback,
      // and call initialValue to give subclasses a chance to do some
      // initialization.
      if(!transactionMap.containsKey(transaction))
      {
         initialValue();
         registerSynchronization(transaction);
      }

      // if value is null replace it with the null value standin
      if(value == null)
      {
         value = NULL_VALUE;
      }

      // finally store the value
      transactionMap.put(transaction, value);
   }

   private void registerSynchronization(Transaction transaction)
   {
      try
      {
         transaction.registerSynchronization(
               new TransactionLocalSynchronization(transaction));
      }
      catch(Exception e)
      {
         throw new IllegalStateException("An error occured while " +
               "registering synchronization callback with the " +
               "transaction: " + e);
      }
   }

   protected Transaction getTransaction()
   {
      try
      {
         Transaction transaction = transactionManager.getTransaction();
         if(transaction == null)
         {
            throw new IllegalStateException("There is no tranaction " +
                  "associated with the current thread");
         }
         return transaction;
      }
      catch(SystemException e)
      {
         throw new IllegalStateException("An error occured while getting the " +
               "transaction associated with the current thread: " + e);
      }
   }

   private final class TransactionLocalSynchronization
         implements Synchronization
   {
      private Transaction transaction;

      private TransactionLocalSynchronization(Transaction transaction)
      {
         this.transaction = transaction;
      }

      public void beforeCompletion()
      {
         if(TransactionLocal.this.synchronization != null)
         {
            TransactionLocal.this.synchronization.beforeCompletion();
         }
      }

      public void afterCompletion(int status)
      {
         if(TransactionLocal.this.synchronization != null)
         {
            TransactionLocal.this.synchronization.afterCompletion(status);
         }

         // remove this value from the map
         synchronized(transactionMap)
         {
            TransactionLocal.this.transactionMap.remove(transaction);
         }

         transaction = null;
      }
   }
}
