/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.transaction;

/**
 *  The RollbackException exception indicates that either the transaction
 *  has been rolled back or an operation cannot complete because the
 *  transaction is marked for rollback only.
 *  <p>
 *  It is thrown under two circumstances:
 *  <ul>
 *    <li>
 *    At transaction commit time, if the transaction has been marked for
 *    rollback only. In this case, the <code>commit</code> method will roll
 *    back the transaction and throw this exception to indicate that the
 *    transaction could not be committed.
 *    </li>
 *    <li>
 *    At other times, if an operation cannot be completed because the
 *    transaction is marked for rollback only.
 *    The {@link Transaction#enlistResource(javax.transaction.xa.XAResource) enlistResource}
 *    and {@link Transaction#registerSynchronization(Synchronization) registerSynchronization}
 *    methods in the {@link Transaction} interface throw this exception to
 *    indicate that the operation cannot be completed because the transaction
 *    is marked for rollback only. In this case, the state of the transaction
 *    remains unchanged.
 *    </li>
 *  </ul>
 *
 *  @version $Revision: 1.2 $
 */
public class RollbackException extends Exception
{

    /**
     *  Creates a new <code>RollbackException</code> without a detail message.
     */
    public RollbackException()
    {
    }

    /**
     *  Constructs an <code>RollbackException</code> with the specified
     *  detail message.
     *
     *  @param msg the detail message.
     */
    public RollbackException(String msg)
    {
        super(msg);
    }
}
