/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.transaction;

import java.rmi.RemoteException;

/**
 *  This exception indicates an invalid transaction.
 *  <p>
 *  It is thrown from the {@link TransactionManager#resume(Transaction)}
 *  method if the argument is not a valid transaction.
 *  It may also be thrown from an EJB container invocation is the invocation
 *  request carries an invalid transaction propagation context.
 *
 *  @version $Revision: 1.2 $
 */
public class InvalidTransactionException extends RemoteException
{

    /**
     *  Creates a new <code>InvalidTransactionException</code> without a
     *  detail message.
     */
    public InvalidTransactionException()
    {
    }

    /**
     *  Constructs an <code>InvalidTransactionException</code> with the
     *  specified detail message.
     *
     *  @param msg the detail message.
     */
    public InvalidTransactionException(String msg)
    {
        super(msg);
    }
}
