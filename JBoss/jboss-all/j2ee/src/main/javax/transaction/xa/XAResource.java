/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.transaction.xa;

/**
 *  The XAResource interface is a Java mapping of the industry standard XA
 *  interface based on the X/Open CAE Specification (Distributed Transaction
 *  Processing: The XA Specification). 
 *  <p>
 *  The XA interface defines the contract between a Resource Manager and a
 *  Transaction Manager in a distributed transaction processing (DTP)
 *  environment.
 *  An XA resource such as a JDBC driver or a JMS provider implements this
 *  interface to support association between a global transaction and a
 *  database or message service connection. 
 *  <p>
 *  The XAResource interface can be supported by any transactional resource
 *  that is intended to be used by application programs in an environment
 *  where transactions are controlled by an external transaction manager.
 *  An example of such a resource is a database management system.
 *  An application may access data through multiple database connections.
 *  Each database connection is enlisted with the transaction manager as a
 *  transactional resource. The transaction manager obtains an XAResource for
 *  each connection participating in a global transaction. The transaction
 *  manager uses the {@link #start(Xid, int) start} method to associate the
 *  global transaction with the resource, and it uses the
 *  {@link #end(Xid, int) end} method to disassociate the transaction from
 *  the resource.
 *  The resource manager is responsible for associating the global
 *  transaction to all work performed on its data between the start and
 *  end method invocation. 
 *  <p>
 *  At transaction commit time, the resource managers are informed by the
 *  transaction manager to prepare, commit, or rollback a transaction
 *  according to the two-phase commit protocol.
 *  <p>
 *  Behind the resources that implement this interface the resource manager
 *  exists. The resource manager does not have a public interface or direct
 *  references, and can manage several resources.
 *  To see if two resources are managed by the same resource manager, the
 *  {@link #isSameRM(XAResource)} method can be used.
 *
 * @version $Revision: 1.2 $
 */
public interface XAResource
{
    /**
     *  Flag value indicating that no flags are set.
     */
    public static final int TMNOFLAGS = 0;

    /**
     *  JTA specifies this constant and states that it indicates that the
     *  caller is using one-phase optimization, but this constant seems
     *  not to be used by JTA.
     */
    public static final int TMONEPHASE = 1073741824;

    /**
     *  Flag value for the {@link #start(Xid, int) start} method indicating
     *  that the resource should associate with a transaction previously seen
     *  by this resource manager.
     */
    public static final int TMJOIN = 2097152;

    /**
     *  Flag value for the {@link #start(Xid, int) start} method indicating
     *  that the resource should associate with a transaction where the
     *  association was suspended.
     */
    public static final int TMRESUME = 134217728;

    /**
     *  Flag value for the {@link #end(Xid, int) end} method indicating that
     *  the transaction should be disassociated, and that the work has
     *  completed sucessfully.
     */
    public static final int TMSUCCESS = 67108864;

    /**
     *  Flag value for the {@link #end(Xid, int) end} method indicating that
     *  the transaction should be disassociated, and that the work has
     *  failed
     */
    public static final int TMFAIL = 536870912;

    /**
     *  Flag value for the {@link #end(Xid, int) end} method indicating that
     *  the resource should temporarily suspend the association with the
     *  transaction.
     */
    public static final int TMSUSPEND = 33554432;

    /**
     *  Value returned from the {@link #prepare(Xid) prepare} method to
     *  indicate that the resource was not changed in this transaction.
     */
    public static final int XA_RDONLY = 3;

    /**
     *  Value returned from the {@link #prepare(Xid) prepare} method to
     *  indicate that the resource has successfully prepared to commit
     *  the transaction.
     */
    public static final int XA_OK = 0;

    /**
     *  Flag value for the {@link #recover(int) recover} method indicating
     *  that the resource manager should start a new recovery scan.
     */
    public static final int TMSTARTRSCAN = 16777216;

    /**
     *  Flag value for the {@link #recover(int) recover} method indicating
     *  that the resource manager should end the current recovery scan.
     */
    public static final int TMENDRSCAN = 8388608;


    /**
     *  Called to associate the resource with a transaction.
     *
     *  If the flags argument is {@link #TMNOFLAGS}, the transaction must not
     *  previously have been seen by this resource manager, or an
     *  {@link XAException} with error code XAER_DUPID will be thrown.
     *
     *  If the flags argument is {@link #TMJOIN}, the resource will join a
     *  transaction previously seen by tis resource manager.
     *
     *  If the flags argument is {@link #TMRESUME} the resource will
     *  resume the transaction association that was suspended with
     *  end(TMSUSPEND).
     *
     *  @param xid The id of the transaction to associate with.
     *  @param flags Must be either {@link #TMNOFLAGS}, {@link #TMJOIN}
     *               or {@link #TMRESUME}.
     *  @throws XAException If an error occurred.
     */
    public void start(Xid xid, int flags) throws XAException;

