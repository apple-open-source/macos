/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.spi;

import javax.resource.ResourceException;

/**
 * The LocalTransaction interface is for transactions which are managed
 * locally to the underlying resource and don't need an external transaction
 * manager.
 *
 * If a resource implements the LocalTransaction interface then the
 * Application Server can choose to do local transacton optimization.
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
