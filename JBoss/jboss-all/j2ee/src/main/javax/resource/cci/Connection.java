/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.cci;

import javax.resource.ResourceException;

/**
 * The Connection provides a handle for use by the application to access
 * an underlying physical connection.
 *
 * The client calls the getConnection() method on a ConnectionFactory
 * object to get a Connection.
 */

public interface Connection {

    /**
     * Closes a connection
     */
    public void close() throws ResourceException;

    /**
     * Creates a new interaction associated with this connection.
     */
    public Interaction createInteraction() throws ResourceException;

    /**
     * Gets a LocalTransaction object which allows the client to manage 
     * local transactions for the connection.
     */
    public LocalTransaction getLocalTransaction() throws ResourceException;

    /**
     * Gets meta data for the underlying resource represented by this
     * connection.
     */
    public ConnectionMetaData getMetaData() throws ResourceException;

    /**
     * Gets information on ResultSet functionality supported by the
     * underlying resource for the connection.
     */
    public ResultSetInfo getResultSetInfo() throws ResourceException;
}
