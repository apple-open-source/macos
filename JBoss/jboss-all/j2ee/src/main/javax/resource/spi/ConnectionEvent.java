/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.spi;

import javax.resource.ResourceException;

/**
 * The ConnectionEvent class provides information about the source of a
 * Connection related event.  A ConnectionEvent contains:<ul>
 *
 *     <li>Type of connection event.</li>
 *     <li>Managed connection instance that generated the event.</li>
 *     <li>Connection handle associated with the managed connection.</li>
 *     <li>Optionally an exception indicating an error.</li>
 *</ul><p>
 * This class is used for the following types of notifications:<ul>
 *
 *     <li>Connection closed</li>
 *     <li>Local transaction started</li>
 *     <li>Local transaction commited</li>
 *     <li>Local transaction rolled back</li>
 *     <li>Connection error occurred</li>
 *</ul>
 */

public class ConnectionEvent extends java.util.EventObject implements java.io.Serializable {

    /**
     * Connection has been closed
     */
    public static final int CONNECTION_CLOSED = 1;

    /**
     * Local transaction has been started
     */
    public static final int LOCAL_TRANSACTION_STARTED = 2;

    /**
     * Local transaction has been committed
     */
    public static final int LOCAL_TRANSACTION_COMMITTED = 3;

    /**
     * Local transaction has been rolled back 
     */
    public static final int LOCAL_TRANSACTION_ROLLEDBACK = 4;

    /**
     * Connection error has occurred
     */
    public static final int CONNECTION_ERROR_OCCURRED = 5;

    /**
     * Type of event
     */
    protected int id;

    private Exception e = null;
    private Object connectionHandle = null;

    public ConnectionEvent( ManagedConnection source, int eid ) {
        super( source );
	id = eid;
    }

    public ConnectionEvent( ManagedConnection source, int eid,
                            Exception exception ) {
	super( source );
	id = eid;
	e = exception;
    }

    /**
     * Get the event type
     */
    public int getId() {
        return id;
    }

    /**
     * Get the exception
     */
    public Exception getException() {
         return e;
    }

    /**
     * Set the ConnectionHandle
     */
    public void setConnectionHandle( Object connectionHandle ) {
        this.connectionHandle = connectionHandle;
    }

    /**
     * Get the ConnectionHandle
     */
    public Object getConnectionHandle() {
        return connectionHandle;
    }
}
