/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.cci;

import javax.resource.ResourceException;

/**
 * The LocalTransaction interface is the transaction demarcation interface for
 * transactions local to the resource manager.  This interface is used for
 * application level transaction demarcation, the spi.LocalTransaction
 * interface is used for transaction management within a resource adapter.
 *
 * Implementation of this interface is optional for a resource manager.
 */

public interface LocalTransaction {
    /**
     * Begins a local transaction on the userlying resource.
     */
    public void begin() throws ResourceException;

    /**
     * Commits a local transaction on the userlying resource.
     */
    public void commit() throws ResourceException;

    /**
     * Rolls back a local transaction on the userlying resource.
     */
    public void rollback() throws ResourceException;
}
