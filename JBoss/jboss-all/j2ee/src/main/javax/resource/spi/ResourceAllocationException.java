/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.spi;

import javax.resource.ResourceException;

/**
 * A ResourceAllocationException can be thrown to indicate a failure
 * to allocate system resources such as threads or physical connections.
 */

public class ResourceAllocationException extends ResourceException {
    /**
     * Create an exception with a reason.
     */
    public ResourceAllocationException( String reason ) {
        super( reason );
    }

    /**
     * Create an exception with a reason and an errorCode.
     */
    public ResourceAllocationException( String reason, String errorCode ) {
        super( reason, errorCode );
    }
}
