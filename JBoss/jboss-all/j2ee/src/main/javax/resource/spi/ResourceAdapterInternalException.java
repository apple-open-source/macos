/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.spi;

import javax.resource.ResourceException;

/**
 * A ResourceAdapterInternalException indicates any system level error
 * conditions related to a resource adapter.  Examples are invalid
 * configuration, failure to create a connection to an underlying
 * resource, other error condition internal to the resource adapter.
 */

public class ResourceAdapterInternalException extends ResourceException {
    /**
     * Create an exception with a reason.
     */
    public ResourceAdapterInternalException( String reason ) {
        super( reason );
    }

    /**
     * Create an exception with a reason and an errorCode.
     */
    public ResourceAdapterInternalException( String reason, String errorCode ) {
        super( reason, errorCode );
    }
}
