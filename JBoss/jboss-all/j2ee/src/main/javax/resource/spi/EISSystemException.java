/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.spi;

import javax.resource.ResourceException;

/**
 * A EISSystemException is used to indicate EIS specific error conditios.
 * Common error conditions are failure of the EIS, communication failure
 * or an EIS specific failure during creation of a new connection.
 */

public class EISSystemException extends ResourceException {
    /**
     * Create an exception with a reason.
     */
    public EISSystemException( String reason ) {
        super( reason );
    }

    /**
     * Create an exception with a reason and an errorCode.
     */
    public EISSystemException( String reason, String errorCode ) {
        super( reason, errorCode );
    }
}
