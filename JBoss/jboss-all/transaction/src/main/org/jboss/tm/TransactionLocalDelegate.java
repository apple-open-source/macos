/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license
 * See terms of license at gnu.org.
 */
package org.jboss.tm;

import javax.transaction.Transaction;

/**
 * The interface to implementated for a transaction local implementation
 *
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public interface TransactionLocalDelegate
{
   /**
    * get the transaction local value.
    */
   Object getValue(TransactionLocal local, Transaction tx);

   /**
    * put the value in the transaction local
    */
   void storeValue(TransactionLocal local, Transaction tx, Object value);

   /**
    * does Transaction contain object?
    */
   boolean containsValue(TransactionLocal local, Transaction tx);
}
