/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.tm.plugins.tyrex;

import javax.naming.Name;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.Reference;
import javax.transaction.Transaction;
import javax.transaction.TransactionManager;

import org.apache.log4j.Category;

import tyrex.tm.TransactionDomain;
import org.omg.CosTransactions.PropagationContextHolder;
import org.omg.CosTransactions.PropagationContext;
import org.omg.CosTSPortability.Sender;
import org.omg.CosTSPortability.Receiver;

/**
 *   This object implements the TransactionPropagationContext importer and
 *   exporter for JBoss.
 *
 *   @see TransactionManagerService
 *   @author <a href="mailto:akkerman@cs.nyu.edu">Anatoly Akkerman</a>
 *   @version $Revision: 1.3 $
 */

public class TyrexTransactionPropagationContextManager implements
    org.jboss.tm.TransactionPropagationContextFactory,
    org.jboss.tm.TransactionPropagationContextImporter
{
// -- Constants -------------------------------------------------------

  private static String JNDI_TPC_SENDER = "java:/TPCSender";
  private static String JNDI_TPC_RECEIVER = "java:/TPCReceiver";
  private static String JNDI_TM = "java:/TransactionManager";

// -- Private stuff ---------------------------------------------------
  private Sender sender = null;
  private Receiver receiver = null;
  private TransactionManager tm = null;

  private Category log = Category.getInstance(this.getClass());

// -- Constructors ----------------------------------------------------
  protected TyrexTransactionPropagationContextManager() {
    try {
      Context ctx = new InitialContext();
      this.sender = (Sender) ctx.lookup(JNDI_TPC_SENDER);
      this.receiver = (Receiver) ctx.lookup(JNDI_TPC_RECEIVER);
      this.tm = (TransactionManager) ctx.lookup(JNDI_TM);
    } catch (Exception e) {
      e.printStackTrace();
    }
  }


// ------------ TransactionPropagationContextFactory methods ------------
  /**
   * Return a transaction propagation context associated with
   * transaction that the calling thread came in with
   */
  public Object getTransactionPropagationContext() {
    Object tpc = null;

    try {
      if (tm.getTransaction() != null) {

        PropagationContextHolder tpcHolder = new PropagationContextHolder();
        sender.sending_request(0,tpcHolder);

        // now modify the tpc that is inside this tpcHolder and package it
        // into a serializable entity
        tpc = new TyrexTxPropagationContext(tpcHolder.value);
        // DEBUG        log.debug("Exporting a transaction");
      } else {
        // this tpc represents a null transaction and will be propagated to remote side
        tpc = new TyrexTxPropagationContext();
        // DEBUG        log.debug("Exporting null transaction");
      }
    } catch (Exception e) {
      // DEBUG
      log.warn ("TyrexTransactionPropagationContextManager: unable to create propagation ctx!");
      e.printStackTrace();
    } finally {
      return tpc;
    }

  }

   /**
    *  Return a transaction propagation context for the transaction
    *  given as an argument, or <code>null</code>
    *  if the argument is <code>null</code> or of a type unknown to
    *  this factory.
    */
  public Object getTransactionPropagationContext(Transaction tx) {
    Transaction oldTx = null;
    Object tpc = null;

    try {
      oldTx = tm.getTransaction();
      if ( (tx == null) || (tx.equals(oldTx)) ) {
        // we are being called in the context of this transaction
        tpc = getTransactionPropagationContext();
      }
      else {
        tm.suspend();
        tm.resume(tx);
        tpc = getTransactionPropagationContext();
        tm.suspend();
        tm.resume(oldTx);
      }
    } catch (Exception e) {
      e.printStackTrace();
    } finally {
        return tpc;
    }
  }

// ------------- TransactionPropagationContextImporter methods -----------

  public Transaction importTransactionPropagationContext(Object tpc) {
    if (tpc instanceof TyrexTxPropagationContext) {
      Transaction oldTx;
      try {
        // DEBUG        log.debug ("TyrexTransactionPropagationContextManager: importing tpc.");
        oldTx = tm.suspend(); //cleanup the incoming thread
        PropagationContext omgTpc = ((TyrexTxPropagationContext) tpc).getPropagationContext();

        Transaction newTx = null; // if omgTpc is null, then newTx will remain null

        if (omgTpc != null) {
          receiver.received_request(0, omgTpc);
          // transaction gets resumed during the call
          // to txFactory, since we need just the transaction object,
          // get it and then suspend the transaction
          newTx = tm.getTransaction();
          tm.suspend();
          // now restart the original transaction
          if (oldTx != null)
            tm.resume(oldTx);
        } else {
          //DEBUG          log.debug("Importing null transaction");
        }
        // DEBUG        log.debug ("TyrexTransactionPropagationContextManager: transaction imported.");
        return newTx;
      } catch (Exception e) {
        e.printStackTrace();
        return null;
      }
    }
    else {
      log.warn("TyrexTransactionPropagationContextManager: unknown Tx PropagationContex");
      return null;
    }
  }
}
