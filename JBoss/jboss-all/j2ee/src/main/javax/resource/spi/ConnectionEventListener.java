/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.spi;

import javax.resource.ResourceException;

/**
 * The ConnectionEventListener interface provides for a callback mechanism
 * to enable objects to listen for events of the ConnectionEvent class.
 *
 * An Application server uses these events to manage its connection pools.
 */

public interface ConnectionEventListener extends java.util.EventListener {

    /**
     * Notifies the listener that a connection has been closed
     */
    public void connectionClosed( ConnectionEvent event );

    /**
     * Local transaction has been started
     */
    public void localTransactionStarted( ConnectionEvent event );

    /**
     * Local transaction has been committed
     */
    public void localTransactionCommitted( ConnectionEvent event );

    /**
     * Local transaction has been rolled back 
     */
    public void localTransactionRolledback( ConnectionEvent event );

    /**
     * Connection error has occurred
     */
    public void connectionErrorOccurred( ConnectionEvent event );
}
