/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins;

import org.jboss.invocation.Invocation;
import org.jboss.invocation.InvocationType;
import org.jboss.metadata.BeanMetaData;
import org.jboss.metadata.MetaData;
import org.jboss.tm.JBossTransactionRolledbackException;
import org.jboss.tm.JBossTransactionRolledbackLocalException;
import org.jboss.util.NestedException;
import org.jboss.util.deadlock.ApplicationDeadlockException;

import javax.ejb.EJBException;
import javax.ejb.TransactionRequiredLocalException;
import javax.transaction.HeuristicMixedException;
import javax.transaction.HeuristicRollbackException;
import javax.transaction.RollbackException;
import javax.transaction.Status;
import javax.transaction.SystemException;
import javax.transaction.Transaction;
import javax.transaction.TransactionRequiredException;
import javax.transaction.TransactionRolledbackException;
import java.lang.reflect.Method;
import java.rmi.RemoteException;
import java.util.HashMap;
import java.util.Map;
import java.util.Random;

/**
 *  This interceptor handles transactions for CMT beans.
 *
 *  @author <a href="mailto:rickard.oberg@telkel.com">Rickard �berg</a>
 *  @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 *  @author <a href="mailto:sebastien.alborini@m4x.org">Sebastien Alborini</a>
 *  @author <a href="mailto:akkerman@cs.nyu.edu">Anatoly Akkerman</a>
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.27.2.10 $
 */
