/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.transaction.xa;

/**
 *  The XAException is thrown by resource managers in case of problems.
 *
 *  @version $Revision: 1.6 $
 */
public class XAException extends java.lang.Exception {

    /**
     *  The error code.
     */
    public int errorCode = 0;

    /**
     *  Creates new <code>XAException</code> without detail message.
     */
    public XAException() {
    }

    /**
     *  Constructs an <code>XAException</code> with the specified detail
     *  message.
     *
     *  @param msg the detail message.
     */
    public XAException(String msg) {
        super(msg);
    }

    /**
     *  Constructs an <code>XAException</code> for the specified error code.
     *
     *  @param errorCode the error code.
     */
    public XAException(int errorCode) {
        super();
        this.errorCode = errorCode;
    }

    // STATIC VARIABLES ---------------------------------

    // changed numbers to match SUNs by pkendall for interoperability

    // added by kimptoc - needed for jbossmq to compile...
    // others added by jwalters for completeness

    /**
     *  Error code indicating that an asynchronous operation is outstanding.
     */
    public static final int XAER_ASYNC     = -2;

    /**
     *  Error code indicating that a resource manager error has occurred.
     */
    public static final int XAER_RMERR     = -3;

    /**
     *  Error code indicating that an {@link Xid} is not valid.
     */
    public static final int XAER_NOTA      = -4;

    /**
     *  Error code indicating that invalid arguments were passed.
     */
    public static final int XAER_INVAL     = -5;

    /**
     *  Error code indicating a protocol error. This happens if a method
     *  is invoked on a resource when it is not in the correct state for it.
     */
    public static final int XAER_PROTO     = -6;

    /**
     *  Error code indicating that the resource manager has failed and is
     *  not available.
     */
    public static final int XAER_RMFAIL    = -7;

    /**
     *  Error code indicating that a Xid given as an argument is already
     *  known to the resource manager.
     */
    public static final int XAER_DUPID     = -8;

    /**
     *  Error code indicating that the resource manager is doing work
     *  outside the global transaction.
     */
    public static final int XAER_OUTSIDE   = -9;

    // added by jwalters - needed for jboss 

    /**
     *  Error code indicating that the transaction branch was read-only,
     *  and has already been committed.
     */
    public static final int XA_RDONLY      = 3;

    /**
     *  Error code indicating that the method invoked returned without having
     *  any effect, and that it may be invoked again.
     *  Note that this constant is not defined in JTA 1.0.1, but appears in
     *  J2EE(TM) as shipped by SUN.
     */
    public static final int XA_RETRY       = 4;

    /**
     *  Error code indicating that a heuristic mixed decision was made.
     *  This indicates that parts of the transaction were committed,
     *  while other parts were rolled back.
     */
    public static final int XA_HEURMIX     = 5;

    /**
     *  Error code indicating that a heuristic rollback decision was made.
     */
    public static final int XA_HEURRB      = 6;

    /**
     *  Error code indicating that a heuristic commit decision was made.
     */
    public static final int XA_HEURCOM     = 7;

    /**
     *  Error code indicating that a heuristic decision may have been made.
     *  The outcome of all parts of the transaction is not known, but the
     *  outcome of the known parts are either all committed, or all rolled
     *  back.
     */
    public static final int XA_HEURHAZ     = 8;

    /**
     *  Error code indicating that the transaction resumption must happen
     *  where the suspension occurred.
     */
    public static final int XA_NOMIGRATE   = 9;

    /**
     *  This is not an error code, but the same as the rollback error code
     *  with the lowest number.
     */
    public static final int XA_RBBASE      = 100;

    /**
     *  Rollback error code indicating that the rollback happened for
     *  an unspecified reason.
     */
    public static final int XA_RBROLLBACK  = 100;

    /**
     *  Rollback error code indicating that the rollback happened due to a
     *  communications failure.
     */
    public static final int XA_RBCOMMFAIL  = 101;

    /**
     *  Rollback error code indicating that the rollback happened because
     *  deadlock was detected.
     */
    public static final int XA_RBDEADLOCK  = 102;

    /**
     *  Rollback error code indicating that the rollback happened because
     *  an internal integrity check failed.
     */
    public static final int XA_RBINTEGRITY = 103;

    /**
     *  Rollback error code indicating that the rollback happened for some
     *  reason not fitting any of the other rollback error codes.
     */
    public static final int XA_RBOTHER     = 104;

    /**
     *  Rollback error code indicating that the rollback happened due to
     *  a protocol error in the resource manager.
     */
    public static final int XA_RBPROTO     = 105;

    /**
     *  Rollback error code indicating that the rollback happened because
     *  of a timeout.
     */
    public static final int XA_RBTIMEOUT   = 106;

    /**
     *  Rollback error code indicating that the rollback happened due to a
     *  transient failure. The transaction branch may be retried.
     */
    public static final int XA_RBTRANSIENT = 107;

    /**
     *  This is not an error code, but the same as the rollback error code
     *  with the highest number.
     */
    public static final int XA_RBEND       = 107;
}


