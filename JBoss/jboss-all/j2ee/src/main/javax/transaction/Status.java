/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.transaction;

/**
 *  The Status interface defines the constants for transaction status codes.
 *
 *  @version $Revision: 1.2 $
 */
public interface Status
{
    /**
     *  Status code indicating an active transaction.
     */
    public static final int STATUS_ACTIVE = 0;

    /**
     *  Status code indicating a transaction that has been marked for
     *  rollback only.
     */
    public static final int STATUS_MARKED_ROLLBACK = 1;

    /**
     *  Status code indicating a transaction that has completed the first
     *  phase of the two-phase commit protocol, but not yet begun the
     *  second phase.
     *  Probably the transaction is waiting for instruction from a superior
     *  coordinator on how to proceed.
     */
    public static final int STATUS_PREPARED = 2;

    /**
     *  Status code indicating a transaction that has been committed.
     *  Probably heuristics still exists, or the transaction would no
     *  longer exist.
     */
    public static final int STATUS_COMMITTED = 3;

    /**
     *  Status code indicating a transaction that has been rolled back.
     *  Probably heuristics still exists, or the transaction would no
     *  longer exist.
     */
    public static final int STATUS_ROLLEDBACK = 4;

    /**
     *  Status code indicating that the transaction status could not be
     *  determined.
     */
    public static final int STATUS_UNKNOWN = 5;

    /**
     *  Status code indicating that no transaction exists.
     */
    public static final int STATUS_NO_TRANSACTION = 6;

    /**
     *  Status code indicating a transaction that has begun the first
     *  phase of the two-phase commit protocol, not not yet completed
     *  this phase.
     */
    public static final int STATUS_PREPARING = 7;

    /**
     *  Status code indicating a transaction that has begun the second
     *  phase of the two-phase commit protocol, but not yet completed
     *  this phase.
     */
    public static final int STATUS_COMMITTING = 8;

    /**
     *  Status code indicating a transaction that is in the process of
     *  rolling back.
     */
    public static final int STATUS_ROLLING_BACK = 9;
}
