/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.spi;

import javax.resource.ResourceException;

/**
 * A IllegalStateException is thrown when a method has been invoked on
 * an object which is in the wrong state to execute the method.
 */

public class IllegalStateException extends ResourceException {
    /**
     * Create an exception with a reason.
     */
    public IllegalStateException( String reason ) {
        super( reason );
    }

    /**
     * Create an exception with a reason and an errorCode.
     */
    public IllegalStateException( String reason, String errorCode ) {
        super( reason, errorCode );
    }
}
