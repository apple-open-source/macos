/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: TransactionRequestHandler.java,v 1.1 2002/04/02 13:48:39 cgjung Exp $

package org.jboss.net.axis.server;

import org.apache.axis.MessageContext;
import org.apache.axis.AxisFault;

import javax.transaction.Status;
import javax.transaction.SystemException;
import javax.transaction.NotSupportedException;

/**
 * This handler is to create an artifical "client"-side transaction
 * around the web-service request. Useful for interacting with entity beans.
 * It should be complemented by a seperate 
 * <code>org.jboss.net.axis.server.TransactionResponseHandler</code>
 * in the response chain to finish the transaction.
 * <br>
 * <h3>Change notes</h3>
 *   <ul>
 *   </ul>
 * @created  22.03.2002
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1 $
 */

public class TransactionRequestHandler extends TransactionResponseHandler {

   protected static final Object MARKER = new Object();

   public TransactionRequestHandler() throws Exception {
   }

   //
   // API
   //

   /**
    * begins a new transaction if not yet started
    * @see Handler#invoke(MessageContext)
    */
   public void invoke(MessageContext msgContext) throws AxisFault {
      try {
         if (userTransaction.getStatus() == Status.STATUS_NO_TRANSACTION
            && msgContext.getProperty(Constants.TRANSACTION_PROPERTY) == null) {
            userTransaction.begin();
            msgContext.setProperty(Constants.TRANSACTION_PROPERTY, MARKER);
         }
      } catch (SystemException e) {
         throw new AxisFault("Could not analyze tx setting.", e);
      } catch (NotSupportedException e) {
         throw new AxisFault("Could not begin tx.", e);
      }
   }
}
