/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.spi;

import javax.resource.ResourceException;

/**
 * A LocalTransactionException represents various error conditions
 * related to local transaction management.  Common error conditions
 * which are indicated by this exception include failure to commit,
 * attempt to start a transaction while inside a transaction, or
 * any resource adapter transaction related error condition.
 */

public class LocalTransactionException extends ResourceException {
    /**
     * Create an exception with a reason.
     */
    public LocalTransactionException( String reason ) {
        super( reason );
    }

    /**
     * Create an exception with a reason and an errorCode.
     */
    public LocalTransactionException( String reason, String errorCode ) {
        super( reason, errorCode );
    }
}
