/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.spi;

import javax.resource.ResourceException;

/**
 * A ManagedConnection instance represents a connection to the underlying
 * recource.  A ManagedConnection provides access to the two transaction
 * interfaces, XAResource and LocalTransaction.  These interfaces are used
 * to manage transactions on the resource.
 */

public interface ManagedConnection {
    /**
     * Adds a connection event listener
     */
    public void addConnectionEventListener( ConnectionEventListener listener );

    /**
     * Removes a connection event listener
     */
    public void removeConnectionEventListener( ConnectionEventListener listener );

    /**
     * Associates a new application level connection handle with the
     * connection.
     */
    public void associateConnection( Object connection ) throws ResourceException;

    /**
     * Application server calls this to force cleanup of connection.
     */
    public void cleanup() throws ResourceException;

    /**
     * Destroys the connection to the underlying resource.
     */
    public void destroy() throws ResourceException;

    /**
     * Creates a new connection handle for the underlying connection.
     */
    public Object getConnection( javax.security.auth.Subject subject,
                                 ConnectionRequestInfo cxRequestInfo )
		throws ResourceException;

    /**
     * Returns a LocalTransaction instance.
     */
    public LocalTransaction getLocalTransaction() throws ResourceException;

    /**
     * Gets the logwriter for this instance.
     */
    public java.io.PrintWriter getLogWriter() throws ResourceException;

    /**
     * Gets metadata inormation for this instances underlying resource
     * manager instance.
     */
    public ManagedConnectionMetaData getMetaData() throws ResourceException;

    /**
     * Returns an XAResource instance.
     */
    public javax.transaction.xa.XAResource getXAResource() throws ResourceException;

    /**
     * Sets the logwriter for this instance.
     */
    public void setLogWriter( java.io.PrintWriter out ) throws ResourceException;
}