    /**
     *  Called to disassociate the resource from a transaction.
     *
     *  If the flags argument is {@link #TMSUCCESS}, the portion of work
     *  was done sucessfully.
     *
     *  If the flags argument is {@link #TMFAIL}, the portion of work
     *  failed. The resource manager may mark the transaction for
     *  rollback only to avoid the transaction being committed.
     *
     *  If the flags argument is {@link #TMSUSPEND} the resource will
     *  temporarily suspend the transaction association. The transaction
     *  must later be re-associated by giving the {@link #TMRESUME} flag
     *  to the {@link #start(Xid,int) start} method.
     *
     *  @param xid The id of the transaction to disassociate from.
     *  @param flags Must be either {@link #TMSUCCESS}, {@link #TMFAIL}
     *               or {@link #TMSUSPEND}.
     *  @throws XAException If an error occurred.
     */
    public void end(Xid xid, int flags) throws XAException;

    /**
     *  Prepare to commit the work done on this resource in the given
     *  transaction.
     *
     *  This method cannot return a status indicating that the transaction
     *  should be rolled back. If the resource wants the transaction to
     *  be rolled back, it should throw an <code>XAException</code> at the
     *  caller.
     *
     *  @param xid The id of the transaction to prepare to commit work for.
     *  @return Either {@link #XA_OK} or {@link #XA_RDONLY}.
     *  @throws XAException If an error occurred.
     */
    public int prepare(Xid xid) throws XAException;

    /**
     *  Commit the work done on this resource in the given transaction.
     *
     *  If the <code>onePhase</code> argument is true, one-phase
     *  optimization is being used, and the {@link #prepare(Xid) prepare}
     *  method must not have been called for this transaction.
     *  Otherwise, this is the second phase of the two-phase commit protocol.
     *
     *  @param xid The id of the transaction to commit work for.
     *  @param onePhase If true, the transaction manager is using one-phase
     *                  optimization.
     *  @throws XAException If an error occurred.
     */
    public void commit(Xid xid, boolean onePhase) throws XAException;

    /**
     *  Roll back the work done on this resource in the given transaction.
     *
     *  @param xid The id of the transaction to commit work for.
     *  @throws XAException If an error occurred.
     */
    public void rollback(Xid xid) throws XAException;

    /**
     *  Tells the resource manager to forget about a heuristic decision.
     *
     *  @param xid The id of the transaction that was ended with a heuristic
     *             decision.
     *  @throws XAException If an error occurred.
     */
    public void forget(Xid xid) throws XAException;

    /**
     *  Return a list of transactions that are in a prepared or heuristically
     *  state.
     *
     *  This method looks not only at the resource it is invoked on, but
     *  also on all other resources managed by the same resource manager.
     *  It is intended to be used by the application server when recovering
     *  after a server crash.
     *  <p>
     *  A recovery scan is done with one or more calls to this method.
     *  At the first call, {@link #TMSTARTRSCAN} must be in the
     *  <code>flag</code> argument to indicate that the scan should be started.
     *  During the recovery scan, the resource manager maintains an internal
     *  cursor that keeps track of the progress of the recovery scan.
     *  To end the recovery scan, the {@link #TMENDRSCAN} must be passed
     *  in the <code>flag</code> argument.
     *
     *  @param flag Must be either {@link #TMNOFLAGS}, {@link #TMSTARTRSCAN},
     *              {@link #TMENDRSCAN} or <code>TMSTARTRSCAN|TMENDRSCAN</code>.
     *  @return An array of zero or more transaction ids.
     *  @throws XAException If an error occurred.
     */
    public Xid[] recover(int flag) throws XAException;

    /**
     *  Tells the caller if this resource has the same resource manager
     *  as the argument resource.
     *
     *  The transaction manager needs this method to be able to decide
     *  if the {@link #start(Xid,int) start} method should be given the
     *  {@link #TMJOIN} flag.
     *
     *  @throws XAException If an error occurred.
     */
    public boolean isSameRM(XAResource xaRes) throws XAException;

    /**
     *  Get the current transaction timeout value for this resource.
     *
     *  @return The current timeout value, in seconds.
     *  @throws XAException If an error occurred.
     */
    public int getTransactionTimeout() throws XAException;

    /**
     *  Set the transaction timeout value for this resource.
     *
     *  If the <code>seconds</code> argument is <code>0</code>, the
     *  timeout value is set to the default timeout value of the resource
     *  manager.
     *
     *  Not all resource managers support setting the timeout value.
     *  If the resource manager does not support setting the timeout
     *  value, it should return false.
     *
     *  @param seconds The timeout value, in seconds.
     *  @return True if the timeout value could be set, otherwise false.
     *  @throws XAException If an error occurred.
     */
    public boolean setTransactionTimeout(int seconds) throws XAException;
}
