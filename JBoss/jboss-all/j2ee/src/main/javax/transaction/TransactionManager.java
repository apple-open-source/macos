/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.transaction;

/**
 *  The TransactionManager interface defines the methods that allow an
 *  application server to manage transactions on behalf of the applications.
 *  <p>
 *  User applications should not use this interface directly, but use
 *  {@link UserTransaction} insted if they need to do their own transaction
 *  management.
 *  <p>
 *  Internally, the transaction manager associates transactions with threads,
 *  and the methods here operate on the transaction associated with the
 *  calling thread.
 *
 *  @version $Revision: 1.2 $
 */
public interface TransactionManager
{
    /**
     *  Starts a new transaction, and associate it with the calling thread.
     *
     *  @throws NotSupportedException If the calling thread is already
     *          associated with a transaction, and nested transactions are
     *          not supported.
     *  @throws SystemException If the transaction service fails in an
     *          unexpected way.
     */
    public void begin() throws NotSupportedException, SystemException;

    /**
     *  Commit the transaction associated with the calling thread.
     *
     *  @throws RollbackException If the transaction was marked for rollback
     *          only, the transaction is rolled back and this exception is
     *          thrown.
     *  @throws IllegalStateException If the calling thread is not associated
     *          with a transaction.
     *  @throws SystemException If the transaction service fails in an
     *          unexpected way.
     *  @throws HeuristicMixedException If a heuristic decision was made and
     *          some some parts of the transaction have been committed while
     *          other parts have been rolled back.
     *  @throws HeuristicRollbackException If a heuristic decision to roll
     *          back the transaction was made.
     *  @throws SecurityException If the caller is not allowed to commit this
     *          transaction.
     */
    public void commit() throws RollbackException, HeuristicMixedException,
                   HeuristicRollbackException, SecurityException,
                   IllegalStateException, SystemException;

    /**
     *  Rolls back the transaction associated with the calling thread.
     * 
     *  @throws IllegalStateException If the transaction is in a state
     *          where it cannot be rolled back. This could be because the
     *          calling thread is not associated with a transaction, or
     *          because it is in the
     *          {@link Status#STATUS_PREPARED prepared state}.
     *  @throws SecurityException If the caller is not allowed to roll back
     *          this transaction.
     *  @throws SystemException If the transaction service fails in an
     *          unexpected way.
     */
    public void rollback() throws IllegalStateException, SecurityException,
                   SystemException;

    /**
     *  Mark the transaction associated with the calling thread for rollback
     *  only.
     * 
     *  @throws IllegalStateException If the transaction is in a state
     *          where it cannot be rolled back. This could be because the
     *          calling thread is not associated with a transaction, or
     *          because it is in the
     *          {@link Status#STATUS_PREPARED prepared state}.
     *  @throws SystemException If the transaction service fails in an
     *          unexpected way.
     */
    public void setRollbackOnly() throws IllegalStateException, SystemException;

    /**
     *  Get the status of the transaction associated with the calling thread.
     *
     *  @return The status of the transaction. This is one of the
     *          {@link Status} constants. If no transaction is associated
     *          with the calling thread,
     *          {@link Status#STATUS_NO_TRANSACTION} is returned. 
     *
     *  @throws SystemException If the transaction service fails in an
     *          unexpected way.
     */
    public int getStatus() throws SystemException;

    /**
     *  Get the transaction associated with the calling thread.
     *
     *  @return The transaction associated with the calling thread, or
     *          <code>null</code> if the calling thread is not associated
     *          with a transaction.
     *
     *  @throws SystemException If the transaction service fails in an
     *          unexpected way.
     */
    public Transaction getTransaction() throws SystemException;

    /**
     *  Change the transaction timeout for transactions started by the calling
     *  thread with the {@link #begin()} method.
     *
     *  @param seconds The new timeout value, in seconds. If this parameter
     *         is <code>0</code>, the timeout value is reset to the default
     *         value.
     *
     *  @throws SystemException If the transaction service fails in an
     *          unexpected way.
     */
    public void setTransactionTimeout(int seconds) throws SystemException;

    /**
     *  Suspend the association the calling thread has to a transaction,
     *  and return the suspended transaction.
     *  When returning from this method, the calling thread is no longer
     *  associated with a transaction.
     *
     *  @return The transaction that the calling thread was associated with,
     *          or <code>null</code> if the calling thread was not associated
     *          with a transaction.
     *
     *  @throws SystemException If the transaction service fails in an
     *          unexpected way.
     */
    public Transaction suspend() throws SystemException;

    /**
     *  Resume the association of the calling thread with the given
     *  transaction.
     *
     *  @param tobj The transaction to be associated with the calling thread.
     *
     *  @throws InvalidTransactionException If the argument does not represent
     *          a valid transaction.
     *  @throws IllegalStateException If the calling thread is already
     *          associated with a transaction.
     *  @throws SystemException If the transaction service fails in an
     *          unexpected way.
     */
    public void resume(Transaction tobj) throws InvalidTransactionException,
                   IllegalStateException, SystemException;
}
