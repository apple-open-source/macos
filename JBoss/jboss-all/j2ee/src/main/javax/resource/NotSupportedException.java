/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource;

/**
 * A NotSupportedException is thrown to indicate that the callee
 * (resource adapter or application server for system contracts)
 * cannot execute an operation because the operation is not a
 * supported feature.  For example, if the transaction support level
 * for a resource adapter is NO_TRANSACTION, an invocation of
 * ManagedConnection.getXAResource method should throw 
 * NotSupportedException exception.
 */

public class NotSupportedException extends ResourceException {

    /**
     * Create a not supported exception with a reason.
     */
    public NotSupportedException( String reason ) {
        super( reason );
    }

    /**
     * Create a not supported exception with a reason and an errorCode.
     */
    public NotSupportedException( String reason, String errorCode ) {
        super( reason, errorCode );
    }
}
