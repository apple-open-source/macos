/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.spi;

import javax.resource.ResourceException;

/**
 * A SecurityException indicates error conditions related to the security
 * contract between an application server and a resource adapter.  Common
 * types of conditions represented by this exception include: invalid
 * security information - subject, failure to connect, failure to
 * authenticate, access control exception.
 */

public class SecurityException extends ResourceException {
    /**
     * Create an exception with a reason.
     */
    public SecurityException( String reason ) {
        super( reason );
    }

    /**
     * Create an exception with a reason and an errorCode.
     */
    public SecurityException( String reason, String errorCode ) {
        super( reason, errorCode );
    }
}
