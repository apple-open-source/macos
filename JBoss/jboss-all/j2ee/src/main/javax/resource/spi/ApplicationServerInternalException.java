/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.spi;

import javax.resource.ResourceException;

/**
 * A ApplicationServerInternalException is thrown to indicate error
 * conditions specific to the Applcation server.  This could include
 * such errors as configuration related or implementation mechanism
 * related errors.
 */

public class ApplicationServerInternalException extends ResourceException {
    /**
     * Create an exception with a reason.
     */
    public ApplicationServerInternalException( String reason ) {
        super( reason );
    }

    /**
     * Create an exception with a reason and an errorCode.
     */
    public ApplicationServerInternalException( String reason, String errorCode ) {
        super( reason, errorCode );
    }
}
