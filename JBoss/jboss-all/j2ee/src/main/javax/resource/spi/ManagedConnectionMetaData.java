/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.spi;

import javax.resource.ResourceException;

/**
 * The ManagedConnectionMetaData interface provides information about the
 * underlying resource associated with a ManagedConnetion.  The Application
 * Server can use this information to get information at runtime from the
 * underlying resource.
 */

public interface ManagedConnectionMetaData {
    /**
     * Returns product name of the underlying resource.
     */
    public String getEISProductName() throws ResourceException;

    /**
     * Returns product version of the underlying resource.
     */
    public String getEISProductVersion() throws ResourceException;

    /**
     * Returns the maximum supported number of connections allowed to the
     * underlying resource.
     */
    public int getMaxConnections() throws ResourceException;

    /**
     * Returns user name associated with the underlying connection.
     */
    public String getUserName() throws ResourceException;
}
