/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.spi;

import javax.resource.ResourceException;

/**
 * The ConnectionRequestInfo allows a resource adapter to pass its own
 * information along with a request for a connection. In order to make
 * use of this functionality, a resource adapter needs to extend this
 * interface and add it's information.
 */

public interface ConnectionRequestInfo {
    /**
     * Tests object for equality
     */
    public boolean equals( Object other );

    /**
     * Generates a hashCode for this object
     */
    public int hashCode();
}
