/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.ejb.plugins;

import java.io.PrintWriter;
import java.io.StringWriter;

import java.rmi.RemoteException;
import java.rmi.ServerException;
import java.rmi.NoSuchObjectException;

import javax.transaction.TransactionManager;
import javax.transaction.TransactionRolledbackException;
import javax.transaction.SystemException;

import javax.ejb.EJBException;
import javax.ejb.NoSuchEntityException;
import javax.ejb.NoSuchObjectLocalException;
import javax.ejb.TransactionRolledbackLocalException;

import org.jboss.invocation.Invocation;
import org.jboss.invocation.InvocationType;

/**
 * A common superclass for the transaction interceptors.
 *
 * The only important method in this class is invokeNext which is incharge
 * of invoking the next interceptor and if an exception is thrown, it must
 * follow the rules in the EJB 2.0 specification section 18.3.  These 
 * rules specify if the transaction is rolled back and what exception
 * should be thrown.
 *
 * @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.10.2.2 $
 */
abstract class AbstractTxInterceptor
   extends AbstractInterceptor
{

   /** 
    * Local reference to the container's TransactionManager. 
    */
   protected TransactionManager tm;

   public void create() throws Exception
   {
      tm = getContainer().getTransactionManager();
   }

   /**
    * This method calls the next interceptor in the chain.
    *
    * All Throwables are caught and divided into two groups: application
    * exceptions and system exceptions.  Application exception are simply
    * rethrown.  System exceptions result in the transaction being marked 
    * for rollback only.  If the transaction was not started by the container
    * (i.e., it was inherited from the client) the system exception is wrapped
    * in a TransactionRolledBack[Local]Exception.
    *
    * @param invocation The <code>Invocation</code> of this call.
    *
    * @param inheritedTx If <code>true</code> the transaction has just been started 
    * in this interceptor.
    *
    * @throws Excetion if an exception occures in the interceptor chain.  The
    * actual exception throw is governed by the rules in the EJB 2.0 
    * specification section 18.3
    */
   protected Object invokeNext(
         Invocation invocation,
         boolean inheritedTx)
      throws Exception
   {
      InvocationType type = invocation.getType();
      try 
      {
         if (type == InvocationType.REMOTE || type == InvocationType.LOCAL) 
         {
            return getNext().invoke(invocation);
         }
         else
         {
            return getNext().invokeHome(invocation);
         }
      } 
      catch (Throwable e) 
      {
         // if this is an ApplicationException, just rethrow it
         if (e instanceof Exception && 
               !(e instanceof RuntimeException || e instanceof RemoteException))
         {
            throw (Exception)e;
         }

         // attempt to rollback the transaction
         if(invocation.getTransaction() != null) 
         {
            try 
            {
               invocation.getTransaction().setRollbackOnly();
            }
            catch (SystemException ex) 
            {
               log.error("SystemException while setting transaction " +
                     "for rollback only", ex);
            } 
            catch (IllegalStateException ex) 
            {
               log.error("IllegalStateException while setting transaction " +
                     "for rollback only", ex);
            }
         } 

         // is this a local invocation
         boolean isLocal = 
               type == InvocationType.LOCAL ||
               type == InvocationType.LOCALHOME;

         // if this transaction was NOT inherited from the caller we simply
         // rethrow the exception, and LogInterceptor will handle 
         // all exception conversions.
         if (!inheritedTx) 
         {
            if(e instanceof Exception) {
               throw (Exception) e;
            }
            if(e instanceof Error) {
               throw (Error) e;
            }

            // we have some funky throwable, wrap it
            if (isLocal) 
            {
               String msg = formatException("Unexpected Throwable", e);
               throw new EJBException(msg);
            }
            else 
            {
               ServerException ex = new ServerException("Unexpected Throwable");
               ex.detail = e;
               throw ex;
            }
         }
 
         // to be nice we coerce the execption to an interface friendly type
         // before wrapping it with a transaction rolled back exception
         Throwable cause;
         if (e instanceof NoSuchEntityException) 
         {
            NoSuchEntityException nsee = (NoSuchEntityException)e;
            if (isLocal) 
            {
               cause = new NoSuchObjectLocalException(
                     nsee.getMessage(),
                     nsee.getCausedByException());
            } else {
               cause = new NoSuchObjectException(nsee.getMessage());

               // set the detil of the exception
               ((NoSuchObjectException)cause).detail = 
                     nsee.getCausedByException();
            }
         }
         else 
         {
            if (isLocal) 
            {
               // local transaction rolled back exception can only wrap 
               // an exception so we create an EJBException for the cause
               if(e instanceof Exception) 
               {
                 cause = e; 
               }
               else if(e instanceof Error) 
               {
                 String msg = formatException("Unexpected Error", e);
                 cause = new EJBException(msg);
               }
               else
               {
                  String msg = formatException("Unexpected Throwable", e);
                  cause = new EJBException(msg);
               }
            }
            else 
            {
               // remote transaction rolled back exception can wrap
               // any throwable so we are ok
               cause = e;
            }
         }
         
         // We inherited tx: Tell caller we marked for rollback only.
         if (isLocal) 
         {
            if(cause instanceof TransactionRolledbackLocalException) {
               throw (TransactionRolledbackLocalException)cause;
            } else {
               throw new TransactionRolledbackLocalException(
                     cause.getMessage(),
                     (Exception)cause);
            }
         }
         else
         {
            if(cause instanceof TransactionRolledbackException) {
               throw (TransactionRolledbackException)cause;
            } else {
               TransactionRolledbackException ex = 
                     new TransactionRolledbackException(cause.getMessage());
               ex.detail = cause;
               throw ex;
            }
         }
      }
   }
   
   private String formatException(String msg, Throwable t)
   {
      StringWriter sw = new StringWriter();
      PrintWriter pw = new PrintWriter(sw);
      if( msg != null )
      {
         pw.println(msg);
      }
      t.printStackTrace(pw);
      return sw.toString();
   }
}
