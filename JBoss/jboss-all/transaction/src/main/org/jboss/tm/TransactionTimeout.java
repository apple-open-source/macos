package org.jboss.tm;

import javax.transaction.TransactionManager;
import javax.transaction.Transaction;
import javax.naming.InitialContext;
import javax.naming.NamingException;

/**
 * Created by IntelliJ IDEA.
 * User: wburke
 * Date: Oct 9, 2003
 * Time: 9:48:57 AM
 * To change this template use Options | File Templates.
 */
public class TransactionTimeout
{
   public static class TransactionTimeoutException extends Exception {}
   /**
    * The transaction manager is maintained by the system and
    * manges the assocation of transaction to threads.
    */
   protected final TransactionManager transactionManager;

   private TransactionTimeout()
   {
      try
      {
         InitialContext context = new InitialContext();
         transactionManager = (TransactionManager) context.lookup("java:/TransactionManager");
      }
      catch (NamingException e)
      {
         throw new IllegalStateException(
                 "An error occured while looking up the transaction manager: " + e
         );
      }
   }

   /**
    *
    * @return -1 if unable to determine time left in transaction
    * @throws TransactionTimeoutException if transaction is timed out
    */
   public long getTimeLeftInTx() throws TransactionTimeoutException
   {
      try
      {
         Transaction tx = transactionManager.getTransaction();
         if (TxUtils.isCompleted(tx)) return -1;
         if (!(tx instanceof TransactionImpl)) return -1;
         TransactionImpl txImpl = (TransactionImpl)tx;
         return txImpl.getTimeLeftBeforeTimeout();
      }
      catch (Exception ignored)
      {
         return -1;
      }
   }

   private static TransactionTimeout instance = null;

   public static long getTimeLeftInTransaction() throws TransactionTimeoutException
   {
      if (instance == null) instance = new TransactionTimeout();
      return instance.getTimeLeftInTx();
   }

}
