/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.spi;

import javax.resource.ResourceException;

/**
 * A CommException indicates error conditions related to failed or
 * interrupted communication with an underlying resource.  examples
 * include: communication protocol error or connection loss due to
 * server failure.
 */

public class CommException extends ResourceException {
    /**
     * Create an exception with a reason.
     */
    public CommException( String reason ) {
        super( reason );
    }

    /**
     * Create an exception with a reason and an errorCode.
     */
    public CommException( String reason, String errorCode ) {
        super( reason, errorCode );
    }
}
