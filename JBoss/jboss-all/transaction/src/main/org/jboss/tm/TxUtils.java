/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.tm;

import javax.transaction.Status;
import javax.transaction.SystemException;
import javax.transaction.Transaction;
import javax.transaction.TransactionManager;
import javax.transaction.UserTransaction;

import org.jboss.util.NestedRuntimeException;

/**
 * TxUtils.java has utility methods for determining transaction status
 * in various useful ways.
 *
 *
 * Created: Sat May 10 09:53:51 2003
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @author <a href="mailto:bill@jboss.org">Bill Burke</a>
 * @version $1.0$
 */
public class TxUtils
{
   private TxUtils()
   {

   } // TxUtils constructor

   public static boolean isActive(Transaction tx)
   {
      try
      {
         return tx != null && (tx.getStatus() == Status.STATUS_ACTIVE || tx.getStatus() == Status.STATUS_MARKED_ROLLBACK);
      }
      catch (SystemException ignored)
      {
         throw new NestedRuntimeException(ignored);
      }
   }

   public static boolean isActive(TransactionManager tm)
   {
      try
      {
         return isActive(tm.getTransaction());
      }
      catch (SystemException ignored)
      {
         throw new NestedRuntimeException(ignored);
      }
   }

   public static boolean isActive(UserTransaction ut)
   {
      try
      {
         return ut.getStatus() == Status.STATUS_ACTIVE;
      }
      catch (SystemException ignored)
      {
         throw new NestedRuntimeException(ignored);
      }
   }

   public static boolean isCompleted(Transaction tx)
   {
      try
      {
         return tx == null
                 || tx.getStatus() == Status.STATUS_COMMITTED
                 || tx.getStatus() == Status.STATUS_ROLLEDBACK
                 || tx.getStatus() == Status.STATUS_NO_TRANSACTION;
      }
      catch (SystemException ignored)
      {
         throw new NestedRuntimeException(ignored);
      }
   }

   public static boolean isCompleted(TransactionManager tm)
   {
      try
      {
         return isCompleted(tm.getTransaction());
      }
      catch (SystemException ignored)
      {
         throw new NestedRuntimeException(ignored);
      }
   }

   public static boolean isCompleted(UserTransaction ut)
   {
      try
      {
         return ut.getStatus() == Status.STATUS_COMMITTED
                 || ut.getStatus() == Status.STATUS_ROLLEDBACK
                 || ut.getStatus() == Status.STATUS_NO_TRANSACTION;
      }
      catch (SystemException ignored)
      {
         throw new NestedRuntimeException(ignored);
      }
   }

} // TxUtils
