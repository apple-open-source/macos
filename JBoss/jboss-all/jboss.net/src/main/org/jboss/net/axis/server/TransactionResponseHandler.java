/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: TransactionResponseHandler.java,v 1.1 2002/04/02 13:48:39 cgjung Exp $

package org.jboss.net.axis.server;

import org.apache.axis.AxisFault;
import org.apache.axis.MessageContext;
import org.apache.axis.handlers.BasicHandler;

import javax.transaction.Transaction;
import javax.transaction.UserTransaction;
import javax.transaction.RollbackException;
import javax.transaction.SystemException;
import javax.transaction.HeuristicMixedException;
import javax.transaction.HeuristicRollbackException;

import javax.naming.InitialContext;
import javax.naming.NamingException;

/**
 * This handler is to finish a previously opened client-side transaction.
 * <br>
 * <h3>Change notes</h3>
 *   <ul>
 *   </ul>
 * @created  22.03.2002
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1 $
 */

public class TransactionResponseHandler extends BasicHandler {

   final protected UserTransaction userTransaction;
   
   public TransactionResponseHandler() throws NamingException {
      userTransaction =
      	(UserTransaction) new InitialContext().
      	lookup(Constants.USER_TRANSACTION_JNDI_NAME);
   }
   
   //
   // Protected Helpers
   //

   protected void endTransaction(MessageContext msgContext, boolean commit)
      throws AxisFault {
      Object tx =
         msgContext.getProperty(Constants.TRANSACTION_PROPERTY);
      if (tx != null) {
         try {
            if (commit) {
               userTransaction.commit();
            } else {
               userTransaction.rollback();
            }
         } catch(RollbackException e) {
            throw new AxisFault("Could not rollback tx.",e);
         } catch(SystemException e) {
            throw new AxisFault("Could not influence tx setting.",e);
         } catch(HeuristicMixedException e) {
            throw new AxisFault("Could not commit tx.",e);
         } catch(HeuristicRollbackException e) {
            throw new AxisFault("Could not commit tx.",e);
         } finally {
            msgContext.setProperty(Constants.TRANSACTION_PROPERTY, null);
         }
      }
   }

   //
   // API
   //

   /*
    * @see Handler#invoke(MessageContext)
    */
   public void invoke(MessageContext msgContext) throws AxisFault {
      endTransaction(msgContext,true);
   }

   /*
    * @see Handler#onFault(MessageContext)
    */
   public void onFault(MessageContext msgContext)  {
      try{
         endTransaction(msgContext,false);
      } catch(AxisFault e) {
      }
   }

}