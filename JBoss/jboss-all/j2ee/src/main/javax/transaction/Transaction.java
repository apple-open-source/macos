/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.transaction;

import java.lang.SecurityException;
import javax.transaction.xa.XAResource;

/**
 *  The Transaction interface allows operations to be performed on
 *  transactions.
 *
 *  @version $Revision: 1.2 $
 */
public interface Transaction
{
    /**
     *  Attempt to commit this transaction.
     *
     *  @throws RollbackException If the transaction was marked for rollback
     *          only, the transaction is rolled back and this exception is
     *          thrown.
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
    public void commit()
        throws RollbackException, HeuristicMixedException,
            HeuristicRollbackException, SecurityException, SystemException;

    /**
     *  Rolls back this transaction.
     * 
     *  @throws IllegalStateException If the transaction is in a state
     *          where it cannot be rolled back. This could be because the
     *          transaction is no longer active, or because it is in the
     *          {@link Status#STATUS_PREPARED prepared state}.
     *  @throws SystemException If the transaction service fails in an
     *          unexpected way.
     */
    public void rollback()
        throws IllegalStateException, SystemException;

    /**
     *  Mark the transaction so that the only possible outcome is a rollback.
     * 
     *  @throws IllegalStateException If the transaction is not in an active
     *          state.
     *  @throws SystemException If the transaction service fails in an
     *          unexpected way.
     */
    public void setRollbackOnly()
        throws IllegalStateException, SystemException;

    /**
     *  Get the status of the transaction.
     *
     *  @return The status of the transaction. This is one of the
     *          {@link Status} constants.
     *
     *  @throws SystemException If the transaction service fails in an
     *          unexpected way.
     */
    public int getStatus() throws SystemException;

    /**
     *  Enlist an XA resource with this transaction.
     *
     *  @return <code>true</code> if the resource could be enlisted with
     *          this transaction, otherwise <code>false</code>.
     *
     *  @throws RollbackException If the transaction is marked for rollback
     *          only.
     *  @throws IllegalStateException If the transaction is in a state
     *          where resources cannot be enlisted. This could be because the
     *          transaction is no longer active, or because it is in the
     *          {@link Status#STATUS_PREPARED prepared state}.
     *  @throws SystemException If the transaction service fails in an
     *          unexpected way.
     */
    public boolean enlistResource(XAResource xaRes)
        throws RollbackException, IllegalStateException, SystemException;

    /**
     *  Delist an XA resource from this transaction.
     *
     *  @return <code>true</code> if the resource could be delisted from
     *          this transaction, otherwise <code>false</code>.
     *
     *  @throws IllegalStateException If the transaction is in a state
     *          where resources cannot be delisted. This could be because the
     *          transaction is no longer active.
     *  @throws SystemException If the transaction service fails in an
     *          unexpected way.
     */
    public boolean delistResource(XAResource xaRes, int flag)
        throws IllegalStateException, SystemException;

    /**
     *  Register a {@link Synchronization} callback with this transaction.
     *
     *  @throws RollbackException If the transaction is marked for rollback
     *          only.
     *  @throws IllegalStateException If the transaction is in a state
     *          where {@link Synchronization} callbacks cannot be registered.
     *          This could be because the transaction is no longer active,
     *          or because it is in the
     *          {@link Status#STATUS_PREPARED prepared state}.
     *  @throws SystemException If the transaction service fails in an
     *          unexpected way.
     */
    public void registerSynchronization(Synchronization sync)
        throws RollbackException, IllegalStateException, SystemException;
}