public class TxInterceptorCMT
extends AbstractTxInterceptor
{

   // Attributes ----------------------------------------------------

   /** A cache mapping methods to transaction attributes. */
   private HashMap methodTx = new HashMap();

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   // Public --------------------------------------------------------

   // Interceptor implementation --------------------------------------


   public static int MAX_RETRIES = 5;
   public static Random random = new Random();

   /**
    * Detects exception contains is or a ApplicationDeadlockException.
    */
   public static ApplicationDeadlockException isADE(Throwable t)
   {
      while (t!=null)
      {
         if (t instanceof ApplicationDeadlockException)
         {
            return (ApplicationDeadlockException)t;
         }
         else if (t instanceof RemoteException)
         {
            t = ((RemoteException)t).detail;
         }
         else if (t instanceof EJBException)
         {
            t = ((EJBException)t).getCausedByException();
         }
         else
         {
            return null;
         }
      }
      return null;
   }
   public Object invokeHome(Invocation invocation) throws Exception
   {
      Transaction oldTransaction = invocation.getTransaction();
      for (int i = 0; i < MAX_RETRIES; i++)
      {
         try
         {
            return runWithTransactions(invocation);
         }
         catch (Exception ex)
         {
            ApplicationDeadlockException deadlock = isADE(ex);
            if (deadlock != null)
            {
               if (!deadlock.retryable() || oldTransaction != null || i + 1 >= MAX_RETRIES) throw deadlock;
               log.debug(deadlock.getMessage() + " retrying " + (i + 1));
               Thread.sleep(random.nextInt(1 + i), random.nextInt(1000) + 10);
            }
            else
            {
               throw ex;
            }
         }
      }
      throw new RuntimeException("Unreachable");
   }

   /**
    *  This method does invocation interpositioning of tx management
    */
   public Object invoke(Invocation invocation) throws Exception
   {
      Transaction oldTransaction = invocation.getTransaction();
      for (int i = 0; i < MAX_RETRIES; i++)
      {
         try
         {
            return runWithTransactions(invocation);
         }
         catch (Exception ex)
         {
            ApplicationDeadlockException deadlock = isADE(ex);
            if (deadlock != null)
            {
               if (!deadlock.retryable() || oldTransaction != null || i + 1 >= MAX_RETRIES) throw deadlock;
               log.debug(deadlock.getMessage() + " retrying " + (i + 1));

               Thread.sleep(random.nextInt(1 + i), random.nextInt(1000));
            }
            else
            {
               throw ex;
            }
         }
      }
      throw new RuntimeException("Unreachable");
   }

   // Private  ------------------------------------------------------

   private void printMethod(Method m, byte type)
   {
      String name;
      switch(type)
      {
         case MetaData.TX_MANDATORY:
            name = "TX_MANDATORY";
            break;
         case MetaData.TX_NEVER:
            name = "TX_NEVER";
            break;
         case MetaData.TX_NOT_SUPPORTED:
            name = "TX_NOT_SUPPORTED";
            break;
         case MetaData.TX_REQUIRED:
            name = "TX_REQUIRED";
            break;
         case MetaData.TX_REQUIRES_NEW:
            name = "TX_REQUIRES_NEW";
            break;
         case MetaData.TX_SUPPORTS:
            name = "TX_SUPPORTS";
            break;
         default:
            name = "TX_UNKNOWN";
      }

      String methodName;
      if(m != null) {
         methodName = m.getName();
      }
      else
      {
         methodName ="<no method>";
      }

      if (log.isTraceEnabled())
      {
         log.trace(name + " for " + methodName);
      }
   }

    /*
     *  This method does invocation interpositioning of tx management.
     *
     *  This is where the meat is.  We define what to do with the Tx based
     *  on the declaration.
     *  The Invocation is always the final authority on what the Tx
     *  looks like leaving this interceptor.  In other words, interceptors
     *  down the chain should not rely on the thread association with Tx but
     *  on the Tx present in the Invocation.
     *
     *  @param remoteInvocation If <code>true</code> this is an invocation
     *                          of a method in the remote interface, otherwise
     *                          it is an invocation of a method in the home
     *                          interface.
     *  @param invocation The <code>Invocation</code> of this call.
     */
   private Object runWithTransactions(Invocation invocation) throws Exception
   {
      // Old transaction is the transaction that comes with the MI
      Transaction oldTransaction = invocation.getTransaction();
      // New transaction is the new transaction this might start
      Transaction newTransaction = null;

      boolean trace = log.isTraceEnabled();
      if( trace )
         log.trace("Current transaction in MI is " + oldTransaction);

      InvocationType type = invocation.getType();
      byte transType = getTransactionMethod(invocation.getMethod(), type);

      if ( trace )
         printMethod(invocation.getMethod(), transType);

      // Thread arriving must be clean (jboss doesn't set the thread
      // previously). However optimized calls come with associated
      // thread for example. We suspend the thread association here, and
      // resume in the finally block of the following try.
      Transaction threadTx = tm.suspend();
      if( trace )
         log.trace("Thread came in with tx " + threadTx);
      try
      {
         switch (transType)
         {
            case MetaData.TX_NOT_SUPPORTED:
            {
               // Do not set a transaction on the thread even if in MI, just run
               return invokeNext(invocation, false);
            }
            case MetaData.TX_REQUIRED:
            {
               if (oldTransaction == null)
               { // No tx running
                  // Create tx
                  tm.begin();

                  // get the tx
                  newTransaction = tm.getTransaction();
                  if( trace )
                     log.trace("Starting new tx " + newTransaction);

                  // Let the method invocation know
                  invocation.setTransaction(newTransaction);
               }
               else
               {
                  // We have a tx propagated
                  // Associate it with the thread
                  tm.resume(oldTransaction);
               }

               // Continue invocation
               try
               {
                  return invokeNext(invocation, oldTransaction != null);
               }
               finally
               {
                  if( trace )
                     log.trace("TxInterceptorCMT: In finally");

                  // Only do something if we started the transaction
                  if (newTransaction != null)
                  {
                     endTransaction(invocation, newTransaction, oldTransaction);
                  }
                  else
                  {
                     tm.suspend();
                  } // end of else
               }
            }
            case MetaData.TX_SUPPORTS:
            {
               // Associate old transaction with the thread
               // Some TMs cannot resume a null transaction and will throw
               // an exception (e.g. Tyrex), so make sure it is not null
               if (oldTransaction != null)
               {
                  tm.resume(oldTransaction);
               }

               try
               {
                  return invokeNext(invocation, oldTransaction != null);
               }
               finally
               {
                  tm.suspend();
               }

               // Even on error we don't do anything with the tx,
               // we didn't start it
            }
            case MetaData.TX_REQUIRES_NEW:
            {
               // Always begin a transaction
               tm.begin();

               // get it
               newTransaction = tm.getTransaction();

               // Set it on the method invocation
               invocation.setTransaction(newTransaction);
               // Continue invocation
               try
               {
                  return invokeNext(invocation, false);
               }
               finally
               {
                  // We started the transaction for sure so we commit or roll back
                  endTransaction(invocation, newTransaction, oldTransaction);
               }
            }
            case MetaData.TX_MANDATORY:
            {
               if (oldTransaction == null)
               {
                  if (type == InvocationType.LOCAL ||
                        type == InvocationType.LOCALHOME)
                  {
                     throw new TransactionRequiredLocalException(
                           "Transaction Required");
                  }
                  else
                  {
                     throw new TransactionRequiredException(
                           "Transaction Required");
                  }
               }

               // Associate it with the thread
               tm.resume(oldTransaction);
               try
               {
                  return invokeNext(invocation, true);
               } finally
               {
                  tm.suspend();
               }
            }
            case MetaData.TX_NEVER:
            {
               if (oldTransaction != null)
               {
                  throw new EJBException("Transaction not allowed");
               }
               return invokeNext(invocation, false);
            }
            default:
                log.error("Unknown TX attribute "+transType+" for method"+invocation.getMethod());
         }
      }
      finally
      {
         // IN case we had a Tx associated with the thread reassociate
         if (threadTx != null)
            tm.resume(threadTx);
      }

      return null;
   }

   private void endTransaction(final Invocation invocation, final Transaction tx, final Transaction oldTx) 
      throws TransactionRolledbackException, SystemException
   {
      // Assert the correct transaction association
      Transaction current = tm.getTransaction();
      if ((tx == null && current != null) || tx.equals(current) == false)
         throw new IllegalStateException("Wrong transaction association: expected " + tx + " was " + current);

      try
      {
         // Marked rollback
         if (tx.getStatus() == Status.STATUS_MARKED_ROLLBACK)
         {
            tx.rollback();
         }
         else
         {
            // Commit tx
            // This will happen if
            // a) everything goes well
            // b) app. exception was thrown
            tx.commit();
         }
      }
      catch (RollbackException e)
      {
         throwJBossException(e, invocation.getType());
      }
      catch (HeuristicMixedException e)
      {
         throwJBossException(e, invocation.getType());
      }
      catch (HeuristicRollbackException e)
      {
         throwJBossException(e, invocation.getType());
      }
      catch (SystemException e)
      {
         throwJBossException(e, invocation.getType());
      }
      finally
      {
         // reassociate the oldTransaction with the Invocation (even null)
         invocation.setTransaction(oldTx);
         // Always drop thread association even if committing or
         // rolling back the newTransaction because not all TMs
         // will drop thread associations when commit() or rollback()
         // are called through tx itself (see JTA spec that seems to
         // indicate that thread assoc is required to be dropped only
         // when commit() and rollback() are called through TransactionManager
         // interface)
         //tx has committed, so we can't throw txRolledbackException.
         tm.suspend();
      }

   }


   // Protected  ----------------------------------------------------

   // This should be cached, since this method is called very often
   protected byte getTransactionMethod(Method m, InvocationType iface)
   {
      if(m == null)
      {
         return MetaData.TX_SUPPORTS;
      }

      Byte b = (Byte)methodTx.get(m);
      if (b != null) return b.byteValue();

      BeanMetaData bmd = container.getBeanMetaData();

      //DEBUG        log.debug("Found metadata for bean '"+bmd.getEjbName()+"'"+" method is "+m.getName());

      byte result = bmd.getMethodTransactionType(m.getName(), m.getParameterTypes(), iface);

      // provide default if method is not found in descriptor
      if (result == MetaData.TX_UNKNOWN) 
         result = MetaData.TX_REQUIRED;

      methodTx.put(m, new Byte(result));
      return result;
   }

   /**
    * Rethrow the exception as a rollback or rollback local
    *
    * @param e the exception
    * @param type the invocation type
    */
   protected void throwJBossException(Exception e, InvocationType type)
      throws TransactionRolledbackException
   {
      // Unwrap a nested exception if possible.  There is no
      // point in the extra wrapping, and the EJB spec should have
      // just used javax.transaction exceptions
      if (e instanceof NestedException)
         {
            NestedException rollback = (NestedException) e;
            if(rollback.getCause() instanceof Exception)
            {
               e = (Exception) rollback.getCause();
            }
         }
         if (type == InvocationType.LOCAL
             || type == InvocationType.LOCALHOME)
         {
            throw new JBossTransactionRolledbackLocalException(e);
         }
         else
         {
            throw new JBossTransactionRolledbackException(e);
         }
   }

   // Inner classes -------------------------------------------------

   // Monitorable implementation ------------------------------------
   public void sample(Object s)
   {
      // Just here to because Monitorable request it but will be removed soon
   }
   public Map retrieveStatistic()
   {
      return null;
   }
   public void resetStatistic()
   {
   }
}
